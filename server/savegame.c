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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "capability.h"
#include "city.h"
#include "fcintl.h"
#include "game.h"
#include "idex.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "rand.h"
#include "registry.h"
#include "shared.h"
#include "support.h"
#include "unit.h"
#include "version.h"

#include "citytools.h"
#include "cityturn.h"
#include "diplhand.h"
#include "mapgen.h"
#include "maphand.h"
#include "meta.h"
#include "plrhand.h"
#include "ruleset.h"
#include "spacerace.h"
#include "srv_main.h"
#include "stdinhand.h"
#include "unittools.h"

#include "aicity.h"
#include "aidata.h"
#include "aiunit.h"

#include "savegame.h"

/* 
 * This loops over the entire map to save data. It collects all the data of
 * a line using GET_XY_CHAR and then executes the macro SECFILE_INSERT_LINE.
 * Internal variables map_x, map_y, nat_x, nat_y, and line are allocated
 * within the macro but definable by the caller of the macro.
 *
 * Parameters:
 *   line: buffer variable to hold a line of chars
 *   map_x, map_y: variables for internal map coordinates
 *   nat_x, nat_y: variables for output/native coordinates
 *   GET_XY_CHAR: macro returning the map character for each position
 *   SECFILE_INSERT_LINE: macro to output each processed line (line nat_y)
 *
 * Note: don't use this macro DIRECTLY, use 
 * SAVE_NORMAL_MAP_DATA or SAVE_PLAYER_MAP_DATA instead.
 */
#define SAVE_MAP_DATA(map_x, map_y, nat_x, nat_y, line,                     \
                      GET_XY_CHAR, SECFILE_INSERT_LINE)                     \
{                                                                           \
  char line[map.xsize + 1];                                                 \
  int nat_x, nat_y, map_x, map_y;                                           \
                                                                            \
  for (nat_y = 0; nat_y < map.ysize; nat_y++) {                             \
    for (nat_x = 0; nat_x < map.xsize; nat_x++) {                           \
      native_to_map_pos(&map_x, &map_y, nat_x, nat_y);                      \
      line[nat_x] = (GET_XY_CHAR);                                          \
      if (!my_isprint(line[nat_x] & 0x7f)) {                                \
          die("Trying to write invalid map "                                \
              "data: '%c' %d", line[nat_x], line[nat_x]);                   \
      }                                                                     \
    }                                                                       \
    line[map.xsize] = '\0';                                                 \
    (SECFILE_INSERT_LINE);                                                  \
  }                                                                         \
}

/*
 * Wrappers for SAVE_MAP_DATA.
 *
 * SAVE_NORMAL_MAP_DATA saves a standard line of map data.
 *
 * SAVE_PLAYER_MAP_DATA saves a line of map data from a playermap.
 */
#define SAVE_NORMAL_MAP_DATA(map_x, map_y, secfile, secname, GET_XY_CHAR)   \
  SAVE_MAP_DATA(map_x, map_y, _nat_x, _nat_y, _line, GET_XY_CHAR,           \
		secfile_insert_str(secfile, _line, secname, _nat_y))

#define SAVE_PLAYER_MAP_DATA(map_x, map_y, secfile, secname, plrno,         \
			     GET_XY_CHAR)                                   \
  SAVE_MAP_DATA(map_x, map_y, _nat_x, _nat_y, _line, GET_XY_CHAR,           \
		secfile_insert_str(secfile, _line, secname, plrno, _nat_y))

/*
 * This loops over the entire map to load data. It inputs a line of data
 * using the macro SECFILE_LOOKUP_LINE and then loops using the macro
 * SET_XY_CHAR to load each char into the map at (map_x, map_y).  Internal
 * variables ch, map_x, map_y, nat_x, and nat_y are allocated within the
 * macro but definable by the caller.
 *
 * Parameters:
 *   ch: a variable to hold a char (data for a single position)
 *   map_x, map_y: variables for internal map coordinates
 *   nat_x, nat_y: variables for output/native coordinates
 *   SET_XY_CHAR: macro to load the map character at each (map_x, map_y)
 *   SECFILE_LOOKUP_LINE: macro to input the nat_y line for processing
 *
 * Note: some (but not all) of the code this is replacing used to
 * skip over lines that did not exist.  This allowed for
 * backward-compatibility.  We could add another parameter that
 * specified whether it was OK to skip the data, but there's not
 * really much advantage to exiting early in this case.  Instead,
 * we let any map data type to be empty, and just print an
 * informative warning message about it.
 */
#define LOAD_MAP_DATA(ch, map_x, map_y, nat_x, nat_y,                       \
		      SECFILE_LOOKUP_LINE, SET_XY_CHAR)                     \
{                                                                           \
  int nat_x, nat_y;                                                         \
                                                                            \
  bool _warning_printed = FALSE;                                            \
  for (nat_y = 0; nat_y < map.ysize; nat_y++) {                             \
    const char *_line = (SECFILE_LOOKUP_LINE);                              \
                                                                            \
    if (!_line || strlen(_line) != map.xsize) {                             \
      if (!_warning_printed) {                                              \
        /* TRANS: Error message. */                                         \
        freelog(LOG_ERROR, _("The save file contains incomplete "           \
                "map data.  This can happen with old saved "                \
                "games, or it may indicate an invalid saved "               \
                "game file.  Proceed at your own risk."));                  \
        if(!_line) {                                                        \
          /* TRANS: Error message. */                                       \
          freelog(LOG_ERROR, _("Reason: line not found"));                  \
        } else {                                                            \
          /* TRANS: Error message. */                                       \
          freelog(LOG_ERROR, _("Reason: line too short "                    \
                  "(expected %d got %lu"), map.xsize,                       \
                  (unsigned long) strlen(_line));                           \
        }                                                                   \
        /* Do not translate.. */                                            \
        freelog(LOG_ERROR, "secfile_lookup_line='%s'",                      \
                #SECFILE_LOOKUP_LINE);                                      \
        _warning_printed = TRUE;                                            \
      }                                                                     \
      continue;                                                             \
    }                                                                       \
    for (nat_x = 0; nat_x < map.xsize; nat_x++) {                           \
      int map_x, map_y;                                                     \
      const char ch = _line[nat_x];                                         \
                                                                            \
      native_to_map_pos(&map_x, &map_y, nat_x, nat_y);                      \
      (SET_XY_CHAR);                                                        \
    }                                                                       \
  }                                                                         \
}

/* The following should be removed when compatibility with
   pre-1.13.0 savegames is broken: startoptions, spacerace2
   and rulesets */
#define SAVEFILE_OPTIONS "startoptions spacerace2 rulesets" \
" diplchance_percent worklists2 map_editor known32fix turn " \
"attributes watchtower rulesetdir client_worklists orders " \
"startunits turn_last_built improvement_order"

static const char hex_chars[] = "0123456789abcdef";
static const char terrain_chars[] = "adfghjm prstu";

/***************************************************************
This returns an ascii hex value of the given half-byte of the binary
integer. See ascii_hex2bin().
  example: bin2ascii_hex(0xa00, 2) == 'a'
***************************************************************/
#define bin2ascii_hex(value, halfbyte_wanted) \
  hex_chars[((value) >> ((halfbyte_wanted) * 4)) & 0xf]

/***************************************************************
This returns a binary integer value of the ascii hex char, offset by
the given number of half-bytes. See bin2ascii_hex().
  example: ascii_hex2bin('a', 2) == 0xa00
This is only used in loading games, and it requires some error
checking so it's done as a function.
***************************************************************/
static int ascii_hex2bin(char ch, int halfbyte)
{
  char *pch;

  if (ch == ' ') {
    /* 
     * Sane value. It is unknow if there are savegames out there which
     * need this fix. Savegame.c doesn't write such savegames
     * (anymore) since the inclusion into CVS (2000-08-25).
     */
    return 0;
  }
  
  pch = strchr(hex_chars, ch);

  if (!pch || ch == '\0') {
    die("Unknown hex value: '%c' %d", ch, ch);
  }
  return (pch - hex_chars) << (halfbyte * 4);
}

/***************************************************************
Dereferences the terrain character.  See terrain_chars[].
  example: char2terrain('a') == 0
***************************************************************/
static int char2terrain(char ch)
{
  char *pch = strchr(terrain_chars, ch);

  if (!pch || ch == '\0') {
    die("Unknown terrain type: '%c' %d", ch, ch);
  }
  return pch - terrain_chars;
}

/***************************************************************
Quote the memory block denoted by data and length so it consists only
of " a-f0-9:". The returned string has to be freed by the caller using
free().
***************************************************************/
static char *quote_block(const void *const data, int length)
{
  char *buffer = fc_malloc(length * 3 + 10);
  size_t offset;
  int i;

  sprintf(buffer, "%d:", length);
  offset = strlen(buffer);

  for (i = 0; i < length; i++) {
    sprintf(buffer + offset, "%02x ", ((unsigned char *) data)[i]);
    offset += 3;
  }
  return buffer;
}

/***************************************************************
Unquote a string. The unquoted data is written into dest. If the
unqoted data will be largern than dest_length the function aborts. It
returns the actual length of the unquoted block.
***************************************************************/
static int unquote_block(const char *const quoted_, void *dest,
			 int dest_length)
{
  int i, length, parsed, tmp;
  char *endptr;
  const char *quoted = quoted_;

  parsed = sscanf(quoted, "%d", &length);
  assert(parsed == 1);

  assert(length <= dest_length);
  quoted = strchr(quoted, ':');
  assert(quoted != NULL);
  quoted++;

  for (i = 0; i < length; i++) {
    tmp = strtol(quoted, &endptr, 16);
    assert((endptr - quoted) == 2);
    assert(*endptr == ' ');
    assert((tmp & 0xff) == tmp);
    ((unsigned char *) dest)[i] = tmp;
    quoted += 3;
  }
  return length;
}

/***************************************************************
load starting positions for the players from a savegame file
Now we don't know how many start positions there are nor how many
should be because rulesets are loaded later. So try to load as
many as they are; there should be at least enough for every
player.  This could be changed/improved in future.
***************************************************************/
static void map_startpos_load(struct section_file *file)
{
  int i;

  for (i = 0; secfile_lookup_int_default(file, -1, "map.r%dsx", i) != -1;
       i++) {
    /* Nothing. */
  }

  map.num_start_positions = i;
  if (map.num_start_positions == 0) {
    /* This scenario has no preset start positions. */
    return;
  }

  map.start_positions = fc_realloc(map.start_positions,
				   map.num_start_positions
				   * sizeof(*map.start_positions));
  for (i = 0; i < map.num_start_positions; i++) {
    int nat_x, nat_y;
    char *nation = secfile_lookup_str_default(file, NULL, "map.r%dsnation",
					      i);

    nat_x = secfile_lookup_int(file, "map.r%dsx", i);
    nat_y = secfile_lookup_int(file, "map.r%dsy", i);

    native_to_map_pos(&map.start_positions[i].x, &map.start_positions[i].y,
		      nat_x, nat_y);

    if (nation) {
      /* This will fall back to NO_NATION_SELECTED if the string doesn't
       * match any nation. */
      map.start_positions[i].nation = find_nation_by_name_orig(nation);
    } else {
      /* Old-style nation ordering is useless to us because the nations
       * have been reordered.  Just ignore it and order the nations
       * randomly. */
      map.start_positions[i].nation = NO_NATION_SELECTED;
    }
  }

  if (map.num_start_positions < game.max_players) {
    freelog(LOG_VERBOSE,
	    _("Number of starts (%d) are lower than max_players (%d),"
	      " lowering max_players."),
 	    map.num_start_positions, game.max_players);
    game.max_players = map.num_start_positions;
  }
}

/***************************************************************
load the tile map from a savegame file
***************************************************************/
static void map_tiles_load(struct section_file *file)
{
  map.topology_id = secfile_lookup_int_default(file, MAP_ORIGINAL_TOPO,
					       "map.topology_id");

  /* In some cases we read these before, but not always, and
   * its safe to read them again:
   */
  map.xsize=secfile_lookup_int(file, "map.width");
  map.ysize=secfile_lookup_int(file, "map.height");

  /* With a FALSE parameter [xy]size are not changed by this call. */
  map_init_topology(FALSE);

  map_allocate();

  /* get the terrain type */
  LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		secfile_lookup_str(file, "map.t%03d", nat_y),
		map_get_tile(x, y)->terrain = char2terrain(ch));

  assign_continent_numbers();

  whole_map_iterate(mx, my) {
    struct tile *ptile = map_get_tile(mx, my);
    int nx, ny;

    map_to_native_pos(&nx, &ny, mx, my);

    ptile->spec_sprite = secfile_lookup_str_default(file, NULL,
				"map.spec_sprite_%d_%d", nx, ny);
    if (ptile->spec_sprite) {
      ptile->spec_sprite = mystrdup(ptile->spec_sprite);
    }
  } whole_map_iterate_end;
}

/***************************************************************
load the rivers overlay map from a savegame file

(This does not need to be called from map_load(), because
 map_load() loads the rivers overlay along with the rest of
 the specials.  Call this only if you've already called
 map_tiles_load(), and want to overlay rivers defined as
 specials, rather than as terrain types.)
***************************************************************/
static void map_rivers_overlay_load(struct section_file *file)
{
  /* Get the bits of the special flags which contain the river special
     and extract the rivers overlay from them. */
  LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		secfile_lookup_str_default(file, NULL, "map.n%03d", nat_y),
		map_get_tile(x, y)->special |=
		(ascii_hex2bin(ch, 2) & S_RIVER));
  map.have_rivers_overlay = TRUE;
}

/***************************************************************
load a complete map from a savegame file
***************************************************************/
static void map_load(struct section_file *file)
{
  char *savefile_options = secfile_lookup_str(file, "savefile.options");

  /* map_init();
   * This is already called in game_init(), and calling it
   * here stomps on map.huts etc.  --dwp
   */

  map_tiles_load(file);
  if (secfile_lookup_bool_default(file, TRUE, "game.save_starts")) {
    map_startpos_load(file);
  } else {
    map.num_start_positions = 0;
  }

  /* get 4-bit segments of 16-bit "special" field. */
  LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		secfile_lookup_str(file, "map.l%03d", nat_y),
		map_get_tile(x, y)->special = ascii_hex2bin(ch, 0));
  LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		secfile_lookup_str(file, "map.u%03d", nat_y),
		map_get_tile(x, y)->special |= ascii_hex2bin(ch, 1));
  LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		secfile_lookup_str_default(file, NULL, "map.n%03d", nat_y),
		map_get_tile(x, y)->special |= ascii_hex2bin(ch, 2));
  LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		secfile_lookup_str_default(file, NULL, "map.f%03d", nat_y),
		map_get_tile(x, y)->special |= ascii_hex2bin(ch, 3));

  if (secfile_lookup_bool_default(file, TRUE, "game.save_known")) {

    /* get 4-bit segments of the first half of the 32-bit "known" field */
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "map.a%03d", nat_y),
		  map_get_tile(x, y)->known = ascii_hex2bin(ch, 0));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "map.b%03d", nat_y),
		  map_get_tile(x, y)->known |= ascii_hex2bin(ch, 1));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "map.c%03d", nat_y),
		  map_get_tile(x, y)->known |= ascii_hex2bin(ch, 2));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "map.d%03d", nat_y),
		  map_get_tile(x, y)->known |= ascii_hex2bin(ch, 3));

    if (has_capability("known32fix", savefile_options)) {
      /* get 4-bit segments of the second half of the 32-bit "known" field */
      LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		    secfile_lookup_str(file, "map.e%03d", nat_y),
		    map_get_tile(x, y)->known |= ascii_hex2bin(ch, 4));
      LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		    secfile_lookup_str(file, "map.g%03d", nat_y),
		    map_get_tile(x, y)->known |= ascii_hex2bin(ch, 5));
      LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		    secfile_lookup_str(file, "map.h%03d", nat_y),
		    map_get_tile(x, y)->known |= ascii_hex2bin(ch, 6));
      LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		    secfile_lookup_str(file, "map.i%03d", nat_y),
		    map_get_tile(x, y)->known |= ascii_hex2bin(ch, 7));
    }
  }


  map.have_specials = TRUE;
}

/***************************************************************
...
***************************************************************/
static void map_save(struct section_file *file)
{
  int i;

  /* map.xsize and map.ysize (saved as map.width and map.height)
   * are now always saved in game_save()
   */

  /* Old freecivs expect map.is_earth to be present in the savegame. */
  secfile_insert_bool(file, FALSE, "map.is_earth");

  secfile_insert_bool(file, game.save_options.save_starts, "game.save_starts");
  if (game.save_options.save_starts) {
    for (i=0; i<map.num_start_positions; i++) {
      do_in_native_pos(nat_x, nat_y,
		       map.start_positions[i].x, map.start_positions[i].y) {
	secfile_insert_int(file, nat_x, "map.r%dsx", i);
	secfile_insert_int(file, nat_y, "map.r%dsy", i);
      } do_in_native_pos_end;

      if (map.start_positions[i].nation != NO_NATION_SELECTED) {
	const char *nation = get_nation_name(map.start_positions[i].nation);

	secfile_insert_str(file, nation, "map.r%dsnation", i);
      }
    }
  }
    
  /* put the terrain type */
  SAVE_NORMAL_MAP_DATA(x, y, file, "map.t%03d",
		       terrain_chars[map_get_tile(x, y)->terrain]);

  if (!map.have_specials) {
    if (map.have_rivers_overlay) {
      /* 
       * Save the rivers overlay map; this is a special case to allow
       * re-saving scenarios which have rivers overlay data.  This only
       * applies if don't have rest of specials.
       */

      /* bits 8-11 of special flags field */
      SAVE_NORMAL_MAP_DATA(x, y, file, "map.n%03d",
			   bin2ascii_hex(map_get_tile(x, y)->special, 2));
    }
    return;
  }

  /* put 4-bit segments of 12-bit "special flags" field */
  SAVE_NORMAL_MAP_DATA(x, y, file, "map.l%03d",
		       bin2ascii_hex(map_get_tile(x, y)->special, 0));
  SAVE_NORMAL_MAP_DATA(x, y, file, "map.u%03d",
		       bin2ascii_hex(map_get_tile(x, y)->special, 1));
  SAVE_NORMAL_MAP_DATA(x, y, file, "map.n%03d",
		       bin2ascii_hex(map_get_tile(x, y)->special, 2));

  secfile_insert_bool(file, game.save_options.save_known, "game.save_known");
  if (game.save_options.save_known) {
    /* put the top 4 bits (bits 12-15) of special flags */
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.f%03d",
			 bin2ascii_hex(map_get_tile(x, y)->special, 3));

    /* put 4-bit segments of the 32-bit "known" field */
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.a%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 0));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.b%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 1));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.c%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 2));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.d%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 3));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.e%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 4));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.g%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 5));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.h%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 6));
    SAVE_NORMAL_MAP_DATA(x, y, file, "map.i%03d",
			 bin2ascii_hex(map_get_tile(x, y)->known, 7));
  }

  whole_map_iterate(mx, my) {
    struct tile *ptile = map_get_tile(mx, my);

    if (ptile->spec_sprite) {
      int nx, ny;

      map_to_native_pos(&nx, &ny, mx, my);
      secfile_insert_str(file, ptile->spec_sprite, 
			 "map.spec_sprite_%d_%d", nx, ny);
    }
  } whole_map_iterate_end;
}

/*
 * Previously (with 1.14.1 and earlier) units had their type saved by ID.
 * This meant any time a unit was added (unless it was added at the end)
 * savegame compatability would be broken.  Sometime after 1.14.1 this
 * method was changed so the type is saved by name.  However to preserve
 * backwards compatability we have here a list of unit names from before
 * the change was made.  When loading an old savegame (one that doesn't
 * have the type string) we need to lookup the type into this array
 * to get the "proper" type string.  And when saving a new savegame we
 * insert the "old" type index into the array so that old servers can
 * load the savegame.
 *
 * Note that this list includes the AWACS, which was not in 1.14.1.
 */

/* old (~1.14.1) unit order in default/civ2/history ruleset */
static const char* old_default_unit_types[] = {
  "Settlers",	"Engineers",	"Warriors",	"Phalanx",
  "Archers",	"Legion",	"Pikemen",	"Musketeers",
  "Fanatics",	"Partisan",	"Alpine Troops","Riflemen",
  "Marines",	"Paratroopers",	"Mech. Inf.",	"Horsemen",
  "Chariot",	"Elephants",	"Crusaders",	"Knights",
  "Dragoons",	"Cavalry",	"Armor",	"Catapult",
  "Cannon",	"Artillery",	"Howitzer",	"Fighter",
  "Bomber",	"Helicopter",	"Stealth Fighter", "Stealth Bomber",
  "Trireme",	"Caravel",	"Galleon",	"Frigate",
  "Ironclad",	"Destroyer",	"Cruiser",	"AEGIS Cruiser",
  "Battleship",	"Submarine",	"Carrier",	"Transport",
  "Cruise Missile", "Nuclear",	"Diplomat",	"Spy",
  "Caravan",	"Freight",	"Explorer",	"Barbarian Leader",
  "AWACS"
};

/* old (~1.14.1) unit order in civ1 ruleset */
static const char* old_civ1_unit_types[] = {
  "Settlers",	"Engineers",	"Militia",	"Phalanx",
  "Archers",	"Legion",	"Pikemen",	"Musketeers",
  "Fanatics",	"Partisan",	"Alpine Troops","Riflemen",
  "Marines",	"Paratroopers",	"Mech. Inf.",	"Cavalry",
  "Chariot",	"Elephants",	"Crusaders",	"Knights",
  "Dragoons",	"Civ2-Cavalry",	"Armor",	"Catapult",
  "Cannon",	"Civ2-Artillery","Artillery",	"Fighter",
  "Bomber",	"Helicopter",	"Stealth Fighter", "Stealth Bomber",
  "Trireme",	"Sail",		"Galleon",	"Frigate",
  "Ironclad",	"Destroyer",	"Cruiser",	"AEGIS Cruiser",
  "Battleship",	"Submarine",	"Carrier",	"Transport",
  "Cruise Missile", "Nuclear",	"Diplomat",	"Spy",
  "Caravan",	"Freight",	"Explorer",	"Barbarian Leader"
};

/* old (1.14.1) improvement order in default ruleset */
const char* old_impr_types[] =
{
  "Airport",		"Aqueduct",		"Bank",
  "Barracks",		"Barracks II",		"Barracks III",
  "Cathedral",		"City Walls",		"Coastal Defense",
  "Colosseum",		"Courthouse",		"Factory",
  "Granary",		"Harbour",		"Hydro Plant",
  "Library",		"Marketplace",		"Mass Transit",
  "Mfg. Plant",		"Nuclear Plant",	"Offshore Platform",
  "Palace",		"Police Station",	"Port Facility",
  "Power Plant",	"Recycling Center",	"Research Lab",
  "SAM Battery",	"SDI Defense",		"Sewer System",
  "Solar Plant",	"Space Component",	"Space Module",
  "Space Structural",	"Stock Exchange",	"Super Highways",
  "Supermarket",	"Temple",		"University",
  "Apollo Program",	"A.Smith's Trading Co.","Colossus",
  "Copernicus' Observatory", "Cure For Cancer",	"Darwin's Voyage",
  "Eiffel Tower",	"Great Library",	"Great Wall",
  "Hanging Gardens",	"Hoover Dam",		"Isaac Newton's College",
  "J.S. Bach's Cathedral","King Richard's Crusade", "Leonardo's Workshop",
  "Lighthouse",		"Magellan's Expedition","Manhattan Project",
  "Marco Polo's Embassy","Michelangelo's Chapel","Oracle",
  "Pyramids",		"SETI Program",		"Shakespeare's Theatre",
  "Statue of Liberty",	"Sun Tzu's War Academy","United Nations",
  "Women's Suffrage",	"Coinage"
};

/****************************************************************************
  Nowadays unit types are saved by name, but old servers need the
  unit_type_id.  This function tries to find the correct _old_ id for the
  unit's type.  It is used when the unit is saved.
****************************************************************************/
static int old_unit_type_id(Unit_Type_id type)
{
  const char** types;
  int num_types, i;

  if (strcmp(game.rulesetdir, "civ1") == 0) {
    types = old_civ1_unit_types;
    num_types = ARRAY_SIZE(old_civ1_unit_types);
  } else {
    types = old_default_unit_types;
    num_types = ARRAY_SIZE(old_default_unit_types);
  }

  for (i = 0; i < num_types; i++) {
    if (mystrcasecmp(unit_name_orig(type), types[i]) == 0) {
      return i;
    }
  }

  /* It's a new unit. Savegame cannot be backward compatible so we can
   * return anything */
  return type;
}

/****************************************************************************
  Convert an old-style unit type id into a unit type name.
****************************************************************************/
static const char* old_unit_type_name(int id)
{
  /* before 1.15.0 unit types used to be saved by id */
  if (id < 0) {
    freelog(LOG_ERROR, _("Wrong unit type id value (%d)"), id);
    exit(EXIT_FAILURE);
  }
  /* Different rulesets had different unit names. */
  if (strcmp(game.rulesetdir, "civ1") == 0) {
    if (id >= ARRAY_SIZE(old_civ1_unit_types)) {
      freelog(LOG_ERROR, _("Wrong unit type id value (%d)"), id);
      exit(EXIT_FAILURE);
    }
    return old_civ1_unit_types[id];
  } else {
    if (id >= ARRAY_SIZE(old_default_unit_types)) {
      freelog(LOG_ERROR, _("Wrong unit type id value (%d)"), id);
      exit(EXIT_FAILURE);
    }
    return old_default_unit_types[id];
  }
}

/****************************************************************************
  Nowadays improvement types are saved by name, but old servers need the
  Impr_type_id.  This function tries to find the correct _old_ id for the
  improvements's type.  It is used when the improvement is saved.
****************************************************************************/
static int old_impr_type_id(Impr_Type_id type)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(old_impr_types); i++) {
    if (mystrcasecmp(unit_name_orig(type), old_impr_types[i]) == 0) {
      return i;
    }
  }

  /* It's a new improvement. Savegame cannot be backward compatible so we can
   * return anything */
  return type;
}

/***************************************************************
  Convert old-style improvement type id into improvement type name
***************************************************************/
static const char* old_impr_type_name(int id)
{
  /* before 1.15.0 improvement types used to be saved by id */
  if (id < 0 || id >= ARRAY_SIZE(old_impr_types)) {
    freelog(LOG_ERROR, _("Wrong improvement type id value (%d)"), id);
    exit(EXIT_FAILURE);
  }
  return old_impr_types[id];
}

/****************************************************************************
  Initialize the old-style improvement bitvector so that all improvements
  are marked as not present.
****************************************************************************/
static void init_old_improvement_bitvector(char* bitvector)
{
  int i;

  for (i = 0; i < ARRAY_SIZE(old_impr_types); i++) {
    bitvector[i] = '0';
  }
  bitvector[ARRAY_SIZE(old_impr_types)] = '\0';
}

/****************************************************************************
  Insert improvement into old-style bitvector

  Improvement lists in cities and destroyed_wonders are saved as a
  bitvector in a string array.  New bitvectors do not depend on ruleset
  order. However, we want to create savegames which can be read by 
  1.14.x and earlier servers.  This function adds an improvement into the
  bitvector string according to the 1.14.1 improvement ordering.
****************************************************************************/
static void add_improvement_into_old_bitvector(char* bitvector,
                                               Impr_Type_id id)
{
  int old_id;

  old_id = old_impr_type_id(id);
  if (old_id < 0 || old_id >= ARRAY_SIZE(old_impr_types)) {
    return;
  }
  bitvector[old_id] = '1';
}

/***************************************************************
Load the worklist elements specified by path, given the arguments
plrno and wlinx, into the worklist pointed to by pwl.
***************************************************************/
static void worklist_load(struct section_file *file,
			  const char *path, int plrno, int wlinx,
			  struct worklist *pwl)
{
  char efpath[64];
  char idpath[64];
  char namepath[64];
  int i;
  bool end = FALSE;
  const char* name;

  sz_strlcpy(efpath, path);
  sz_strlcat(efpath, ".wlef%d");
  sz_strlcpy(idpath, path);
  sz_strlcat(idpath, ".wlid%d");
  sz_strlcpy(namepath, path);
  sz_strlcat(namepath, ".wlname%d");

  for (i = 0; i < MAX_LEN_WORKLIST; i++) {
    if (end) {
      pwl->wlefs[i] = WEF_END;
      pwl->wlids[i] = 0;
      (void) section_file_lookup(file, efpath, plrno, wlinx, i);
      (void) section_file_lookup(file, idpath, plrno, wlinx, i);
    } else {
      pwl->wlefs[i] =
	secfile_lookup_int_default(file, WEF_END, efpath, plrno, wlinx, i);
      name = secfile_lookup_str_default(file, NULL, namepath, plrno, wlinx, i);

      if (pwl->wlefs[i] == WEF_UNIT) {
	Unit_Type_id type;

	if (!name) {
	    /* before 1.15.0 unit types used to be saved by id */
	    name = old_unit_type_name(secfile_lookup_int(file, idpath,
							 plrno, wlinx, i));
	}

	type = find_unit_type_by_name_orig(name);
	if (type == U_LAST) {
	  freelog(LOG_ERROR, _("Unknown unit type '%s' in worklist"),
		  name);
	  exit(EXIT_FAILURE);
	}
	pwl->wlids[i] = type;
      } else if (pwl->wlefs[i] == WEF_IMPR) {
	Impr_Type_id type;

	if (!name) {
	  name = old_impr_type_name(secfile_lookup_int(file, idpath,
						       plrno, wlinx, i));
	}

	type = find_improvement_by_name_orig(name);
	if (type == B_LAST) {
	  freelog(LOG_ERROR, _("Unknown improvement type '%s' in worklist"),
	           name);
	}
	pwl->wlids[i] = type;
      }

      if ((pwl->wlefs[i] <= WEF_END) || (pwl->wlefs[i] >= WEF_LAST) ||
	  ((pwl->wlefs[i] == WEF_UNIT) && !unit_type_exists(pwl->wlids[i])) ||
	  ((pwl->wlefs[i] == WEF_IMPR) && !improvement_exists(pwl->wlids[i]))) {
	pwl->wlefs[i] = WEF_END;
	pwl->wlids[i] = 0;
	end = TRUE;
      }
    }
  }
}

/***************************************************************
Load the worklist elements specified by path, given the arguments
plrno and wlinx, into the worklist pointed to by pwl.
Assumes original save-file format.  Use for backward compatibility.
***************************************************************/
static void worklist_load_old(struct section_file *file,
			      const char *path, int plrno, int wlinx,
			      struct worklist *pwl)
{
  int i, id;
  bool end = FALSE;
  const char* name;

  for (i = 0; i < MAX_LEN_WORKLIST; i++) {
    if (end) {
      pwl->wlefs[i] = WEF_END;
      pwl->wlids[i] = 0;
      (void) section_file_lookup(file, path, plrno, wlinx, i);
    } else {
      id = secfile_lookup_int_default(file, -1, path, plrno, wlinx, i);

      if ((id < 0) || (id >= 284)) { /* 284 was flag value for end of list */
	pwl->wlefs[i] = WEF_END;
	pwl->wlids[i] = 0;
	end = TRUE;
      } else if (id >= 68) {		/* 68 was offset to unit ids */
	name = old_unit_type_name(id-68);
	pwl->wlefs[i] = WEF_UNIT;
	pwl->wlids[i] = find_unit_type_by_name_orig(name);
	end = !unit_type_exists(pwl->wlids[i]);
      } else {				/* must be an improvement id */
	name = old_impr_type_name(id);
	pwl->wlefs[i] = WEF_IMPR;
	pwl->wlids[i] = find_improvement_by_name_orig(name);
	end = !improvement_exists(pwl->wlids[i]);
      }
    }
  }

}

/****************************************************************************
  Loads the units for the given player.
****************************************************************************/
static void load_player_units(struct player *plr, int plrno,
			      struct section_file *file)
{
  int nunits, i, j;
  enum unit_activity activity;
  char *savefile_options = secfile_lookup_str(file, "savefile.options");

  unit_list_init(&plr->units);
  nunits = secfile_lookup_int(file, "player%d.nunits", plrno);
  if (!plr->is_alive && nunits > 0) {
    nunits = 0; /* Some old savegames may be buggy. */
  }
  
  for (i = 0; i < nunits; i++) {
    struct unit *punit;
    struct city *pcity;
    int nat_x, nat_y;
    const char* type_name;
    Unit_Type_id type;
    
    type_name = secfile_lookup_str_default(file, NULL, 
                                           "player%d.u%d.type_by_name",
					   plrno, i);
    if (!type_name) {
      /* before 1.15.0 unit types used to be saved by id. */
      int t = secfile_lookup_int(file, "player%d.u%d.type",
                             plrno, i);
      if (t < 0) {
        freelog(LOG_ERROR, _("Wrong player%d.u%d.type value (%d)"),
	        plrno, i, t);
	exit(EXIT_FAILURE);
      }
      type_name = old_unit_type_name(t);

    }
    
    type = find_unit_type_by_name_orig(type_name);
    if (type == U_LAST) {
      freelog(LOG_ERROR, _("Unknown unit type '%s' in player%d section"),
              type_name, plrno);
      exit(EXIT_FAILURE);
    }
    
    punit = create_unit_virtual(plr, NULL, type,
	secfile_lookup_int(file, "player%d.u%d.veteran", plrno, i));
    punit->id = secfile_lookup_int(file, "player%d.u%d.id", plrno, i);
    alloc_id(punit->id);
    idex_register_unit(punit);

    nat_x = secfile_lookup_int(file, "player%d.u%d.x", plrno, i);
    nat_y = secfile_lookup_int(file, "player%d.u%d.y", plrno, i);
    native_to_map_pos(&punit->x, &punit->y, nat_x, nat_y);

    punit->foul
      = secfile_lookup_bool_default(file, FALSE, "player%d.u%d.foul",
				    plrno, i);
    punit->homecity = secfile_lookup_int(file, "player%d.u%d.homecity",
					 plrno, i);

    if ((pcity = find_city_by_id(punit->homecity))) {
      unit_list_insert(&pcity->units_supported, punit);
    }
    
    punit->moves_left
      = secfile_lookup_int(file, "player%d.u%d.moves", plrno, i);
    punit->fuel = secfile_lookup_int(file, "player%d.u%d.fuel", plrno, i);
    activity = secfile_lookup_int(file, "player%d.u%d.activity", plrno, i);
    if (activity == ACTIVITY_PATROL_UNUSED) {
      /* Previously ACTIVITY_PATROL and ACTIVITY_GOTO were used for
       * client-side goto.  Now client-side goto is handled by setting
       * a special flag, and units with orders generally have ACTIVITY_IDLE.
       * Old orders are lost.  Old client-side goto units will still have
       * ACTIVITY_GOTO and will goto the correct position via server goto.
       * Old client-side patrol units lose their patrol routes and are put
       * into idle mode. */
      activity = ACTIVITY_IDLE;
    }
    set_unit_activity(punit, activity);

    /* need to do this to assign/deassign settlers correctly -- Syela
     *
     * was punit->activity=secfile_lookup_int(file,
     *                             "player%d.u%d.activity",plrno, i); */
    punit->activity_count = secfile_lookup_int(file, 
					       "player%d.u%d.activity_count",
					       plrno, i);
    punit->activity_target
      = secfile_lookup_int_default(file, (int) S_NO_SPECIAL,
				   "player%d.u%d.activity_target", plrno, i);

    punit->connecting
      = secfile_lookup_bool_default(file, FALSE,
				    "player%d.u%d.connecting", plrno, i);
    punit->done_moving = secfile_lookup_bool_default(file,
	(punit->moves_left == 0), "player%d.u%d.done_moving", plrno, i);

    /* Load the goto information.  Older savegames will not have the
     * "go" field, so we just load the goto destination by default. */
    if (secfile_lookup_bool_default(file, TRUE,
				    "player%d.u%d.go", plrno, i)) {
      int nat_x = secfile_lookup_int(file, "player%d.u%d.goto_x", plrno, i);
      int nat_y = secfile_lookup_int(file, "player%d.u%d.goto_y", plrno, i);
      int map_x, map_y;

      native_to_map_pos(&map_x, &map_y, nat_x, nat_y);
      set_goto_dest(punit, map_x, map_y);
    } else {
      clear_goto_dest(punit);
    }

    punit->ai.control
      = secfile_lookup_bool(file, "player%d.u%d.ai", plrno, i);
    punit->hp = secfile_lookup_int(file, "player%d.u%d.hp", plrno, i);
    
    punit->ord_map
      = secfile_lookup_int_default(file, 0,
				   "player%d.u%d.ord_map", plrno, i);
    punit->ord_city
      = secfile_lookup_int_default(file, 0,
				   "player%d.u%d.ord_city", plrno, i);
    punit->moved
      = secfile_lookup_bool_default(file, FALSE,
				    "player%d.u%d.moved", plrno, i);
    punit->paradropped
      = secfile_lookup_bool_default(file, FALSE,
				    "player%d.u%d.paradropped", plrno, i);
    punit->transported_by
      = secfile_lookup_int_default(file, -1, "player%d.u%d.transported_by",
				   plrno, i);
    /* Initialize upkeep values: these are hopefully initialized
       elsewhere before use (specifically, in city_support(); but
       fixme: check whether always correctly initialized?).
       Below is mainly for units which don't have homecity --
       otherwise these don't get initialized (and AI calculations
       etc may use junk values).
    */

    /* load the unit orders */
    if (has_capability("orders", savefile_options)) {
      int len = secfile_lookup_int_default(file, 0,
			"player%d.u%d.orders_length", plrno, i);
      if (len > 0) {
	char *orders_buf, *dir_buf;

	punit->orders.list = fc_malloc(len * sizeof(*(punit->orders.list)));
	punit->orders.length = len;
	punit->orders.index = secfile_lookup_int_default(file, 0,
			"player%d.u%d.orders_index", plrno, i);
	punit->orders.repeat = secfile_lookup_bool_default(file, FALSE,
			"player%d.u%d.orders_repeat", plrno, i);
	punit->orders.vigilant = secfile_lookup_bool_default(file, FALSE,
			"player%d.u%d.orders_vigilant", plrno, i);

	orders_buf = secfile_lookup_str_default(file, "",
			"player%d.u%d.orders_list", plrno, i);
	dir_buf = secfile_lookup_str_default(file, "",
			"player%d.u%d.dir_list", plrno, i);
	for (j = 0; j < len; j++) {
	  if (orders_buf[j] == '\0' || dir_buf == '\0') {
	    freelog(LOG_ERROR, _("Savegame error: invalid unit orders."));
	    free_unit_orders(punit);
	    break;
	  }
	  punit->orders.list[j].order = orders_buf[j] - 'a';
	  punit->orders.list[j].dir = dir_buf[j] - 'a';
	}
	punit->has_orders = TRUE;
      } else {
	punit->has_orders = FALSE;
	punit->orders.list = NULL;
      }
    } else {
      /* Old-style goto routes get discarded. */
    }

    {
      /* Sanity: set the map to known for all tiles within the vision
       * range.
       *
       * FIXME: shouldn't this take into account modifiers like 
       * watchtowers? */
      int range = unit_type(punit)->vision_range;

      square_iterate(punit->x, punit->y, range, x1, y1) {
	map_set_known(x1, y1, plr);
      } square_iterate_end;
    }

    /* allocate the unit's contribution to fog of war */
    if (unit_profits_of_watchtower(punit)
	&& map_has_special(punit->x, punit->y, S_FORTRESS)) {
      unfog_area(unit_owner(punit), punit->x, punit->y,
		 get_watchtower_vision(punit));
    } else {
      unfog_area(unit_owner(punit), punit->x, punit->y,
		 unit_type(punit)->vision_range);
    }

    unit_list_insert_back(&plr->units, punit);

    unit_list_insert(&map_get_tile(punit->x, punit->y)->units, punit);
  }
}

/****************************************************************************
  Load all information about player "plrno" into the structure pointed to
  by "plr".
****************************************************************************/
static void player_load(struct player *plr, int plrno,
			struct section_file *file,
			char** improvement_order,
			int improvement_order_size)
{
  int i, j, x, y, ncities, c_s;
  const char *p;
  char *savefile_options = secfile_lookup_str(file, "savefile.options");
  struct ai_data *ai;

  server_player_init(plr, TRUE);
  ai_data_init(plr);
  ai = ai_data_get(plr);

  plr->ai.barbarian_type = secfile_lookup_int_default(file, 0, "player%d.ai.is_barbarian",
                                                    plrno);
  if (is_barbarian(plr)) game.nbarbarians++;

  sz_strlcpy(plr->name, secfile_lookup_str(file, "player%d.name", plrno));
  sz_strlcpy(plr->username,
	     secfile_lookup_str_default(file, "", "player%d.username", plrno));

  /* 1.15 and later versions store nations by name.  Try that first. */
  p = secfile_lookup_str_default(file, NULL, "player%d.nation", plrno);
  if (!p) {
    /*
     * Otherwise read as a pre-1.15 savefile with numeric nation indexes.
     * This random-looking order is from the old nations/ruleset file.
     * Use it to convert old-style nation indices to name strings.
     * The idea is not to be dependent on the order in which nations 
     * get read into the registry.
     */
    const char *name_order[] = {
      "roman", "babylonian", "german", "egyptian", "american", "greek",
      "indian", "russian", "zulu", "french", "aztec", "chinese", "english",
      "mongol", "turk", "spanish", "persian", "arab", "carthaginian", "inca",
      "viking", "polish", "hungarian", "danish", "dutch", "swedish",
      "japanese", "portuguese", "finnish", "sioux", "czech", "australian",
      "welsh", "korean", "scottish", "israeli", "argentine", "canadian",
      "ukrainian", "lithuanian", "kenyan", "dunedain", "vietnamese", "thai",
      "mordor", "bavarian", "brazilian", "irish", "cornish", "italian",
      "filipino", "estonian", "latvian", "boer", "silesian", "singaporean",
      "chilean", "catalan", "croatian", "slovenian", "serbian", "barbarian",
    };
    int index = secfile_lookup_int(file, "player%d.race", plrno);

    if (index >= 0 && index < ARRAY_SIZE(name_order)) {
      p = name_order[index];
    } else {
      p = "";
    }
  }
  plr->nation = find_nation_by_name_orig(p);
  if (plr->nation == NO_NATION_SELECTED) {
    freelog(LOG_FATAL, _("Nation %s (used by %s) isn't available."),
	    p, plr->name);
    exit(EXIT_FAILURE);
  }

  /* Add techs from game and nation, but ignore game.tech. */
  init_tech(plr, 0);

  /* not all players have teams */
  if (section_file_lookup(file, "player%d.team", plrno)) {
    char tmp[MAX_LEN_NAME];

    sz_strlcpy(tmp, secfile_lookup_str(file, "player%d.team", plrno));
    team_add_player(plr, tmp);
    plr->team = team_find_by_name(tmp);
  } else {
    plr->team = TEAM_NONE;
  }
  if (is_barbarian(plr)) {
    plr->nation=game.nation_count-1;
  }
  plr->government=secfile_lookup_int(file, "player%d.government", plrno);
  plr->embassy=secfile_lookup_int(file, "player%d.embassy", plrno);

  p = secfile_lookup_str_default(file, NULL, "player%d.city_style_by_name",
                                 plrno);
  if (!p) {
    char* old_order[4] = {"European", "Classical", "Tropical", "Asian"};
    c_s = secfile_lookup_int_default(file, 0, "player%d.city_style", plrno);
    if (c_s < 0 || c_s > 3) {
      c_s = 0;
    }
    p = old_order[c_s];
  }
  c_s = get_style_by_name_orig(p);
  if (c_s == -1) {
    freelog(LOG_ERROR, _("Unsupported city style found in player%d section. "
                         "Changed to %s"), plrno, get_city_style_name(0));
    c_s = 0;
  }	
  plr->city_style = c_s;

  plr->nturns_idle=0;
  plr->is_male=secfile_lookup_bool_default(file, TRUE, "player%d.is_male", plrno);
  plr->is_alive=secfile_lookup_bool(file, "player%d.is_alive", plrno);
  plr->ai.control = secfile_lookup_bool(file, "player%d.ai.control", plrno);
  for (i = 0; i < MAX_NUM_PLAYERS; i++) {
    ai->diplomacy.player_intel[i].love
         = secfile_lookup_int_default(file, 1, "player%d.ai.love%d", plrno, i);
    ai->diplomacy.player_intel[i].spam 
         = secfile_lookup_int_default(file, 0, "player%d.ai.spam%d", plrno, i);
    ai->diplomacy.player_intel[i].ally_patience
         = secfile_lookup_int_default(file, 0, "player%d.ai.patience%d", plrno, i);
    ai->diplomacy.player_intel[i].warned_about_space
         = secfile_lookup_int_default(file, 0, "player%d.ai.warn_space%d", plrno, i);
    ai->diplomacy.player_intel[i].asked_about_peace
         = secfile_lookup_int_default(file, 0, "player%d.ai.ask_peace%d", plrno, i);
    ai->diplomacy.player_intel[i].asked_about_alliance
         = secfile_lookup_int_default(file, 0, "player%d.ai.ask_alliance%d", plrno, i);
    ai->diplomacy.player_intel[i].asked_about_ceasefire
         = secfile_lookup_int_default(file, 0, "player%d.ai.ask_ceasefire%d", plrno, i);
  }
  plr->ai.tech_goal = secfile_lookup_int(file, "player%d.ai.tech_goal", plrno);
  if (plr->ai.tech_goal == A_NONE
      || !tech_exists(plr->ai.tech_goal)) {
    /* The value of A_UNSET could change in the future, since it
     * is not ruleset-dependent.  And it used to be A_NONE, so we check for
     * that as well.  This is a hack since there's no way to distinguish
     * from A_FUTURE (which shouldn't ever be here anyway). */
    plr->ai.tech_goal = A_UNSET;
  }
  /* Some sane defaults */
  plr->ai.handicap = 0;		/* set later */
  plr->ai.fuzzy = 0;		/* set later */
  plr->ai.expand = 100;		/* set later */
  plr->ai.science_cost = 100;	/* set later */
  plr->ai.skill_level =
    secfile_lookup_int_default(file, game.skill_level,
			       "player%d.ai.skill_level", plrno);
  if (plr->ai.control && plr->ai.skill_level==0) {
    plr->ai.skill_level = GAME_OLD_DEFAULT_SKILL_LEVEL;
  }
  if (plr->ai.control) {
    /* Set AI parameters */
    set_ai_level_directer(plr, plr->ai.skill_level);
  }

  plr->economic.gold=secfile_lookup_int(file, "player%d.gold", plrno);
  plr->economic.tax=secfile_lookup_int(file, "player%d.tax", plrno);
  plr->economic.science=secfile_lookup_int(file, "player%d.science", plrno);
  plr->economic.luxury=secfile_lookup_int(file, "player%d.luxury", plrno);

  plr->future_tech=secfile_lookup_int(file, "player%d.futuretech", plrno);

  /* We use default values for bulbs_researched_before, changed_from
   * and got_tech to preserve backwards-compatability with save files
   * that didn't store this information. */
  plr->research.bulbs_researched=secfile_lookup_int(file, 
					     "player%d.researched", plrno);
  plr->research.bulbs_researched_before =
	  secfile_lookup_int_default(file, 0,
				     "player%d.researched_before", plrno);
  plr->research.changed_from =
	  secfile_lookup_int_default(file, -1,
				     "player%d.research_changed_from",
				     plrno);
  plr->got_tech = secfile_lookup_bool_default(file, FALSE,
					      "player%d.research_got_tech",
					      plrno);
  plr->research.techs_researched=secfile_lookup_int(file, 
					     "player%d.researchpoints", plrno);
  plr->research.researching=secfile_lookup_int(file, 
					     "player%d.researching", plrno);
  if (plr->research.researching == A_NONE
      || !tech_exists(plr->research.researching)) {
    /* The value of A_FUTURE could change in the future, since it
     * is not ruleset-dependent.  And it used to be A_NONE, so we check for
     * that as well.  This is a hack since there's no way to distinguish
     * from A_UNSET (which shouldn't ever be here anyway). */
    plr->research.researching = A_FUTURE;
  }

  p=secfile_lookup_str(file, "player%d.invs", plrno);
    
  plr->capital=secfile_lookup_bool(file, "player%d.capital", plrno);
  plr->revolution=secfile_lookup_int_default(file, 0, "player%d.revolution",
                                             plrno);

  tech_type_iterate(i) {
    if (p[i] == '1') {
      set_invention(plr, i, TECH_KNOWN);
    }
  } tech_type_iterate_end;

  update_research(plr);

  plr->reputation=secfile_lookup_int_default(file, GAME_DEFAULT_REPUTATION,
					     "player%d.reputation", plrno);
  for (i=0; i < game.nplayers; i++) {
    plr->diplstates[i].type = 
      secfile_lookup_int_default(file, DS_WAR,
				 "player%d.diplstate%d.type", plrno, i);
    plr->diplstates[i].turns_left = 
      secfile_lookup_int_default(file, -2,
				 "player%d.diplstate%d.turns_left", plrno, i);
    plr->diplstates[i].has_reason_to_cancel = 
      secfile_lookup_int_default(file, 0,
				 "player%d.diplstate%d.has_reason_to_cancel",
				 plrno, i);
    plr->diplstates[i].contact_turns_left = 
      secfile_lookup_int_default(file, 0,
			   "player%d.diplstate%d.contact_turns_left", plrno, i);
  }
  /* We don't need this info, but savegames carry it anyway.
     To avoid getting "unused" warnings we touch the values like this. */
  for (i=game.nplayers; i<MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS; i++) {
    secfile_lookup_int_default(file, DS_NEUTRAL,
			       "player%d.diplstate%d.type", plrno, i);
    secfile_lookup_int_default(file, 0,
			       "player%d.diplstate%d.turns_left", plrno, i);
    secfile_lookup_int_default(file, 0,
			       "player%d.diplstate%d.has_reason_to_cancel",
			       plrno, i);
    secfile_lookup_int_default(file, 0,
			   "player%d.diplstate%d.contact_turns_left", plrno, i);
  }
  /* Sanity check alliances, prevent allied-with-ally-of-enemy */
  players_iterate(aplayer) {
    if (pplayers_allied(plr, aplayer)
        && !pplayer_can_ally(plr, aplayer)) {
      freelog(LOG_ERROR, _("Illegal alliance structure detected: "
              "%s's alliance to %s reduced to peace treaty."),
              plr->name, aplayer->name);
      plr->diplstates[aplayer->player_no].type = DS_PEACE;
      aplayer->diplstates[plr->player_no].type = DS_PEACE;
      resolve_unit_stacks(plr, aplayer, FALSE);
    }
  } players_iterate_end;

  { /* spacerace */
    struct player_spaceship *ship = &plr->spaceship;
    char prefix[32];
    char *st;
    
    my_snprintf(prefix, sizeof(prefix), "player%d.spaceship", plrno);
    spaceship_init(ship);
    ship->state = secfile_lookup_int(file, "%s.state", prefix);

    if (ship->state != SSHIP_NONE) {
      ship->structurals = secfile_lookup_int(file, "%s.structurals", prefix);
      ship->components = secfile_lookup_int(file, "%s.components", prefix);
      ship->modules = secfile_lookup_int(file, "%s.modules", prefix);
      ship->fuel = secfile_lookup_int(file, "%s.fuel", prefix);
      ship->propulsion = secfile_lookup_int(file, "%s.propulsion", prefix);
      ship->habitation = secfile_lookup_int(file, "%s.habitation", prefix);
      ship->life_support = secfile_lookup_int(file, "%s.life_support", prefix);
      ship->solar_panels = secfile_lookup_int(file, "%s.solar_panels", prefix);

      st = secfile_lookup_str(file, "%s.structure", prefix);
      for (i = 0; i < NUM_SS_STRUCTURALS; i++) {
	if (st[i] == '0') {
	  ship->structure[i] = FALSE;
	} else if (st[i] == '1') {
	  ship->structure[i] = TRUE;
	} else {
	  freelog(LOG_ERROR, "invalid spaceship structure '%c' %d", st[i],
		  st[i]);
	  ship->structure[i] = FALSE;
	}
      }
      if (ship->state >= SSHIP_LAUNCHED) {
	ship->launch_year = secfile_lookup_int(file, "%s.launch_year", prefix);
      }
      spaceship_calc_derived(ship);
    }
  }

  city_list_init(&plr->cities);
  ncities=secfile_lookup_int(file, "player%d.ncities", plrno);
  if (!plr->is_alive && ncities > 0) {
    ncities = 0; /* Some old savegames may be buggy. */
  }

  for (i = 0; i < ncities; i++) { /* read the cities */
    struct city *pcity;
    int nat_x = secfile_lookup_int(file, "player%d.c%d.x", plrno, i);
    int nat_y = secfile_lookup_int(file, "player%d.c%d.y", plrno, i);
    int map_x, map_y;
    const char* name;
    int id, k;

    native_to_map_pos(&map_x, &map_y, nat_x, nat_y);
    pcity = create_city_virtual(plr, map_x, map_y,
                      secfile_lookup_str(file, "player%d.c%d.name", plrno, i));

    pcity->id=secfile_lookup_int(file, "player%d.c%d.id", plrno, i);
    alloc_id(pcity->id);
    idex_register_city(pcity);
    
    if (section_file_lookup(file, "player%d.c%d.original", plrno, i))
      pcity->original = secfile_lookup_int(file, "player%d.c%d.original", 
					   plrno,i);
    else 
      pcity->original = plrno;
    pcity->size=secfile_lookup_int(file, "player%d.c%d.size", plrno, i);

    pcity->steal=secfile_lookup_int(file, "player%d.c%d.steal", plrno, i);

    specialist_type_iterate(sp) {
      pcity->specialists[sp]
	= secfile_lookup_int(file, "player%d.c%d.n%s", plrno, i,
			     game.rgame.specialists[sp].name);
    } specialist_type_iterate_end;

    for (j = 0; j < NUM_TRADEROUTES; j++)
      pcity->trade[j]=secfile_lookup_int(file, "player%d.c%d.traderoute%d",
					 plrno, i, j);
    
    pcity->food_stock=secfile_lookup_int(file, "player%d.c%d.food_stock", 
						 plrno, i);
    pcity->shield_stock=secfile_lookup_int(file, "player%d.c%d.shield_stock", 
						   plrno, i);
    pcity->tile_trade=pcity->trade_prod=0;
    pcity->anarchy=secfile_lookup_int(file, "player%d.c%d.anarchy", plrno,i);
    pcity->rapture=secfile_lookup_int_default(file, 0, "player%d.c%d.rapture", plrno,i);
    pcity->was_happy=secfile_lookup_bool(file, "player%d.c%d.was_happy", plrno,i);
    pcity->is_building_unit=
      secfile_lookup_bool(file, 
			 "player%d.c%d.is_building_unit", plrno, i);
    name = secfile_lookup_str_default(file, NULL,
				      "player%d.c%d.currently_building_name",
				      plrno, i);
    if (pcity->is_building_unit) {
      if (!name) {
	id = secfile_lookup_int(file, "player%d.c%d.currently_building", 
				plrno, i);
	name = old_unit_type_name(id);
      }
      pcity->currently_building = find_unit_type_by_name_orig(name);
    } else {
      if (!name) {
	id = secfile_lookup_int(file, "player%d.c%d.currently_building",
				plrno, i);
	name = old_impr_type_name(id);
      }
      pcity->currently_building = find_improvement_by_name_orig(name);
    }

    if (has_capability("turn_last_built", savefile_options)) {
      pcity->turn_last_built = secfile_lookup_int(file,
				"player%d.c%d.turn_last_built", plrno, i);
    } else {
      /* Before, turn_last_built was stored as a year.  There is no easy
       * way to convert this into a turn value. */
      pcity->turn_last_built = 0;
    }
    pcity->changed_from_is_unit=
      secfile_lookup_bool_default(file, pcity->is_building_unit,
				 "player%d.c%d.changed_from_is_unit", plrno, i);
    name = secfile_lookup_str_default(file, NULL,
				      "player%d.c%d.changed_from_name",
				      plrno, i);
    if (pcity->changed_from_is_unit) {
      if (!name) {
	id = secfile_lookup_int(file, "player%d.c%d.changed_from_id", 
				plrno, i);
	name = old_unit_type_name(id);
      }
      pcity->changed_from_id = find_unit_type_by_name_orig(name);
    } else {
      if (!name) {
	id = secfile_lookup_int(file, "player%d.c%d.changed_from_id",
				plrno, i);
	name = old_impr_type_name(id);
      }
      pcity->changed_from_id = find_improvement_by_name_orig(name);
    }
			 
    pcity->before_change_shields=
      secfile_lookup_int_default(file, pcity->shield_stock,
				 "player%d.c%d.before_change_shields", plrno, i);
    pcity->disbanded_shields=
      secfile_lookup_int_default(file, 0,
				 "player%d.c%d.disbanded_shields", plrno, i);
    pcity->caravan_shields=
      secfile_lookup_int_default(file, 0,
				 "player%d.c%d.caravan_shields", plrno, i);
    pcity->last_turns_shield_surplus =
      secfile_lookup_int_default(file, 0,
				 "player%d.c%d.last_turns_shield_surplus",
				 plrno, i);

    pcity->synced = FALSE; /* must re-sync with clients */

    pcity->turn_founded =
	secfile_lookup_int_default(file, -2, "player%d.c%d.turn_founded",
				   plrno, i);

    j = secfile_lookup_int(file, "player%d.c%d.did_buy", plrno, i);
    pcity->did_buy = (j != 0);
    if (j == -1 && pcity->turn_founded == -2) {
      pcity->turn_founded = game.turn;
    }

    pcity->did_sell =
      secfile_lookup_bool_default(file, FALSE, "player%d.c%d.did_sell", plrno,i);
    
    pcity->airlift = secfile_lookup_bool_default(file, FALSE,
					"player%d.c%d.airlift", plrno,i);

    pcity->city_options =
      secfile_lookup_int_default(file, CITYOPT_DEFAULT,
				 "player%d.c%d.options", plrno, i);

    /* Fix for old buggy savegames. */
    if (!has_capability("known32fix", savefile_options)
	&& plrno >= 16) {
      map_city_radius_iterate(pcity->x, pcity->y, x1, y1) {
	map_set_known(x1, y1, plr);
      } map_city_radius_iterate_end;
    }
    
    /* adding the cities contribution to fog-of-war */
    map_unfog_pseudo_city_area(&game.players[plrno],pcity->x,pcity->y);

    unit_list_init(&pcity->units_supported);

    /* Initialize pcity->city_map[][], using set_worker_city() so that
       ptile->worked gets initialized correctly.  The pre-initialisation
       to C_TILE_EMPTY is necessary because set_worker_city() accesses
       the existing value to possibly adjust ptile->worked, so need to
       initialize a non-worked value so ptile->worked (possibly already
       set from neighbouring city) does not get unset for C_TILE_EMPTY
       or C_TILE_UNAVAILABLE here.  -- dwp
    */
    p=secfile_lookup_str(file, "player%d.c%d.workers", plrno, i);
    for(y=0; y<CITY_MAP_SIZE; y++) {
      for(x=0; x<CITY_MAP_SIZE; x++) {
	pcity->city_map[x][y] =
	    is_valid_city_coords(x, y) ? C_TILE_EMPTY : C_TILE_UNAVAILABLE;
	if (*p == '0') {
	  int map_x, map_y;

	  set_worker_city(pcity, x, y,
			  city_map_to_map(&map_x, &map_y, pcity, x, y) ?
			  C_TILE_EMPTY : C_TILE_UNAVAILABLE);
	} else if (*p=='1') {
	  int map_x, map_y;
	  bool is_real;

	  is_real = city_map_to_map(&map_x, &map_y, pcity, x, y);
	  assert(is_real);

	  if (map_get_tile(map_x, map_y)->worked) {
	    /* oops, inconsistent savegame; minimal fix: */
	    freelog(LOG_VERBOSE, "Inconsistent worked for %s (%d,%d), "
		    "converting to elvis", pcity->name, x, y);
	    pcity->specialists[SP_ELVIS]++;
	    set_worker_city(pcity, x, y, C_TILE_UNAVAILABLE);
	  } else {
	    set_worker_city(pcity, x, y, C_TILE_WORKER);
	  }
	} else {
	  assert(*p == '2');
	  if (is_valid_city_coords(x, y)) {
	    set_worker_city(pcity, x, y, C_TILE_UNAVAILABLE);
	  }
	  assert(pcity->city_map[x][y] == C_TILE_UNAVAILABLE);
	}
        p++;
      }
    }

    /* Initialise list of improvements with City- and Building-wide
       equiv_ranges */
    improvement_status_init(pcity->improvements,
			    ARRAY_SIZE(pcity->improvements));

    p = secfile_lookup_str_default(file, NULL,
				   "player%d.c%d.improvements_new",
                                   plrno, i);
    if (!p) {
      /* old savegames */
      p = secfile_lookup_str(file, "player%d.c%d.improvements", plrno, i);
      for (k = 0; p[k]; k++) {
        if (p[k] == '1') {
	  name = old_impr_type_name(k);
	  id = find_improvement_by_name_orig(name);
	  if (id != -1) {
	    city_add_improvement(pcity, id);
	  }
	}
      }
    } else {
      for (k = 0; k < improvement_order_size && p[k]; k++) {
        if (p[k] == '1') {
	  id = find_improvement_by_name_orig(improvement_order[k]);
	  if (id != -1) {
	    city_add_improvement(pcity, id);
	  }
	}
      }
    }

    init_worklist(&pcity->worklist);
    if (has_capability("worklists2", savefile_options)) {
      worklist_load(file, "player%d.c%d", plrno, i, &pcity->worklist);
    } else {
      worklist_load_old(file, "player%d.c%d.worklist%d",
			plrno, i, &pcity->worklist);
    }

    /* FIXME: remove this when the urgency is properly recalculated. */
    pcity->ai.urgency = secfile_lookup_int_default(file, 0, 
				"player%d.c%d.ai.urgency", plrno, i);

    map_set_city(pcity->x, pcity->y, pcity);

    city_list_insert_back(&plr->cities, pcity);
  }

  load_player_units(plr, plrno, file);

  if (section_file_lookup(file, "player%d.attribute_v2_block_length", plrno)) {
    int raw_length1, raw_length2, part_nr, parts;
    size_t quoted_length;
    char *quoted;

    raw_length1 =
	secfile_lookup_int(file, "player%d.attribute_v2_block_length", plrno);
    if (plr->attribute_block.data) {
      free(plr->attribute_block.data);
      plr->attribute_block.data = NULL;
    }
    plr->attribute_block.data = fc_malloc(raw_length1);
    plr->attribute_block.length = raw_length1;

    quoted_length = secfile_lookup_int
	(file, "player%d.attribute_v2_block_length_quoted", plrno);
    quoted = fc_malloc(quoted_length + 1);
    quoted[0] = 0;

    parts =
	secfile_lookup_int(file, "player%d.attribute_v2_block_parts", plrno);

    for (part_nr = 0; part_nr < parts; part_nr++) {
      char *current = secfile_lookup_str(file,
					 "player%d.attribute_v2_block_data.part%d",
					 plrno, part_nr);
      if (!current)
	break;
      freelog(LOG_DEBUG, "quoted_length=%lu quoted=%lu current=%lu",
	      (unsigned long) quoted_length,
	      (unsigned long) strlen(quoted),
	      (unsigned long) strlen(current));
      assert(strlen(quoted) + strlen(current) <= quoted_length);
      strcat(quoted, current);
    }
    if (quoted_length != strlen(quoted)) {
      freelog(LOG_NORMAL, "quoted_length=%lu quoted=%lu",
	      (unsigned long) quoted_length,
	      (unsigned long) strlen(quoted));
      assert(0);
    }

    raw_length2 =
	unquote_block(quoted,
		      plr->attribute_block.data,
		      plr->attribute_block.length);
    assert(raw_length1 == raw_length2);
    free(quoted);
  }
}

/********************************************************************** 
The private map for fog of war
***********************************************************************/
static void player_map_load(struct player *plr, int plrno,
			    struct section_file *file)
{
  int i;

  if (!plr->is_alive)
    whole_map_iterate(x, y) {
      map_change_seen(x, y, plr, +1);
    } whole_map_iterate_end;

  /* load map if:
     1) it from a fog of war build
     2) fog of war was on (otherwise the private map wasn't saved)
     3) is not from a "unit only" fog of war save file
  */
  if (secfile_lookup_int_default(file, -1, "game.fogofwar") != -1
      && game.fogofwar == TRUE
      && secfile_lookup_int_default(file, -1,"player%d.total_ncities", plrno) != -1
      && secfile_lookup_bool_default(file, TRUE, "game.save_private_map")) {
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "player%d.map_t%03d",
				     plrno, nat_y),
		  map_get_player_tile(x, y, plr)->terrain =
		  char2terrain(ch));

    /* get 4-bit segments of 12-bit "special" field. */
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "player%d.map_l%03d",
				     plrno, nat_y),
		  map_get_player_tile(x, y, plr)->special =
		  ascii_hex2bin(ch, 0));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str(file, "player%d.map_u%03d",
				     plrno, nat_y),
		  map_get_player_tile(x, y, plr)->special |=
		  ascii_hex2bin(ch, 1));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str_default
		  (file, NULL, "player%d.map_n%03d", plrno, nat_y),
		  map_get_player_tile(x, y, plr)->special |=
		  ascii_hex2bin(ch, 2));

    /* get 4-bit segments of 16-bit "updated" field */
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str
		  (file, "player%d.map_ua%03d", plrno, nat_y),
		  map_get_player_tile(x, y, plr)->last_updated =
		  ascii_hex2bin(ch, 0));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str
		  (file, "player%d.map_ub%03d", plrno, nat_y),
		  map_get_player_tile(x, y, plr)->last_updated |=
		  ascii_hex2bin(ch, 1));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str
		  (file, "player%d.map_uc%03d", plrno, nat_y),
		  map_get_player_tile(x, y, plr)->last_updated |=
		  ascii_hex2bin(ch, 2));
    LOAD_MAP_DATA(ch, x, y, nat_x, nat_y,
		  secfile_lookup_str
		  (file, "player%d.map_ud%03d", plrno, nat_y),
		  map_get_player_tile(x, y, plr)->last_updated |=
		  ascii_hex2bin(ch, 3));

    {
      int j;
      struct dumb_city *pdcity;
      i = secfile_lookup_int(file, "player%d.total_ncities", plrno);
      for (j = 0; j < i; j++) {
	int nat_x, nat_y, map_x, map_y;

	pdcity = fc_malloc(sizeof(struct dumb_city));
	pdcity->id = secfile_lookup_int(file, "player%d.dc%d.id", plrno, j);
	nat_x = secfile_lookup_int(file, "player%d.dc%d.x", plrno, j);
	nat_y = secfile_lookup_int(file, "player%d.dc%d.y", plrno, j);
	native_to_map_pos(&map_x, &map_y, nat_x, nat_y);
	sz_strlcpy(pdcity->name, secfile_lookup_str(file, "player%d.dc%d.name", plrno, j));
	pdcity->size = secfile_lookup_int(file, "player%d.dc%d.size", plrno, j);
	pdcity->has_walls = secfile_lookup_bool(file, "player%d.dc%d.has_walls", plrno, j);    
	pdcity->occupied = secfile_lookup_bool_default(file, FALSE,
					"player%d.dc%d.occupied", plrno, j);
	pdcity->happy = secfile_lookup_bool_default(file, FALSE,
					"player%d.dc%d.happy", plrno, j);
	pdcity->unhappy = secfile_lookup_bool_default(file, FALSE,
					"player%d.dc%d.unhappy", plrno, j);
	pdcity->owner = secfile_lookup_int(file, "player%d.dc%d.owner", plrno, j);
	map_get_player_tile(map_x, map_y, plr)->city = pdcity;
	alloc_id(pdcity->id);
      }
    }

    /* This shouldn't be neccesary if the savegame was consistent, but there
       is a bug in some pre-1.11 savegames. Anyway, it can't hurt */
    whole_map_iterate(x, y) {
      if (map_is_known_and_seen(x, y, plr)) {
	update_tile_knowledge(plr, x, y);
	reality_check_city(plr, x, y);
	if (map_get_city(x, y)) {
	  update_dumb_city(plr, map_get_city(x, y));
	}
      }
    } whole_map_iterate_end;

  } else {
    /* We have an old savegame or fog of war was turned off; the
       players private knowledge is set to be what he could see
       without fog of war */
    whole_map_iterate(x, y) {
      if (map_is_known(x, y, plr)) {
	struct city *pcity = map_get_city(x, y);
	update_player_tile_last_seen(plr, x, y);
	update_tile_knowledge(plr, x, y);
	if (pcity)
	  update_dumb_city(plr, pcity);
      }
    } whole_map_iterate_end;
  }
}

/***************************************************************
Save the worklist elements specified by path, given the arguments
plrno and wlinx, from the worklist pointed to by pwl.
***************************************************************/
static void worklist_save(struct section_file *file,
			  const char *path, int plrno, int wlinx,
			  struct worklist *pwl)
{
  char efpath[64];
  char idpath[64];
  char namepath[64];
  int i;

  sz_strlcpy(efpath, path);
  sz_strlcat(efpath, ".wlef%d");
  sz_strlcpy(idpath, path);
  sz_strlcat(idpath, ".wlid%d");
  sz_strlcpy(namepath, path);
  sz_strlcat(namepath, ".wlname%d");

  for (i = 0; i < MAX_LEN_WORKLIST; i++) {
    secfile_insert_int(file, pwl->wlefs[i], efpath, plrno, wlinx, i);
    if (pwl->wlefs[i] == WEF_UNIT) {
      secfile_insert_int(file, old_unit_type_id(pwl->wlids[i]), idpath,
			 plrno, wlinx, i);
      secfile_insert_str(file, unit_name_orig(pwl->wlids[i]), namepath, plrno,
			 wlinx, i);
    } else if (pwl->wlefs[i] == WEF_IMPR) {
      secfile_insert_int(file, pwl->wlids[i], idpath, plrno, wlinx, i);
      secfile_insert_str(file, get_improvement_name_orig(pwl->wlids[i]),
			 namepath, plrno, wlinx, i);
    } else {
      secfile_insert_int(file, 0, idpath, plrno, wlinx, i);
      secfile_insert_str(file, "", namepath, plrno, wlinx, i);
    }
    if (pwl->wlefs[i] == WEF_END) {
      break;
    }
  }

  /* Fill out remaining worklist entries. */
  for (i++; i < MAX_LEN_WORKLIST; i++) {
    /* These values match what worklist_load fills in for unused entries. */
    secfile_insert_int(file, WEF_END, efpath, plrno, wlinx, i);
    secfile_insert_int(file, 0, idpath, plrno, wlinx, i);
    secfile_insert_str(file, "", namepath, plrno, wlinx, i);
  }
}

/***************************************************************
...
***************************************************************/
static void player_save(struct player *plr, int plrno,
			struct section_file *file)
{
  int i;
  char invs[A_LAST+1];
  struct player_spaceship *ship = &plr->spaceship;
  struct ai_data *ai = ai_data_get(plr);

  secfile_insert_str(file, plr->name, "player%d.name", plrno);
  secfile_insert_str(file, plr->username, "player%d.username", plrno);
  secfile_insert_str(file, get_nation_name_orig(plr->nation),
		     "player%d.nation", plrno);
  /* 1.15 and later won't use the race field, they key on the nation string */
  secfile_insert_int(file, plr->nation, "player%d.race", plrno);
  if (plr->team != TEAM_NONE) {
    secfile_insert_str(file, (char *) team_get_by_id(plr->team)->name, 
                       "player%d.team", plrno);
  }
  secfile_insert_int(file, plr->government, "player%d.government", plrno);
  secfile_insert_int(file, plr->embassy, "player%d.embassy", plrno);

  /* This field won't be used, kept only for backward compatibility. 
   * City styles are specified by name since CVS 12/01-04. */
  secfile_insert_int(file, 0, "player%d.city_style", plrno);

  /* This is the new city style field to be used */
  secfile_insert_str(file, get_city_style_name_orig(plr->city_style),
                      "player%d.city_style_by_name", plrno);

  secfile_insert_bool(file, plr->is_male, "player%d.is_male", plrno);
  secfile_insert_bool(file, plr->is_alive, "player%d.is_alive", plrno);
  secfile_insert_bool(file, plr->ai.control, "player%d.ai.control", plrno);
  for (i = 0; i < MAX_NUM_PLAYERS; i++) {
    secfile_insert_int(file, ai->diplomacy.player_intel[i].love, 
                       "player%d.ai.love%d", plrno, i);
    secfile_insert_int(file, ai->diplomacy.player_intel[i].spam, 
                       "player%d.ai.spam%d", plrno, i);
    secfile_insert_int(file, ai->diplomacy.player_intel[i].ally_patience, 
                       "player%d.ai.patience%d", plrno, i);
    secfile_insert_int(file, ai->diplomacy.player_intel[i].warned_about_space, 
                       "player%d.ai.warn_space%d", plrno, i);
    secfile_insert_int(file, ai->diplomacy.player_intel[i].asked_about_peace, 
                       "player%d.ai.ask_peace%d", plrno, i);
    secfile_insert_int(file, ai->diplomacy.player_intel[i].asked_about_alliance, 
                       "player%d.ai.ask_alliance%d", plrno, i);
    secfile_insert_int(file, ai->diplomacy.player_intel[i].asked_about_ceasefire, 
                       "player%d.ai.ask_ceasefire%d", plrno, i);
  }
  secfile_insert_int(file, plr->ai.tech_goal, "player%d.ai.tech_goal", plrno);
  secfile_insert_int(file, plr->ai.skill_level,
		     "player%d.ai.skill_level", plrno);
  secfile_insert_int(file, plr->ai.barbarian_type, "player%d.ai.is_barbarian", plrno);
  secfile_insert_int(file, plr->economic.gold, "player%d.gold", plrno);
  secfile_insert_int(file, plr->economic.tax, "player%d.tax", plrno);
  secfile_insert_int(file, plr->economic.science, "player%d.science", plrno);
  secfile_insert_int(file, plr->economic.luxury, "player%d.luxury", plrno);

  secfile_insert_int(file,plr->future_tech,"player%d.futuretech", plrno);

  secfile_insert_int(file, plr->research.bulbs_researched, 
		     "player%d.researched", plrno);
  secfile_insert_int(file, plr->research.bulbs_researched_before,
		     "player%d.researched_before", plrno);
  secfile_insert_bool(file, plr->got_tech,
		      "player%d.research_got_tech", plrno);
  secfile_insert_int(file, plr->research.changed_from,
		     "player%d.research_changed_from", plrno);
  secfile_insert_int(file, plr->research.techs_researched,
		     "player%d.researchpoints", plrno);
  secfile_insert_int(file, plr->research.researching,
		     "player%d.researching", plrno);  

  secfile_insert_bool(file, plr->capital, "player%d.capital", plrno);
  secfile_insert_int(file, plr->revolution, "player%d.revolution", plrno);

  tech_type_iterate(tech_id) {
    invs[tech_id] = (get_invention(plr, tech_id) == TECH_KNOWN) ? '1' : '0';
  } tech_type_iterate_end;
  invs[game.num_tech_types] = '\0';
  secfile_insert_str(file, invs, "player%d.invs", plrno);

  secfile_insert_int(file, plr->reputation, "player%d.reputation", plrno);
  for (i = 0; i < MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS; i++) {
    secfile_insert_int(file, plr->diplstates[i].type,
		       "player%d.diplstate%d.type", plrno, i);
    secfile_insert_int(file, plr->diplstates[i].turns_left,
		       "player%d.diplstate%d.turns_left", plrno, i);
    secfile_insert_int(file, plr->diplstates[i].has_reason_to_cancel,
		       "player%d.diplstate%d.has_reason_to_cancel", plrno, i);
    secfile_insert_int(file, plr->diplstates[i].contact_turns_left,
		       "player%d.diplstate%d.contact_turns_left", plrno, i);
  }

  {
    char vision[MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS+1];

    for (i=0; i < MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS; i++)
      vision[i] = gives_shared_vision(plr, get_player(i)) ? '1' : '0';
    vision[i] = '\0';
    secfile_insert_str(file, vision, "player%d.gives_shared_vision", plrno);
  }

  secfile_insert_int(file, ship->state, "player%d.spaceship.state", plrno);

  if (ship->state != SSHIP_NONE) {
    char prefix[32];
    char st[NUM_SS_STRUCTURALS+1];
    int i;
    
    my_snprintf(prefix, sizeof(prefix), "player%d.spaceship", plrno);

    secfile_insert_int(file, ship->structurals, "%s.structurals", prefix);
    secfile_insert_int(file, ship->components, "%s.components", prefix);
    secfile_insert_int(file, ship->modules, "%s.modules", prefix);
    secfile_insert_int(file, ship->fuel, "%s.fuel", prefix);
    secfile_insert_int(file, ship->propulsion, "%s.propulsion", prefix);
    secfile_insert_int(file, ship->habitation, "%s.habitation", prefix);
    secfile_insert_int(file, ship->life_support, "%s.life_support", prefix);
    secfile_insert_int(file, ship->solar_panels, "%s.solar_panels", prefix);
    
    for(i=0; i<NUM_SS_STRUCTURALS; i++) {
      st[i] = (ship->structure[i]) ? '1' : '0';
    }
    st[i] = '\0';
    secfile_insert_str(file, st, "%s.structure", prefix);
    if (ship->state >= SSHIP_LAUNCHED) {
      secfile_insert_int(file, ship->launch_year, "%s.launch_year", prefix);
    }
  }

  secfile_insert_int(file, unit_list_size(&plr->units), "player%d.nunits", 
		     plrno);
  secfile_insert_int(file, city_list_size(&plr->cities), "player%d.ncities", 
		     plrno);

  i = -1;
  unit_list_iterate(plr->units, punit) {
    i++;
    secfile_insert_int(file, punit->id, "player%d.u%d.id", plrno, i);
    do_in_native_pos(nat_x, nat_y, punit->x, punit->y) {
      secfile_insert_int(file, nat_x, "player%d.u%d.x", plrno, i);
      secfile_insert_int(file, nat_y, "player%d.u%d.y", plrno, i);
    } do_in_native_pos_end;
    secfile_insert_int(file, punit->veteran, "player%d.u%d.veteran", 
				plrno, i);
    secfile_insert_bool(file, punit->foul, "player%d.u%d.foul", 
				plrno, i);
    secfile_insert_int(file, punit->hp, "player%d.u%d.hp", plrno, i);
    secfile_insert_int(file, punit->homecity, "player%d.u%d.homecity",
				plrno, i);
    /* .type is actually kept only for backward compatibility */
    secfile_insert_int(file, old_unit_type_id(punit->type), "player%d.u%d.type",
		       plrno, i);
    secfile_insert_str(file, unit_name_orig(punit->type),
		       "player%d.u%d.type_by_name",
		       plrno, i);
    secfile_insert_int(file, punit->activity, "player%d.u%d.activity",
				plrno, i);
    secfile_insert_int(file, punit->activity_count, 
				"player%d.u%d.activity_count",
				plrno, i);
    secfile_insert_int(file, punit->activity_target, 
				"player%d.u%d.activity_target",
				plrno, i);
    secfile_insert_bool(file, punit->connecting, 
				"player%d.u%d.connecting",
				plrno, i);
    secfile_insert_bool(file, punit->done_moving,
			"player%d.u%d.done_moving", plrno, i);
    secfile_insert_int(file, punit->moves_left, "player%d.u%d.moves",
		                plrno, i);
    secfile_insert_int(file, punit->fuel, "player%d.u%d.fuel",
		                plrno, i);

    if (is_goto_dest_set(punit)) {
      secfile_insert_bool(file, TRUE, "player%d.u%d.go", plrno, i);
      do_in_native_pos(nat_x, nat_y,
		       goto_dest_x(punit), goto_dest_y(punit)) {
	secfile_insert_int(file, nat_x, "player%d.u%d.goto_x", plrno, i);
	secfile_insert_int(file, nat_y, "player%d.u%d.goto_y", plrno, i);
      } do_in_native_pos_end;
    } else {
      secfile_insert_bool(file, FALSE, "player%d.u%d.go", plrno, i);
      /* for compatility with older servers */
      secfile_insert_int(file, 0, "player%d.u%d.goto_x", plrno, i);
      secfile_insert_int(file, 0, "player%d.u%d.goto_y", plrno, i);
    }

    secfile_insert_bool(file, punit->ai.control, "player%d.u%d.ai", plrno, i);
    secfile_insert_int(file, punit->ord_map, "player%d.u%d.ord_map", plrno, i);
    secfile_insert_int(file, punit->ord_city, "player%d.u%d.ord_city", plrno, i);
    secfile_insert_bool(file, punit->moved, "player%d.u%d.moved", plrno, i);
    secfile_insert_bool(file, punit->paradropped, "player%d.u%d.paradropped", plrno, i);
    secfile_insert_int(file, punit->transported_by,
		       "player%d.u%d.transported_by", plrno, i);
    if (punit->has_orders) {
      int len = punit->orders.length, j;
      char orders_buf[len + 1], dir_buf[len + 1];

      secfile_insert_int(file, len, "player%d.u%d.orders_length", plrno, i);
      secfile_insert_int(file, punit->orders.index,
			 "player%d.u%d.orders_index", plrno, i);
      secfile_insert_bool(file, punit->orders.repeat,
			  "player%d.u%d.orders_repeat", plrno, i);
      secfile_insert_bool(file, punit->orders.vigilant,
			  "player%d.u%d.orders_vigilant", plrno, i);

      for (j = 0; j < len; j++) {
	orders_buf[j] = 'a' + punit->orders.list[j].order;
	dir_buf[j] = 'a' + punit->orders.list[j].dir;
      }
      orders_buf[len] = dir_buf[len] = '\0';

      secfile_insert_str(file, orders_buf,
			 "player%d.u%d.orders_list", plrno, i);
      secfile_insert_str(file, dir_buf,
			 "player%d.u%d.dir_list", plrno, i);
    } else {
      /* Put all the same fields into the savegame.  Otherwise the
       * registry code gets confused (although it still works). */
      secfile_insert_int(file, 0, "player%d.u%d.orders_length", plrno, i);
      secfile_insert_int(file, 0, "player%d.u%d.orders_index", plrno, i);
      secfile_insert_bool(file, FALSE,
			  "player%d.u%d.orders_repeat", plrno, i);
      secfile_insert_bool(file, FALSE,
			  "player%d.u%d.orders_vigilant", plrno, i);
      secfile_insert_str(file, "-",
			 "player%d.u%d.orders_list", plrno, i);
      secfile_insert_str(file, "-",
			 "player%d.u%d.dir_list", plrno, i);
    }
  }
  unit_list_iterate_end;

  i = -1;
  city_list_iterate(plr->cities, pcity) {
    int j, x, y;
    char buf[512];

    i++;
    secfile_insert_int(file, pcity->id, "player%d.c%d.id", plrno, i);
    do_in_native_pos(nat_x, nat_y, pcity->x, pcity->y) {
      secfile_insert_int(file, nat_x, "player%d.c%d.x", plrno, i);
      secfile_insert_int(file, nat_y, "player%d.c%d.y", plrno, i);
    } do_in_native_pos_end;
    secfile_insert_str(file, pcity->name, "player%d.c%d.name", plrno, i);
    secfile_insert_int(file, pcity->original, "player%d.c%d.original", 
		       plrno, i);
    secfile_insert_int(file, pcity->size, "player%d.c%d.size", plrno, i);
    secfile_insert_int(file, pcity->steal, "player%d.c%d.steal", plrno, i);
    specialist_type_iterate(sp) {
      secfile_insert_int(file, pcity->specialists[sp],
			 "player%d.c%d.n%s", plrno, i,
			 game.rgame.specialists[sp].name);
    } specialist_type_iterate_end;

    for (j = 0; j < NUM_TRADEROUTES; j++)
      secfile_insert_int(file, pcity->trade[j], "player%d.c%d.traderoute%d", 
			 plrno, i, j);
    
    secfile_insert_int(file, pcity->food_stock, "player%d.c%d.food_stock", 
		       plrno, i);
    secfile_insert_int(file, pcity->shield_stock, "player%d.c%d.shield_stock", 
		       plrno, i);
    secfile_insert_int(file, pcity->turn_last_built,
		       "player%d.c%d.turn_last_built", plrno, i);
    secfile_insert_bool(file, pcity->changed_from_is_unit,
		       "player%d.c%d.changed_from_is_unit", plrno, i);
    if (pcity->changed_from_is_unit) {
      secfile_insert_int(file, old_unit_type_id(pcity->changed_from_id),
		         "player%d.c%d.changed_from_id", plrno, i);
      secfile_insert_str(file, unit_name_orig(pcity->changed_from_id),
                         "player%d.c%d.changed_from_name", plrno, i);
    } else {
      secfile_insert_int(file, old_impr_type_id(pcity->changed_from_id),
		         "player%d.c%d.changed_from_id", plrno, i);    
      secfile_insert_str(file, get_improvement_name_orig(
                                 pcity->changed_from_id),
                         "player%d.c%d.changed_from_name", plrno, i);
    }

    secfile_insert_int(file, pcity->before_change_shields,
		       "player%d.c%d.before_change_shields", plrno, i);
    secfile_insert_int(file, pcity->disbanded_shields,
		       "player%d.c%d.disbanded_shields", plrno, i);
    secfile_insert_int(file, pcity->caravan_shields,
		       "player%d.c%d.caravan_shields", plrno, i);
    secfile_insert_int(file, pcity->last_turns_shield_surplus,
		       "player%d.c%d.last_turns_shield_surplus", plrno, i);

    secfile_insert_int(file, pcity->anarchy, "player%d.c%d.anarchy", plrno,i);
    secfile_insert_int(file, pcity->rapture, "player%d.c%d.rapture", plrno,i);
    secfile_insert_bool(file, pcity->was_happy, "player%d.c%d.was_happy", plrno,i);
    if (pcity->turn_founded == game.turn) {
      j = -1;
    } else {
      assert(pcity->did_buy == TRUE || pcity->did_buy == FALSE);
      j = pcity->did_buy ? 1 : 0;
    }
    secfile_insert_int(file, j, "player%d.c%d.did_buy", plrno, i);
    secfile_insert_int(file, pcity->turn_founded,
		       "player%d.c%d.turn_founded", plrno, i);
    secfile_insert_bool(file, pcity->did_sell, "player%d.c%d.did_sell", plrno,i);
    secfile_insert_bool(file, pcity->airlift, "player%d.c%d.airlift", plrno,i);

    /* for auto_attack */
    secfile_insert_int(file, pcity->city_options,
		       "player%d.c%d.options", plrno, i);
    
    j=0;
    for(y=0; y<CITY_MAP_SIZE; y++) {
      for(x=0; x<CITY_MAP_SIZE; x++) {
	switch (get_worker_city(pcity, x, y)) {
	  case C_TILE_EMPTY:       buf[j++] = '0'; break;
	  case C_TILE_WORKER:      buf[j++] = '1'; break;
	  case C_TILE_UNAVAILABLE: buf[j++] = '2'; break;
	}
      }
    }
    buf[j]='\0';
    secfile_insert_str(file, buf, "player%d.c%d.workers", plrno, i);

    secfile_insert_bool(file, pcity->is_building_unit, 
		       "player%d.c%d.is_building_unit", plrno, i);
    if (pcity->is_building_unit) {
      secfile_insert_int(file, old_unit_type_id(pcity->currently_building), 
		         "player%d.c%d.currently_building", plrno, i);
      secfile_insert_str(file, unit_name_orig(pcity->currently_building),
                         "player%d.c%d.currently_building_name", plrno, i);
    } else {
      secfile_insert_int(file, old_impr_type_id(pcity->currently_building),
                         "player%d.c%d.currently_building", plrno, i);
      secfile_insert_str(file, get_improvement_name_orig(
                                   pcity->currently_building),
                         "player%d.c%d.currently_building_name", plrno, i);
    }

    /* 1.14 servers depend on improvement order in ruleset. Here we
     * are trying to simulate 1.14.1 default order
     */
    init_old_improvement_bitvector(buf);
    impr_type_iterate(id) {
      if (pcity->improvements[id] != I_NONE) {
        add_improvement_into_old_bitvector(buf, id);
      }
    } impr_type_iterate_end;
    secfile_insert_str(file, buf, "player%d.c%d.improvements", plrno, i);

    /* Save improvement list as bitvector. Note that improvement order
     * is saved in savefile.improvement_order.
     */
    impr_type_iterate(id) {
      buf[id] = (pcity->improvements[id] != I_NONE) ? '1' : '0';
    } impr_type_iterate_end;
    buf[game.num_impr_types] = '\0';
    secfile_insert_str(file, buf, "player%d.c%d.improvements_new", plrno, i);    

    worklist_save(file, "player%d.c%d", plrno, i, &pcity->worklist);

    /* FIXME: remove this when the urgency is properly recalculated. */
    secfile_insert_int(file, pcity->ai.urgency,
		       "player%d.c%d.ai.urgency", plrno, i);
  }
  city_list_iterate_end;

  /********** Put the players private map **********/
 /* Otherwise the player can see all, and there's no reason to save the private map. */
  if (game.fogofwar
      && game.save_options.save_private_map) {

    /* put the terrain type */
    SAVE_PLAYER_MAP_DATA(x, y, file,"player%d.map_t%03d", plrno, 
			 terrain_chars[map_get_player_tile
				       (x, y, plr)->terrain]);

    /* put 4-bit segments of 12-bit "special flags" field */
    SAVE_PLAYER_MAP_DATA(x, y, file,"player%d.map_l%03d", plrno,
			 bin2ascii_hex(map_get_player_tile(x, y, plr)->
				       special, 0));
    SAVE_PLAYER_MAP_DATA(x, y, file, "player%d.map_u%03d", plrno,
			 bin2ascii_hex(map_get_player_tile(x, y, plr)->
				       special, 1));
    SAVE_PLAYER_MAP_DATA(x, y, file, "player%d.map_n%03d", plrno,
			 bin2ascii_hex(map_get_player_tile(x, y, plr)->
				       special, 2));

    /* put 4-bit segments of 16-bit "updated" field */
    SAVE_PLAYER_MAP_DATA(x, y, file,"player%d.map_ua%03d", plrno,
			 bin2ascii_hex(map_get_player_tile
				       (x, y, plr)->last_updated, 0));
    SAVE_PLAYER_MAP_DATA(x, y, file, "player%d.map_ub%03d", plrno,
			 bin2ascii_hex(map_get_player_tile
				       (x, y, plr)->last_updated, 1));
    SAVE_PLAYER_MAP_DATA(x, y, file,"player%d.map_uc%03d", plrno,
			 bin2ascii_hex(map_get_player_tile
				       (x, y, plr)->last_updated, 2));
    SAVE_PLAYER_MAP_DATA(x, y, file, "player%d.map_ud%03d", plrno,
			 bin2ascii_hex(map_get_player_tile
				       (x, y, plr)->last_updated, 3));

    if (TRUE) {
      struct dumb_city *pdcity;
      i = 0;
      
      whole_map_iterate(x, y) {
	if ((pdcity = map_get_player_tile(x, y, plr)->city)) {
	  secfile_insert_int(file, pdcity->id, "player%d.dc%d.id", plrno,
			     i);
	  do_in_native_pos(nat_x, nat_y, x, y) {
	    secfile_insert_int(file, nat_x, "player%d.dc%d.x", plrno, i);
	    secfile_insert_int(file, nat_y, "player%d.dc%d.y", plrno, i);
	  } do_in_native_pos_end;
	  secfile_insert_str(file, pdcity->name, "player%d.dc%d.name",
			     plrno, i);
	  secfile_insert_int(file, pdcity->size, "player%d.dc%d.size",
			     plrno, i);
	  secfile_insert_bool(file, pdcity->has_walls,
			     "player%d.dc%d.has_walls", plrno, i);
	  secfile_insert_bool(file, pdcity->occupied,
			      "player%d.dc%d.occupied", plrno, i);
	  secfile_insert_bool(file, pdcity->happy,
			      "player%d.dc%d.happy", plrno, i);
	  secfile_insert_bool(file, pdcity->unhappy,
			      "player%d.dc%d.unhappy", plrno, i);
	  secfile_insert_int(file, pdcity->owner, "player%d.dc%d.owner",
			     plrno, i);
	  i++;
	}
      } whole_map_iterate_end;
    }
    secfile_insert_int(file, i, "player%d.total_ncities", plrno);
  }

#define PART_SIZE (2*1024)
  if (plr->attribute_block.data) {
    char *quoted = quote_block(plr->attribute_block.data,
			       plr->attribute_block.length);
    char part[PART_SIZE + 1];
    int current_part_nr, parts;
    size_t bytes_left;

    secfile_insert_int(file, plr->attribute_block.length,
		       "player%d.attribute_v2_block_length", plrno);
    secfile_insert_int(file, strlen(quoted),
		       "player%d.attribute_v2_block_length_quoted", plrno);

    parts = (strlen(quoted) - 1) / PART_SIZE + 1;
    bytes_left = strlen(quoted);

    secfile_insert_int(file, parts,
		       "player%d.attribute_v2_block_parts", plrno);

    for (current_part_nr = 0; current_part_nr < parts; current_part_nr++) {
      size_t size_of_current_part = MIN(bytes_left, PART_SIZE);

      assert(bytes_left);

      memcpy(part, quoted + PART_SIZE * current_part_nr,
	     size_of_current_part);
      part[size_of_current_part] = 0;
      secfile_insert_str(file, part,
			 "player%d.attribute_v2_block_data.part%d", plrno,
			 current_part_nr);
      bytes_left -= size_of_current_part;
    }
    assert(bytes_left == 0);
    free(quoted);
  }
}


/***************************************************************
 Assign values to ord_city and ord_map for each unit, so the
 values can be saved.
***************************************************************/
static void calc_unit_ordering(void)
{
  int j;

  players_iterate(pplayer) {
    /* to avoid junk values for unsupported units: */
    unit_list_iterate(pplayer->units, punit) {
      punit->ord_city = 0;
    } unit_list_iterate_end;
    city_list_iterate(pplayer->cities, pcity) {
      j = 0;
      unit_list_iterate(pcity->units_supported, punit) {
	punit->ord_city = j++;
      } unit_list_iterate_end;
    } city_list_iterate_end;
  } players_iterate_end;

  whole_map_iterate(x, y) {
    j = 0;
    unit_list_iterate(map_get_tile(x, y)->units, punit) {
      punit->ord_map = j++;
    } unit_list_iterate_end;
  } whole_map_iterate_end;
}

/***************************************************************
 For each city and tile, sort unit lists according to
 ord_city and ord_map values.
***************************************************************/
static void apply_unit_ordering(void)
{
  players_iterate(pplayer) {
    city_list_iterate(pplayer->cities, pcity) {
      unit_list_sort_ord_city(&pcity->units_supported);
    }
    city_list_iterate_end;
  } players_iterate_end;

  whole_map_iterate(x, y) {
    unit_list_sort_ord_map(&map_get_tile(x, y)->units);
  } whole_map_iterate_end;
}

/***************************************************************
Old savegames have defects...
***************************************************************/
static void check_city(struct city *pcity)
{
  city_map_iterate(x, y) {
    bool res = city_can_work_tile(pcity, x, y);
    switch (pcity->city_map[x][y]) {
    case C_TILE_EMPTY:
      if (!res) {
	set_worker_city(pcity, x, y, C_TILE_UNAVAILABLE);
	freelog(LOG_DEBUG, "unavailable tile marked as empty!");
      }
      break;
    case C_TILE_WORKER:
      if (!res) {
	int map_x, map_y;
	bool is_real;

	pcity->specialists[SP_ELVIS]++;
	set_worker_city(pcity, x, y, C_TILE_UNAVAILABLE);
	freelog(LOG_DEBUG, "Worked tile was unavailable!");

	is_real = city_map_to_map(&map_x, &map_y, pcity, x, y);
	assert(is_real);

	map_city_radius_iterate(map_x, map_y, x2, y2) {
	  struct city *pcity2 = map_get_city(x2, y2);
	  if (pcity2)
	    check_city(pcity2);
	} map_city_radius_iterate_end;
      }
      break;
    case C_TILE_UNAVAILABLE:
      if (res) {
	set_worker_city(pcity, x, y, C_TILE_EMPTY);
	freelog(LOG_DEBUG, "Empty tile Marked as unavailable!");
      }
      break;
    }
  } city_map_iterate_end;

  city_refresh(pcity);
}

/***************************************************************
...
***************************************************************/
void game_load(struct section_file *file)
{
  int i, k, id;
  enum server_states tmp_server_state;
  char *savefile_options;
  const char *string;
  char** improvement_order = NULL;
  int improvement_order_size = 0;
  const char* name;

  game.version = secfile_lookup_int_default(file, 0, "game.version");
  tmp_server_state = (enum server_states)
    secfile_lookup_int_default(file, RUN_GAME_STATE, "game.server_state");

  savefile_options = secfile_lookup_str(file, "savefile.options");
  if (has_capability("improvement_order", savefile_options)) {
    improvement_order = secfile_lookup_str_vec(file, &improvement_order_size,
                                               "savefile.improvement_order");
  }

  /* we require at least version 1.9.0 */
  if (10900 > game.version) {
    freelog(LOG_FATAL,
	    _("Savegame too old, at least version 1.9.0 required."));
    exit(EXIT_FAILURE);
  }

  {
    sz_strlcpy(srvarg.metaserver_info_line,
	       secfile_lookup_str_default(file, default_meta_server_info_string(),
					  "game.metastring"));
    sz_strlcpy(srvarg.metaserver_addr,
	       secfile_lookup_str_default(file, DEFAULT_META_SERVER_ADDR,
					  "game.metaserver"));
    meta_addr_split();

    game.gold          = secfile_lookup_int(file, "game.gold");
    game.tech          = secfile_lookup_int(file, "game.tech");
    game.skill_level   = secfile_lookup_int(file, "game.skill_level");
    if (game.skill_level==0)
      game.skill_level = GAME_OLD_DEFAULT_SKILL_LEVEL;

    game.timeout       = secfile_lookup_int(file, "game.timeout");
    game.timeoutint = secfile_lookup_int_default(file,
						 GAME_DEFAULT_TIMEOUTINT,
						 "game.timeoutint");
    game.timeoutintinc =
	secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINTINC,
				   "game.timeoutintinc");
    game.timeoutinc =
	secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINC,
				   "game.timeoutinc");
    game.timeoutincmult =
	secfile_lookup_int_default(file, GAME_DEFAULT_TIMEOUTINCMULT,
				   "game.timeoutincmult");
    game.timeoutcounter =
	secfile_lookup_int_default(file, 1, "game.timeoutcounter");

    game.end_year      = secfile_lookup_int(file, "game.end_year");
    game.researchcost  = secfile_lookup_int_default(file, 0, "game.researchcost");
    if (game.researchcost == 0)
      game.researchcost = secfile_lookup_int(file, "game.techlevel");

    game.year          = secfile_lookup_int(file, "game.year");

    if (has_capability("turn", savefile_options)) {
      game.turn = secfile_lookup_int(file, "game.turn");
    } else {
      game.turn = -2;
    }

    game.min_players   = secfile_lookup_int(file, "game.min_players");
    game.max_players   = secfile_lookup_int(file, "game.max_players");
    game.nplayers      = secfile_lookup_int(file, "game.nplayers");
    game.globalwarming = secfile_lookup_int(file, "game.globalwarming");
    game.warminglevel  = secfile_lookup_int(file, "game.warminglevel");
    game.nuclearwinter = secfile_lookup_int_default(file, 0, "game.nuclearwinter");
    game.coolinglevel  = secfile_lookup_int_default(file, 8, "game.coolinglevel");
    game.notradesize   = secfile_lookup_int_default(file, 0, "game.notradesize");
    game.fulltradesize = secfile_lookup_int_default(file, 1, "game.fulltradesize");
    game.unhappysize   = secfile_lookup_int(file, "game.unhappysize");
    game.angrycitizen  = secfile_lookup_bool_default(file, FALSE, "game.angrycitizen");

    if (game.version >= 10100) {
      game.cityfactor  = secfile_lookup_int(file, "game.cityfactor");
      game.diplcost    = secfile_lookup_int(file, "game.diplcost");
      game.freecost    = secfile_lookup_int(file, "game.freecost");
      game.conquercost = secfile_lookup_int(file, "game.conquercost");
      game.foodbox     = secfile_lookup_int(file, "game.foodbox");
      game.techpenalty = secfile_lookup_int(file, "game.techpenalty");
      game.razechance  = secfile_lookup_int(file, "game.razechance");

      /* suppress warnings about unused entries in old savegames: */
      (void) section_file_lookup(file, "game.rail_food");
      (void) section_file_lookup(file, "game.rail_prod");
      (void) section_file_lookup(file, "game.rail_trade");
      (void) section_file_lookup(file, "game.farmfood");
    }
    if (game.version >= 10300) {
      game.civstyle = secfile_lookup_int_default(file, 0, "game.civstyle");
      game.save_nturns = secfile_lookup_int(file, "game.save_nturns");
    }

    game.citymindist  = secfile_lookup_int_default(file,
      GAME_DEFAULT_CITYMINDIST, "game.citymindist");

    game.rapturedelay  = secfile_lookup_int_default(file,
      GAME_DEFAULT_RAPTUREDELAY, "game.rapturedelay");

    /* National borders setting. */
    game.borders = secfile_lookup_int_default(file, 0, "game.borders");
    game.happyborders = secfile_lookup_bool_default(file, FALSE, 
						    "game.happyborders");

    /* Diplomacy. */
    game.diplomacy = secfile_lookup_int_default(file, GAME_DEFAULT_DIPLOMACY, 
                                                "game.diplomacy");

    if (has_capability("watchtower", savefile_options)) {
      game.watchtower_extra_vision =
	  secfile_lookup_int(file, "game.watchtower_extra_vision");
      game.watchtower_vision =
	  secfile_lookup_int(file, "game.watchtower_vision");
    } else {
      game.watchtower_extra_vision = 0;
      game.watchtower_vision = 1;
    }

    sz_strlcpy(game.save_name,
	       secfile_lookup_str_default(file, GAME_DEFAULT_SAVE_NAME,
					  "game.save_name"));

    game.aifill = secfile_lookup_int_default(file, 0, "game.aifill");

    game.scorelog = secfile_lookup_bool_default(file, FALSE, "game.scorelog");
    sz_strlcpy(game.id, secfile_lookup_str_default(file, "", "game.id"));

    game.fogofwar = secfile_lookup_bool_default(file, FALSE, "game.fogofwar");
    game.fogofwar_old = game.fogofwar;
  
    game.civilwarsize =
      secfile_lookup_int_default(file, GAME_DEFAULT_CIVILWARSIZE,
				 "game.civilwarsize");
    game.contactturns =
      secfile_lookup_int_default(file, GAME_DEFAULT_CONTACTTURNS,
				 "game.contactturns");
  
    if(has_capability("diplchance_percent", savefile_options)) {
      game.diplchance = secfile_lookup_int_default(file, game.diplchance,
						   "game.diplchance");
    } else {
      game.diplchance = secfile_lookup_int_default(file, 3, /* old default */
						   "game.diplchance");
      if (game.diplchance < 2) {
	game.diplchance = GAME_MAX_DIPLCHANCE;
      } else if (game.diplchance > 10) {
	game.diplchance = GAME_MIN_DIPLCHANCE;
      } else {
	game.diplchance = 100 - (10 * (game.diplchance - 1));
      }
    }
    game.aqueductloss = secfile_lookup_int_default(file, game.aqueductloss,
						   "game.aqueductloss");
    game.killcitizen = secfile_lookup_int_default(file, game.killcitizen,
						  "game.killcitizen");
    game.savepalace = secfile_lookup_bool_default(file,game.savepalace,
						"game.savepalace");
    game.turnblock = secfile_lookup_bool_default(file,game.turnblock,
						"game.turnblock");
    game.fixedlength = secfile_lookup_bool_default(file,game.fixedlength,
						  "game.fixedlength");
    game.barbarianrate = secfile_lookup_int_default(file, game.barbarianrate,
						    "game.barbarians");
    game.onsetbarbarian = secfile_lookup_int_default(file, game.onsetbarbarian,
						     "game.onsetbarbs");
    game.revolution_length
      = secfile_lookup_int_default(file, game.revolution_length,
				   "game.revolen");
    game.nbarbarians = 0; /* counted in player_load for compatibility with 
			     1.10.0 */
    game.occupychance = secfile_lookup_int_default(file, game.occupychance,
						   "game.occupychance");
    game.randseed = secfile_lookup_int_default(file, game.randseed,
					       "game.randseed");
    game.allowed_city_names =
	secfile_lookup_int_default(file, game.allowed_city_names,
				   "game.allowed_city_names"); 

    if(game.civstyle == 1) {
      string = "civ1";
    } else {
      string = "default";
      game.civstyle = GAME_DEFAULT_CIVSTYLE;
    }

    if (!has_capability("rulesetdir", savefile_options)) {
      char *str2, *str =
	  secfile_lookup_str_default(file, "default", "game.ruleset.techs");

      if (strcmp("classic",
		 secfile_lookup_str_default(file, "default",
					    "game.ruleset.terrain")) == 0) {
	freelog(LOG_FATAL, _("The savegame uses the classic terrain "
			     "ruleset which is no longer supported."));
	exit(EXIT_FAILURE);
      }


#define T(x) \
      str2 = secfile_lookup_str_default(file, "default", x); \
      if (strcmp(str, str2) != 0) { \
	freelog(LOG_NORMAL, _("Warning: Different rulesetdirs " \
			      "('%s' and '%s') are no longer supported. " \
			      "Using '%s'."), \
			      str, str2, str); \
      }

      T("game.ruleset.units");
      T("game.ruleset.buildings");
      T("game.ruleset.terrain");
      T("game.ruleset.governments");
      T("game.ruleset.nations");
      T("game.ruleset.cities");
      T("game.ruleset.game");
#undef T

      sz_strlcpy(game.rulesetdir, str);
    } else {
      sz_strlcpy(game.rulesetdir, 
	       secfile_lookup_str_default(file, string,
					  "game.rulesetdir"));
    }

    sz_strlcpy(game.demography,
	       secfile_lookup_str_default(file, GAME_DEFAULT_DEMOGRAPHY,
					  "game.demography"));

    game.spacerace = secfile_lookup_bool_default(file, game.spacerace,
						"game.spacerace");

    game.auto_ai_toggle = secfile_lookup_bool_default(file, game.auto_ai_toggle,
						     "game.auto_ai_toggle");

    game.heating=0;
    game.cooling=0;

    load_rulesets();
  }

  {
    if (game.version >= 10300) {
      {
	if (!has_capability("startunits", savefile_options)) {
	  int settlers = secfile_lookup_int(file, "game.settlers");
	  int explorer = secfile_lookup_int(file, "game.explorer");
	  int i;
	  for (i = 0; settlers>0; i++, settlers--) {
	    game.start_units[i] = 'c';
	  }
	  for (; explorer>0; i++, explorer--) {
	    game.start_units[i] = 'x';
	  }
	  game.start_units[i] = '\0';
	} else {
	  secfile_lookup_str(file, "game.start_units");
	}
	game.dispersion =
	  secfile_lookup_int_default(file, GAME_DEFAULT_DISPERSION, "game.dispersion");
      }

      map.riches = secfile_lookup_int(file, "map.riches");
      map.huts = secfile_lookup_int(file, "map.huts");
      map.generator = secfile_lookup_int(file, "map.generator");
      map.seed = secfile_lookup_int(file, "map.seed");
      map.landpercent = secfile_lookup_int(file, "map.landpercent");
      map.grasssize =
	secfile_lookup_int_default(file, MAP_DEFAULT_GRASS, "map.grasssize");
      map.swampsize = secfile_lookup_int(file, "map.swampsize");
      map.deserts = secfile_lookup_int(file, "map.deserts");
      map.riverlength = secfile_lookup_int(file, "map.riverlength");
      map.mountains = secfile_lookup_int(file, "map.mountains");
      map.forestsize = secfile_lookup_int(file, "map.forestsize");
      map.have_huts = secfile_lookup_bool_default(file, TRUE, "map.have_huts");

      if (has_capability("startoptions", savefile_options)) {
	map.xsize = secfile_lookup_int(file, "map.width");
	map.ysize = secfile_lookup_int(file, "map.height");
      } else {
	/* old versions saved with these names in PRE_GAME_STATE: */
	map.xsize = secfile_lookup_int(file, "map.xsize");
	map.ysize = secfile_lookup_int(file, "map.ysize");
      }

      if (tmp_server_state==PRE_GAME_STATE
	  && map.generator == 0
	  && !has_capability("map_editor",savefile_options)) {
	/* generator 0 = map done with map editor */
	/* aka a "scenario" */
        if (has_capability("specials",savefile_options)) {
          map_load(file);
          return;
        }
        map_tiles_load(file);
        if (has_capability("riversoverlay",savefile_options)) {
	  map_rivers_overlay_load(file);
	}
        if (has_capability("startpos",savefile_options)) {
          map_startpos_load(file);
          return;
        }
	return;
      }
    }
    if(tmp_server_state==PRE_GAME_STATE) {
      return;
    }
  }

  /* We check
     1) if the block exists at all.
     2) if it is saved. */
  if (section_file_lookup(file, "random.index_J")
      && secfile_lookup_bool_default(file, TRUE, "game.save_random")) {
    RANDOM_STATE rstate;
    rstate.j = secfile_lookup_int(file,"random.index_J");
    rstate.k = secfile_lookup_int(file,"random.index_K");
    rstate.x = secfile_lookup_int(file,"random.index_X");
    for(i=0;i<8;i++) {
      char name[20];
      my_snprintf(name, sizeof(name), "random.table%d",i);
      string=secfile_lookup_str(file,name);
      sscanf(string,"%8x %8x %8x %8x %8x %8x %8x", &rstate.v[7*i],
	     &rstate.v[7*i+1], &rstate.v[7*i+2], &rstate.v[7*i+3],
	     &rstate.v[7*i+4], &rstate.v[7*i+5], &rstate.v[7*i+6]);
    }
    rstate.is_init = TRUE;
    set_myrand_state(rstate);
  } else {
    /* mark it */
    (void) secfile_lookup_bool_default(file, TRUE, "game.save_random");
  }


  game.is_new_game = !secfile_lookup_bool_default(file, TRUE,
						  "game.save_players");

  if (!game.is_new_game) { /* If new game, this is done in srv_main.c */
    /* Initialise lists of improvements with World and Island equiv_ranges */
    improvement_status_init(game.improvements,
			    ARRAY_SIZE(game.improvements));
  }

  map_load(file);

  if (!game.is_new_game) {
    /* destroyed wonders: */
    string = secfile_lookup_str_default(file, NULL,
                                        "game.destroyed_wonders_new");
    if (!string) {
      /* old savegames */
      string = secfile_lookup_str_default(file, "",
                                          "game.destroyed_wonders");
      for (k = 0; string[k]; k++) {
        if (string[k] == '1') {
	  name = old_impr_type_name(k);
	  id = find_improvement_by_name_orig(name);
	  if (id != -1) {
	    game.global_wonders[id] = -1;
	  }
	}
      }
    } else {
      for (k = 0; k < improvement_order_size && string[k]; k++) {
        if (string[k] == '1') {
	  id = find_improvement_by_name_orig(improvement_order[k]);
	  if (id != -1) {
            game.global_wonders[id] = -1;
	  }
	}
      }
    }

    /* This is done after continents are assigned, but before effects 
     * are added. */
    allot_island_improvs();

    for (i = 0; i < game.nplayers; i++) {
      player_load(&game.players[i], i, file, improvement_order,
                  improvement_order_size); 
    }

    cities_iterate(pcity) {
      /* Update all city information.  This must come after all cities are
       * loaded (in player_load) but before player (dumb) cities are loaded
       * (in player_map_load). */
      generic_city_refresh(pcity, FALSE, NULL);
    } cities_iterate_end;

    /* Since the cities must be placed on the map to put them on the
       player map we do this afterwards */
    for(i=0; i<game.nplayers; i++) {
      player_map_load(&game.players[i], i, file); 
    }

    /* We do this here since if the did it in player_load, player 1
       would try to unfog (unloaded) player 2's map when player 1's units
       were loaded */
    players_iterate(pplayer) {
      pplayer->really_gives_vision = 0;
      pplayer->gives_shared_vision = 0;
    } players_iterate_end;
    players_iterate(pplayer) {
      char *vision;
      int plrno = pplayer->player_no;

      vision = secfile_lookup_str_default(file, NULL,
					  "player%d.gives_shared_vision",
					  plrno);
      if (vision) {
	players_iterate(pplayer2) {
	  if (vision[pplayer2->player_no] == '1') {
	    give_shared_vision(pplayer, pplayer2);
	  }
	} players_iterate_end;
      }
    } players_iterate_end;

    initialize_globals();
    apply_unit_ordering();

    /* Rebuild national borders. */
    map_calculate_borders();

    /* Make sure everything is consistent. */
    players_iterate(pplayer) {
      unit_list_iterate(pplayer->units, punit) {
	if (!can_unit_continue_current_activity(punit)) {
	  freelog(LOG_ERROR, "ERROR: Unit doing illegal activity in savegame!");
	  punit->activity = ACTIVITY_IDLE;
	}
      } unit_list_iterate_end;

      city_list_iterate(pplayer->cities, pcity) {
	check_city(pcity);
      } city_list_iterate_end;
    } players_iterate_end;
  } else {
    game.nplayers = 0;
  }

  if (secfile_lookup_int_default(file, -1,
				 "game.shuffled_player_%d", 0) >= 0) {
    int shuffled_players[game.nplayers];

    for (i = 0; i < game.nplayers; i++) {
      shuffled_players[i]
	= secfile_lookup_int(file, "game.shuffled_player_%d", i);
    }
    set_shuffled_players(shuffled_players);
  } else {
    /* No shuffled players included, so shuffle them (this may include
     * scenarios). */
    shuffle_players();
  }

  if (!game.is_new_game) {
    /* Set active city improvements/wonders and their effects */
    improvements_update_obsolete();
  }

  game.player_idx=0;
  game.player_ptr=&game.players[0];  

  /* Fix ferrying sanity */
  players_iterate(pplayer) {
    unit_list_iterate_safe(pplayer->units, punit) {
      struct unit *ferry = find_unit_by_id(punit->transported_by);

      if (is_ocean(map_get_terrain(punit->x, punit->y))
          && is_ground_unit(punit) && !ferry) {
        freelog(LOG_ERROR, "Removing %s's unferried %s in ocean at (%d, %d)",
                pplayer->name, unit_name(punit->type), punit->x, punit->y);
        bounce_unit(punit, TRUE);
      }
    } unit_list_iterate_safe_end;
  } players_iterate_end;

  return;
}

/***************************************************************
...
***************************************************************/
void game_save(struct section_file *file)
{
  int i;
  int version;
  char options[512];
  char temp[B_LAST+1];

  version = MAJOR_VERSION *10000 + MINOR_VERSION *100 + PATCH_VERSION; 
  secfile_insert_int(file, version, "game.version");

  /* Game state: once the game is no longer a new game (ie, has been
   * started the first time), it should always be considered a running
   * game for savegame purposes:
   */
  secfile_insert_int(file, (int) (game.is_new_game ? server_state :
				  RUN_GAME_STATE), "game.server_state");
  
  secfile_insert_str(file, srvarg.metaserver_info_line, "game.metastring");
  secfile_insert_str(file, meta_addr_port(), "game.metaserver");
  
  sz_strlcpy(options, SAVEFILE_OPTIONS);
  if (game.is_new_game) {
    if (map.num_start_positions>0) {
      sz_strlcat(options, " startpos");
    }
    if (map.have_specials) {
      sz_strlcat(options, " specials");
    }
    if (map.have_rivers_overlay && !map.have_specials) {
      sz_strlcat(options, " riversoverlay");
    }
  }
  secfile_insert_str(file, options, "savefile.options");
  /* Save improvement order in savegame, so we are not dependent on
   * ruleset order.
   * If the game isn't started improvements aren't loaded
   * so we can not save the order.
   */
  if (game.num_impr_types > 0) {
    const char* buf[game.num_impr_types];
    impr_type_iterate(id) {
      buf[id] = get_improvement_name_orig(id);
    } impr_type_iterate_end;
    secfile_insert_str_vec(file, buf, game.num_impr_types,
                           "savefile.improvement_order");
  }
  secfile_insert_int(file, game.gold, "game.gold");
  secfile_insert_int(file, game.tech, "game.tech");
  secfile_insert_int(file, game.skill_level, "game.skill_level");
  secfile_insert_int(file, game.timeout, "game.timeout");
  secfile_insert_int(file, game.timeoutint, "game.timeoutint");
  secfile_insert_int(file, game.timeoutintinc, "game.timeoutintinc");
  secfile_insert_int(file, game.timeoutinc, "game.timeoutinc");
  secfile_insert_int(file, game.timeoutincmult, "game.timeoutincmult"); 
  secfile_insert_int(file, game.timeoutcounter, "game.timeoutcounter"); 
  secfile_insert_int(file, game.end_year, "game.end_year");
  secfile_insert_int(file, game.year, "game.year");
  secfile_insert_int(file, game.turn, "game.turn");
  secfile_insert_int(file, game.researchcost, "game.researchcost");
  secfile_insert_int(file, game.min_players, "game.min_players");
  secfile_insert_int(file, game.max_players, "game.max_players");
  secfile_insert_int(file, game.nplayers, "game.nplayers");
  secfile_insert_int(file, game.globalwarming, "game.globalwarming");
  secfile_insert_int(file, game.warminglevel, "game.warminglevel");
  secfile_insert_int(file, game.nuclearwinter, "game.nuclearwinter");
  secfile_insert_int(file, game.coolinglevel, "game.coolinglevel");
  secfile_insert_int(file, game.notradesize, "game.notradesize");
  secfile_insert_int(file, game.fulltradesize, "game.fulltradesize");
  secfile_insert_int(file, game.unhappysize, "game.unhappysize");
  secfile_insert_bool(file, game.angrycitizen, "game.angrycitizen");
  secfile_insert_int(file, game.cityfactor, "game.cityfactor");
  secfile_insert_int(file, game.citymindist, "game.citymindist");
  secfile_insert_int(file, game.civilwarsize, "game.civilwarsize");
  secfile_insert_int(file, game.contactturns, "game.contactturns");
  secfile_insert_int(file, game.rapturedelay, "game.rapturedelay");
  secfile_insert_int(file, game.diplcost, "game.diplcost");
  secfile_insert_int(file, game.freecost, "game.freecost");
  secfile_insert_int(file, game.conquercost, "game.conquercost");
  secfile_insert_int(file, game.foodbox, "game.foodbox");
  secfile_insert_int(file, game.techpenalty, "game.techpenalty");
  secfile_insert_int(file, game.razechance, "game.razechance");
  secfile_insert_int(file, game.civstyle, "game.civstyle");
  secfile_insert_int(file, game.save_nturns, "game.save_nturns");
  secfile_insert_str(file, game.save_name, "game.save_name");
  secfile_insert_int(file, game.aifill, "game.aifill");
  secfile_insert_bool(file, game.scorelog, "game.scorelog");
  secfile_insert_str(file, game.id, "game.id");
  secfile_insert_bool(file, game.fogofwar, "game.fogofwar");
  secfile_insert_bool(file, game.spacerace, "game.spacerace");
  secfile_insert_bool(file, game.auto_ai_toggle, "game.auto_ai_toggle");
  secfile_insert_int(file, game.diplchance, "game.diplchance");
  secfile_insert_int(file, game.aqueductloss, "game.aqueductloss");
  secfile_insert_int(file, game.killcitizen, "game.killcitizen");
  secfile_insert_bool(file, game.turnblock, "game.turnblock");
  secfile_insert_bool(file, game.savepalace, "game.savepalace");
  secfile_insert_bool(file, game.fixedlength, "game.fixedlength");
  secfile_insert_int(file, game.barbarianrate, "game.barbarians");
  secfile_insert_int(file, game.onsetbarbarian, "game.onsetbarbs");
  secfile_insert_int(file, game.revolution_length, "game.revolen");
  secfile_insert_int(file, game.occupychance, "game.occupychance");
  secfile_insert_str(file, game.demography, "game.demography");
  secfile_insert_int(file, game.borders, "game.borders");
  secfile_insert_bool(file, game.happyborders, "game.happyborders");
  secfile_insert_int(file, game.diplomacy, "game.diplomacy");
  secfile_insert_int(file, game.watchtower_vision, "game.watchtower_vision");
  secfile_insert_int(file, game.watchtower_extra_vision, "game.watchtower_extra_vision");
  secfile_insert_int(file, game.allowed_city_names, "game.allowed_city_names");

  /* old (1.14.1) servers need to have these server variables.  The values
   * don't matter, though. */
  secfile_insert_int(file, 2, "game.settlers");
  secfile_insert_int(file, 1, "game.explorer");

  if (TRUE) {
    /* Now always save these, so the server options reflect the
     * actual values used at the start of the game.
     * The first two used to be saved as "map.xsize" and "map.ysize"
     * when PRE_GAME_STATE, but I'm standardizing on width,height --dwp
     */
    secfile_insert_int(file, map.topology_id, "map.topology_id");
    secfile_insert_int(file, map.xsize, "map.width");
    secfile_insert_int(file, map.ysize, "map.height");
    secfile_insert_str(file, game.start_units, "game.start_units");
    secfile_insert_int(file, game.dispersion, "game.dispersion");
    secfile_insert_int(file, map.seed, "map.seed");
    secfile_insert_int(file, map.landpercent, "map.landpercent");
    secfile_insert_int(file, map.riches, "map.riches");
    secfile_insert_int(file, map.swampsize, "map.swampsize");
    secfile_insert_int(file, map.deserts, "map.deserts");
    secfile_insert_int(file, map.riverlength, "map.riverlength");
    secfile_insert_int(file, map.mountains, "map.mountains");
    secfile_insert_int(file, map.forestsize, "map.forestsize");
    secfile_insert_int(file, map.huts, "map.huts");
    secfile_insert_int(file, map.generator, "map.generator");
    secfile_insert_bool(file, map.have_huts, "map.have_huts");
  } 

  secfile_insert_int(file, game.randseed, "game.randseed");
  
  if (myrand_is_init() && game.save_options.save_random) {
    RANDOM_STATE rstate = get_myrand_state();
    secfile_insert_int(file, 1, "game.save_random");
    assert(rstate.is_init);

    secfile_insert_int(file, rstate.j, "random.index_J");
    secfile_insert_int(file, rstate.k, "random.index_K");
    secfile_insert_int(file, rstate.x, "random.index_X");

    for (i = 0; i < 8; i++) {
      char name[20], vec[100];

      my_snprintf(name, sizeof(name), "random.table%d", i);
      my_snprintf(vec, sizeof(vec),
		  "%8x %8x %8x %8x %8x %8x %8x", rstate.v[7 * i],
		  rstate.v[7 * i + 1], rstate.v[7 * i + 2],
		  rstate.v[7 * i + 3], rstate.v[7 * i + 4],
		  rstate.v[7 * i + 5], rstate.v[7 * i + 6]);
      secfile_insert_str(file, vec, name);
    }
  } else {
    secfile_insert_int(file, 0, "game.save_random");
  }

  secfile_insert_str(file, game.rulesetdir, "game.rulesetdir");

  if (!map_is_empty()) {
    map_save(file);
  }
  
  if ((server_state == PRE_GAME_STATE) && game.is_new_game) {
    return; /* want to save scenarios as well */
  }

  secfile_insert_bool(file, game.save_options.save_players,
		      "game.save_players");
  if (game.save_options.save_players) {
    /* 1.14 servers depend on improvement order in ruleset. Here we
     * are trying to simulate 1.14.1 default order
     */
    init_old_improvement_bitvector(temp);
    impr_type_iterate(id) {
      if (is_wonder(id) && game.global_wonders[id] != 0
	  && !find_city_by_id(game.global_wonders[id])) {
        add_improvement_into_old_bitvector(temp, id);
      } 
    } impr_type_iterate_end;
    secfile_insert_str(file, temp, "game.destroyed_wonders");
    
    /* Save destroyed wonders as bitvector. Note that improvement order
     * is saved in savefile.improvement_order
     */
    impr_type_iterate(id) {
      if (is_wonder(id) && game.global_wonders[id] != 0
	  && !find_city_by_id(game.global_wonders[id])) {
	temp[id] = '1';
      } else {
        temp[id] = '0';
      }
    } impr_type_iterate_end;
    temp[game.num_impr_types] = '\0';
    secfile_insert_str(file, temp, "game.destroyed_wonders_new");

    calc_unit_ordering();

    players_iterate(pplayer) {
      player_save(pplayer, pplayer->player_no, file);
    } players_iterate_end;

    for (i = 0; i < game.nplayers; i++) {
      secfile_insert_int(file, shuffled_player(i)->player_no,
			 "game.shuffled_player_%d", i);
    }
  }
}
