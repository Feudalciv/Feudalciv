/********************************************************************** 
 Freeciv - Copyright (C) 1996 - A Kjeldberg, L Gregersen, P Unold
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#ifdef HAVE_NEWLIBREADLINE
#define completion_matches(x,y) rl_completion_matches(x,y)
#define filename_completion_function rl_filename_completion_function
#endif
#endif

#include "astring.h"
#include "events.h"
#include "fcintl.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "player.h"
#include "registry.h"
#include "shared.h"		/* fc__attribute, bool type, etc. */
#include "support.h"
#include "timing.h"
#include "version.h"

#include "citytools.h"
#include "connecthand.h"
#include "console.h"
#include "diplhand.h"
#include "gamehand.h"
#include "gamelog.h"
#include "mapgen.h"
#include "meta.h"
#include "plrhand.h"
#include "report.h"
#include "ruleset.h"
#include "savegame.h"
#include "sernet.h"
#include "srv_main.h"

#include "advmilitary.h"	/* assess_danger_player() */

#include "stdinhand.h"


static enum cmdlevel_id default_access_level = ALLOW_INFO;
static enum cmdlevel_id   first_access_level = ALLOW_CTRL;

static void cut_client_connection(struct connection *caller, char *name);
static void show_help(struct connection *caller, char *arg);
static void show_list(struct connection *caller, char *arg);
static void show_connections(struct connection *caller);
static void set_ai_level(struct connection *caller, char *name, int level);
static void set_away(struct connection *caller, char *name);

static void fix_command(struct connection *caller, char *str, int cmd_enum);
static void take_command(struct connection *caller, char *name);

static const char horiz_line[] =
"------------------------------------------------------------------------------";

/* The following classes determine what can be changed when.
 * Actually, some of them have the same "changeability", but
 * different types are separated here in case they have
 * other uses.
 * Also, SSET_GAME_INIT/SSET_RULES separate the two sections
 * of server settings sent to the client.
 * See the settings[] array for what these correspond to and
 * explanations.
 */
enum sset_class {
  SSET_MAP_SIZE,
  SSET_MAP_GEN,
  SSET_MAP_ADD,
  SSET_PLAYERS,
  SSET_GAME_INIT,
  SSET_RULES,
  SSET_RULES_FLEXIBLE,
  SSET_META,
  SSET_LAST
};

/* Whether settings are sent to the client when the client lists
 * server options; also determines whether clients can set them in principle.
 * Eg, not sent: seeds, saveturns, etc.
 */
enum sset_to_client {
  SSET_TO_CLIENT, SSET_SERVER_ONLY
};

/*
 * The type of the setting.
 */
enum sset_type {
  SSET_BOOL, SSET_INT, SSET_STRING
};

#define SSET_MAX_LEN  16             /* max setting name length (plus nul) */
#define TOKEN_DELIMITERS " \t\n,"

struct settings_s {
  const char *name;
  enum sset_class sclass;
  enum sset_to_client to_client;

  /*
   * Sould be less than 42 chars (?), or shorter if the values may
   * have more than about 4 digits.  Don't put "." on the end.
   */
  const char *short_help;

  /*
   * May be empty string, if short_help is sufficient.  Need not
   * include embedded newlines (but may, for formatting); lines will
   * be wrapped (and indented) automatically.  Should have punctuation
   * etc, and should end with a "."
   */
  const char *extra_help;
  enum sset_type type;

  /* 
   * About the *_validate functions: If the function is non-NULL, it
   * is called with the new value, and returns whether the change is
   * legal. The char * is an error message in the case of reject. 
   */

  /*** bool part ***/
  bool *bool_value;
  bool bool_default_value;
  bool (*bool_validate)(bool, char **);

  /*** int part ***/
  int *int_value;
  int int_default_value;
  bool (*int_validate)(int, char **);
  int int_min_value, int_max_value;

  /*** string part ***/
  char *string_value;
  const char *string_default_value;
  bool (*string_validate)(const char *, char **);
  size_t string_value_size;	/* max size we can write into string_value */
};

/*
 * Triggers used in settings_s.
 */
static bool valid_notradesize(int value, char **reject_message);
static bool valid_fulltradesize(int value, char **reject_message);
static bool autotoggle(bool value, char **reject_message);
static bool is_valid_allowconnect(const char *allow_connect,
				  char **error_string);

static bool valid_max_players(int v, char **r_m)
{
  static char buffer[MAX_LEN_CONSOLE_LINE];

  *r_m = buffer;

  if (v < game.nplayers) {
    my_snprintf(buffer, sizeof(buffer), _("Number of players is higher "
					  "than requested value, keeping "
					  "old value"));
    return FALSE;
  }

  buffer[0] = '\0';
  return TRUE;
}
  
#define GEN_BOOL(name, value, sclass, to_client, short_help, extra_help, func, default) \
 { name, sclass, to_client, short_help, extra_help, SSET_BOOL, \
   &value, default, func, \
   NULL, 0, NULL, 0, 0, \
   NULL, NULL, NULL, 0 },

#define GEN_INT(name, value, sclass, to_client, short_help, extra_help, func, min, max, default) \
 { name, sclass, to_client, short_help, extra_help, SSET_INT, \
   NULL, FALSE, NULL, \
   &value, default, func, min, max, \
   NULL, NULL, NULL, 0 },

#define GEN_STRING(name, value, sclass, to_client, short_help, extra_help, func, default) \
 { name, sclass, to_client, short_help, extra_help, SSET_STRING, \
   NULL, FALSE, NULL, \
   NULL, 0, NULL, 0, 0, \
   value, default, func, sizeof(value)},

#define GEN_END \
 { NULL, SSET_LAST, SSET_SERVER_ONLY, NULL, NULL, SSET_INT, \
   NULL, FALSE, NULL, \
   NULL, 0, NULL, 0, 0, \
   NULL, NULL, NULL, 0 },

static struct settings_s settings[] = {

  /* These should be grouped by sclass */
  
/* Map size parameters: adjustable if we don't yet have a map */  
  GEN_INT("xsize", map.xsize, SSET_MAP_SIZE, SSET_TO_CLIENT,
	  N_("Map width in squares"), "", NULL,
	  MAP_MIN_WIDTH, MAP_MAX_WIDTH, MAP_DEFAULT_WIDTH)
    
  GEN_INT("ysize", map.ysize, SSET_MAP_SIZE, SSET_TO_CLIENT,
	  N_("Map height in squares"), "", NULL,
	  MAP_MIN_HEIGHT, MAP_MAX_HEIGHT, MAP_DEFAULT_HEIGHT)

/* Map generation parameters: once we have a map these are of historical
 * interest only, and cannot be changed.
 */
  GEN_INT("generator", map.generator, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Method used to generate map"),
    N_("1 = standard, with random continents;\n"
       "2 = equally sized large islands with one player each, and "
       "twice that many\n"
       "    smaller islands;\n"
       "3 = equally sized large islands with one player each, and "
       "a number of other\n"
       "    islands of similar size;\n"
       "4 = equally sized large islands with two players on every "
       "island (or one\n"
       "    with three players for an odd number of players), and "
       "additional\n"
       "    smaller islands.\n"
       "5 = one or more large earthlike continents with some scatter.\n"
       "Note: values 2,3 and 4 generate \"fairer\" (but more boring) "
       "maps.\n"
       "(Zero indicates a scenario map.)"), NULL,
	  MAP_MIN_GENERATOR, MAP_MAX_GENERATOR, MAP_DEFAULT_GENERATOR)

  GEN_BOOL("tinyisles", map.tinyisles, SSET_MAP_GEN, SSET_TO_CLIENT,
	   N_("Presence or absence of 1x1 islands"),
	   N_("0 = no 1x1 islands; 1 = some 1x1 islands"), NULL,
	   MAP_DEFAULT_TINYISLES)

  GEN_BOOL("separatepoles", map.separatepoles, SSET_MAP_GEN, SSET_TO_CLIENT,
	   N_("Whether the poles are separate continents"),
	   N_("0 = continents may attach to poles; 1 = poles will "
	      "be separate"), NULL, 
	   MAP_DEFAULT_SEPARATE_POLES)

  GEN_INT("landmass", map.landpercent, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of land vs ocean"), "", NULL,
	  MAP_MIN_LANDMASS, MAP_MAX_LANDMASS, MAP_DEFAULT_LANDMASS)

  GEN_INT("mountains", map.mountains, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of hills/mountains"),
	  N_("Small values give flat maps, higher values give more "
	     "hills and mountains."), NULL,
	  MAP_MIN_MOUNTAINS, MAP_MAX_MOUNTAINS, MAP_DEFAULT_MOUNTAINS)

  GEN_INT("rivers", map.riverlength, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of river squares"), "", NULL,
	  MAP_MIN_RIVERS, MAP_MAX_RIVERS, MAP_DEFAULT_RIVERS)

  GEN_INT("grass", map.grasssize, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of grass squares"), "", NULL,
	  MAP_MIN_GRASS, MAP_MAX_GRASS, MAP_DEFAULT_GRASS)

  GEN_INT("forests", map.forestsize, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of forest squares"), "", NULL, 
	  MAP_MIN_FORESTS, MAP_MAX_FORESTS, MAP_DEFAULT_FORESTS)

  GEN_INT("swamps", map.swampsize, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of swamp squares"), "", NULL, 
	  MAP_MIN_SWAMPS, MAP_MAX_SWAMPS, MAP_DEFAULT_SWAMPS)
    
  GEN_INT("deserts", map.deserts, SSET_MAP_GEN, SSET_TO_CLIENT,
	  N_("Amount of desert squares"), "", NULL, 
	  MAP_MIN_DESERTS, MAP_MAX_DESERTS, MAP_DEFAULT_DESERTS)

  GEN_INT("seed", map.seed, SSET_MAP_GEN, SSET_SERVER_ONLY,
	  N_("Map generation random seed"),
	  N_("The same seed will always produce the same map; "
	     "for zero (the default) a seed will be chosen based on "
	     "the time, to give a random map."), NULL, 
	  MAP_MIN_SEED, MAP_MAX_SEED, MAP_DEFAULT_SEED)

/* Map additional stuff: huts and specials.  randseed also goes here
 * because huts and specials are the first time the randseed gets used (?)
 * These are done when the game starts, so these are historical and
 * fixed after the game has started.
 */
  GEN_INT("randseed", game.randseed, SSET_MAP_ADD, SSET_SERVER_ONLY,
	  N_("General random seed"),
	  N_("For zero (the default) a seed will be chosen based "
	     "on the time."), NULL, 
	  GAME_MIN_RANDSEED, GAME_MAX_RANDSEED, GAME_DEFAULT_RANDSEED)

  GEN_INT("specials", map.riches, SSET_MAP_ADD, SSET_TO_CLIENT,
	  N_("Amount of \"special\" resource squares"), 
	  N_("Special resources improve the basic terrain type they "
	     "are on.  The server variable's scale is parts per "
	     "thousand."), NULL,
	  MAP_MIN_RICHES, MAP_MAX_RICHES, MAP_DEFAULT_RICHES)

  GEN_INT("huts", map.huts, SSET_MAP_ADD, SSET_TO_CLIENT,
	  N_("Amount of huts (minor tribe villages)"), "", NULL, 
	  MAP_MIN_HUTS, MAP_MAX_HUTS, MAP_DEFAULT_HUTS)

/* Options affecting numbers of players and AI players.  These only
 * affect the start of the game and can not be adjusted after that.
 * (Actually, minplayers does also affect reloads: you can't start a
 * reload game until enough players have connected (or are AI).)
 */
  GEN_INT("minplayers", game.min_players, SSET_PLAYERS, SSET_TO_CLIENT,
	  N_("Minimum number of players"),
	  N_("There must be at least this many players (connected "
	     "players or AI's) before the game can start."), NULL,
	  GAME_MIN_MIN_PLAYERS, GAME_MAX_MIN_PLAYERS,
	  GAME_DEFAULT_MIN_PLAYERS)
  
  GEN_INT("maxplayers", game.max_players, SSET_PLAYERS, SSET_TO_CLIENT,
	  N_("Maximum number of players"),
          N_("The maximal number of human and AI players who can be in "
             "the game. When this number of players are connected in "
             "the pregame state, any new players who try to connect "
             "will be rejected."), valid_max_players,
	  GAME_MIN_MAX_PLAYERS, GAME_MAX_MAX_PLAYERS,
	  GAME_DEFAULT_MAX_PLAYERS)

  GEN_INT("aifill", game.aifill, SSET_PLAYERS, SSET_TO_CLIENT,
	  N_("Number of players to fill to with AI's"),
	  N_("If there are fewer than this many players when the "
	     "game starts, extra AI players will be created to "
	     "increase the total number of players to the value of "
	     "this option."), NULL, 
	  GAME_MIN_AIFILL, GAME_MAX_AIFILL, GAME_DEFAULT_AIFILL)

/* Game initialization parameters (only affect the first start of the game,
 * and not reloads).  Can not be changed after first start of game.
 */
  GEN_INT("settlers", game.settlers, SSET_GAME_INIT, SSET_TO_CLIENT,
	  N_("Number of initial settlers per player"), "", NULL, 
	  GAME_MIN_SETTLERS, GAME_MAX_SETTLERS, GAME_DEFAULT_SETTLERS)

  GEN_INT("explorer", game.explorer, SSET_GAME_INIT, SSET_TO_CLIENT,
	  N_("Number of initial explorers per player"), "", NULL, 
	  GAME_MIN_EXPLORER, GAME_MAX_EXPLORER, GAME_DEFAULT_EXPLORER)

  GEN_INT("dispersion", game.dispersion, SSET_GAME_INIT, SSET_TO_CLIENT,
	  N_("Area where initial units are located"),
	  N_("This is half the length of a side of the square within "
	     "which the initial units are dispersed."), NULL,
	  GAME_MIN_DISPERSION, GAME_MAX_DISPERSION, GAME_DEFAULT_DISPERSION)

  GEN_INT("gold", game.gold, SSET_GAME_INIT, SSET_TO_CLIENT,
	  N_("Starting gold per player"), "", NULL,
	  GAME_MIN_GOLD, GAME_MAX_GOLD, GAME_DEFAULT_GOLD)

  GEN_INT("techlevel", game.tech, SSET_GAME_INIT, SSET_TO_CLIENT,
	  N_("Number of initial advances per player"), "", NULL,
	  GAME_MIN_TECHLEVEL, GAME_MAX_TECHLEVEL, GAME_DEFAULT_TECHLEVEL)

  GEN_INT("researchcost", game.researchcost, SSET_RULES, SSET_TO_CLIENT,
	  N_("Points required to gain a new advance"),
	  N_("This affects how quickly players can research new "
	     "technology."), NULL,
	  GAME_MIN_RESEARCHCOST, GAME_MAX_RESEARCHCOST, 
	  GAME_DEFAULT_RESEARCHCOST)

  GEN_INT("techpenalty", game.techpenalty, SSET_RULES, SSET_TO_CLIENT,
	  N_("Percentage penalty when changing tech"),
	  N_("If you change your current research technology, and you have "
	     "positive research points, you lose this percentage of those "
	     "research points.  This does not apply if you have just gained "
	     "tech this turn."), NULL,
	  GAME_MIN_TECHPENALTY, GAME_MAX_TECHPENALTY,
	  GAME_DEFAULT_TECHPENALTY)

  GEN_INT("diplcost", game.diplcost, SSET_RULES, SSET_TO_CLIENT,
	  N_("Penalty when getting tech from treaty"),
	  N_("For each advance you gain from a diplomatic treaty, you lose "
	     "research points equal to this percentage of the cost to "
	     "research an new advance.  You can end up with negative "
	     "research points if this is non-zero."), NULL, 
	  GAME_MIN_DIPLCOST, GAME_MAX_DIPLCOST, GAME_DEFAULT_DIPLCOST)

  GEN_INT("conquercost", game.conquercost, SSET_RULES, SSET_TO_CLIENT,
	  N_("Penalty when getting tech from conquering"),
	  N_("For each advance you gain by conquering an enemy city, you "
	     "lose research points equal to this percentage of the cost "
	     "to research an new advance.  You can end up with negative "
	     "research points if this is non-zero."), NULL,
	  GAME_MIN_CONQUERCOST, GAME_MAX_CONQUERCOST,
	  GAME_DEFAULT_CONQUERCOST)

  GEN_INT("freecost", game.freecost, SSET_RULES, SSET_TO_CLIENT,
	  N_("Penalty when getting a free tech"),
	  N_("For each advance you gain \"for free\" (other than covered by "
	     "diplcost or conquercost: specifically, from huts or from the "
	     "Great Library), you lose research points equal to this "
	     "percentage of the cost to research a new advance.  You can "
	     "end up with negative research points if this is non-zero."), 
	  NULL, 
	  GAME_MIN_FREECOST, GAME_MAX_FREECOST, GAME_DEFAULT_FREECOST)

  GEN_INT("foodbox", game.foodbox, SSET_RULES, SSET_TO_CLIENT,
	  N_("Food required for a city to grow"), "", NULL,
	  GAME_MIN_FOODBOX, GAME_MAX_FOODBOX, GAME_DEFAULT_FOODBOX)

  GEN_INT("aqueductloss", game.aqueductloss, SSET_RULES, SSET_TO_CLIENT,
	  N_("Percentage food lost when need aqueduct"),
	  N_("If a city would expand, but it can't because it needs "
	     "an Aqueduct (or Sewer System), it loses this percentage "
	     "of its foodbox (or half that amount if it has a "
	     "Granary)."), NULL, 
	  GAME_MIN_AQUEDUCTLOSS, GAME_MAX_AQUEDUCTLOSS, 
	  GAME_DEFAULT_AQUEDUCTLOSS)

  GEN_INT("fulltradesize", game.fulltradesize, SSET_RULES, SSET_TO_CLIENT,
	  N_("Minimum city size to get full trade"),
	  N_("There is a trade penalty in all cities smaller than this. "
	     "The penalty is 100% (no trade at all) for sizes up to "
	     "notradesize, and decreases gradually to 0% (no penalty "
	     "except the normal corruption) for size=fulltradesize.  "
	     "See also notradesize."), valid_fulltradesize, 
	  GAME_MIN_FULLTRADESIZE, GAME_MAX_FULLTRADESIZE, 
	  GAME_DEFAULT_FULLTRADESIZE)

  GEN_INT("notradesize", game.notradesize, SSET_RULES, SSET_TO_CLIENT,
	  N_("Maximum size of a city without trade"),
	  N_("All the cities of smaller or equal size to this do not "
	     "produce trade at all. The produced trade increases "
	     "gradually for cities larger than notradesize and smaller "
	     "than fulltradesize.  See also fulltradesize."),
	  valid_notradesize,
	  GAME_MIN_NOTRADESIZE, GAME_MAX_NOTRADESIZE,
	  GAME_DEFAULT_NOTRADESIZE)

  GEN_INT("unhappysize", game.unhappysize, SSET_RULES, SSET_TO_CLIENT,
	  N_("City size before people become unhappy"),
	  N_("Before other adjustments, the first unhappysize citizens in a "
	     "city are content, and subsequent citizens are unhappy.  "
	     "See also cityfactor."), NULL,
	  GAME_MIN_UNHAPPYSIZE, GAME_MAX_UNHAPPYSIZE,
	  GAME_DEFAULT_UNHAPPYSIZE)

  GEN_BOOL("angrycitizen", game.angrycitizen, SSET_RULES, SSET_TO_CLIENT,
	  N_("Whether angry citizens are enabled"),
	  N_("Introduces angry citizens like civilization II. Angry "
	     "citizens have to become unhappy before any other class "
	     "of citizens may be considered. See also unhappysize, "
	     "cityfactor and governments."), NULL, 
	  GAME_DEFAULT_ANGRYCITIZEN)

  GEN_INT("cityfactor", game.cityfactor, SSET_RULES, SSET_TO_CLIENT,
	  N_("Number of cities for higher unhappiness"),
	  N_("When the number of cities a player owns is greater than "
	     "cityfactor, one extra citizen is unhappy before other "
	     "adjustments; see also unhappysize.  This assumes a "
	     "Democracy; for other governments the effect occurs at "
	     "smaller numbers of cities."), NULL, 
	  GAME_MIN_CITYFACTOR, GAME_MAX_CITYFACTOR, GAME_DEFAULT_CITYFACTOR)

  GEN_INT("citymindist", game.citymindist, SSET_RULES, SSET_TO_CLIENT,
	  N_("Minimum distance between cities (move distance)"),
	  N_("When a player founds a new city, it is checked if there is "
	     "no other city in citymindist distance. For example, if "
	     "citymindist is 3, there have to be at least two empty "
	     "fields between two cities every direction. If it is set "
	     "to 0 (default), it is overwritten by the current ruleset "
	     "when the game starts."), NULL,
	  GAME_MIN_CITYMINDIST, GAME_MAX_CITYMINDIST,
	  GAME_DEFAULT_CITYMINDIST)
  
  GEN_INT("rapturedelay", game.rapturedelay, SSET_RULES, SSET_TO_CLIENT,
          N_("Number of turns between rapture effect"),
          N_("Sets the number of turns between rapture growth of a city. "
             "If set to n a city will grow after rapturing n+1 turns."), NULL,
          GAME_MIN_RAPTUREDELAY, GAME_MAX_RAPTUREDELAY,
          GAME_DEFAULT_RAPTUREDELAY)

  GEN_INT("razechance", game.razechance, SSET_RULES, SSET_TO_CLIENT,
	  N_("Chance for conquered building destruction"),
	  N_("When a player conquers a city, each City Improvement has this "
	     "percentage chance to be destroyed."), NULL, 
	  GAME_MIN_RAZECHANCE, GAME_MAX_RAZECHANCE, GAME_DEFAULT_RAZECHANCE)

  GEN_INT("civstyle", game.civstyle, SSET_RULES, SSET_TO_CLIENT,
	  N_("Style of Civ rules"),
	  N_("Sets some basic rules; 1 means style of Civ1, 2 means Civ2.\n"
	     "Currently this option affects the following rules:\n"
	     "  - Apollo shows whole map in Civ2, only cities in Civ1.\n"
	     "See also README.rulesets."), NULL, 
	  GAME_MIN_CIVSTYLE, GAME_MAX_CIVSTYLE, GAME_DEFAULT_CIVSTYLE)

  GEN_INT("occupychance", game.occupychance, SSET_RULES, SSET_TO_CLIENT,
	  N_("Chance of moving into tile after attack"),
	  N_("If set to 0, combat is Civ1/2-style (when you attack, "
	     "you remain in place).  If set to 100, attacking units "
	     "will always move into the tile they attacked if they win "
	     "the combat (and no enemy units remain in the tile).  If "
	     "set to a value between 0 and 100, this will be used as "
	     "the percent chance of \"occupying\" territory."), NULL, 
	  GAME_MIN_OCCUPYCHANCE, GAME_MAX_OCCUPYCHANCE, 
	  GAME_DEFAULT_OCCUPYCHANCE)

  GEN_INT("killcitizen", game.killcitizen, SSET_RULES, SSET_TO_CLIENT,
	  N_("Reduce city population after attack"),
	  N_("This flag indicates if city population is reduced "
	     "after successful attack of enemy unit, depending on "
	     "its movement type (OR-ed):\n"
	     "  1 = land\n"
	     "  2 = sea\n"
	     "  4 = heli\n"
	     "  8 = air"), NULL,
	  GAME_MIN_KILLCITIZEN, GAME_MAX_KILLCITIZEN,
	  GAME_DEFAULT_KILLCITIZEN)

  GEN_INT("wtowervision", game.watchtower_vision, SSET_RULES, SSET_TO_CLIENT,
	  N_("Range of vision for units in a fortress"),
	  N_("watchtower vision range: If set to 1, it has no effect. "
	     "If 2 or larger, the vision range of a unit inside a "
	     "fortress is set to this value, if the necessary invention "
	     "has been made. This invention is determined by the flag "
	     "'Watchtower' in the techs ruleset. Also see wtowerevision."), 
	  NULL, 
	  GAME_MIN_WATCHTOWER_VISION, GAME_MAX_WATCHTOWER_VISION, 
	  GAME_DEFAULT_WATCHTOWER_VISION)

  GEN_INT("wtowerevision", game.watchtower_extra_vision, SSET_RULES,
	  SSET_TO_CLIENT,
	  N_("Extra vision range for units in a fortress"),
	  N_("watchtower extra vision range: If set to 0, it has no "
	     "effect. If larger than 0, the visionrange of a unit is "
	     "raised by this value, if the unit is inside a fortress "
	     "and the invention determined by the flag 'Watchtower' "
	     "in the techs ruleset has been made. Always the larger "
	     "value of wtowervision and wtowerevision will be used. "
	     "Also see wtowervision."), NULL, 
	  GAME_MIN_WATCHTOWER_EXTRA_VISION, GAME_MAX_WATCHTOWER_EXTRA_VISION, 
	  GAME_DEFAULT_WATCHTOWER_EXTRA_VISION)

  GEN_INT("citynames", game.allowed_city_names, SSET_RULES, SSET_TO_CLIENT,
	  N_("Allowed city names"),
	  N_("If set to 0, there are no restrictions: players can have "
	     "multiple cities with the same names. "
	     "If set to 1, city names have to be unique to a player: "
	     "one player can't have multiple cities with the same name. "
	     "If set to 2 or 3, city names have to be globally unique: "
	     "all cities in a game have to have different names. "
	     "If set to 3, a player isn't allowed to use a default city name "
	     "of another nations."),NULL,
	  GAME_MIN_ALLOWED_CITY_NAMES, GAME_MAX_ALLOWED_CITY_NAMES, 
	  GAME_DEFAULT_ALLOWED_CITY_NAMES)
  
/* Flexible rules: these can be changed after the game has started.
 *
 * The distinction between "rules" and "flexible rules" is not always
 * clearcut, and some existing cases may be largely historical or
 * accidental.  However some generalizations can be made:
 *
 *   -- Low-level game mechanics should not be flexible (eg, rulesets).
 *   -- Options which would affect the game "state" (city production etc)
 *      should not be flexible (eg, foodbox).
 *   -- Options which are explicitly sent to the client (eg, in
 *      packet_game_info) should probably not be flexible, or at
 *      least need extra care to be flexible.
 */
  GEN_INT("barbarians", game.barbarianrate, SSET_RULES_FLEXIBLE,
	  SSET_TO_CLIENT,
	  N_("Barbarian appearance frequency"),
	  N_("0 - no barbarians \n"
	     "1 - barbarians only in huts \n"
	     "2 - normal rate of barbarian appearance \n"
	     "3 - frequent barbarian uprising \n"
	     "4 - raging hordes, lots of barbarians"), NULL, 
	  GAME_MIN_BARBARIANRATE, GAME_MAX_BARBARIANRATE, 
	  GAME_DEFAULT_BARBARIANRATE)

  GEN_INT("onsetbarbs", game.onsetbarbarian, SSET_RULES_FLEXIBLE,
	  SSET_TO_CLIENT,
	  N_("Barbarian onset year"),
	  N_("Barbarians will not appear before this year."), NULL, 
	  GAME_MIN_ONSETBARBARIAN, GAME_MAX_ONSETBARBARIAN, 
	  GAME_DEFAULT_ONSETBARBARIAN)

  GEN_BOOL("fogofwar", game.fogofwar, SSET_RULES_FLEXIBLE, SSET_TO_CLIENT,
	  N_("Whether to enable fog of war"),
	  N_("If this is set to 1, only those units and cities within "
	     "the sightrange of your own units and cities will be "
	     "revealed to you. You will not see new cities or terrain "
	     "changes in squares not observed."), NULL, 
	  GAME_DEFAULT_FOGOFWAR)

  GEN_INT("diplchance", game.diplchance, SSET_RULES_FLEXIBLE, SSET_TO_CLIENT,
	  N_("Chance in diplomat/spy contests"),
	  /* xgettext:no-c-format */
	  N_("A Diplomat or Spy acting against a city which has one or "
	     "more defending Diplomats or Spies has a diplchance "
	     "(percent) chance to defeat each such defender.  Also, the "
	     "chance of a Spy returning from a successful mission is "
	     "diplchance percent.  (Diplomats never return.)  Also, a "
	     "basic chance of success for Diplomats and Spies. "
	     "Defending Spys are generally twice as capable as "
	     "Diplomats, veteran units 50% more capable than "
	     "non-veteran ones."), NULL, 
	  GAME_MIN_DIPLCHANCE, GAME_MAX_DIPLCHANCE, GAME_DEFAULT_DIPLCHANCE)

  GEN_BOOL("spacerace", game.spacerace, SSET_RULES_FLEXIBLE, SSET_TO_CLIENT,
	   N_("Whether to allow space race"),
	   N_("If this option is 1, players can build spaceships."), NULL, 
	   GAME_DEFAULT_SPACERACE)

  GEN_INT("civilwarsize", game.civilwarsize, SSET_RULES_FLEXIBLE,
	  SSET_TO_CLIENT,
	  N_("Minimum number of cities for civil war"),
	  N_("A civil war is triggered if a player has at least this "
	     "many cities and the player's capital is captured.  If "
	     "this option is set to the maximum value, civil wars are "
	     "turned off altogether."), NULL, 
	  GAME_MIN_CIVILWARSIZE, GAME_MAX_CIVILWARSIZE, 
	  GAME_DEFAULT_CIVILWARSIZE)

  GEN_BOOL("savepalace", game.savepalace, SSET_RULES_FLEXIBLE, SSET_TO_CLIENT,
	   N_("Rebuild palace if capital is conquered"),
	   N_("If this is set to 1 when the capital is conquered, palace "
	      "is automatically rebuilt for free in another randomly "
	      "choosed city, regardless on the knowledge of Masonry."), NULL, 
	   GAME_DEFAULT_SAVEPALACE)

  GEN_BOOL("naturalcitynames", game.natural_city_names,
           SSET_RULES_FLEXIBLE, SSET_TO_CLIENT,
           N_("Whether to use natural city names"),
           N_("If enabled, the default city names will be determined based "
              "on the surrounding terrain."),
           NULL, GAME_DEFAULT_NATURALCITYNAMES)

/* Meta options: these don't affect the internal rules of the game, but
 * do affect players.  Also options which only produce extra server
 * "output" and don't affect the actual game.
 * ("endyear" is here, and not RULES_FLEXIBLE, because it doesn't
 * affect what happens in the game, it just determines when the
 * players stop playing and look at the score.)
 */
  GEN_STRING("allowconnect", game.allow_connect, SSET_META, SSET_TO_CLIENT,
	     N_("What connections are allowed"),
	     N_("This should be a string of characters, each of which "
		"specifies a type or status of a civilization, or "
		"\"player\".  Clients will only be permitted to connect "
		"to those players which match one of the specified "
		"letters.  This only affects future connections, and "
		"existing connections are unchanged.  "
		"The characters and their meanings are:\n"
		"    N   = New players (only applies pre-game)\n"
		"    H,h = Human players (pre-existing)\n"
		"    A,a = AI players\n"
		"    d   = Dead players\n"
		"    b   = Barbarian players\n"
		"The first description from the _bottom_ which matches a "
		"player is the one which applies.  Thus 'd' does not "
		"include Barbarians, 'a' does not include dead AI "
		"players, and so on.  Upper case letters apply before "
		"the game has started, lower case letters afterwards.\n"
		"Each character above may be followed by one of the "
		"following special characters to allow or restrict "
		"multiple and observer connections:\n"
		"   * = Multiple controlling connections allowed;\n"
		"   + = One controller and multiple observers allowed;\n"
		"   = = Multiple observers allowed, no controllers;\n"
		"   - = Single observer only, no controllers;\n"
		"   (none) = Single controller only.\n"
		"Multiple connections and observer connections are "
		"currently EXPERIMENTAL."), is_valid_allowconnect,
	     GAME_DEFAULT_ALLOW_CONNECT)

  GEN_BOOL("autotoggle", game.auto_ai_toggle, SSET_META, SSET_TO_CLIENT,
	   N_("Whether AI-status toggles with connection"),
	   N_("If this is set to 1, AI status is turned off when a player "
	      "connects, and on when a player disconnects."), autotoggle, 
	   GAME_DEFAULT_AUTO_AI_TOGGLE)

  GEN_INT("endyear", game.end_year, SSET_META, SSET_TO_CLIENT,
	  N_("Year the game ends"), "", NULL, 
	  GAME_MIN_END_YEAR, GAME_MAX_END_YEAR, GAME_DEFAULT_END_YEAR)

#ifndef NDEBUG
  GEN_INT( "timeout", game.timeout, SSET_META, SSET_TO_CLIENT,
	   N_("Maximum seconds per turn"),
	   N_("If all players have not hit \"Turn Done\" before this "
	      "time is up, then the turn ends automatically. Zero "
	      "means there is no timeout. In DEBUG servers, a timeout "
	      "of -1 sets the autogame test mode. Use this with the command "
              "\"timeoutincrease\" to have a dynamic timer."), NULL, 
	   GAME_MIN_TIMEOUT, GAME_MAX_TIMEOUT, GAME_DEFAULT_TIMEOUT)
#else
  GEN_INT( "timeout", game.timeout, SSET_META, SSET_TO_CLIENT,
	   N_("Maximum seconds per turn"),
	   N_("If all players have not hit \"Turn Done\" before this "
	      "time is up, then the turn ends automatically. Zero "
	      "means there is no timeout. Use this with the command "
              "\"timeoutincrease\" to have a dynamic timer."), NULL, 
	   GAME_MIN_TIMEOUT, GAME_MAX_TIMEOUT, GAME_DEFAULT_TIMEOUT)
#endif

  GEN_INT("tcptimeout", game.tcptimeout, SSET_META, SSET_TO_CLIENT,
	  N_("Seconds to let a client connection block"),
	  N_("If a TCP connection is blocking for a time greater than "
	     "this value, then the TCP connection is closed. Zero "
	     "means there is no timeout beyond that enforced by the "
	     "TCP protocol implementation itself."), NULL, 
	  GAME_MIN_TCPTIMEOUT, GAME_MAX_TCPTIMEOUT, GAME_DEFAULT_TCPTIMEOUT)

  GEN_INT("netwait", game.netwait, SSET_META, SSET_TO_CLIENT,
	  N_("Max seconds for TCP buffers to drain"),
	  N_("The civserver will wait for upto the value of this "
	     "parameter in seconds, for all client connection TCP "
	     "buffers to unblock. Zero means the server will not "
	     "wait at all."), NULL, 
	  GAME_MIN_NETWAIT, GAME_MAX_NETWAIT, GAME_DEFAULT_NETWAIT)

  GEN_INT("pingtime", game.pingtime, SSET_META, SSET_TO_CLIENT,
	  N_("Seconds between PINGs"),
	  N_("The civserver will poll the clients with a PING request "
	     "each time this period elapses."), NULL, 
	  GAME_MIN_PINGTIME, GAME_MAX_PINGTIME, GAME_DEFAULT_PINGTIME)

  GEN_INT("pingtimeout", game.pingtimeout, SSET_META, SSET_TO_CLIENT,
	  N_("Time to cut a client"),
	  N_("If a client doesn't reply to a PONG in this time the "
	     "client is disconnected."), NULL, 
	  GAME_MIN_PINGTIMEOUT, GAME_MAX_PINGTIMEOUT, GAME_DEFAULT_PINGTIMEOUT)

  GEN_BOOL("turnblock", game.turnblock, SSET_META, SSET_TO_CLIENT,
	   N_("Turn-blocking game play mode"),
	   N_("If this is set to 1 the game turn is not advanced "
	      "until all players have finished their turn, including "
	      "disconnected players."), NULL, 
	   FALSE)

  GEN_BOOL("fixedlength", game.fixedlength, SSET_META, SSET_TO_CLIENT,
	   N_("Fixed-length turns play mode"),
	   N_("If this is set to 1 the game turn will not advance "
	      "until the timeout has expired, irrespective of players "
	      "clicking on \"Turn Done\"."), NULL,
	   FALSE)
  
  GEN_STRING("demography", game.demography, SSET_META, SSET_TO_CLIENT,
	     N_("What is in the Demographics report"),
	     N_("This should be a string of characters, each of which "
		"specifies the the inclusion of a line of information "
		"in the Demographics report.\n"
		"The characters and their meanings are:\n"
		"    N = include Population           P = include Production\n"
		"    A = include Land Area            E = include Economics\n"
		"    S = include Settled Area         M = include Military Service\n"
		"    R = include Research Speed       O = include Pollution\n"
		"    L = include Literacy\n"
		"Additionally, the following characters control whether "
		"or not certain columns are displayed in the report:\n"
		"    q = display \"quantity\" column    r = display \"rank\" column\n"
		"    b = display \"best nation\" column\n"
		"(The order of these characters is not significant, but their case is.)"),
	     is_valid_demography,
	     GAME_DEFAULT_DEMOGRAPHY)

  GEN_INT("saveturns", game.save_nturns, SSET_META, SSET_SERVER_ONLY,
	  N_("Turns per auto-save"),
	  N_("The game will be automatically saved per this number of turns.\n"
	     "Zero means never auto-save."), NULL, 
	  0, 200, 10)

  /* Could undef entire option if !HAVE_LIBZ, but this way users get to see
   * what they're missing out on if they didn't compile with zlib?  --dwp
   */
#ifdef HAVE_LIBZ
  GEN_INT("compress", game.save_compress_level, SSET_META, SSET_SERVER_ONLY,
	  N_("Savegame compression level"),
	  N_("If non-zero, saved games will be compressed using zlib "
	     "(gzip format).  Larger values will give better "
	     "compression but take longer.  If the maximum is zero "
	     "this server was not compiled to use zlib."), NULL, 

	  GAME_MIN_COMPRESS_LEVEL, GAME_MAX_COMPRESS_LEVEL,
	  GAME_DEFAULT_COMPRESS_LEVEL)
#else
  GEN_INT("compress", game.save_compress_level, SSET_META, SSET_SERVER_ONLY,
	  N_("Savegame compression level"),
	  N_("If non-zero, saved games will be compressed using zlib "
	     "(gzip format).  Larger values will give better "
	     "compression but take longer.  If the maximum is zero "
	     "this server was not compiled to use zlib."), NULL, 

	  GAME_NO_COMPRESS_LEVEL, GAME_NO_COMPRESS_LEVEL, 
	  GAME_NO_COMPRESS_LEVEL)
#endif

  GEN_STRING("savename", game.save_name, SSET_META, SSET_SERVER_ONLY,
	     N_("Auto-save name prefix"),
	     N_("Automatically saved games will have name "
		"\"<prefix><year>.sav\".\nThis setting sets "
		"the <prefix> part."), NULL,
	     GAME_DEFAULT_SAVE_NAME)

  GEN_BOOL("scorelog", game.scorelog, SSET_META, SSET_SERVER_ONLY,
	   N_("Whether to log player statistics"),
	   N_("If this is set to 1, player statistics are appended to "
	      "the file \"civscore.log\" every turn.  These statistics "
	      "can be used to create power graphs after the game."), NULL,
	   GAME_DEFAULT_SCORELOG)

  GEN_INT("gamelog", gamelog_level, SSET_META, SSET_SERVER_ONLY,
	  N_("Detail level for logging game events"),
	  N_("Only applies if the game log feature is enabled "
	     "(with the -g command line option).  "
	     "Levels: 0=no logging, 20=standard logging, 30=detailed logging, "
	     "40=debuging logging."), NULL, 
	  0, 40, 20)

  GEN_END
};

#define SETTINGS_NUM (ARRAY_SIZE(settings)-1)

/********************************************************************
Returns whether the specified server setting (option) has been
explicitly protected from change at runtime with /fix.  Array of booleans.
*********************************************************************/
static bool sset_is_fixed[SETTINGS_NUM];

/********************************************************************
Returns whether the specified server setting (option) can currently
be changed.  Does not indicate whether it can be changed by clients.
*********************************************************************/
static bool sset_is_changeable(int idx)
{
  struct settings_s *op = &settings[idx];

  switch(op->sclass) {
  case SSET_MAP_SIZE:
  case SSET_MAP_GEN:
    /* Only change map options if we don't yet have a map: */
    return map_is_empty();	
  case SSET_MAP_ADD:
  case SSET_PLAYERS:
  case SSET_GAME_INIT:
  case SSET_RULES:
    /* Only change start params and most rules if we don't yet have a map,
     * or if we do have a map but its a scenario one (ie, the game has
     * never actually been started).
     */
    return (map_is_empty() || game.is_new_game);
  case SSET_RULES_FLEXIBLE:
  case SSET_META:
    /* These can always be changed: */
    return TRUE;
  default:
    freelog(LOG_ERROR, "Unexpected case %d in %s line %d",
	    op->sclass, __FILE__, __LINE__);
    return FALSE;
  }
}

/********************************************************************
Returns whether the specified server setting (option) can ever be changed
by a client while the game is running.  Does not test whether the client
has sufficient access or the option has been /fixed.
*********************************************************************/
static bool sset_is_runtime_changeable_by_client(int idx)
{
  struct settings_s *op = &settings[idx];

  switch(op->sclass) {
  case SSET_RULES_FLEXIBLE:
  case SSET_META:
    return TRUE;
  default:
   return FALSE;
  }
}

/********************************************************************
Returns whether the specified server setting (option) should be
sent to the client.
*********************************************************************/
static bool sset_is_to_client(int idx)
{
  return (settings[idx].to_client == SSET_TO_CLIENT);
}

typedef enum {
    PNameOk,
    PNameEmpty,
    PNameTooLong
} PlayerNameStatus;

/**************************************************************************
...
**************************************************************************/
static PlayerNameStatus test_player_name(char* name)
{
  size_t len = strlen(name);

  if (len == 0) {
      return PNameEmpty;
  } else if (len > MAX_LEN_NAME-1) {
      return PNameTooLong;
  }

  return PNameOk;
}

/**************************************************************************
  Commands - can be recognised by unique prefix
**************************************************************************/
struct command {
  const char *name;             /* name - will be matched by unique prefix   */
  enum cmdlevel_id level;  	/* access level required to use the command  */
  const char *synopsis;	   	/* one or few-line summary of usage */
  const char *short_help;	/* one line (about 70 chars) description */
  const char *extra_help;	/* extra help information; will be line-wrapped */
};

/* Order here is important: for ambiguous abbreviations the first
   match is used.  Arrange order to:
   - allow old commands 's', 'h', 'l', 'q', 'c' to work.
   - reduce harm for ambiguous cases, where "harm" includes inconvenience,
     eg accidently removing a player in a running game.
*/
enum command_id {
  /* old one-letter commands: */
  CMD_START_GAME = 0,
  CMD_HELP,
  CMD_LIST,
  CMD_QUIT,
  CMD_CUT,

  /* completely non-harmful: */
  CMD_EXPLAIN,
  CMD_SHOW,
  CMD_SCORE,
  CMD_WALL,
  
  /* mostly non-harmful: */
  CMD_SET,
  CMD_TEAM,
  CMD_FIX,
  CMD_UNFIX,
  CMD_RULESETDIR,
  CMD_RENAME,
  CMD_METAINFO,
  CMD_METACONN,
  CMD_METASERVER,
  CMD_AITOGGLE,
  CMD_TAKE,
  CMD_CREATE,
  CMD_AWAY,
  CMD_EASY,
  CMD_NORMAL,
  CMD_HARD,
#ifdef DEBUG
  CMD_EXPERIMENTAL,
#endif
  CMD_CMDLEVEL,
  CMD_FIRSTLEVEL,
  CMD_TIMEOUT,

  /* potentially harmful: */
  CMD_END_GAME,
  CMD_REMOVE,
  CMD_SAVE,
  CMD_LOAD,
  CMD_READ_SCRIPT,
  CMD_WRITE_SCRIPT,

  /* undocumented */
  CMD_RFCSTYLE,

  /* pseudo-commands: */
  CMD_NUM,		/* the number of commands - for iterations */
  CMD_UNRECOGNIZED,	/* used as a possible iteration result */
  CMD_AMBIGUOUS		/* used as a possible iteration result */
};


static const struct command commands[] = {
  {"start",	ALLOW_CTRL,
   "start",
   N_("Start the game, or restart after loading a savegame."),
   N_("This command starts the game.  When starting a new game, "
      "it should be used after all human players have connected, and "
      "AI players have been created (if required), and any desired "
      "changes to initial server options have been made.  "
      "After 'start', each human player will be able to "
      "choose their nation, and then the game will begin.  "
      "This command is also required after loading a savegame "
      "for the game to recommence.  Once the game is running this command "
      "is no longer available, since it would have no effect.")
  },

  {"help",	ALLOW_INFO,
   /* TRANS: translate text between <> only */
   N_("help\n"
      "help commands\n"
      "help options\n"
      "help <command-name>\n"
      "help <option-name>"),
   N_("Show help about server commands and server options."),
   N_("With no arguments gives some introductory help.  "
      "With argument \"commands\" or \"options\" gives respectively "
      "a list of all commands or all options.  "
      "Otherwise the argument is taken as a command name or option name, "
      "and help is given for that command or option.  For options, the help "
      "information includes the current and default values for that option.  "
      "The argument may be abbreviated where unambiguous.")
  },

  {"list",	ALLOW_INFO,
   "list\n"
   "list players\n"
   "list connections",
   N_("Show a list of players or connections."),
   N_("Show a list of players, or a list of connections to the server.  "
      "The argument may be abbreviated, and defaults to 'players' if absent.")
  },
  {"quit",	ALLOW_HACK,
   "quit",
   N_("Quit the game and shutdown the server."), NULL
  },
  {"cut",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("cut <connection-name>"),
   N_("Cut a client's connection to server."),
   N_("Cut specified client's connection to the server, removing that client "
      "from the game.  If the game has not yet started that client's player "
      "is removed from the game, otherwise there is no effect on the player.  "
      "Note that this command now takes connection names, not player names.")
  },
  {"explain",	ALLOW_INFO,
   /* TRANS: translate text between <> only */
   N_("explain\n"
      "explain <option-name>"),
   N_("Explain server options."),
   N_("The 'explain' command gives a subset of the functionality of 'help', "
      "and is included for backward compatibility.  With no arguments it "
      "gives a list of options (like 'help options'), and with an argument "
      "it gives help for a particular option (like 'help <option-name>').")
  },
  {"show",	ALLOW_INFO,
   /* TRANS: translate text between <> only */
   N_("show\n"
      "show <option-name>\n"
      "show <option-prefix>"),
   N_("Show server options."),
   N_("With no arguments, shows all server options (or available options, when "
      "used by clients).  With an argument, show only the named option, "
      "or options with that prefix.")
  },
  {"score",	ALLOW_CTRL,
   "score",
   N_("Show current scores."),
   N_("For each connected client, pops up a window showing the current "
      "player scores.")
  },
  {"wall",	ALLOW_HACK,
   N_("wall <message>"),
   N_("Send message to all connections."),
   N_("For each connected client, pops up a window showing the message "
      "entered.")
  },
  {"set",	ALLOW_CTRL,
   N_("set <option-name> <value>"),
   N_("Set server option."), NULL
  },
  {"team",	ALLOW_CTRL,
   N_("team <player> [team]"),
   N_("Change, add or remove a player's team affiliation."),
   N_("Sets a player as member of a team. If no team specified, the "
      "player is set teamless. Use \"\" if names contain whitespace. "
      "A team is a group of players that start out allied, with shared "
      "vision and embassies, and fight together to achieve team victory "
      "with averaged individual scores.")
  },
  {"fix",       ALLOW_CTRL,
   N_("fix <option-name>"),
   N_("Make server option unchangeable during game."), NULL
  },
  {"unfix",      ALLOW_CTRL,
   N_("unfix <option-name>"),
   N_("Make server option changeable during game."), NULL
  },
  {"rulesetdir", ALLOW_CTRL,
   N_("rulesetdir <directory>"),
   N_("Choose new ruleset directory or modpack. Calling this\n "
      "without any arguments will show you the currently selected "
      "ruleset."), NULL
  },
  {"rename",	ALLOW_CTRL,
   NULL,
   N_("This command is not currently implemented."), NULL
  },
  {"metainfo",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("metainfo <meta-line>"),
   N_("Set metaserver info line."), NULL
  },
  {"metaconnection",	ALLOW_HACK,
   "metaconnection u|up\n"
   "metaconnection d|down\n"
   "metaconnection ?",
   N_("Control metaserver connection."),
   N_("'metaconnection ?' reports on the status of the connection to metaserver.\n"
      "'metaconnection down' or 'metac d' brings the metaserver connection down.\n"
      "'metaconnection up' or 'metac u' brings the metaserver connection up.")
  },
  {"metaserver",ALLOW_HACK,
   /* TRANS: translate text between <> only */
   N_("metaserver <address>"),
   N_("Set address for metaserver to report to."), NULL
  },
  {"aitoggle",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("aitoggle <player-name>"),
   N_("Toggle AI status of player."), NULL
  },
  {"take",    ALLOW_INFO,
   /* TRANS: translate text between [] and <> only */
   N_("take [connection-name] <player-name>"),
   N_("Take over a player's place in the game."),
   N_("Only the console and connections with cmdlevel 'hack' can force "
      "other connections to take over a player. If you're not one of these, "
      "only the <player-name> argument is allowed")
  },
  {"create",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("create <player-name>"),
   N_("Create an AI player with a given name."),
   N_("The 'create' command is only available before the game has "
      "been started.")
  },
  {"away",	ALLOW_INFO,
   N_("away\n"
      "away"),
   N_("Set yourself in away mode. The AI will watch your back."),
   N_("The AI will govern your nation but do minimal changes."),
  },
  {"easy",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("easy\n"
      "easy <player-name>"),
   N_("Set one or all AI players to 'easy'."),
   N_("With no arguments, sets all AI players to skill level 'easy', and "
      "sets the default level for any new AI players to 'easy'.  With an "
      "argument, sets the skill level for that player only.")
  },
  {"normal",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("normal\n"
      "normal <player-name>"),
   N_("Set one or all AI players to 'normal'."),
   N_("With no arguments, sets all AI players to skill level 'normal', and "
      "sets the default level for any new AI players to 'normal'.  With an "
      "argument, sets the skill level for that player only.")
  },
  {"hard",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("hard\n"
      "hard <player-name>"),
   N_("Set one or all AI players to 'hard'."),
   N_("With no arguments, sets all AI players to skill level 'hard', and "
      "sets the default level for any new AI players to 'hard'.  With an "
      "argument, sets the skill level for that player only.")
  },
#ifdef DEBUG
  {"experimental",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("experimental\n"
      "experimental <player-name>"),
   N_("Set one or all AI players to 'experimental'."),
   N_("With no arguments, sets all AI players to skill 'experimental', and "
      "sets the default level for any new AI players to this.  With an "
      "argument, sets the skill level for that player only. THIS IS ONLY "
      "FOR TESTING OF NEW AI FEATURES! For ordinary servers, this option "
      "has no effect.")
  },
#endif
  {"cmdlevel",	ALLOW_HACK,  /* confusing to leave this at ALLOW_CTRL */
   /* TRANS: translate text between <> only */
   N_("cmdlevel\n"
      "cmdlevel <level>\n"
      "cmdlevel <level> new\n"
      "cmdlevel <level> first\n"
      "cmdlevel <level> <connection-name>"),
   N_("Query or set command access level access."),
   N_("The command access level controls which server commands are available\n"
      "to users via the client chatline.  The available levels are:\n"
      "    none  -  no commands\n"
      "    info  -  informational commands only\n"
      "    ctrl  -  commands that affect the game and users\n"
      "    hack  -  *all* commands - dangerous!\n"
      "With no arguments, the current command access levels are reported.\n"
      "With a single argument, the level is set for all existing "
      "connections,\nand the default is set for future connections.\n"
      "If 'new' is specified, the level is set for newly connecting clients.\n"
      "If 'first come' is specified, the 'first come' level is set; it will be\n"
      "granted to the first client to connect, or if there are connections\n"
      "already, the first client to issue the 'firstlevel' command.\n"
      "If a connection name is specified, the level is set for that "
      "connection only.\n"
      "Command access levels do not persist if a client disconnects, "
      "because some untrusted person could reconnect with the same name.  "
      "Note that this command now takes connection names, not player names."
      )
  },
  {"firstlevel", ALLOW_INFO,  /* Not really "informational", but needs to
				 be ALLOW_INFO to be useful. */
   "firstlevel",
   N_("Grab the 'first come' command access level."),
   N_("If 'cmdlevel first come' has been used to set a special 'first come'\n"
      "command access level, this is the command to grab it with.")
  },
  {"timeoutincrease", ALLOW_CTRL, 
   /* TRANS: translate text between <> only */
   N_("timeoutincrease <turn> <turninc> <value> <valuemult>"), 
   N_("See \"help timeoutincrease\"."),
   N_("Every <turn> turns, add <value> to timeout timer, then add <turninc> "
      "to <turn> and multiply <value> by <valuemult>.  Use this command in "
      "concert with the option \"timeout\". Defaults are 0 0 0 1")
  },
  {"endgame",	ALLOW_HACK,
   "endgame",
   N_("End the game."),
   N_("This command ends the game immediately.")
  },
  {"remove",	ALLOW_CTRL,
   /* TRANS: translate text between <> only */
   N_("remove <player-name>"),
   N_("Fully remove player from game."),
   N_("This *completely* removes a player from the game, including "
      "all cities and units etc.  Use with care!")
  },
  {"save",	ALLOW_HACK,
   /* TRANS: translate text between <> only */
   N_("save\n"
      "save <file-name>"),
   N_("Save game to file."),
   N_("Save the current game to file <file-name>.  If no file-name "
      "argument is given saves to \"<auto-save name prefix><year>m.sav[.gz]\".\n"
      "To reload a savegame created by 'save', start the server with "
      "the command-line argument:\n"
      "    --file <filename>\n"
      "and use the 'start' command once players have reconnected.")
  },
  {"load",      ALLOW_HACK,
   /* TRANS: translate text between <> only */
   N_("load\n"
      "load <file-name>"),
   N_("Load game from file."),
   N_("Load a game from <file-name>. Any current data including players, "
      "rulesets and server options are lost.\n")
  },
  {"read",	ALLOW_HACK,
   /* TRANS: translate text between <> only */
   N_("read <file-name>"),
   N_("Process server commands from file."), NULL
  },
  {"write",	ALLOW_HACK,
   /* TRANS: translate text between <> only */
   N_("write <file-name>"),
   N_("Write current settings as server commands to file."), NULL
  },
  {"rfcstyle",	ALLOW_HACK,
   "rfcstyle",
   N_("Switch server output between 'RFC-style' and normal style."), NULL
  }
};

static const char *cmdname_accessor(int i) {
  return commands[i].name;
}
/**************************************************************************
  Convert a named command into an id.
  If accept_ambiguity is true, return the first command in the
  enum list which matches, else return CMD_AMBIGOUS on ambiguity.
  (This is a trick to allow ambiguity to be handled in a flexible way
  without importing notify_player() messages inside this routine - rp)
**************************************************************************/
static enum command_id command_named(const char *token, bool accept_ambiguity)
{
  enum m_pre_result result;
  int ind;

  result = match_prefix(cmdname_accessor, CMD_NUM, 0,
			mystrncasecmp, token, &ind);

  if (result < M_PRE_AMBIGUOUS) {
    return ind;
  } else if (result == M_PRE_AMBIGUOUS) {
    return accept_ambiguity ? ind : CMD_AMBIGUOUS;
  } else {
    return CMD_UNRECOGNIZED;
  }
}

/**************************************************************************
  Whether the caller can use the specified command.
  Does not see if the setting has been /fixed.  caller == NULL means console.
**************************************************************************/
static bool may_use(struct connection *caller, enum command_id cmd)
{
  if (!caller) {
    return TRUE;  /* on the console, everything is allowed */
  }
  return (caller->access_level >= commands[cmd].level);
}

/**************************************************************************
  Whether the caller cannot use any commands at all.
  caller == NULL means console.
**************************************************************************/
static bool may_use_nothing(struct connection *caller)
{
  if (!caller) {
    return FALSE;  /* on the console, everything is allowed */
  }
  return (caller->access_level == ALLOW_NONE);
}

/**************************************************************************
  Whether the caller can set the specified option (assuming that
  the state of the game would allow changing the option at all).
  Does not see if the setting has been /fixed.  caller == NULL means console.
**************************************************************************/
static bool may_set_option(struct connection *caller, int option_idx)
{
  if (!caller) {
    return TRUE;  /* on the console, everything is allowed */
  } else {
    int level = caller->access_level;
    return ((level == ALLOW_HACK)
	    || (level == ALLOW_CTRL && sset_is_to_client(option_idx)));
  }
}

/**************************************************************************
  Whether the caller can set the specified option, taking into account
  access, the game state, and /fix.  caller == NULL means console.
**************************************************************************/
static bool may_set_option_now(struct connection *caller, int option_idx)
{
  return (may_set_option(caller, option_idx)
	  && !sset_is_fixed[option_idx]
	  && sset_is_changeable(option_idx));
}

/**************************************************************************
  Whether the caller can SEE the specified option.
  caller == NULL means console, which can see all.
  client players can see "to client" options, or if player
  has command access level to change option.
**************************************************************************/
static bool may_view_option(struct connection *caller, int option_idx)
{
  if (!caller) {
    return TRUE;  /* on the console, everything is allowed */
  } else {
    return sset_is_to_client(option_idx)
      || may_set_option(caller, option_idx);
  }
}

/**************************************************************************
  feedback related to server commands
  caller == NULL means console.
  No longer duplicate all output to console.

  This lowlevel function takes a single line; prefix is prepended to line.
**************************************************************************/
static void cmd_reply_line(enum command_id cmd, struct connection *caller,
			   enum rfc_status rfc_status, const char *prefix,
			   const char *line)
{
  const char *cmdname = cmd < CMD_NUM ? commands[cmd].name :
                  cmd == CMD_AMBIGUOUS ? _("(ambiguous)") :
                  cmd == CMD_UNRECOGNIZED ? _("(unknown)") :
			"(?!?)";  /* this case is a bug! */

  if (caller) {
    notify_conn(&caller->self, "/%s: %s%s", cmdname, prefix, line);
    /* cc: to the console - testing has proved it's too verbose - rp
    con_write(rfc_status, "%s/%s: %s%s", caller->name, cmdname, prefix, line);
    */
  } else {
    con_write(rfc_status, "%s%s", prefix, line);
  }

  if (rfc_status == C_OK) {
    notify_player(NULL, _("Game: %s"), line);
  }
}

/**************************************************************************
  va_list version which allow embedded newlines, and each line is sent
  separately. 'prefix' is prepended to every line _after_ the first line.
**************************************************************************/
static void vcmd_reply_prefix(enum command_id cmd, struct connection *caller,
			      enum rfc_status rfc_status, const char *prefix,
			      const char *format, va_list ap)
{
  char buf[4096];
  char *c0, *c1;

  my_vsnprintf(buf, sizeof(buf), format, ap);

  c0 = buf;
  while ((c1=strstr(c0, "\n"))) {
    *c1 = '\0';
    cmd_reply_line(cmd, caller, rfc_status, (c0==buf?"":prefix), c0);
    c0 = c1+1;
  }
  cmd_reply_line(cmd, caller, rfc_status, (c0==buf?"":prefix), c0);
}

/**************************************************************************
  var-args version of above
  duplicate declaration required for attribute to work...
**************************************************************************/
static void cmd_reply_prefix(enum command_id cmd, struct connection *caller,
			     enum rfc_status rfc_status, const char *prefix,
			     const char *format, ...)
     fc__attribute((format (printf, 5, 6)));
static void cmd_reply_prefix(enum command_id cmd, struct connection *caller,
			     enum rfc_status rfc_status, const char *prefix,
			     const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vcmd_reply_prefix(cmd, caller, rfc_status, prefix, format, ap);
  va_end(ap);
}

/**************************************************************************
  var-args version as above, no prefix
**************************************************************************/
static void cmd_reply(enum command_id cmd, struct connection *caller,
		      enum rfc_status rfc_status, const char *format, ...)
     fc__attribute((format (printf, 4, 5)));
static void cmd_reply(enum command_id cmd, struct connection *caller,
		      enum rfc_status rfc_status, const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vcmd_reply_prefix(cmd, caller, rfc_status, "", format, ap);
  va_end(ap);
}

/**************************************************************************
...
**************************************************************************/
static void cmd_reply_no_such_player(enum command_id cmd,
				     struct connection *caller,
				     char *name,
				     enum m_pre_result match_result)
{
  switch(match_result) {
  case M_PRE_EMPTY:
    cmd_reply(cmd, caller, C_SYNTAX,
	      _("Name is empty, so cannot be a player."));
    break;
  case M_PRE_LONG:
    cmd_reply(cmd, caller, C_SYNTAX,
	      _("Name is too long, so cannot be a player."));
    break;
  case M_PRE_AMBIGUOUS:
    cmd_reply(cmd, caller, C_FAIL,
	      _("Player name prefix '%s' is ambiguous."), name);
    break;
  case M_PRE_FAIL:
    cmd_reply(cmd, caller, C_FAIL,
	      _("No player by the name of '%s'."), name);
    break;
  default:
    cmd_reply(cmd, caller, C_FAIL,
	      _("Unexpected match_result %d (%s) for '%s'."),
	      match_result, _(m_pre_description(match_result)), name);
    freelog(LOG_ERROR,
	    "Unexpected match_result %d (%s) for '%s'.",
	    match_result, m_pre_description(match_result), name);
  }
}

/**************************************************************************
...
**************************************************************************/
static void cmd_reply_no_such_conn(enum command_id cmd,
				   struct connection *caller,
				   const char *name,
				   enum m_pre_result match_result)
{
  switch(match_result) {
  case M_PRE_EMPTY:
    cmd_reply(cmd, caller, C_SYNTAX,
	      _("Name is empty, so cannot be a connection."));
    break;
  case M_PRE_LONG:
    cmd_reply(cmd, caller, C_SYNTAX,
	      _("Name is too long, so cannot be a connection."));
    break;
  case M_PRE_AMBIGUOUS:
    cmd_reply(cmd, caller, C_FAIL,
	      _("Connection name prefix '%s' is ambiguous."), name);
    break;
  case M_PRE_FAIL:
    cmd_reply(cmd, caller, C_FAIL,
	      _("No connection by the name of '%s'."), name);
    break;
  default:
    cmd_reply(cmd, caller, C_FAIL,
	      _("Unexpected match_result %d (%s) for '%s'."),
	      match_result, _(m_pre_description(match_result)), name);
    freelog(LOG_ERROR,
	    "Unexpected match_result %d (%s) for '%s'.",
	    match_result, m_pre_description(match_result), name);
  }
}

/**************************************************************************
...
**************************************************************************/
static void open_metaserver_connection(struct connection *caller)
{
  server_open_udp();
  if (send_server_info_to_metaserver(TRUE, FALSE)) {
    notify_player(NULL, _("Open metaserver connection to [%s]."),
		  meta_addr_port());
  }
}

/**************************************************************************
...
**************************************************************************/
static void close_metaserver_connection(struct connection *caller)
{
  if (send_server_info_to_metaserver(TRUE, TRUE)) {
    server_close_udp();
    notify_player(NULL, _("Close metaserver connection to [%s]."),
		  meta_addr_port());
  }
}

/**************************************************************************
...
**************************************************************************/
static void metaconnection_command(struct connection *caller, char *arg)
{
  if ((*arg == '\0') ||
      (0 == strcmp (arg, "?"))) {
    if (server_is_open) {
      cmd_reply(CMD_METACONN, caller, C_COMMENT,
		_("Metaserver connection is open."));
    } else {
      cmd_reply(CMD_METACONN, caller, C_COMMENT,
		_("Metaserver connection is closed."));
    }
  } else if ((0 == mystrcasecmp(arg, "u")) ||
	     (0 == mystrcasecmp(arg, "up"))) {
    if (!server_is_open) {
      open_metaserver_connection(caller);
    } else {
      cmd_reply(CMD_METACONN, caller, C_METAERROR,
		_("Metaserver connection is already open."));
    }
  } else if ((0 == mystrcasecmp(arg, "d")) ||
	     (0 == mystrcasecmp(arg, "down"))) {
    if (server_is_open) {
      close_metaserver_connection(caller);
    } else {
      cmd_reply(CMD_METACONN, caller, C_METAERROR,
		_("Metaserver connection is already closed."));
    }
  } else {
    cmd_reply(CMD_METACONN, caller, C_METAERROR,
	      _("Argument must be 'u', 'up', 'd', 'down', or '?'."));
  }
}

/**************************************************************************
...
**************************************************************************/
static void metainfo_command(struct connection *caller, char *arg)
{
  sz_strlcpy(srvarg.metaserver_info_line, arg);
  if (!send_server_info_to_metaserver(TRUE, FALSE)) {
    cmd_reply(CMD_METAINFO, caller, C_METAERROR,
	      _("Not reporting to the metaserver."));
  } else {
    notify_player(NULL, _("Metaserver infostring set to '%s'."),
		  srvarg.metaserver_info_line);
  }
}

/**************************************************************************
...
**************************************************************************/
static void metaserver_command(struct connection *caller, char *arg)
{
  close_metaserver_connection(caller);

  sz_strlcpy(srvarg.metaserver_addr, arg);
  meta_addr_split();

  notify_player(NULL, _("Metaserver is now [%s]."),
		meta_addr_port());
}

/***************************************************************
 This could be in common/player if the client ever gets
 told the ai player skill levels.
***************************************************************/
static const char *name_of_skill_level(int level)
{
  const char *nm[11] = { "UNUSED", "away", "UNKNOWN", "easy",
			 "UNKNOWN", "normal", "UNKNOWN", "hard",
			 "UNKNOWN", "UNKNOWN", "experimental" };
  
  assert(level>0 && level<=10);
  return nm[level];
}

/***************************************************************
...
***************************************************************/
static int handicap_of_skill_level(int level)
{
  int h[11] = { -1,
		H_AWAY,
		H_NONE,
 /* easy */	H_RATES | H_TARGETS | H_HUTS | H_NOPLANES 
                        | H_DIPLOMAT | H_LIMITEDHUTS | H_DEFENSIVE,
		H_NONE,
 /* medium */	H_RATES | H_TARGETS | H_HUTS | H_DIPLOMAT,
		H_NONE,
 /* hard */	H_NONE,
		H_NONE,
		H_NONE,
 /* testing */	H_EXPERIMENTAL,
		};
  
  assert(level>0 && level<=10);
  return h[level];
}

/**************************************************************************
Return the AI fuzziness (0 to 1000) corresponding to a given skill
level (1 to 10).  See ai_fuzzy() in common/player.c
**************************************************************************/
static int fuzzy_of_skill_level(int level)
{
  int f[11] = { -1, 0, 0, 300/*easy*/, 0, 0, 0, 0, 0, 0, 0 };
  
  assert(level>0 && level<=10);
  return f[level];
}

/**************************************************************************
Return the AI expansion tendency, a percentage factor to value new cities,
compared to defaults.  0 means _never_ build new cities, > 100 means to
(over?)value them even more than the default (already expansionistic) AI.
**************************************************************************/
static int expansionism_of_skill_level(int level)
{
  int x[11] = { -1, 100, 100, 10/*easy*/, 100, 100, 100, 100, 100, 100, 100 };
  
  assert(level>0 && level<=10);
  return x[level];
}

/**************************************************************************
For command "save foo";
Save the game, with filename=arg, provided server state is ok.
**************************************************************************/
static void save_command(struct connection *caller, char *arg)
{
  if (server_state==SELECT_RACES_STATE) {
    cmd_reply(CMD_SAVE, caller, C_SYNTAX,
	      _("The game cannot be saved before it is started."));
    return;
  }
  save_game(arg);
}

/**************************************************************************
...
**************************************************************************/
void toggle_ai_player_direct(struct connection *caller, struct player *pplayer)
{
  assert(pplayer != NULL);
  if (is_barbarian(pplayer)) {
    cmd_reply(CMD_AITOGGLE, caller, C_FAIL,
	      _("Cannot toggle a barbarian player."));
    return;
  }

  pplayer->ai.control = !pplayer->ai.control;
  if (pplayer->ai.control) {
    cmd_reply(CMD_AITOGGLE, caller, C_OK,
	      _("%s is now under AI control."), pplayer->name);
    if (pplayer->ai.skill_level==0) {
      pplayer->ai.skill_level = game.skill_level;
    }
    /* Set the skill level explicitly, because eg: the player skill
       level could have been set as AI, then toggled, then saved,
       then reloaded. */ 
    set_ai_level(caller, pplayer->name, pplayer->ai.skill_level);
    /* the AI can't do active diplomacy */
    cancel_all_meetings(pplayer);
    /* The following is sometimes necessary to avoid using
       uninitialized data... */
    if (server_state == RUN_GAME_STATE)
      assess_danger_player(pplayer);
  } else {
    cmd_reply(CMD_AITOGGLE, caller, C_OK,
	      _("%s is now under human control."), pplayer->name);

    /* because the hard AI `cheats' with government rates but humans shouldn't */
    if (!game.is_new_game) {
      check_player_government_rates(pplayer);
    }
  }

  if (server_state == RUN_GAME_STATE) {
    send_player_info(pplayer, NULL);
  }
}

/**************************************************************************
...
**************************************************************************/
static void toggle_ai_player(struct connection *caller, char *arg)
{
  enum m_pre_result match_result;
  struct player *pplayer;

  pplayer = find_player_by_name_prefix(arg, &match_result);

  if (!pplayer) {
    cmd_reply_no_such_player(CMD_AITOGGLE, caller, arg, match_result);
    return;
  }
  toggle_ai_player_direct(caller, pplayer);
}

/**************************************************************************
...
**************************************************************************/
static void create_ai_player(struct connection *caller, char *arg)
{
  struct player *pplayer;
  PlayerNameStatus PNameStatus;
   
  if (server_state!=PRE_GAME_STATE)
  {
    cmd_reply(CMD_CREATE, caller, C_SYNTAX,
	      _("Can't add AI players once the game has begun."));
    return;
  }

  if (game.nplayers >= game.max_players) 
  {
    cmd_reply(CMD_CREATE, caller, C_FAIL,
	      _("Can't add more players, server is full."));
    return;
  }

  if ((PNameStatus = test_player_name(arg)) == PNameEmpty)
  {
    cmd_reply(CMD_CREATE, caller, C_SYNTAX, _("Can't use an empty name."));
    return;
  }

  if (PNameStatus == PNameTooLong)
  {
    cmd_reply(CMD_CREATE, caller, C_SYNTAX,
	      _("That name exceeds the maximum of %d chars."), MAX_LEN_NAME-1);
    return;
  }

  if ((pplayer=find_player_by_name(arg))) {
    cmd_reply(CMD_CREATE, caller, C_BOUNCE,
	      _("A player already exists by that name."));
    return;
  }

  if ((pplayer = find_player_by_user(arg))) {
    cmd_reply(CMD_CREATE, caller, C_BOUNCE,
              _("A user already exists by that name."));
    return;
  }

  pplayer = &game.players[game.nplayers];
  server_player_init(pplayer, FALSE);
  sz_strlcpy(pplayer->name, arg);
  sz_strlcpy(pplayer->username, "Unassigned");

  game.nplayers++;

  notify_player(NULL, _("Game: %s has been added as an AI-controlled player."),
                arg);
  (void) send_server_info_to_metaserver(TRUE, FALSE);

  pplayer = find_player_by_name(arg);
  if (!pplayer)
  {
    cmd_reply(CMD_CREATE, caller, C_FAIL,
	      _("Error creating new AI player: %s."), arg);
    return;
  }

  pplayer->ai.control = TRUE;
  set_ai_level_directer(pplayer, game.skill_level);
}


/**************************************************************************
...
**************************************************************************/
static void remove_player(struct connection *caller, char *arg)
{
  enum m_pre_result match_result;
  struct player *pplayer;
  char name[MAX_LEN_NAME];

  pplayer=find_player_by_name_prefix(arg, &match_result);
  
  if(!pplayer) {
    cmd_reply_no_such_player(CMD_REMOVE, caller, arg, match_result);
    return;
  }

  if (!(game.is_new_game && (server_state==PRE_GAME_STATE ||
			     server_state==SELECT_RACES_STATE))) {
    cmd_reply(CMD_REMOVE, caller, C_FAIL,
	      _("Players cannot be removed once the game has started."));
    return;
  }

  sz_strlcpy(name, pplayer->name);
  team_remove_player(pplayer);
  server_remove_player(pplayer);
  if (!caller || caller->used) {     /* may have removed self */
    cmd_reply(CMD_REMOVE, caller, C_OK,
	      _("Removed player %s from the game."), name);
  }
}

/**************************************************************************
...
**************************************************************************/
static void generic_not_implemented(struct connection *caller, const char *cmd)
{
  cmd_reply(CMD_RENAME, caller, C_FAIL,
	    _("Sorry, the '%s' command is not implemented yet."), cmd);
}

/**************************************************************************
...
**************************************************************************/
static void rename_player(struct connection *caller, char *arg)
{
  generic_not_implemented(caller, "rename");
}

/**************************************************************************
  Returns FALSE iff there was an error.
**************************************************************************/
bool read_init_script(struct connection *caller, char *script_filename)
{
  FILE *script_file;

  freelog(LOG_NORMAL, _("Loading script file: %s"), script_filename);
 
  if (is_reg_file_for_access(script_filename, FALSE)
      && (script_file = fopen(script_filename, "r"))) {
    char buffer[MAX_LEN_CONSOLE_LINE];
    /* the size is set as to not overflow buffer in handle_stdin_input */
    while(fgets(buffer,MAX_LEN_CONSOLE_LINE-1,script_file))
      handle_stdin_input((struct connection *)NULL, buffer);
    fclose(script_file);
    return TRUE;
  } else {
    cmd_reply(CMD_READ_SCRIPT, caller, C_FAIL,
	_("Cannot read command line scriptfile '%s'."), script_filename);
    freelog(LOG_ERROR,
	_("Could not read script file '%s'."), script_filename);
    return FALSE;
  }
}

/**************************************************************************
...
**************************************************************************/
static void read_command(struct connection *caller, char *arg)
{
  /* warning: there is no recursion check! */
  (void) read_init_script(caller, arg);
}

/**************************************************************************
...
(Should this take a 'caller' argument for output? --dwp)
**************************************************************************/
static void write_init_script(char *script_filename)
{
  FILE *script_file;

  if (is_reg_file_for_access(script_filename, TRUE)
      && (script_file = fopen(script_filename, "w"))) {

    int i;

    fprintf(script_file,
	"#FREECIV SERVER COMMAND FILE, version %s\n", VERSION_STRING);
    fputs("# These are server options saved from a running civserver.\n",
	script_file);

    /* first, some state info from commands (we can't save everything) */

    fprintf(script_file, "cmdlevel %s new\n",
	cmdlevel_name(default_access_level));

    fprintf(script_file, "cmdlevel %s first\n",
	cmdlevel_name(first_access_level));

    fprintf(script_file, "%s\n",
	(game.skill_level <= 3) ?	"easy" :
	(game.skill_level == 5) ?	"medium" :
	(game.skill_level < 10) ?	"hard" :
					"experimental");

    if (*srvarg.metaserver_addr != '\0' &&
	((0 != strcmp(srvarg.metaserver_addr, DEFAULT_META_SERVER_ADDR)) ||
	 (srvarg.metaserver_port != DEFAULT_META_SERVER_PORT))) {
      fprintf(script_file, "metaserver %s\n", meta_addr_port());
    }

    if (*srvarg.metaserver_info_line != '\0' &&
	(0 != strcmp(srvarg.metaserver_info_line,
		     default_meta_server_info_string()))) {
      fprintf(script_file, "metainfo %s\n", srvarg.metaserver_info_line);
    }

    /* then, the 'set' option settings */

    for (i=0;settings[i].name;i++) {
      struct settings_s *op = &settings[i];

      switch (op->type) {
      case SSET_INT:
	fprintf(script_file, "set %s %i\n", op->name, *op->int_value);
	break;
      case SSET_BOOL:
	fprintf(script_file, "set %s %i\n", op->name,
		(*op->bool_value) ? 1 : 0);
	break;
      case SSET_STRING:
	fprintf(script_file, "set %s %s\n", op->name, op->string_value);
	break;
      }
    }

    /* the 'fix'ed settings */

    for (i = 0; settings[i].name; i++) {
      if (sset_is_fixed[i]) {
        fprintf(script_file, "fix %s\n", settings[i].name);
      }
    }

    fclose(script_file);

  } else {
    freelog(LOG_ERROR,
	_("Could not write script file '%s'."), script_filename);
  }
}

/**************************************************************************
...
('caller' argument is unused)
**************************************************************************/
static void write_command(struct connection *caller, char *arg)
{
  write_init_script(arg);
}

/**************************************************************************
 set ptarget's cmdlevel to level if caller is allowed to do so
**************************************************************************/
static bool set_cmdlevel(struct connection *caller,
			struct connection *ptarget,
			enum cmdlevel_id level)
{
  assert(ptarget != NULL);    /* only ever call me for specific connection */

  if (caller && ptarget->access_level > caller->access_level) {
    /*
     * This command is intended to be used at ctrl access level
     * and thus this if clause is needed.
     * (Imagine a ctrl level access player that wants to change
     * access level of a hack level access player)
     * At the moment it can be used only by hack access level 
     * and thus this clause is never used.
     */
    cmd_reply(CMD_CMDLEVEL, caller, C_FAIL,
	      _("Cannot decrease command access level '%s' for connection '%s';"
		" you only have '%s'."),
	      cmdlevel_name(ptarget->access_level),
	      ptarget->username,
	      cmdlevel_name(caller->access_level));
    return FALSE;
  } else {
    ptarget->access_level = level;
    return TRUE;
  }
}

/********************************************************************
  Returns true if there is at least one established connection.
*********************************************************************/
static bool a_connection_exists(void)
{
  return conn_list_size(&game.est_connections) > 0;
}

/********************************************************************
...
*********************************************************************/
static bool first_access_level_is_taken(void)
{
  conn_list_iterate(game.est_connections, pconn) {
    if (pconn->access_level >= first_access_level) {
      return TRUE;
    }
  }
  conn_list_iterate_end;
  return FALSE;
}

/********************************************************************
...
*********************************************************************/
enum cmdlevel_id access_level_for_next_connection(void)
{
  if ((first_access_level > default_access_level)
			&& !a_connection_exists()) {
    return first_access_level;
  } else {
    return default_access_level;
  }
}

/********************************************************************
...
*********************************************************************/
void notify_if_first_access_level_is_available(void)
{
  if (first_access_level > default_access_level
      && !first_access_level_is_taken()) {
    notify_player(NULL, _("Game: Anyone can assume command access level "
			  "'%s' now by issuing the 'firstlevel' command."),
		  cmdlevel_name(first_access_level));
  }
}

/**************************************************************************
 Change command access level for individual player, or all, or new.
**************************************************************************/
static void cmdlevel_command(struct connection *caller, char *str)
{
  char arg_level[MAX_LEN_CONSOLE_LINE]; /* info, ctrl etc */
  char arg_name[MAX_LEN_CONSOLE_LINE];	 /* a player name, or "new" */
  char *cptr_s, *cptr_d;	 /* used for string ops */

  enum m_pre_result match_result;
  enum cmdlevel_id level;
  struct connection *ptarget;

  /* find the start of the level: */
  for (cptr_s = str; *cptr_s != '\0' && !my_isalnum(*cptr_s); cptr_s++) {
    /* nothing */
  }

  /* copy the level into arg_level[] */
  for(cptr_d=arg_level; *cptr_s != '\0' && my_isalnum(*cptr_s); cptr_s++, cptr_d++) {
    *cptr_d=*cptr_s;
  }
  *cptr_d='\0';
  
  if (arg_level[0] == '\0') {
    /* no level name supplied; list the levels */

    cmd_reply(CMD_CMDLEVEL, caller, C_COMMENT, _("Command access levels in effect:"));

    conn_list_iterate(game.est_connections, pconn) {
      cmd_reply(CMD_CMDLEVEL, caller, C_COMMENT, "cmdlevel %s %s",
		cmdlevel_name(pconn->access_level), pconn->username);
    }
    conn_list_iterate_end;
    cmd_reply(CMD_CMDLEVEL, caller, C_COMMENT,
	      _("Command access level for new connections: %s"),
	      cmdlevel_name(default_access_level));
    cmd_reply(CMD_CMDLEVEL, caller, C_COMMENT,
	      _("Command access level for first player to take it: %s"),
	      cmdlevel_name(first_access_level));
    return;
  }

  /* a level name was supplied; set the level */

  if ((level = cmdlevel_named(arg_level)) == ALLOW_UNRECOGNIZED) {
    cmd_reply(CMD_CMDLEVEL, caller, C_SYNTAX,
	      _("Error: command access level must be one of"
		" 'none', 'info', 'ctrl', or 'hack'."));
    return;
  } else if (caller && level > caller->access_level) {
    cmd_reply(CMD_CMDLEVEL, caller, C_FAIL,
	      _("Cannot increase command access level to '%s';"
		" you only have '%s' yourself."),
	      arg_level, cmdlevel_name(caller->access_level));
    return;
  }

  /* find the start of the name: */
  for (; *cptr_s != '\0' && !my_isalnum(*cptr_s); cptr_s++) {
    /* nothing */
  }

  /* copy the name into arg_name[] */
  for(cptr_d=arg_name;
      *cptr_s != '\0' && (*cptr_s == '-' || *cptr_s == ' ' || my_isalnum(*cptr_s));
      cptr_s++ , cptr_d++) {
    *cptr_d=*cptr_s;
  }
  *cptr_d='\0';
 
  if (arg_name[0] == '\0') {
    /* no playername supplied: set for all connections, and set the default */
    conn_list_iterate(game.est_connections, pconn) {
      if (set_cmdlevel(caller, pconn, level)) {
	cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		  _("Command access level set to '%s' for connection %s."),
		  cmdlevel_name(level), pconn->username);
      } else {
	cmd_reply(CMD_CMDLEVEL, caller, C_FAIL,
		  _("Command access level could not be set to '%s' for "
		    "connection %s."),
		  cmdlevel_name(level), pconn->username);
      }
    }
    conn_list_iterate_end;
    
    default_access_level = level;
    cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for new players."),
		cmdlevel_name(level));
    first_access_level = level;
    cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for first player to grab it."),
		cmdlevel_name(level));
  }
  else if (strcmp(arg_name,"new") == 0) {
    default_access_level = level;
    cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for new players."),
		cmdlevel_name(level));
    if (level > first_access_level) {
      first_access_level = level;
      cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for first player to grab it."),
		cmdlevel_name(level));
    }
  }
  else if (strcmp(arg_name,"first") == 0) {
    first_access_level = level;
    cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for first player to grab it."),
		cmdlevel_name(level));
    if (level < default_access_level) {
      default_access_level = level;
      cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for new players."),
		cmdlevel_name(level));
    }
  }
  else if ((ptarget = find_conn_by_user_prefix(arg_name, &match_result))) {
    if (set_cmdlevel(caller, ptarget, level)) {
      cmd_reply(CMD_CMDLEVEL, caller, C_OK,
		_("Command access level set to '%s' for connection %s."),
		cmdlevel_name(level), ptarget->username);
    } else {
      cmd_reply(CMD_CMDLEVEL, caller, C_FAIL,
		_("Command access level could not be set to '%s'"
		  " for connection %s."),
		cmdlevel_name(level), ptarget->username);
    }
  } else {
    cmd_reply_no_such_conn(CMD_CMDLEVEL, caller, arg_name, match_result);
  }
}

/**************************************************************************
 This special command to set the command access level is not included into
 cmdlevel_command because of its lower access level: it can be used
 to promote one's own connection to 'first come' cmdlevel if that isn't
 already taken.
 **************************************************************************/
static void firstlevel_command(struct connection *caller)
{
  if (!caller) {
    cmd_reply(CMD_FIRSTLEVEL, caller, C_FAIL,
	_("The 'firstlevel' command makes no sense from the server command line."));
  } else if (caller->access_level >= first_access_level) {
    cmd_reply(CMD_FIRSTLEVEL, caller, C_FAIL,
	_("You already have command access level '%s' or better."),
		cmdlevel_name(first_access_level));
  } else if (first_access_level_is_taken()) {
    cmd_reply(CMD_FIRSTLEVEL, caller, C_FAIL,
	_("Someone else already has command access level '%s' or better."),
		cmdlevel_name(first_access_level));
  } else {
    caller->access_level = first_access_level;
    cmd_reply(CMD_FIRSTLEVEL, caller, C_OK,
	_("Command access level '%s' has been grabbed by connection %s."),
		cmdlevel_name(first_access_level),
		caller->username);
  }
}


static const char *optname_accessor(int i) {
  return settings[i].name;
}

/**************************************************************************
  Set timeout options.
**************************************************************************/
static void timeout_command(struct connection *caller, char *str) 
{
  char buf[MAX_LEN_CONSOLE_LINE];
  char *arg[4];
  int i = 0, ntokens;
  int *timeouts[4];

  timeouts[0] = &game.timeoutint;
  timeouts[1] = &game.timeoutintinc;
  timeouts[2] = &game.timeoutinc;
  timeouts[3] = &game.timeoutincmult;

  sz_strlcpy(buf, str);
  ntokens = get_tokens(buf, arg, 4, TOKEN_DELIMITERS);

  for (i = 0; i < ntokens; i++) {
    if (sscanf(arg[i], "%d", timeouts[i]) != 1) {
      cmd_reply(CMD_TIMEOUT, caller, C_FAIL, _("Invalid argument %d."),
		i + 1);
    }
    free(arg[i]);
  }

  if (ntokens == 0) {
    cmd_reply(CMD_TIMEOUT, caller, C_SYNTAX, _("Usage: timeoutincrease "
					       "<turn> <turnadd> "
					       "<value> <valuemult>."));
    return;
  }

  cmd_reply(CMD_TIMEOUT, caller, C_OK, _("Dynamic timeout set to "
					 "%d %d %d %d"),
	    game.timeoutint, game.timeoutintinc,
	    game.timeoutinc, game.timeoutincmult);

  /* if we set anything here, reset the counter */
  game.timeoutcounter = 1;
}

/**************************************************************************
Find option index by name. Return index (>=0) on success, -1 if no
suitable options were found, -2 if several matches were found.
**************************************************************************/
static int lookup_option(const char *name)
{
  enum m_pre_result result;
  int ind;

  result = match_prefix(optname_accessor, SETTINGS_NUM, 0, mystrncasecmp,
			name, &ind);

  return ((result < M_PRE_AMBIGUOUS) ? ind :
	  (result == M_PRE_AMBIGUOUS) ? -2 : -1);
}

/**************************************************************************
 Show the caller detailed help for the single OPTION given by id.
 help_cmd is the command the player used.
 Only show option values for options which the caller can SEE.
**************************************************************************/
static void show_help_option(struct connection *caller,
			     enum command_id help_cmd,
			     int id)
{
  struct settings_s *op = &settings[id];

  if (op->short_help) {
    cmd_reply(help_cmd, caller, C_COMMENT,
	      "%s %s  -  %s", _("Option:"), op->name, _(op->short_help));
  } else {
    cmd_reply(help_cmd, caller, C_COMMENT,
	      "%s %s", _("Option:"), op->name);
  }

  if(op->extra_help && strcmp(op->extra_help,"")!=0) {
    static struct astring abuf = ASTRING_INIT;
    const char *help = _(op->extra_help);

    astr_minsize(&abuf, strlen(help)+1);
    strcpy(abuf.str, help);
    wordwrap_string(abuf.str, 76);
    cmd_reply(help_cmd, caller, C_COMMENT, _("Description:"));
    cmd_reply_prefix(help_cmd, caller, C_COMMENT,
		     "  ", "  %s", abuf.str);
  }
  cmd_reply(help_cmd, caller, C_COMMENT,
	    _("Status: %s"), (sset_is_changeable(id)
				  ? _("changeable") : _("fixed")));
  
  if (may_view_option(caller, id)) {
    switch (op->type) {
    case SSET_BOOL:
      cmd_reply(help_cmd, caller, C_COMMENT,
		_("Value: %d, Minimum: 0, Default: %d, Maximum: 1"),
		(*(op->bool_value)) ? 1 : 0, op->bool_default_value ? 1 : 0);
      break;
    case SSET_INT:
      cmd_reply(help_cmd, caller, C_COMMENT,
		_("Value: %d, Minimum: %d, Default: %d, Maximum: %d"),
		*(op->int_value), op->int_min_value, op->int_default_value,
		op->int_max_value);
      break;
    case SSET_STRING:
      cmd_reply(help_cmd, caller, C_COMMENT,
		_("Value: \"%s\", Default: \"%s\""), op->string_value,
		op->string_default_value);
      break;
    }
  }
}

/**************************************************************************
 Show the caller list of OPTIONS.
 help_cmd is the command the player used.
 Only show options which the caller can SEE.
**************************************************************************/
static void show_help_option_list(struct connection *caller,
				  enum command_id help_cmd)
{
  int i, j;
  
  cmd_reply(help_cmd, caller, C_COMMENT, horiz_line);
  cmd_reply(help_cmd, caller, C_COMMENT,
	    _("Explanations are available for the following server options:"));
  cmd_reply(help_cmd, caller, C_COMMENT, horiz_line);
  if(!caller && con_get_style()) {
    for (i=0; settings[i].name; i++) {
      cmd_reply(help_cmd, caller, C_COMMENT, "%s", settings[i].name);
    }
  } else {
    char buf[MAX_LEN_CONSOLE_LINE];
    buf[0] = '\0';
    for (i=0, j=0; settings[i].name; i++) {
      if (may_view_option(caller, i)) {
	cat_snprintf(buf, sizeof(buf), "%-19s", settings[i].name);
	if ((++j % 4) == 0) {
	  cmd_reply(help_cmd, caller, C_COMMENT, buf);
	  buf[0] = '\0';
	}
      }
    }
    if (buf[0] != '\0')
      cmd_reply(help_cmd, caller, C_COMMENT, buf);
  }
  cmd_reply(help_cmd, caller, C_COMMENT, horiz_line);
}

/**************************************************************************
 ...
**************************************************************************/
static void explain_option(struct connection *caller, char *str)
{
  char command[MAX_LEN_CONSOLE_LINE], *cptr_s, *cptr_d;
  int cmd;

  for (cptr_s = str; *cptr_s != '\0' && !my_isalnum(*cptr_s); cptr_s++) {
    /* nothing */
  }
  for (cptr_d = command; *cptr_s != '\0' && my_isalnum(*cptr_s); cptr_s++, cptr_d++)
    *cptr_d=*cptr_s;
  *cptr_d='\0';

  if (*command != '\0') {
    cmd=lookup_option(command);
    if (cmd==-1) {
      cmd_reply(CMD_EXPLAIN, caller, C_FAIL, _("No explanation for that yet."));
      return;
    } else if (cmd==-2) {
      cmd_reply(CMD_EXPLAIN, caller, C_FAIL, _("Ambiguous option name."));
      return;
    } else {
      show_help_option(caller, CMD_EXPLAIN, cmd);
    }
  } else {
    show_help_option_list(caller, CMD_EXPLAIN);
  }
}
/******************************************************************
  Send a message to all players
******************************************************************/
static void wall(char *str)
{
  notify_conn_ex(&game.game_connections, -1, -1, E_MESSAGE_WALL,
		 _("Server Operator: %s"), str);
}
  
/******************************************************************
Send a report with server options to specified connections.
"which" should be one of:
1: initial options only
2: ongoing options only 
(which=0 means all options; this is now obsolete and no longer used.)
******************************************************************/
void report_server_options(struct conn_list *dest, int which)
{
  int i;
  char buffer[4096];
  char title[128];
  char *caption;
  buffer[0]=0;
  my_snprintf(title, sizeof(title), _("%-20svalue  (min , max)"), _("Option"));
  caption = (which == 1) ?
    _("Server Options (initial)") :
    _("Server Options (ongoing)");

  for (i=0;settings[i].name;i++) {
    struct settings_s *op = &settings[i];
    if (!sset_is_to_client(i)) continue;
    if (which==1 && op->sclass > SSET_GAME_INIT) continue;
    if (which==2 && op->sclass <= SSET_GAME_INIT) continue;
    switch (op->type) {
    case SSET_BOOL:
      cat_snprintf(buffer, sizeof(buffer), "%-20s%c%-6d (0,1)\n",
		   op->name,
		   (*op->bool_value == op->bool_default_value) ? '*' : ' ',
		   *op->bool_value);
      break;

    case SSET_INT:
      cat_snprintf(buffer, sizeof(buffer), "%-20s%c%-6d (%d,%d)\n", op->name,
		   (*op->int_value == op->int_default_value) ? '*' : ' ',
		   *op->int_value, op->int_min_value, op->int_max_value);
      break;
    case SSET_STRING:
      cat_snprintf(buffer, sizeof(buffer), "%-20s%c\"%s\"\n", op->name,
		   (strcmp(op->string_value,
			   op->string_default_value) == 0) ? '*' : ' ',
		   op->string_value);
      break;
    }
  }
  freelog(LOG_DEBUG, "report_server_options buffer len %d", i);
  page_conn(dest, caption, title, buffer);
}

/******************************************************************
  Set an AI level and related quantities, with no feedback.
******************************************************************/
void set_ai_level_directer(struct player *pplayer, int level)
{
  pplayer->ai.handicap = handicap_of_skill_level(level);
  pplayer->ai.fuzzy = fuzzy_of_skill_level(level);
  pplayer->ai.expand = expansionism_of_skill_level(level);
  pplayer->ai.skill_level = level;
}

/******************************************************************
  Translate an AI level back to its CMD_* value.
  If we just used /set ailevel <num> we wouldn't have to do this - rp
******************************************************************/
static enum command_id cmd_of_level(int level)
{
  switch(level) {
    case 1 : return CMD_AWAY;
    case 3 : return CMD_EASY;
    case 5 : return CMD_NORMAL;
    case 7 : return CMD_HARD;
#ifdef DEBUG
    case 10 : return CMD_EXPERIMENTAL;
#endif
  }
  assert(FALSE);
  return CMD_NORMAL; /* to satisfy compiler */
}

/******************************************************************
  Set an AI level from the server prompt.
******************************************************************/
void set_ai_level_direct(struct player *pplayer, int level)
{
  set_ai_level_directer(pplayer,level);
  cmd_reply(cmd_of_level(level), NULL, C_OK,
	_("Player '%s' now has AI skill level '%s'."),
	pplayer->name, name_of_skill_level(level));
  
}

/******************************************************************
  Handle a user command to set an AI level.
******************************************************************/
static void set_ai_level(struct connection *caller, char *name, int level)
{
  enum m_pre_result match_result;
  struct player *pplayer;

  assert(level > 0 && level < 11);

  pplayer=find_player_by_name_prefix(name, &match_result);

  if (pplayer) {
    if (pplayer->ai.control) {
      set_ai_level_directer(pplayer, level);
      cmd_reply(cmd_of_level(level), caller, C_OK,
		_("Player '%s' now has AI skill level '%s'."),
		pplayer->name, name_of_skill_level(level));
    } else {
      cmd_reply(cmd_of_level(level), caller, C_FAIL,
		_("%s is not controlled by the AI."), pplayer->name);
    }
  } else if(match_result == M_PRE_EMPTY) {
    players_iterate(pplayer) {
      if (pplayer->ai.control) {
	set_ai_level_directer(pplayer, level);
        cmd_reply(cmd_of_level(level), caller, C_OK,
		_("Player '%s' now has AI skill level '%s'."),
		pplayer->name, name_of_skill_level(level));
      }
    } players_iterate_end;
    game.skill_level = level;
    cmd_reply(cmd_of_level(level), caller, C_OK,
		_("Default AI skill level set to '%s'."),
		name_of_skill_level(level));
  } else {
    cmd_reply_no_such_player(cmd_of_level(level), caller, name, match_result);
  }
}

/******************************************************************
  Set user to away mode.
******************************************************************/
static void set_away(struct connection *caller, char *name)
{
  if (caller == NULL) {
    cmd_reply(CMD_AWAY, caller, C_FAIL, _("This command is client only."));
  } else if (name && strlen(name) > 0) {
    notify_conn(&caller->self, _("Usage: away"));
  } else if (!caller->player->ai.control) {
    notify_conn(&game.est_connections, _("%s set to away mode."), 
                caller->player->name);
    set_ai_level_directer(caller->player, 1);
    caller->player->ai.control = TRUE;
  } else {
    notify_conn(&game.est_connections, _("%s returned to game."), 
                caller->player->name);
    caller->player->ai.control = FALSE;
  }
}

/******************************************************************
Print a summary of the settings and their values.
Note that most values are at most 4 digits, except seeds,
which we let overflow their columns, plus a sign character.
Only show options which the caller can SEE.
******************************************************************/
static void show_command(struct connection *caller, char *str)
{
  char buf[MAX_LEN_CONSOLE_LINE];
  char command[MAX_LEN_CONSOLE_LINE], *cptr_s, *cptr_d;
  int cmd,i,len1;
  size_t clen = 0;

  for (cptr_s = str; *cptr_s != '\0' && !my_isalnum(*cptr_s); cptr_s++) {
    /* nothing */
  }
  for (cptr_d = command; *cptr_s != '\0' && my_isalnum(*cptr_s); cptr_s++, cptr_d++)
    *cptr_d=*cptr_s;
  *cptr_d='\0';

  if (*command != '\0') {
    cmd=lookup_option(command);
    if (cmd>=0 && !may_view_option(caller, cmd)) {
      cmd_reply(CMD_SHOW, caller, C_FAIL,
		_("Sorry, you do not have access to view option '%s'."),
		command);
      return;
    }
    if (cmd==-1) {
      cmd_reply(CMD_SHOW, caller, C_FAIL, _("Unknown option '%s'."), command);
      return;
    }
    if (cmd==-2) {
      /* allow ambiguous: show all matching */
      clen = strlen(command);
    }
  } else {
   cmd = -1;  /* to indicate that no comannd was specified */
  }

#define cmd_reply_show(string)  cmd_reply(CMD_SHOW, caller, C_COMMENT, string)

#define OPTION_NAME_SPACE 13
  /* under SSET_MAX_LEN, so it fits into 80 cols more easily - rp */

  cmd_reply_show(horiz_line);
  cmd_reply_show(_("+ means you may change the option"));
  cmd_reply_show(_("= means the option is on its default value"));
  cmd_reply_show(horiz_line);
  len1 = my_snprintf(buf, sizeof(buf),
	_("%-*s value   (min,max)      "), OPTION_NAME_SPACE, _("Option"));
  if (len1 == -1)
    len1 = sizeof(buf) -1;
  sz_strlcat(buf, _("description"));
  cmd_reply_show(buf);
  cmd_reply_show(horiz_line);

  buf[0] = '\0';

  for (i=0;settings[i].name;i++) {
    if (may_view_option(caller, i)
	&& (cmd==-1 || cmd==i
	    || (cmd==-2 && mystrncasecmp(settings[i].name, command, clen)==0))) {
      /* in the cmd==i case, this loop is inefficient. never mind - rp */
      struct settings_s *op = &settings[i];
      int len = 0;

      switch (op->type) {
      case SSET_BOOL:
	len = my_snprintf(buf, sizeof(buf),
			  "%-*s %c%c%-5d (0,1)", OPTION_NAME_SPACE, op->name,
			  may_set_option_now(caller, i) ? '+' : ' ',
			  ((*op->bool_value == op->bool_default_value) ?
			   '=' : ' '), (*op->bool_value) ? 1 : 0);
	break;

      case SSET_INT:
	len = my_snprintf(buf, sizeof(buf),
			  "%-*s %c%c%-5d (%d,%d)", OPTION_NAME_SPACE,
			  op->name, may_set_option_now(caller,
						       i) ? '+' : ' ',
			  ((*op->int_value == op->int_default_value) ?
			   '=' : ' '),
			  *op->int_value, op->int_min_value,
			  op->int_max_value);
	break;

      case SSET_STRING:
	len = my_snprintf(buf, sizeof(buf),
			  "%-*s %c%c\"%s\"", OPTION_NAME_SPACE, op->name,
			  may_set_option_now(caller, i) ? '+' : ' ',
			  ((strcmp(op->string_value,
				   op->string_default_value) == 0) ?
			   '=' : ' '), op->string_value);
	break;
      }
      if (len == -1) {
	  len = sizeof(buf) - 1;
      }
      /* Line up the descriptions: */
      if(len < len1) {
        cat_snprintf(buf, sizeof(buf), "%*s", (len1-len), " ");
      } else {
        sz_strlcat(buf, " ");
      }
      sz_strlcat(buf, _(op->short_help));
      cmd_reply_show(buf);
    }
  }
  cmd_reply_show(horiz_line);
#undef cmd_reply_show
#undef OPTION_NAME_SPACE
}

/******************************************************************
  Which characters are allowed within option names: (for 'set')
******************************************************************/
static bool is_ok_opt_name_char(char c)
{
  return my_isalnum(c);
}

/******************************************************************
  Which characters are allowed within option values: (for 'set')
******************************************************************/
static bool is_ok_opt_value_char(char c)
{
  return (c == '-') || (c == '*') || (c == '+') || (c == '=') || my_isalnum(c);
}

/******************************************************************
  Which characters are allowed between option names and values: (for 'set')
******************************************************************/
static bool is_ok_opt_name_value_sep_char(char c)
{
  return (c == '=') || my_isspace(c);
}

/******************************************************************
  ...
******************************************************************/
static void team_command(struct connection *caller, char *str) 
{
  struct player *pplayer;
  enum m_pre_result match_result;
  char buf[MAX_LEN_CONSOLE_LINE];
  char *arg[2];
  int ntokens = 0;

  if (server_state != PRE_GAME_STATE) {
    cmd_reply(CMD_TEAM, caller, C_SYNTAX,
              _("Cannot change teams once game has begun."));
    return;
  }

  if (str != NULL || strlen(str) > 0) {
    sz_strlcpy(buf, str);
    ntokens = get_tokens(buf, arg, 2, TOKEN_DELIMITERS);
  }
  if (ntokens > 2 || ntokens < 1) {
    cmd_reply(CMD_TEAM, caller, C_SYNTAX,
	      _("Undefined argument.  Usage: team <player> [team]."));
    return;
  }

  pplayer = find_player_by_name_prefix(arg[0], &match_result);
  if (pplayer == NULL) {
    cmd_reply_no_such_player(CMD_TEAM, caller, arg[0], match_result);
    return;
  }

  if (pplayer->team != TEAM_NONE) {
    team_remove_player(pplayer);
  }

  if (ntokens == 1) {
    /* Remove from team */
    cmd_reply(CMD_TEAM, caller, C_OK, _("Player %s is made teamless."), 
        pplayer->name);
    return;
  }

  if (!is_sane_name(arg[1])) {
    cmd_reply(CMD_TEAM, caller, C_SYNTAX, _("Bad team name."));
    return;
  }
  team_add_player(pplayer, arg[1]);
  cmd_reply(CMD_TEAM, caller, C_OK, _("Player %s set to team %s."),
	    pplayer->name, team_get_by_id(pplayer->team)->name);
}

/******************************************************************
  ...
******************************************************************/
static void set_command(struct connection *caller, char *str) 
{
  char command[MAX_LEN_CONSOLE_LINE], arg[MAX_LEN_CONSOLE_LINE], *cptr_s, *cptr_d;
  int val, cmd;
  struct settings_s *op;
  bool do_update;
  char buffer[500];

  for (cptr_s = str; *cptr_s != '\0' && !is_ok_opt_name_char(*cptr_s);
       cptr_s++) {
    /* nothing */
  }

  for(cptr_d=command;
      *cptr_s != '\0' && is_ok_opt_name_char(*cptr_s);
      cptr_s++, cptr_d++) {
    *cptr_d=*cptr_s;
  }
  *cptr_d='\0';
  
  for (; *cptr_s != '\0' && is_ok_opt_name_value_sep_char(*cptr_s); cptr_s++) {
    /* nothing */
  }

  for (cptr_d = arg; *cptr_s != '\0' && is_ok_opt_value_char(*cptr_s); cptr_s++, cptr_d++)
    *cptr_d=*cptr_s;
  *cptr_d='\0';

  cmd = lookup_option(command);
  if (cmd==-1) {
    cmd_reply(CMD_SET, caller, C_SYNTAX,
	      _("Undefined argument.  Usage: set <option> <value>."));
    return;
  }
  else if (cmd==-2) {
    cmd_reply(CMD_SET, caller, C_SYNTAX,
	      _("Ambiguous option name."));
    return;
  }
  if (!may_set_option(caller,cmd)) {
     cmd_reply(CMD_SET, caller, C_FAIL,
	       _("You are not allowed to set this option."));
    return;
  }
  if (!sset_is_changeable(cmd)) {
    cmd_reply(CMD_SET, caller, C_BOUNCE,
	      _("This setting can't be modified after the game has started."));
    return;
  }
  if (sset_is_fixed[cmd] && !(map_is_empty() || game.is_new_game)) {
    cmd_reply(CMD_SET, caller, C_BOUNCE,
	      _("This setting is protected from being modified after the "
                "game has started."));
    return;
  }

  op = &settings[cmd];

  do_update = FALSE;
  buffer[0] = '\0';

  switch (op->type) {
  case SSET_BOOL:
    if (sscanf(arg, "%d", &val) != 1) {
      cmd_reply(CMD_SET, caller, C_SYNTAX, _("Value must be an integer."));
    } else if (val != 0 && val != 1) {
      cmd_reply(CMD_SET, caller, C_SYNTAX,
		_("Value out of range (minimum: 0, maximum: 1)."));
    } else {
      char *reject_message = NULL;
      bool b_val = (val != 0);

      if (settings[cmd].bool_validate
	  && !settings[cmd].bool_validate(b_val, &reject_message)) {
	cmd_reply(CMD_SET, caller, C_SYNTAX, reject_message);
      } else {
	*(op->bool_value) = b_val;
	my_snprintf(buffer, sizeof(buffer),
		    _("Option: %s has been set to %d."), op->name,
		    *(op->bool_value) ? 1 : 0);
      }
    }
    break;

  case SSET_INT:
    if (sscanf(arg, "%d", &val) != 1) {
      cmd_reply(CMD_SET, caller, C_SYNTAX, _("Value must be an integer."));
    } else if (val < op->int_min_value || val > op->int_max_value) {
      cmd_reply(CMD_SET, caller, C_SYNTAX,
		_("Value out of range (minimum: %d, maximum: %d)."),
		op->int_min_value, op->int_max_value);
    } else {
      char *reject_message = NULL;

      if (settings[cmd].int_validate
	  && !settings[cmd].int_validate(val, &reject_message)) {
	cmd_reply(CMD_SET, caller, C_SYNTAX, reject_message);
      } else {
	*(op->int_value) = val;
	my_snprintf(buffer, sizeof(buffer),
		    _("Option: %s has been set to %d."), op->name,
		    *(op->int_value));
	do_update = TRUE;
      }
    }
    break;

  case SSET_STRING:
    if (strlen(arg) >= op->string_value_size) {
      cmd_reply(CMD_SET, caller, C_SYNTAX,
		_("String value too long.  Usage: set <option> <value>."));
    } else {
      char *reject_message = NULL;

      if (settings[cmd].string_validate
	  && !settings[cmd].string_validate(arg, &reject_message)) {
	cmd_reply(CMD_SET, caller, C_SYNTAX, reject_message);
      } else {
	strcpy(op->string_value, arg);
	my_snprintf(buffer, sizeof(buffer),
		    _("Option: %s has been set to \"%s\"."), op->name,
		    op->string_value);
      }
    }
    break;
  }

  if (strlen(buffer) > 0 && sset_is_to_client(cmd)) {
    notify_player(NULL, "%s", buffer);
  }

  if (do_update) {
    /* canonify map generator settings (all of which are int) */
    adjust_terrain_param();
    /* 
     * send any modified game parameters to the clients -- if sent
     * before RUN_GAME_STATE, triggers a popdown_races_dialog() call
     * in client/packhand.c#handle_game_info() 
     */
    if (server_state == RUN_GAME_STATE) {
      send_game_info(NULL);
    }
  }
}

/**************************************************************************
  /fix and /unfix commands
  allow runtime modifiable options to be [un]fixed before the game starts
**************************************************************************/
static void fix_command(struct connection *caller, char *str, int cmd_enum)
{
  char buf[MAX_LEN_CONSOLE_LINE], *arg[1];
  int opt, ntokens;

  assert(cmd_enum == CMD_FIX || cmd_enum == CMD_UNFIX);

  /* can't use the command now */
  if (!(map_is_empty() || game.is_new_game)) {
    cmd_reply(cmd_enum, caller, C_FAIL,
	      _("This command may only be used before the game has started."));
    return;
  }

  /* we can use it. find the option name */
  sz_strlcpy(buf, str);
  ntokens = get_tokens(buf, arg, 1, TOKEN_DELIMITERS);

  if (ntokens == 0) {
    cmd_reply(cmd_enum, caller, C_SYNTAX,
              _("Need an argument.  Usage: %s <option-name>."),
              commands[cmd_enum].name);
    return;
  }

  opt = lookup_option(arg[0]);
  free(arg[0]);

  if (opt == -1) {
    cmd_reply(cmd_enum, caller, C_SYNTAX, _("Unknown option name."));
    return;
  } else if (opt == -2) {
    cmd_reply(cmd_enum, caller, C_SYNTAX, _("Ambiguous option name."));
    return;
  }

  /* option's good, can we do anything with it? */
  if (!sset_is_runtime_changeable_by_client(opt)) {
    cmd_reply(cmd_enum, caller, C_BOUNCE,
	    _("Option '%s' can never be modified after the game has started."),
              settings[opt].name);
    return;
  }

  if (!may_set_option(caller, opt)) {
    if (cmd_enum == CMD_FIX) {
      cmd_reply(cmd_enum, caller, C_FAIL,
		_("You are not allowed to set option '%s', so you "
		"may not prevent it from being set, either."),
		settings[opt].name);
    } else {
      cmd_reply(cmd_enum, caller, C_FAIL,
		_("You are not allowed to set option '%s', so you "
		"may not allow it to be set, either."),
		settings[opt].name);
    }
    return;
  }

  if (cmd_enum == CMD_FIX) {
    sset_is_fixed[opt] = TRUE;
    cmd_reply(cmd_enum, caller, C_OK,
		_("Option: '%s' now cannot be modified "
                "after the game has started."),
		settings[opt].name);
  } else {
    sset_is_fixed[opt] = FALSE;
    cmd_reply(cmd_enum, caller, C_OK,
		_("Option: '%s' may now be modified "
                "after the game has started."),
		settings[opt].name);
  }
}

/**************************************************************************
  take over a player. If a connection already has control of that player, 
  disallow unless the connection can multiconnect to the player 
  (not implemented yet).

  if there are two arguments, treat the first as the connection name and the
  second as the player name. (only hack and the console can do this)
  otherwise, there should be one argument, that being the player that the 
  caller wants to take.
**************************************************************************/
static void take_command(struct connection *caller, char *str)
{
  int i = 0, ntokens = 0;
  char buf[MAX_LEN_CONSOLE_LINE];
  char *arg[2];  
  bool was_connected;

  enum m_pre_result match_result;
  struct connection *pconn = NULL;
  struct player *pplayer = NULL;
  
  sz_strlcpy(buf, str);
  ntokens = get_tokens(buf, arg, 2, TOKEN_DELIMITERS);
  
  /* check syntax */
  if (ntokens == 0) {
    cmd_reply(CMD_TAKE, caller, C_SYNTAX,
              _("Usage: take [connection-name] <player-name>"));
    return;
  } 
  
  if (!caller && ntokens != 2) {
    cmd_reply(CMD_TAKE, caller, C_SYNTAX,
              _("Usage: take [connection-name] <player-name>"));
    goto end;
  } 
  
  if (ntokens == 2 && (caller && caller->access_level != ALLOW_HACK)) {
     cmd_reply(CMD_TAKE, caller, C_SYNTAX, 
              _("Usage: take <player-name>"));
    goto end;
  } 
  
  /* match the connection and player */
  if (!caller || (caller && caller->access_level == ALLOW_HACK)) {
    if (!(pconn = find_conn_by_user_prefix(arg[i], &match_result))) {
      cmd_reply_no_such_conn(CMD_TAKE, caller, arg[i], match_result);
      goto end;
    }
    i++; /* found a conn, now reference the second argument */
  } 
  
  if (!(pplayer = find_player_by_name_prefix(arg[i], &match_result))) {
    cmd_reply_no_such_player(CMD_TAKE, caller, arg[i], match_result);
    goto end;
  } 
  
  /* if we can't assign other connections to players, assign us to be pconn. */
  if (!pconn) {
    pconn = caller;
  }

  /* If the connection controls a player and the game is running, don't
   * let the user perform this command. The client isn't yet prepared
   * for this kind of power ;) It'll segfault and/or do crazy things. */
  if (pconn->player && server_state == RUN_GAME_STATE) {
    cmd_reply(CMD_TAKE, caller, C_FAIL,
              _("You can't switch players with \"take\" "
                "after the game has started."));
    goto end;
  }

  if (pconn->player == pplayer) {
    cmd_reply(CMD_TAKE, caller, C_FAIL,
              _("%s already controls or observes %s"),
              pconn->username, pplayer->name);
    goto end;
  } 

  was_connected = pplayer->is_connected;

  /* FIXME: remove when multiconnect becomes mature */
  conn_list_iterate(pplayer->connections, aconn) {
    cmd_reply(CMD_TAKE, caller, C_FAIL,
              _("%s already controls or observes %s.\n"
                "  multiple controlling connections aren't allowed yet."),
              pplayer->username, pplayer->name);
    goto end;
  } conn_list_iterate_end;

  /* if the connection is already attached to a player,
   * unattach and cleanup old player (rename, remove, etc) */
  if (pconn->player) {
    struct player *old_plr = pconn->player;

    /* FIXME: need to reset the client somehow, if this
     * happens while the game is running */

    unattach_connection_from_player(pconn);

    /* need to reattach before we possibly remove the old player */
    attach_connection_to_player(pconn, pplayer);

    cmd_reply(CMD_TAKE, caller, C_COMMENT,
              _("%s unconnecting from  %s"), pconn->username, old_plr->name);

    /* only remove the player if the game is new and in pregame, nobody 
     * is connected to it anymore and it wasn't AI-controlled. */
    if (!old_plr->is_connected && game.is_new_game && !old_plr->ai.control 
          && (server_state == PRE_GAME_STATE
              || server_state == SELECT_RACES_STATE)) {

      /* debugging, should we keep this around? */
      cmd_reply(CMD_TAKE, caller, C_COMMENT,
                _("removing %s [%d]"), old_plr->name, old_plr->player_no);

      game_remove_player(old_plr);
      game_renumber_players(old_plr->player_no);

      /* we screwed up the pplayer pointer by removing old_plr; get it back */
      pplayer = pconn->player;
    } else {
      sz_strlcpy(old_plr->username, ANON_USER_NAME);
    }
  } else {
    attach_connection_to_player(pconn, pplayer);
  }

  if (server_state == RUN_GAME_STATE) {
    send_packet_generic_empty(pconn, PACKET_FREEZE_HINT);
    send_rulesets(&pconn->self);
    send_all_info(&pconn->self);
    send_game_state(&pconn->self, CLIENT_GAME_RUNNING_STATE);
    send_player_info(NULL, NULL);
    send_diplomatic_meetings(pconn);
    send_packet_generic_empty(pconn, PACKET_THAW_HINT);
    send_packet_generic_empty(pconn, PACKET_START_TURN);
  }

  /* if the player we're taking was already connected, then the primary
   * controller set that player to ai control. don't undo that decision */
  if (!was_connected && pplayer->ai.control) {
    toggle_ai_player_direct(pconn, pplayer);
  }

  cmd_reply(CMD_TAKE, caller, C_OK, _("%s now controls %s"),
            pconn->username, pplayer->name);

  end:;
  /* free our args */
  for (i = 0; i < ntokens; i++) {
    free(arg[i]);
  }
}

/**************************************************************************
  ...
**************************************************************************/
void load_command(struct connection *caller, char *arg)
{
  struct timer *loadtimer, *uloadtimer;  
  struct section_file file;

  if (!arg || arg[0] == '\0') {
    cmd_reply(CMD_LOAD, caller, C_FAIL, _("Usage: load <filename>"));
    return;
  }

  if (server_state != PRE_GAME_STATE) {
    cmd_reply(CMD_LOAD, caller, C_FAIL, _("Can't load a game while another "
                                          "is running."));
    return;
  }

  /* attempt to parse the file */

  if (!section_file_load_nodup(&file, arg)) {
    cmd_reply(CMD_LOAD, caller, C_FAIL, _("Couldn't load savefile: %s"), arg);
    return;
  }

  cmd_reply(CMD_LOAD, caller, C_COMMENT, _("Loading saved game: %s..."), arg);

  /* we found it, free all structures */
  server_game_free();

  game_init();

  loadtimer = new_timer_start(TIMER_CPU, TIMER_ACTIVE);
  uloadtimer = new_timer_start(TIMER_USER, TIMER_ACTIVE);

  game_load(&file);
  section_file_check_unused(&file, arg);
  section_file_free(&file);

  freelog(LOG_VERBOSE, "Load time: %g seconds (%g apparent)",
          read_timer_seconds_free(loadtimer),
          read_timer_seconds_free(uloadtimer));

  /* attach connections to players. currently, this applies only 
   * to connections that have the correct username. Any attachments
   * made before the game load are unattached. */
  conn_list_iterate(game.est_connections, pconn) {
    if (pconn->player) {
      unattach_connection_from_player(pconn);
    }
    players_iterate(pplayer) {
      if (strcmp(pconn->username, pplayer->username) == 0) {
        attach_connection_to_player(pconn, pplayer);
        break;
      }
    } players_iterate_end;
  } conn_list_iterate_end;

}

/**************************************************************************
  ...
**************************************************************************/
static void set_rulesetdir(struct connection *caller, char *str)
{
  char filename[512], *pfilename;
  if ((str == NULL) || (strlen(str)==0)) {
    cmd_reply(CMD_RULESETDIR, caller, C_SYNTAX,
             _("Current ruleset directory is \"%s\""), game.rulesetdir);
    return;
  }
  my_snprintf(filename, sizeof(filename), "%s", str);
  pfilename = datafilename(filename);
  if (!pfilename) {
    cmd_reply(CMD_RULESETDIR, caller, C_SYNTAX,
             _("Ruleset directory \"%s\" not found"), str);
    return;
  }
  cmd_reply(CMD_RULESETDIR, caller, 
      C_OK, _("Ruleset directory set to \"%s\""), str);
  sz_strlcpy(game.rulesetdir, str);
}

/**************************************************************************
  Cutting away a trailing comment by putting a '\0' on the '#'. The
  method handles # in single or double quotes. It also takes care of
  "\#".
**************************************************************************/
static void cut_comment(char *str)
{
  int i;
  bool in_single_quotes = FALSE, in_double_quotes = FALSE;

  freelog(LOG_DEBUG,"cut_comment(str='%s')",str);

  for (i = 0; i < strlen(str); i++) {
    if (str[i] == '"' && !in_single_quotes) {
      in_double_quotes = !in_double_quotes;
    } else if (str[i] == '\'' && !in_double_quotes) {
      in_single_quotes = !in_single_quotes;
    } else if (str[i] == '#' && !(in_single_quotes || in_double_quotes)
	       && (i == 0 || str[i - 1] != '\\')) {
      str[i] = '\0';
      break;
    }
  }
  freelog(LOG_DEBUG,"cut_comment: returning '%s'",str);
}

/**************************************************************************
...
**************************************************************************/
static void quit_game(struct connection *caller)
{
  cmd_reply(CMD_QUIT, caller, C_OK, _("Goodbye."));
  server_quit();
}

/**************************************************************************
  Handle "command input", which could really come from stdin on console,
  or from client chat command, or read from file with -r, etc.
  caller==NULL means console, str is the input, which may optionally
  start with SERVER_COMMAND_PREFIX character
**************************************************************************/
void handle_stdin_input(struct connection *caller, char *str)
{
  char command[MAX_LEN_CONSOLE_LINE], arg[MAX_LEN_CONSOLE_LINE],
      allargs[MAX_LEN_CONSOLE_LINE], *cptr_s, *cptr_d;
  int i;
  enum command_id cmd;

  /* notify to the server console */
  if (caller) {
    con_write(C_COMMENT, "%s: '%s'", caller->username, str);
  }

  /* if the caller may not use any commands at all, don't waste any time */
  if (may_use_nothing(caller)) {
    cmd_reply(CMD_HELP, caller, C_FAIL,
	_("Sorry, you are not allowed to use server commands."));
     return;
  }

  /* Is it a comment or a blank line? */
  /* line is comment if the first non-whitespace character is '#': */
  for (cptr_s = str; *cptr_s != '\0' && my_isspace(*cptr_s); cptr_s++) {
    /* nothing */
  }
  if(*cptr_s == 0 || *cptr_s == '#') {
    return;
  }

  /* commands may be prefixed with SERVER_COMMAND_PREFIX, even when
     given on the server command line - rp */
  if (*cptr_s == SERVER_COMMAND_PREFIX) cptr_s++;

  for (; *cptr_s != '\0' && !my_isalnum(*cptr_s); cptr_s++) {
    /* nothing */
  }

  /*
   * cptr_s points now to the beginning of the real command. It has
   * skipped leading whitespace, the SERVER_COMMAND_PREFIX and any
   * other non-alphanumeric characters.
   */
  for (cptr_d = command; *cptr_s != '\0' && my_isalnum(*cptr_s) &&
      cptr_d < command+sizeof(command)-1; cptr_s++, cptr_d++)
    *cptr_d=*cptr_s;
  *cptr_d='\0';

  cmd = command_named(command, FALSE);
  if (cmd == CMD_AMBIGUOUS) {
    cmd = command_named(command, TRUE);
    cmd_reply(cmd, caller, C_SYNTAX,
	_("Warning: '%s' interpreted as '%s', but it is ambiguous."
	  "  Try '%shelp'."),
	command, commands[cmd].name, caller?"/":"");
  } else if (cmd == CMD_UNRECOGNIZED) {
    cmd_reply(cmd, caller, C_SYNTAX,
	_("Unknown command.  Try '%shelp'."), caller?"/":"");
    return;
  }
  if (!may_use(caller, cmd)) {
    assert(caller != NULL);
    cmd_reply(cmd, caller, C_FAIL,
	      _("You are not allowed to use this command."));
    return;
  }

  for (; *cptr_s != '\0' && my_isspace(*cptr_s); cptr_s++) {
    /* nothing */
  }
  sz_strlcpy(arg, cptr_s);

  cut_comment(arg);

  /* keep this before we cut everything after a space */
  sz_strlcpy(allargs, cptr_s);
  cut_comment(allargs);

  i=strlen(arg)-1;
  while(i>0 && my_isspace(arg[i]))
    arg[i--]='\0';

  if (commands[cmd].level > ALLOW_INFO) {
    /*
     * this command will affect the game - inform all players
     *
     * use command,arg instead of str because of the trailing
     * newline in str when it comes from the server command line
     */
    notify_player(NULL, "%s: '%s %s'",
      caller ? caller->username : _("(server prompt)"), command, arg);
  }

  switch(cmd) {
  case CMD_REMOVE:
    remove_player(caller,arg);
    break;
  case CMD_RENAME:
    rename_player(caller,arg);
    break;
  case CMD_SAVE:
    save_command(caller,arg);
    break;
  case CMD_LOAD:
    load_command(caller, arg);
    break;
  case CMD_METAINFO:
    metainfo_command(caller,arg);
    break;
  case CMD_METACONN:
    metaconnection_command(caller,arg);
    break;
  case CMD_METASERVER:
    metaserver_command(caller,arg);
    break;
  case CMD_HELP:
    show_help(caller, arg);
    break;
  case CMD_LIST:
    show_list(caller, arg);
    break;
  case CMD_AITOGGLE:
    toggle_ai_player(caller,arg);
    break;
  case CMD_TAKE:
    take_command(caller, arg);
    break;
  case CMD_CREATE:
    create_ai_player(caller,arg);
    break;
  case CMD_AWAY:
    set_away(caller, arg);
    break;
  case CMD_EASY:
    set_ai_level(caller, arg, 3);
    break;
  case CMD_NORMAL:
    set_ai_level(caller, arg, 5);
    break;
  case CMD_HARD:
    set_ai_level(caller, arg, 7);
    break;
#ifdef DEBUG
  case CMD_EXPERIMENTAL:
    set_ai_level(caller, arg, 10);
    break;
#endif
  case CMD_QUIT:
    quit_game(caller);
    break;
  case CMD_CUT:
    cut_client_connection(caller, arg);
    break;
  case CMD_SHOW:
    show_command(caller,arg);
    break;
  case CMD_EXPLAIN:
    explain_option(caller,arg);
    break;
  case CMD_SET:
    set_command(caller,arg);
    break;
  case CMD_TEAM:
    team_command(caller, arg);
    break;
  case CMD_FIX:
    fix_command(caller,arg, CMD_FIX);
    break;
  case CMD_UNFIX:
    fix_command(caller, arg, CMD_UNFIX);
    break;
  case CMD_RULESETDIR:
    set_rulesetdir(caller, arg);
    break;
  case CMD_SCORE:
    if (server_state == RUN_GAME_STATE || server_state == GAME_OVER_STATE) {
      report_scores(FALSE);
    } else {
      cmd_reply(cmd, caller, C_SYNTAX,
		_("The game must be running before you can see the score."));
    }
    break;
  case CMD_WALL:
    wall(arg);
    break;
  case CMD_READ_SCRIPT:
    read_command(caller,arg);
    break;
  case CMD_WRITE_SCRIPT:
    write_command(caller,arg);
    break;
  case CMD_RFCSTYLE:	/* see console.h for an explanation */
    con_set_style(!con_get_style());
    break;
  case CMD_CMDLEVEL:
    cmdlevel_command(caller,arg);
    break;
  case CMD_FIRSTLEVEL:
    firstlevel_command(caller);
    break;
  case CMD_TIMEOUT:
    timeout_command(caller, allargs);
    break;
  case CMD_START_GAME:
    if (server_state==PRE_GAME_STATE) {

      /* Sanity check scenario */
      if (game.is_new_game) {
	if (map.fixed_start_positions
	    && game.max_players > map.num_start_positions) {
	  freelog(LOG_VERBOSE, "Reduced maxplayers from %i to %i to fit "
		  "to the number of start positions.",
		  game.max_players, map.num_start_positions);
	  game.max_players = map.num_start_positions;
	}

	if (game.nplayers > game.max_players) {
	  /* Because of the way player ids are renumbered during
	     server_remove_player() this is correct */
	  while (game.nplayers > game.max_players) {
	    server_remove_player(get_player(game.max_players));
          }

	  freelog(LOG_VERBOSE,
		  "Had to cut down the number of players to the "
		  "number of map start positions, there must be "
		  "something wrong with the savegame or you "
		  "adjusted the maxplayers value.");
	}
      }

      /* check min_players */
      if (game.nplayers < game.min_players) {
        cmd_reply(cmd,caller, C_FAIL,
		  _("Not enough players, game will not start."));
        break;
      }

      start_game();
    } else {
      cmd_reply(cmd,caller, C_FAIL,
		_("Cannot start the game: it is already running."));
    }
    break;
  case CMD_END_GAME:
    if (server_state==RUN_GAME_STATE) {
      server_state = GAME_OVER_STATE;
      force_end_of_sniff = TRUE;
    } else {
      cmd_reply(cmd,caller, C_FAIL,
		_("Cannot end the game: no game running."));
    }
    break;
  case CMD_NUM:
  case CMD_UNRECOGNIZED:
  case CMD_AMBIGUOUS:
  default:
    freelog(LOG_FATAL, "bug in civserver: impossible command recognized; bye!");
    assert(0);
  }
}

/**************************************************************************
...
**************************************************************************/
static void cut_client_connection(struct connection *caller, char *name)
{
  enum m_pre_result match_result;
  struct connection *ptarget;
  struct player *pplayer;

  ptarget = find_conn_by_user_prefix(name, &match_result);

  if (!ptarget) {
    cmd_reply_no_such_conn(CMD_CUT, caller, name, match_result);
    return;
  }

  pplayer = ptarget->player;

  cmd_reply(CMD_CUT, caller, C_DISCONNECTED,
	    _("Cutting connection %s."), ptarget->username);
  lost_connection_to_client(ptarget);
  close_connection(ptarget);

  /* if we cut the connection, unassign the login name */
  if (pplayer) {
    sz_strlcpy(pplayer->username, ANON_USER_NAME);
  }
}

/**************************************************************************
 Show caller introductory help about the server.
 help_cmd is the command the player used.
**************************************************************************/
static void show_help_intro(struct connection *caller, enum command_id help_cmd)
{
  /* This is formated like extra_help entries for settings and commands: */
  const char *help =
    _("Welcome - this is the introductory help text for the Freeciv server.\n\n"
      "Two important server concepts are Commands and Options.\n"
      "Commands, such as 'help', are used to interact with the server.\n"
      "Some commands take one or more arguments, separated by spaces.\n"
      "In many cases commands and command arguments may be abbreviated.\n"
      "Options are settings which control the server as it is running.\n\n"
      "To find out how to get more information about commands and options,\n"
      "use 'help help'.\n\n"
      "For the impatient, the main commands to get going are:\n"
      "  show   -  to see current options\n"
      "  set    -  to set options\n"
      "  start  -  to start the game once players have connected\n"
      "  save   -  to save the current game\n"
      "  quit   -  to exit");

  static struct astring abuf = ASTRING_INIT;
      
  astr_minsize(&abuf, strlen(help)+1);
  strcpy(abuf.str, help);
  wordwrap_string(abuf.str, 78);
  cmd_reply(help_cmd, caller, C_COMMENT, abuf.str);
}

/**************************************************************************
 Show the caller detailed help for the single COMMAND given by id.
 help_cmd is the command the player used.
**************************************************************************/
static void show_help_command(struct connection *caller,
			      enum command_id help_cmd,
			      enum command_id id)
{
  const struct command *cmd = &commands[id];
  
  if (cmd->short_help) {
    cmd_reply(help_cmd, caller, C_COMMENT,
	      "%s %s  -  %s", _("Command:"), cmd->name, _(cmd->short_help));
  } else {
    cmd_reply(help_cmd, caller, C_COMMENT,
	      "%s %s", _("Command:"), cmd->name);
  }
  if (cmd->synopsis) {
    /* line up the synopsis lines: */
    const char *syn = _("Synopsis: ");
    size_t synlen = strlen(syn);
    char prefix[40];

    my_snprintf(prefix, sizeof(prefix), "%*s", (int) synlen, " ");
    cmd_reply_prefix(help_cmd, caller, C_COMMENT, prefix,
		     "%s%s", syn, _(cmd->synopsis));
  }
  cmd_reply(help_cmd, caller, C_COMMENT,
	    _("Level: %s"), cmdlevel_name(cmd->level));
  if (cmd->extra_help) {
    static struct astring abuf = ASTRING_INIT;
    const char *help = _(cmd->extra_help);
      
    astr_minsize(&abuf, strlen(help)+1);
    strcpy(abuf.str, help);
    wordwrap_string(abuf.str, 76);
    cmd_reply(help_cmd, caller, C_COMMENT, _("Description:"));
    cmd_reply_prefix(help_cmd, caller, C_COMMENT, "  ",
		     "  %s", abuf.str);
  }
}

/**************************************************************************
 Show the caller list of COMMANDS.
 help_cmd is the command the player used.
**************************************************************************/
static void show_help_command_list(struct connection *caller,
				  enum command_id help_cmd)
{
  enum command_id i;
  
  cmd_reply(help_cmd, caller, C_COMMENT, horiz_line);
  cmd_reply(help_cmd, caller, C_COMMENT,
	    _("The following server commands are available:"));
  cmd_reply(help_cmd, caller, C_COMMENT, horiz_line);
  if(!caller && con_get_style()) {
    for (i=0; i<CMD_NUM; i++) {
      cmd_reply(help_cmd, caller, C_COMMENT, "%s", commands[i].name);
    }
  } else {
    char buf[MAX_LEN_CONSOLE_LINE];
    int j;
    
    buf[0] = '\0';
    for (i=0, j=0; i<CMD_NUM; i++) {
      if (may_use(caller, i)) {
	cat_snprintf(buf, sizeof(buf), "%-19s", commands[i].name);
	if((++j % 4) == 0) {
	  cmd_reply(help_cmd, caller, C_COMMENT, buf);
	  buf[0] = '\0';
	}
      }
    }
    if (buf[0] != '\0')
      cmd_reply(help_cmd, caller, C_COMMENT, buf);
  }
  cmd_reply(help_cmd, caller, C_COMMENT, horiz_line);
}

/**************************************************************************
  Additional 'help' arguments
**************************************************************************/
enum HELP_GENERAL_ARGS { HELP_GENERAL_COMMANDS, HELP_GENERAL_OPTIONS,
			 HELP_GENERAL_NUM /* Must be last */ };
static const char * const help_general_args[] = {
  "commands", "options", NULL
};

/**************************************************************************
  Unified indices for help arguments:
    CMD_NUM           -  Server commands
    HELP_GENERAL_NUM  -  General help arguments, above
    SETTINGS_NUM      -  Server options 
**************************************************************************/
#define HELP_ARG_NUM (CMD_NUM + HELP_GENERAL_NUM + SETTINGS_NUM)

/**************************************************************************
  Convert unified helparg index to string; see above.
**************************************************************************/
static const char *helparg_accessor(int i) {
  if (i<CMD_NUM)
    return cmdname_accessor(i);
  i -= CMD_NUM;
  if (i<HELP_GENERAL_NUM)
    return help_general_args[i];
  i -= HELP_GENERAL_NUM;
  return optname_accessor(i);
}
/**************************************************************************
...
**************************************************************************/
static void show_help(struct connection *caller, char *arg)
{
  enum m_pre_result match_result;
  int ind;

  assert(!may_use_nothing(caller));
    /* no commands means no help, either */

  match_result = match_prefix(helparg_accessor, HELP_ARG_NUM, 0,
			      mystrncasecmp, arg, &ind);

  if (match_result==M_PRE_EMPTY) {
    show_help_intro(caller, CMD_HELP);
    return;
  }
  if (match_result==M_PRE_AMBIGUOUS) {
    cmd_reply(CMD_HELP, caller, C_FAIL,
	      _("Help argument '%s' is ambiguous."), arg);
    return;
  }
  if (match_result==M_PRE_FAIL) {
    cmd_reply(CMD_HELP, caller, C_FAIL,
	      _("No match for help argument '%s'."), arg);
    return;
  }

  /* other cases should be above */
  assert(match_result < M_PRE_AMBIGUOUS);
  
  if (ind < CMD_NUM) {
    show_help_command(caller, CMD_HELP, ind);
    return;
  }
  ind -= CMD_NUM;
  
  if (ind == HELP_GENERAL_OPTIONS) {
    show_help_option_list(caller, CMD_HELP);
    return;
  }
  if (ind == HELP_GENERAL_COMMANDS) {
    show_help_command_list(caller, CMD_HELP);
    return;
  }
  ind -= HELP_GENERAL_NUM;
  
  if (ind < SETTINGS_NUM) {
    show_help_option(caller, CMD_HELP, ind);
    return;
  }
  
  /* should have finished by now */
  freelog(LOG_ERROR, "Bug in show_help!");
}

/**************************************************************************
  'list' arguments
**************************************************************************/
enum LIST_ARGS { LIST_PLAYERS, LIST_CONNECTIONS,
		 LIST_ARG_NUM /* Must be last */ };
static const char * const list_args[] = {
  "players", "connections", NULL
};
static const char *listarg_accessor(int i) {
  return list_args[i];
}

/**************************************************************************
  Show list of players or connections, or connection statistics.
**************************************************************************/
static void show_list(struct connection *caller, char *arg)
{
  enum m_pre_result match_result;
  int ind;

  remove_leading_trailing_spaces(arg);
  match_result = match_prefix(listarg_accessor, LIST_ARG_NUM, 0,
			      mystrncasecmp, arg, &ind);

  if (match_result > M_PRE_EMPTY) {
    cmd_reply(CMD_LIST, caller, C_SYNTAX,
	      _("Bad list argument: '%s'.  Try '%shelp list'."),
	      arg, (caller?"/":""));
    return;
  }

  if (match_result == M_PRE_EMPTY) {
    ind = LIST_PLAYERS;
  }

  switch(ind) {
  case LIST_PLAYERS:
    show_players(caller);
    return;
  case LIST_CONNECTIONS:
    show_connections(caller);
    return;
  default:
    cmd_reply(CMD_LIST, caller, C_FAIL,
	      "Internal error: ind %d in show_list", ind);
    freelog(LOG_ERROR, "Internal error: ind %d in show_list", ind);
    return;
  }
}

/**************************************************************************
...
**************************************************************************/
void show_players(struct connection *caller)
{
  char buf[MAX_LEN_CONSOLE_LINE], buf2[MAX_LEN_CONSOLE_LINE];
  int n;

  cmd_reply(CMD_LIST, caller, C_COMMENT, _("List of players:"));
  cmd_reply(CMD_LIST, caller, C_COMMENT, horiz_line);


  if (game.nplayers == 0)
    cmd_reply(CMD_LIST, caller, C_WARNING, _("<no players>"));
  else
  {
    players_iterate(pplayer) {

      /* Low access level callers don't get to see barbarians in list: */
      if (is_barbarian(pplayer) && caller
	  && (caller->access_level < ALLOW_CTRL)) {
	continue;
      }

      /* buf2 contains stuff in brackets after playername:
       * [username,] AI/Barbarian/Human [,Dead] [, skill level] [, nation]
       */
      buf2[0] = '\0';
      if (strlen(pplayer->username) > 0
	  && strcmp(pplayer->username, "nouser") != 0) {
	my_snprintf(buf2, sizeof(buf2), _("user %s, "), pplayer->username);
      }
      
      if (is_barbarian(pplayer)) {
	sz_strlcat(buf2, _("Barbarian"));
      } else if (pplayer->ai.control) {
	sz_strlcat(buf2, _("AI"));
      } else {
	sz_strlcat(buf2, _("Human"));
      }
      if (!pplayer->is_alive) {
	sz_strlcat(buf2, _(", Dead"));
      }
      if(pplayer->ai.control) {
	cat_snprintf(buf2, sizeof(buf2), _(", difficulty level %s"),
		     name_of_skill_level(pplayer->ai.skill_level));
      }
      if (!game.is_new_game) {
	cat_snprintf(buf2, sizeof(buf2), _(", nation %s"),
		     get_nation_name_plural(pplayer->nation));
      }
      if (pplayer->team != TEAM_NONE) {
        cat_snprintf(buf2, sizeof(buf2), (", team %s"),
                     team_get_by_id(pplayer->team)->name);
      }
      my_snprintf(buf, sizeof(buf), "%s (%s)", pplayer->name, buf2);
      
      n = conn_list_size(&pplayer->connections);
      if (n > 0) {
        cat_snprintf(buf, sizeof(buf), 
                     PL_(" %d connection:", " %d connections:", n), n);
      }
      cmd_reply(CMD_LIST, caller, C_COMMENT, "%s", buf);
      
      conn_list_iterate(pplayer->connections, pconn) {
	my_snprintf(buf, sizeof(buf),
		    _("  %s from %s (command access level %s), bufsize=%dkb"),
		    pconn->username, pconn->addr, 
		    cmdlevel_name(pconn->access_level),
		    (pconn->send_buffer->nsize>>10));
	if (pconn->observer) {
	  sz_strlcat(buf, _(" (observer mode)"));
	}
	cmd_reply(CMD_LIST, caller, C_COMMENT, "%s", buf);
      } conn_list_iterate_end;
    } players_iterate_end;
  }
  cmd_reply(CMD_LIST, caller, C_COMMENT, horiz_line);
}

/**************************************************************************
  List connections; initially mainly for debugging
**************************************************************************/
static void show_connections(struct connection *caller)
{
  char buf[MAX_LEN_CONSOLE_LINE];
  
  cmd_reply(CMD_LIST, caller, C_COMMENT, _("List of connections to server:"));
  cmd_reply(CMD_LIST, caller, C_COMMENT, horiz_line);

  if (conn_list_size(&game.all_connections) == 0) {
    cmd_reply(CMD_LIST, caller, C_WARNING, _("<no connections>"));
  }
  else {
    conn_list_iterate(game.all_connections, pconn) {
      sz_strlcpy(buf, conn_description(pconn));
      if (pconn->established) {
	cat_snprintf(buf, sizeof(buf), " command access level %s",
		     cmdlevel_name(pconn->access_level));
      }
      cmd_reply(CMD_LIST, caller, C_COMMENT, "%s", buf);
    }
    conn_list_iterate_end;
  }
  cmd_reply(CMD_LIST, caller, C_COMMENT, horiz_line);
}

/**************************************************************************
  Verify that notradesize is always smaller than fulltradesize
**************************************************************************/
static bool valid_notradesize(int value, char **reject_message)
{
  if (value < game.fulltradesize)
    return TRUE;

  *reject_message = _("notradesize must be always smaller than "
		      "fulltradesize, keeping old value.");
  return FALSE;
}

/**************************************************************************
  Verify that fulltradesize is always bigger than notradesize
**************************************************************************/
static bool valid_fulltradesize(int value, char **reject_message)
{
  if (value > game.notradesize)
    return TRUE;

  *reject_message = _("fulltradesize must be always bigger than "
		      "notradesize, keeping old value.");
  return FALSE;
}

static bool autotoggle(bool value, char **reject_message)
{
  if (!value)
    return TRUE;

  players_iterate(pplayer) {
    if (!pplayer->ai.control
	&& !pplayer->is_connected) {
      toggle_ai_player_direct(NULL, pplayer);
    }
  } players_iterate_end;

  reject_message = NULL; /* we should modify this, but since we cannot fail... */
  return TRUE;
}

/*************************************************************************
  Verify that a given allowconnect string is valid.  See
  game.allow_connect.
*************************************************************************/
static bool is_valid_allowconnect(const char *allow_connect,
				  char **error_string)
{
  int len = strlen(allow_connect), i;
  bool havecharacter_state = FALSE;

  /* We check each character individually to see if it's valid.  This
   * does not check for duplicate entries.
   *
   * We also track the state of the machine.  havecharacter_state is
   * true if the preceeding character was a primary label, e.g.
   * NHhAadb.  It is false if the preceeding character was a modifier
   * or if this is the first character. */

  for (i = 0; i < len; i++) {
    /* Check to see if the character is a primary label. */
    if (strchr("NHhAadb", allow_connect[i])) {
      havecharacter_state = TRUE;
      continue;
    }

    /* If we've already passed a primary label, check to see if the
     * character is a modifier. */
    if (havecharacter_state
	&& strchr("*+=-", allow_connect[i])) {
      havecharacter_state = FALSE;
      continue;
    }

    /* Looks like the character was invalid. */
    *error_string = _("Allowed connections string contains invalid\n"
		      "characters.  Try \"help allowconnect\".");
    return FALSE;
  }

  /* All characters were valid. */
  *error_string = NULL;
  return TRUE;
}

#ifdef HAVE_LIBREADLINE
/********************* RL completion functions ***************************/
/* To properly complete both commands, player names, options and filenames
   there is one array per type of completion with the commands that
   the type is relevant for.
*/

/**************************************************************************
  A generalised generator function: text and state are "standard"
  parameters to a readline generator function;
  num is number of possible completions, and index2str is a function
  which returns each possible completion string by index.
**************************************************************************/
static char *generic_generator(const char *text, int state, int num,
			       const char*(*index2str)(int))
{
  static int list_index, len;
  const char *name;

  /* If this is a new word to complete, initialize now.  This includes
     saving the length of TEXT for efficiency, and initializing the index
     variable to 0. */
  if (state == 0) {
    list_index = 0;
    len = strlen (text);
  }

  /* Return the next name which partially matches: */
  while (list_index < num) {
    name = index2str(list_index);
    list_index++;

    if (mystrncasecmp (name, text, len) == 0)
      return mystrdup(name);
  }

  /* If no names matched, then return NULL. */
  return ((char *)NULL);
}

/**************************************************************************
The valid commands at the root of the prompt.
**************************************************************************/
static char *command_generator(const char *text, int state)
{
  return generic_generator(text, state, CMD_NUM, cmdname_accessor);
}

/**************************************************************************
The valid arguments to "set" and "explain"
**************************************************************************/
static char *option_generator(const char *text, int state)
{
  return generic_generator(text, state, SETTINGS_NUM, optname_accessor);
}

/**************************************************************************
The player names.
**************************************************************************/
static const char *playername_accessor(int idx)
{
  return get_player(idx)->name;
}
static char *player_generator(const char *text, int state)
{
  return generic_generator(text, state, game.nplayers, playername_accessor);
}

/**************************************************************************
The connection user names, from game.all_connections.
**************************************************************************/
static const char *connection_name_accessor(int idx)
{
  return conn_list_get(&game.all_connections, idx)->username;
}
static char *connection_generator(const char *text, int state)
{
  return generic_generator(text, state, conn_list_size(&game.all_connections),
			   connection_name_accessor);
}

/**************************************************************************
The valid arguments for the first argument to "cmdlevel".
Extra accessor function since cmdlevel_name() takes enum argument, not int.
**************************************************************************/
static const char *cmdlevel_arg1_accessor(int idx)
{
  return cmdlevel_name(idx);
}
static char *cmdlevel_arg1_generator(const char *text, int state)
{
  return generic_generator(text, state, ALLOW_NUM, cmdlevel_arg1_accessor);
}

/**************************************************************************
The valid arguments for the second argument to "cmdlevel":
"first" or "new" or a connection name.
**************************************************************************/
static const char *cmdlevel_arg2_accessor(int idx)
{
  return ((idx==0) ? "first" :
	  (idx==1) ? "new" :
	  connection_name_accessor(idx-2));
}
static char *cmdlevel_arg2_generator(const char *text, int state)
{
  return generic_generator(text, state,
			   2 + conn_list_size(&game.all_connections),
			   cmdlevel_arg2_accessor);
}

/**************************************************************************
The valid first arguments to "help".
**************************************************************************/
static char *help_generator(const char *text, int state)
{
  return generic_generator(text, state, HELP_ARG_NUM, helparg_accessor);
}

/**************************************************************************
The valid first arguments to "list".
**************************************************************************/
static char *list_generator(const char *text, int state)
{
  return generic_generator(text, state, LIST_ARG_NUM, listarg_accessor);
}

/**************************************************************************
returns whether the characters before the start position in rl_line_buffer
is of the form [non-alpha]*cmd[non-alpha]*
allow_fluff changes the regexp to [non-alpha]*cmd[non-alpha].*
**************************************************************************/
static bool contains_str_before_start(int start, const char *cmd, bool allow_fluff)
{
  char *str_itr = rl_line_buffer;
  int cmd_len = strlen(cmd);

  while (str_itr < rl_line_buffer + start && !my_isalnum(*str_itr))
    str_itr++;

  if (mystrncasecmp(str_itr, cmd, cmd_len) != 0)
    return FALSE;
  str_itr += cmd_len;

  if (my_isalnum(*str_itr)) /* not a distinct word */
    return FALSE;

  if (!allow_fluff) {
    for (; str_itr < rl_line_buffer + start; str_itr++)
      if (my_isalnum(*str_itr))
	return FALSE;
  }

  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
static bool is_command(int start)
{
  char *str_itr;

  if (contains_str_before_start(start, commands[CMD_HELP].name, FALSE))
    return TRUE;

  /* if there is only it is also OK */
  str_itr = rl_line_buffer;
  while (str_itr - rl_line_buffer < start) {
    if (my_isalnum(*str_itr))
      return FALSE;
    str_itr++;
  }
  return TRUE;
}

/**************************************************************************
Commands that may be followed by a player name
**************************************************************************/
static const int player_cmd[] = {
  CMD_RENAME,
  CMD_AITOGGLE,
  CMD_EASY,
  CMD_NORMAL,
  CMD_HARD,
#ifdef DEBUG
  CMD_EXPERIMENTAL,
#endif
  CMD_REMOVE,
  -1
};

/**************************************************************************
number of tokens in rl_line_buffer before start
**************************************************************************/
static int num_tokens(int start)
{
  int res = 0;
  bool alnum = FALSE;
  char *chptr = rl_line_buffer;

  while (chptr - rl_line_buffer < start) {
    if (my_isalnum(*chptr)) {
      if (!alnum) {
	alnum = TRUE;
	res++;
      }
    } else {
      alnum = FALSE;
    }
    chptr++;
  }

  return res;
}

/**************************************************************************
...
**************************************************************************/
static bool is_player(int start)
{
  int i = 0;

  while (player_cmd[i] != -1) {
    if (contains_str_before_start(start, commands[player_cmd[i]].name, FALSE)) {
      return TRUE;
    }
    i++;
  }

  return FALSE;
}

/**************************************************************************
...
**************************************************************************/
static bool is_connection(int start)
{
  return contains_str_before_start(start, commands[CMD_CUT].name, FALSE);
}

/**************************************************************************
...
**************************************************************************/
static bool is_cmdlevel_arg2(int start)
{
  return (contains_str_before_start(start, commands[CMD_CMDLEVEL].name, TRUE)
	  && num_tokens(start) == 2);
}

/**************************************************************************
...
**************************************************************************/
static bool is_cmdlevel_arg1(int start)
{
  return contains_str_before_start(start, commands[CMD_CMDLEVEL].name, FALSE);
}

/**************************************************************************
Commands that may be followed by a server option name
**************************************************************************/
static const int server_option_cmd[] = {
  CMD_EXPLAIN,
  CMD_SET,
  CMD_SHOW,
  -1
};

/**************************************************************************
...
**************************************************************************/
static bool is_server_option(int start)
{
  int i = 0;

  while (server_option_cmd[i] != -1) {
    if (contains_str_before_start(start, commands[server_option_cmd[i]].name, FALSE))
      return TRUE;
    i++;
  }

  return FALSE;
}

/**************************************************************************
Commands that may be followed by a filename
**************************************************************************/
static const int filename_cmd[] = {
  CMD_LOAD,
  CMD_SAVE,
  CMD_READ_SCRIPT,
  CMD_WRITE_SCRIPT,
  -1
};

/**************************************************************************
...
**************************************************************************/
static bool is_filename(int start)
{
  int i = 0;

  while (filename_cmd[i] != -1) {
    if (contains_str_before_start
	(start, commands[filename_cmd[i]].name, FALSE)) {
      return TRUE;
    }
    i++;
  }

  return FALSE;
}

/**************************************************************************
...
**************************************************************************/
static bool is_help(int start)
{
  return contains_str_before_start(start, commands[CMD_HELP].name, FALSE);
}

/**************************************************************************
...
**************************************************************************/
static bool is_list(int start)
{
  return contains_str_before_start(start, commands[CMD_LIST].name, FALSE);
}

/**************************************************************************
Attempt to complete on the contents of TEXT.  START and END bound the
region of rl_line_buffer that contains the word to complete.  TEXT is
the word to complete.  We can use the entire contents of rl_line_buffer
in case we want to do some simple parsing.  Return the array of matches,
or NULL if there aren't any.
**************************************************************************/
#ifdef HAVE_NEWLIBREADLINE
char **freeciv_completion(const char *text, int start, int end)
#else
char **freeciv_completion(char *text, int start, int end)
#endif
{
  char **matches = (char **)NULL;

  if (is_help(start)) {
    matches = completion_matches(text, help_generator);
  } else if (is_command(start)) {
    matches = completion_matches(text, command_generator);
  } else if (is_list(start)) {
    matches = completion_matches(text, list_generator);
  } else if (is_cmdlevel_arg2(start)) {
    matches = completion_matches(text, cmdlevel_arg2_generator);
  } else if (is_cmdlevel_arg1(start)) {
    matches = completion_matches(text, cmdlevel_arg1_generator);
  } else if (is_connection(start)) {
    matches = completion_matches(text, connection_generator);
  } else if (is_player(start)) {
    matches = completion_matches(text, player_generator);
  } else if (is_server_option(start)) {
    matches = completion_matches(text, option_generator);
  } else if (is_filename(start)) {
    /* This function we get from readline */
    matches = completion_matches(text, filename_completion_function);
  } else /* We have no idea what to do */
    matches = NULL;

  /* Don't automatically try to complete with filenames */
  rl_attempted_completion_over = 1;

  return (matches);
}

#endif /* HAVE_LIBREADLINE */
