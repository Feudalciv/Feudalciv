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

#include "capstr.h"
#include "city.h"
#include "cm.h"
#include "connection.h"
#include "fcintl.h"
#include "government.h"
#include "idex.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "nation.h"
#include "packets.h"
#include "player.h"
#include "shared.h"
#include "spaceship.h"
#include "support.h"
#include "tech.h"
#include "unit.h"

#include "game.h"

struct civ_game game;

/*
struct player_score {
  int happy;
  int content;
  int unhappy;
  int angry;
  int taxmen;
  int scientists;
  int elvis;
  int wonders;
  int techs;
  int landarea;
  int settledarea;
  int population;
  int cities;
  int units;
  int pollution;
  int literacy;
  int bnp;
  int mfg;
  int spaceship;
};
*/

/**************************************************************************
Count the # of thousand citizen in a civilisation.
**************************************************************************/
int civ_population(const struct player *pplayer)
{
  int ppl=0;
  city_list_iterate(pplayer->cities, pcity)
    ppl+=city_population(pcity);
  city_list_iterate_end;
  return ppl;
}


/**************************************************************************
...
**************************************************************************/
struct city *game_find_city_by_name(const char *name)
{
  players_iterate(pplayer) {
    struct city *pcity = city_list_find_name(pplayer->cities, name);

    if (pcity) {
      return pcity;
    }
  } players_iterate_end;

  return NULL;
}


/**************************************************************************
  Often used function to get a city pointer from a city ID.
  City may be any city in the game.  This now always uses fast idex
  method, instead of looking through all cities of all players.
**************************************************************************/
struct city *find_city_by_id(int id)
{
  return idex_lookup_city(id);
}


/**************************************************************************
  Find unit out of all units in game: now uses fast idex method,
  instead of looking through all units of all players.
**************************************************************************/
struct unit *find_unit_by_id(int id)
{
  return idex_lookup_unit(id);
}

/**************************************************************************
  In the server call wipe_unit(), and never this function directly.
**************************************************************************/
void game_remove_unit(struct unit *punit)
{
  struct city *pcity;

  freelog(LOG_DEBUG, "game_remove_unit %d", punit->id);
  freelog(LOG_DEBUG, "removing unit %d, %s %s (%d %d) hcity %d",
	  punit->id, get_nation_name(unit_owner(punit)->nation),
	  unit_name(punit->type), punit->tile->x, punit->tile->y,
	  punit->homecity);

  pcity = player_find_city_by_id(unit_owner(punit), punit->homecity);
  if (pcity) {
    unit_list_unlink(pcity->units_supported, punit);
  }

  if (pcity) {
    freelog(LOG_DEBUG, "home city %s, %s, (%d %d)", pcity->name,
	    get_nation_name(city_owner(pcity)->nation), pcity->tile->x,
	    pcity->tile->y);
  }

  unit_list_unlink(punit->tile->units, punit);
  unit_list_unlink(unit_owner(punit)->units, punit);

  idex_unregister_unit(punit);

  if (game.callbacks.unit_deallocate) {
    (game.callbacks.unit_deallocate)(punit->id);
  }
  destroy_unit_virtual(punit);
}

/**************************************************************************
...
**************************************************************************/
void game_remove_city(struct city *pcity)
{
  freelog(LOG_DEBUG, "game_remove_city %d", pcity->id);
  freelog(LOG_DEBUG, "removing city %s, %s, (%d %d)", pcity->name,
	   get_nation_name(city_owner(pcity)->nation), pcity->tile->x,
	  pcity->tile->y);

  city_map_checked_iterate(pcity->tile, x, y, map_tile) {
    set_worker_city(pcity, x, y, C_TILE_EMPTY);
  } city_map_checked_iterate_end;
  city_list_unlink(city_owner(pcity)->cities, pcity);
  tile_set_city(pcity->tile, NULL);
  idex_unregister_city(pcity);
  remove_city_virtual(pcity);
}

/***************************************************************
...
***************************************************************/
void game_init(void)
{
  int i;

  game.info.globalwarming = 0;
  game.info.warminglevel  = 0; /* set later */
  game.info.nuclearwinter = 0;
  game.info.coolinglevel  = 0; /* set later */
  game.info.gold          = GAME_DEFAULT_GOLD;
  game.info.tech          = GAME_DEFAULT_TECHLEVEL;
  game.info.skill_level   = GAME_DEFAULT_SKILL_LEVEL;
  game.info.timeout       = GAME_DEFAULT_TIMEOUT;
  game.info.tcptimeout    = GAME_DEFAULT_TCPTIMEOUT;
  game.info.netwait       = GAME_DEFAULT_NETWAIT;
  game.info.end_year      = GAME_DEFAULT_END_YEAR;
  game.info.year          = GAME_START_YEAR;
  game.info.turn          = 0;
  game.info.min_players   = GAME_DEFAULT_MIN_PLAYERS;
  game.info.max_players   = GAME_DEFAULT_MAX_PLAYERS;
  game.info.nplayers	   = 0;
  game.info.pingtimeout   = GAME_DEFAULT_PINGTIMEOUT;
  game.info.pingtime      = GAME_DEFAULT_PINGTIME;
  game.info.diplcost      = GAME_DEFAULT_DIPLCOST;
  game.info.diplchance    = GAME_DEFAULT_DIPLCHANCE;
  game.info.freecost      = GAME_DEFAULT_FREECOST;
  game.info.conquercost   = GAME_DEFAULT_CONQUERCOST;
  game.info.dispersion    = GAME_DEFAULT_DISPERSION;
  game.info.cityfactor    = GAME_DEFAULT_CITYFACTOR;
  game.info.citymindist   = GAME_DEFAULT_CITYMINDIST;
  game.info.civilwarsize  = GAME_DEFAULT_CIVILWARSIZE;
  game.info.contactturns  = GAME_DEFAULT_CONTACTTURNS;
  game.info.rapturedelay  = GAME_DEFAULT_RAPTUREDELAY;
  game.info.celebratesize = GAME_DEFAULT_CELEBRATESIZE;
  game.info.savepalace    = GAME_DEFAULT_SAVEPALACE;
  game.info.natural_city_names = GAME_DEFAULT_NATURALCITYNAMES;
  game.info.unhappysize   = GAME_DEFAULT_UNHAPPYSIZE;
  game.info.angrycitizen  = GAME_DEFAULT_ANGRYCITIZEN;
  game.info.foodbox       = GAME_DEFAULT_FOODBOX;
  game.info.sciencebox = GAME_DEFAULT_SCIENCEBOX;
  game.info.aqueductloss  = GAME_DEFAULT_AQUEDUCTLOSS;
  game.info.killcitizen   = GAME_DEFAULT_KILLCITIZEN;
  game.info.techpenalty   = GAME_DEFAULT_TECHPENALTY;
  game.info.razechance    = GAME_DEFAULT_RAZECHANCE;
  game.info.spacerace     = GAME_DEFAULT_SPACERACE;
  game.info.turnblock     = GAME_DEFAULT_TURNBLOCK;
  game.info.fogofwar      = GAME_DEFAULT_FOGOFWAR;
  game.info.borders       = GAME_DEFAULT_BORDERS;
  game.info.happyborders  = GAME_DEFAULT_HAPPYBORDERS;
  game.info.slow_invasions= GAME_DEFAULT_SLOW_INVASIONS;
  game.info.auto_ai_toggle= GAME_DEFAULT_AUTO_AI_TOGGLE;
  game.info.notradesize   = GAME_DEFAULT_NOTRADESIZE;
  game.info.fulltradesize = GAME_DEFAULT_FULLTRADESIZE;
  game.info.barbarianrate = GAME_DEFAULT_BARBARIANRATE;
  game.info.onsetbarbarian= GAME_DEFAULT_ONSETBARBARIAN;
  game.info.nbarbarians   = 0;
  game.info.occupychance  = GAME_DEFAULT_OCCUPYCHANCE;
  game.info.autoattack    = GAME_DEFAULT_AUTOATTACK;
  game.info.revolution_length = GAME_DEFAULT_REVOLUTION_LENGTH;
  game.info.heating       = 0;
  game.info.cooling       = 0;
  game.info.watchtower_extra_vision = GAME_DEFAULT_WATCHTOWER_EXTRA_VISION;
  game.info.allowed_city_names = GAME_DEFAULT_ALLOWED_CITY_NAMES;
  game.info.save_nturns   = 10;
#ifdef HAVE_LIBZ
  game.info.save_compress_level = GAME_DEFAULT_COMPRESS_LEVEL;
#else
  game.info.save_compress_level = GAME_NO_COMPRESS_LEVEL;
#endif
  game.info.government_when_anarchy = G_MAGIC;   /* flag */

  game.info.is_new_game   = TRUE;
  game.simultaneous_phases_stored = GAME_DEFAULT_SIMULTANEOUS_PHASES;
  game.timeoutint    = GAME_DEFAULT_TIMEOUTINT;
  game.timeoutintinc = GAME_DEFAULT_TIMEOUTINTINC;
  game.timeoutinc    = GAME_DEFAULT_TIMEOUTINC;
  game.timeoutincmult= GAME_DEFAULT_TIMEOUTINCMULT;
  game.timeoutcounter= 1;
  game.timeoutaddenemymove = GAME_DEFAULT_TIMEOUTADDEMOVE; 
  game.last_ping     = 0;
  game.aifill      = GAME_DEFAULT_AIFILL;
  sz_strlcpy(game.info.start_units, GAME_DEFAULT_START_UNITS);

  game.seed = GAME_DEFAULT_SEED;
  game.scorelog    = GAME_DEFAULT_SCORELOG;
  game.fogofwar_old = game.info.fogofwar;
  sz_strlcpy(game.save_name, GAME_DEFAULT_SAVE_NAME);
  sz_strlcpy(game.rulesetdir, GAME_DEFAULT_RULESETDIR);

  game.control.num_unit_types = 0;
  game.control.num_impr_types = 0;
  game.control.num_tech_types = 0;
  game.control.nation_count = 0;
  game.control.government_count = 0;

  sz_strlcpy(game.demography, GAME_DEFAULT_DEMOGRAPHY);
  sz_strlcpy(game.allow_take, GAME_DEFAULT_ALLOW_TAKE);

  game.save_options.save_random = TRUE;
  game.save_options.save_players = TRUE;
  game.save_options.save_known = TRUE;
  game.save_options.save_starts = TRUE;
  game.save_options.save_private_map = TRUE;

  init_our_capability();    
  map_init();
  improvements_init();
  techs_init();
  unit_types_init();
  specialists_init();
  teams_init();
  idex_init();
  cm_init();
  
  for(i=0; i<MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS; i++)
    player_init(&game.players[i]);
  for (i=0; i<A_LAST; i++)      /* game.num_tech_types = 0 here */
    game.info.global_advances[i]=0;
  for (i=0; i<B_LAST; i++)      /* game.num_impr_types = 0 here */
    game.info.great_wonders[i]=0;
  game.info.player_idx = 0;
  game.player_ptr=&game.players[0];
  terrain_control.river_help_text[0] = '\0';
}

/****************************************************************************
  Initialize map-specific parts of the game structure.  Maybe these should
  be moved into the map structure?
****************************************************************************/
void game_map_init(void)
{
  /* FIXME: it's not clear where these values should be initialized.  It
   * can't be done in game_init because the map isn't created yet.  Maybe it
   * should be done in the mapgen code or in the maphand code.  It should
   * surely be called when the map is generated. */
  game.info.warminglevel = (map_num_tiles() + 499) / 500;
  game.info.coolinglevel = (map_num_tiles() + 499) / 500;
}

/***************************************************************
  Remove all initialized players. This is all player slots, 
  since we initialize them all on game initialization.
***************************************************************/
static void game_remove_all_players(void)
{
  int i;

  for (i = 0; i < MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS; i++) {
    game_remove_player(&game.players[i]);
  }

  game.info.nplayers=0;
  game.info.nbarbarians=0;
}

/***************************************************************
  Frees all memory of the game.
***************************************************************/
void game_free(void)
{
  clean_players_research();
  game_remove_all_players();
  map_free();
  idex_free();
  ruleset_data_free();
  cm_free();
}

/***************************************************************
 Frees all memory which in objects which are read from a ruleset.
***************************************************************/
void ruleset_data_free()
{
  specialists_free();
  techs_free();
  governments_free();
  nations_free();
  unit_types_free();
  improvements_free();
  city_styles_free();
  tile_types_free();
  ruleset_cache_free();
}

/***************************************************************
...
***************************************************************/
void initialize_globals(void)
{
  players_iterate(plr) {
    city_list_iterate(plr->cities, pcity) {
      built_impr_iterate(pcity, i) {
	if (is_great_wonder(i)) {
	  game.info.great_wonders[i] = pcity->id;
	} else if (is_small_wonder(i)) {
	  plr->small_wonders[i] = pcity->id;
	}
      } built_impr_iterate_end;
    } city_list_iterate_end;
  } players_iterate_end;
}

/***************************************************************
  Returns the next year in the game.
***************************************************************/
int game_next_year(int year)
{
  const int slowdown = (game.info.spacerace
			? get_world_bonus(EFT_SLOW_DOWN_TIMELINE) : 0);

  if (year == 1) /* hacked it to get rid of year 0 */
    year = 0;

    /* !McFred: 
       - want year += 1 for spaceship.
    */

  /* test game with 7 normal AI's, gen 4 map, foodbox 10, foodbase 0: 
   * Gunpowder about 0 AD
   * Railroad  about 500 AD
   * Electricity about 1000 AD
   * Refining about 1500 AD (212 active units)
   * about 1750 AD
   * about 1900 AD
   */

  /* Note the slowdown operates even if Enable_Space is not active.  See
   * README.effects for specifics. */
  if (year >= 1900 || (slowdown >= 3 && year > 0)) {
    year += 1;
  } else if (year >= 1750 || slowdown >= 2) {
    year += 2;
  } else if (year >= 1500 || slowdown >= 1) {
    year += 5;
  } else if( year >= 1000 )
    year += 10;
  else if( year >= 0 )
    year += 20;
  else if( year >= -1000 ) /* used this line for tuning (was -1250) */
    year += 25;
  else
    year += 50; 

  if (year == 0) 
    year = 1;

  return year;
}

/***************************************************************
  Advance the game year.
***************************************************************/
void game_advance_year(void)
{
  game.info.year = game_next_year(game.info.year);
  game.info.turn++;
}

/****************************************************************************
  Reset a player's data to its initial state.  No further initialization
  should be needed before reusing this player (no separate call to
  player_init is needed).
****************************************************************************/
void game_remove_player(struct player *pplayer)
{
  if (pplayer->attribute_block.data) {
    free(pplayer->attribute_block.data);
    pplayer->attribute_block.data = NULL;
  }

  /* Unlink all the lists, but don't free them (they can be used later). */
  conn_list_unlink_all(pplayer->connections);

  unit_list_iterate(pplayer->units, punit) {
    game_remove_unit(punit);
  } unit_list_iterate_end;
  assert(unit_list_size(pplayer->units) == 0);
  unit_list_unlink_all(pplayer->units);

  city_list_iterate(pplayer->cities, pcity) {
    game_remove_city(pcity);
  } city_list_iterate_end;
  assert(city_list_size(pplayer->cities) == 0);
  city_list_unlink_all(pplayer->cities);

  if (is_barbarian(pplayer)) game.info.nbarbarians--;
  player_research_init(pplayer->research);
}

/***************************************************************
...
***************************************************************/
void game_renumber_players(int plrno)
{
  int i;

  for (i = plrno; i < game.info.nplayers - 1; i++) {
    game.players[i]=game.players[i+1];
    game.players[i].player_no=i;
    conn_list_iterate(game.players[i].connections, pconn)
      pconn->player = &game.players[i];
    conn_list_iterate_end;
  }

  if(game.info.player_idx > plrno) {
    game.info.player_idx--;
    game.player_ptr = &game.players[game.info.player_idx];
  }

  game.info.nplayers--;

  /* a bit of cleanup to keep connections sane */
  game.players[game.info.nplayers].connections = conn_list_new();
  game.players[game.info.nplayers].is_connected = FALSE;
  game.players[game.info.nplayers].was_created = FALSE;
  game.players[game.info.nplayers].ai.control = FALSE;
  sz_strlcpy(game.players[game.info.nplayers].name, ANON_PLAYER_NAME);
  sz_strlcpy(game.players[game.info.nplayers].username, ANON_USER_NAME);
}

/**************************************************************************
get_player() - Return player struct pointer corresponding to player_id.
               Eg: player_id = punit->owner, or pcity->owner
**************************************************************************/
struct player *get_player(int player_id)
{
  if (player_id < 0 || player_id >= ARRAY_SIZE(game.players)) {
    assert(player_id >= 0 && player_id < ARRAY_SIZE(game.players));
    return NULL;
  }
  return &game.players[player_id];
}

bool is_valid_player_id(int player_id)
{
  return player_id >= 0 && player_id < game.info.nplayers;
}

/**************************************************************************
This function is used by is_wonder_useful to estimate if it is worthwhile
to build the great library.
**************************************************************************/
int get_num_human_and_ai_players(void)
{
  return game.info.nplayers - game.info.nbarbarians;
}

/**************************************************************************
  Return TRUE if it is this player's phase.
**************************************************************************/
bool is_player_phase(const struct player *pplayer, int phase)
{
  return game.info.simultaneous_phases || pplayer->player_no == phase;
}

/***************************************************************
  For various data, copy eg .name to .name_orig and put
  translated version in .name
  (These could be in the separate modules, but since they are
  all almost the same, and all needed together, it seems a bit
  easier to just do them all here.)
***************************************************************/
void translate_data_names(void)
{
  int i;

  tech_type_iterate(tech_id) {
    struct advance *tthis = &advances[tech_id];

    tthis->name = Q_(tthis->name_orig);
  } tech_type_iterate_end;

  unit_type_iterate(i) {
    struct unit_type *tthis = &unit_types[i];

    tthis->name = Q_(tthis->name_orig);
  } unit_type_iterate_end;

  impr_type_iterate(i) {
    struct impr_type *tthis = &improvement_types[i];

    tthis->name = Q_(tthis->name_orig);
  } impr_type_iterate_end;

  terrain_type_iterate(i) {
    struct tile_type *tthis = &tile_types[i];

    tthis->terrain_name = ((strcmp(tthis->terrain_name_orig, "") != 0)
			   ? Q_(tthis->terrain_name_orig) : "");

    tthis->special[0].name = ((strcmp(tthis->special[0].name_orig, "") != 0)
			      ? Q_(tthis->special[0].name_orig) : "");
    tthis->special[1].name = ((strcmp(tthis->special[1].name_orig, "") != 0)
			      ? Q_(tthis->special[1].name_orig) : "");
  } terrain_type_iterate_end;

  government_iterate(tthis) {
    int j;

    tthis->name = Q_(tthis->name_orig);
    for(j=0; j<tthis->num_ruler_titles; j++) {
      struct ruler_title *that = &tthis->ruler_titles[j];

      that->male_title = Q_(that->male_title_orig);
      that->female_title = Q_(that->female_title_orig);
    }
  } government_iterate_end;
  for (i = 0; i < game.control.nation_count; i++) {
    struct nation_type *tthis = get_nation_by_idx(i);

    tthis->name = Q_(tthis->name_orig);
    tthis->name_plural = Q_(tthis->name_plural_orig);
  }
  for (i = 0; i < game.control.styles_count; i++) {
    struct citystyle *tthis = &city_styles[i];

    tthis->name = Q_(tthis->name_orig);
  }

}

/****************************************************************************
  Return a prettily formatted string containing the population text.  The
  population is passed in as the number of citizens, in thousands.
****************************************************************************/
const char *population_to_text(int thousand_citizen)
{
  /* big_int_to_text can't handle negative values, and in any case we'd
   * better not have a negative population. */
  assert(thousand_citizen >= 0);
  return big_int_to_text(thousand_citizen, 3);
}

