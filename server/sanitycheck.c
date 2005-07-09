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

#include "city.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "player.h"
#include "terrain.h"
#include "unit.h"

#include "citytools.h"
#include "maphand.h"
#include "sanitycheck.h"
#include "unittools.h"

#ifdef SANITY_CHECKING

#ifdef DEBUG
#  define DEBUG_ASSERT(x) assert(x)
#else
#  define DEBUG_ASSERT(x) (void)0
#endif /* DEBUG */

#define SANITY_CHECK(x)							\
  do {									\
    if (!(x)) {								\
      freelog(LOG_ERROR, "Failed sanity check: %s (%s:%d)",		\
	      #x, __FILE__,__LINE__);					\
    }									\
    DEBUG_ASSERT(x);							\
  } while(0)


/**************************************************************************
...
**************************************************************************/
static void check_specials(void)
{
  whole_map_iterate(ptile) {
    Terrain_type_id terrain = tile_get_terrain(ptile);
    bv_special special = tile_get_special(ptile);

    if (contains_special(special, S_RAILROAD))
      SANITY_CHECK(contains_special(special, S_ROAD));
    if (contains_special(special, S_FARMLAND))
      SANITY_CHECK(contains_special(special, S_IRRIGATION));
    if (contains_special(special, S_SPECIAL_1))
      SANITY_CHECK(!contains_special(special,  S_SPECIAL_2));

    if (contains_special(special, S_MINE))
      SANITY_CHECK(get_terrain(terrain)->mining_result == terrain);
    if (contains_special(special, S_IRRIGATION))
      SANITY_CHECK(get_terrain(terrain)->irrigation_result == terrain);

    SANITY_CHECK(terrain >= T_FIRST && terrain < T_COUNT);
  } whole_map_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
static void check_fow(void)
{
  whole_map_iterate(ptile) {
    players_iterate(pplayer) {
      struct player_tile *plr_tile = map_get_player_tile(ptile, pplayer);
      /* underflow of unsigned int */
      SANITY_CHECK(plr_tile->seen_count < 60000);
      SANITY_CHECK(plr_tile->own_seen < 60000);
      SANITY_CHECK(plr_tile->pending_seen < 60000);

      if (plr_tile->seen_count > 0) {
	SANITY_CHECK(BV_ISSET(ptile->tile_seen, pplayer->player_no));
      } else {
	SANITY_CHECK(!BV_ISSET(ptile->tile_seen, pplayer->player_no));
      }

      SANITY_CHECK(plr_tile->own_seen <= plr_tile->seen_count);
      if (map_is_known(ptile, pplayer)) {
	SANITY_CHECK(plr_tile->pending_seen == 0);
      }
    } players_iterate_end;
  } whole_map_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
static void check_misc(void)
{
  int nbarbs = 0;
  players_iterate(pplayer) {
    if (is_barbarian(pplayer)) {
      nbarbs++;
    }
  } players_iterate_end;
  SANITY_CHECK(nbarbs == game.info.nbarbarians);

  SANITY_CHECK(game.info.nplayers <= MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS);
}

/**************************************************************************
...
**************************************************************************/
static void check_map(void)
{
  whole_map_iterate(ptile) {
    struct city *pcity = tile_get_city(ptile);
    int cont = tile_get_continent(ptile), x, y;

    CHECK_INDEX(ptile->index);
    CHECK_MAP_POS(ptile->x, ptile->y);
    CHECK_NATIVE_POS(ptile->nat_x, ptile->nat_y);

    index_to_map_pos(&x, &y, ptile->index);
    SANITY_CHECK(x == ptile->x && y == ptile->y);

    index_to_native_pos(&x, &y, ptile->index);
    SANITY_CHECK(x == ptile->nat_x && y == ptile->nat_y);

    if (is_ocean(tile_get_terrain(ptile))) {
      SANITY_CHECK(cont < 0);
      adjc_iterate(ptile, tile1) {
	if (is_ocean(tile_get_terrain(tile1))) {
	  SANITY_CHECK(tile_get_continent(tile1) == cont);
	}
      } adjc_iterate_end;
    } else {
      SANITY_CHECK(cont > 0);
      adjc_iterate(ptile, tile1) {
	if (!is_ocean(tile_get_terrain(tile1))) {
	  SANITY_CHECK(tile_get_continent(tile1) == cont);
	}
      } adjc_iterate_end;
    }

    if (pcity) {
      SANITY_CHECK(same_pos(pcity->tile, ptile));
    }

    unit_list_iterate(ptile->units, punit) {
      SANITY_CHECK(same_pos(punit->tile, ptile));

      /* Check diplomatic status of stacked units. */
      unit_list_iterate(ptile->units, punit2) {
	SANITY_CHECK(pplayers_allied(unit_owner(punit), unit_owner(punit2)));
      } unit_list_iterate_end;
      if (pcity) {
	SANITY_CHECK(pplayers_allied(unit_owner(punit), city_owner(pcity)));
      }
    } unit_list_iterate_end;
  } whole_map_iterate_end;
}

/**************************************************************************
  Verify that the city has sane values.
**************************************************************************/
void real_sanity_check_city(struct city *pcity, const char *file, int line)
{
  int workers = 0;
  struct player *pplayer = city_owner(pcity);

  SANITY_CHECK(pcity->size >= 1);
  SANITY_CHECK(!terrain_has_flag(tile_get_terrain(pcity->tile),
			   TER_NO_CITIES));

  unit_list_iterate(pcity->units_supported, punit) {
    SANITY_CHECK(punit->homecity == pcity->id);
    SANITY_CHECK(unit_owner(punit) == pplayer);
  } unit_list_iterate_end;

  /* Note that cities may be found on land or water. */

  city_map_iterate(x, y) {
    struct tile *ptile;

    if ((ptile = city_map_to_map(pcity, x, y))) {
      struct player *owner = tile_get_owner(ptile);

      switch (get_worker_city(pcity, x, y)) {
      case C_TILE_EMPTY:
	if (ptile->worked) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "empty but worked by %s!",
		  pcity->name, TILE_XY(ptile),
		  (ptile)->worked->name);
	}
	if (is_enemy_unit_tile(ptile, pplayer)) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "empty but occupied by an enemy unit!",
		  pcity->name, TILE_XY(ptile));
	}
	if (game.info.borders > 0 && owner && owner != pcity->owner) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "empty but in enemy territory!",
		  pcity->name, TILE_XY(ptile));
	}
	if (!city_can_work_tile(pcity, x, y)) {
	  /* Complete check. */
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "empty but is unavailable!",
		  pcity->name, TILE_XY(ptile));
	}
	break;
      case C_TILE_WORKER:
	if ((ptile)->worked != pcity) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "worked but main map disagrees!",
		  pcity->name, TILE_XY(ptile));
	}
	if (is_enemy_unit_tile(ptile, pplayer)) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "worked but occupied by an enemy unit!",
		  pcity->name, TILE_XY(ptile));
	}
	if (game.info.borders > 0 && owner && owner != pcity->owner) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "worked but in enemy territory!",
		  pcity->name, TILE_XY(ptile));
	}
	if (!city_can_work_tile(pcity, x, y)) {
	  /* Complete check. */
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "worked but is unavailable!",
		  pcity->name, TILE_XY(ptile));
	}
	break;
      case C_TILE_UNAVAILABLE:
	if (city_can_work_tile(pcity, x, y)) {
	  freelog(LOG_ERROR, "Tile at %s->%d,%d marked as "
		  "unavailable but seems to be available!",
		  pcity->name, TILE_XY(ptile));
	}
	break;
      }
    } else {
      SANITY_CHECK(get_worker_city(pcity, x, y) == C_TILE_UNAVAILABLE);
    }
  } city_map_iterate_end;

  /* Sanity check city size versus worker and specialist counts. */
  city_map_iterate(x, y) {
    if (get_worker_city(pcity, x, y) == C_TILE_WORKER) {
      workers++;
    }
  } city_map_iterate_end;
  if (workers + city_specialists(pcity) != pcity->size + 1) {
    int diff = pcity->size + 1 - workers - city_specialists(pcity);

    SANITY_CHECK(workers + city_specialists(pcity) == pcity->size + 1);
    if (diff > 0) {
      pcity->specialists[DEFAULT_SPECIALIST] += diff;
    } else if (diff < 0) {
      specialist_type_iterate(sp) {
	int num = MIN(-diff, pcity->specialists[sp]);

	diff += num;
	pcity->specialists[sp] -= num;
      } specialist_type_iterate_end;

      if (diff < 0) {
	city_map_checked_iterate(pcity->tile, city_x, city_y, ptile) {
	  if (ptile->worked == pcity && diff < 0) {
	    server_remove_worker_city(pcity, city_x, city_y);
	    diff++;
	  }
	} city_map_checked_iterate_end;
      }
    }

    generic_city_refresh(pcity, TRUE, NULL);
  }
}

/**************************************************************************
...
**************************************************************************/
static void check_cities(void)
{
  players_iterate(pplayer) {
    city_list_iterate(pplayer->cities, pcity) {
      SANITY_CHECK(city_owner(pcity) == pplayer);

      sanity_check_city(pcity);
    } city_list_iterate_end;
  } players_iterate_end;

  whole_map_iterate(ptile) {
    if (ptile->worked) {
      struct city *pcity = ptile->worked;
      int city_x, city_y;
      bool is_valid;

      is_valid = map_to_city_map(&city_x, &city_y, pcity, ptile);
      SANITY_CHECK(is_valid);

      if (pcity->city_map[city_x][city_y] != C_TILE_WORKER) {
	freelog(LOG_ERROR, "%d,%d is listed as being worked by %s "
		"on the map, but %s lists the tile %d,%d as having "
		"status %d\n",
		TILE_XY(ptile), pcity->name, pcity->name, city_x, city_y,
		pcity->city_map[city_x][city_y]);
      }
    }
  } whole_map_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
static void check_units(void) {
  players_iterate(pplayer) {
    unit_list_iterate(pplayer->units, punit) {
      struct tile *ptile = punit->tile;
      struct city *pcity;
      struct unit *transporter = NULL, *transporter2 = NULL;

      SANITY_CHECK(unit_owner(punit) == pplayer);

      if (punit->homecity != 0) {
	pcity = player_find_city_by_id(pplayer, punit->homecity);
	SANITY_CHECK(pcity != NULL);
	SANITY_CHECK(city_owner(pcity) == pplayer);
      }

      if (!can_unit_continue_current_activity(punit)) {
	freelog(LOG_ERROR, "%s at %d,%d (%s) has activity %s, "
		"which it can't continue!",
		unit_type(punit)->name,
		TILE_XY(ptile), tile_get_info_text(ptile),
		get_activity_text(punit->activity));
      }

      pcity = tile_get_city(ptile);
      if (pcity) {
	SANITY_CHECK(pplayers_allied(city_owner(pcity), pplayer));
      }

      SANITY_CHECK(punit->moves_left >= 0);
      SANITY_CHECK(punit->hp > 0);

      if (punit->transported_by != -1) {
        transporter = find_unit_by_id(punit->transported_by);
        SANITY_CHECK(transporter != NULL);

	/* Make sure the transporter is on the tile. */
	unit_list_iterate(punit->tile->units, tile_unit) {
	  if (tile_unit == transporter) {
	    transporter2 = tile_unit;
	  }
	} unit_list_iterate_end;
	SANITY_CHECK(transporter2 != NULL);

        /* Also in the list of owner? */
        SANITY_CHECK(player_find_unit_by_id(transporter->owner,
				      punit->transported_by) != NULL);
        SANITY_CHECK(same_pos(ptile, transporter->tile));

        /* Transporter capacity will be checked when transporter itself
	 * is checked */
      }

      /* Check for ground units in the ocean. */
      if (!can_unit_exist_at_tile(punit, ptile)) {
        SANITY_CHECK(punit->transported_by != -1);
        SANITY_CHECK(can_unit_transport(transporter, punit));
      }

      /* Check for over-full transports. */
      SANITY_CHECK(get_transporter_occupancy(punit)
	     <= get_transporter_capacity(punit));
    } unit_list_iterate_end;
  } players_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
static void check_players(void)
{
  int player_no;

  players_iterate(pplayer) {
    int found_palace = 0;

    if (!pplayer->is_alive) {
      /* Don't do these checks.  Note there are some dead-players
       * sanity checks below. */
      continue;
    }

    city_list_iterate(pplayer->cities, pcity) {
      if (is_capital(pcity)) {
	found_palace++;
      }
      SANITY_CHECK(found_palace <= 1);
    } city_list_iterate_end;

    players_iterate(pplayer2) {
      SANITY_CHECK(pplayer->diplstates[pplayer2->player_no].type
	     == pplayer2->diplstates[pplayer->player_no].type);
      if (pplayer->diplstates[pplayer2->player_no].type == DS_CEASEFIRE) {
	SANITY_CHECK(pplayer->diplstates[pplayer2->player_no].turns_left
	       == pplayer2->diplstates[pplayer->player_no].turns_left);
      }
      if (pplayers_allied(pplayer, pplayer2)
          && pplayer->is_alive
          && pplayer2->is_alive) {
        SANITY_CHECK(pplayer_can_ally(pplayer, pplayer2));
      }
    } players_iterate_end;

    if (pplayer->revolution_finishes == -1) {
      if (pplayer->government == game.info.government_when_anarchy) {
        freelog(LOG_FATAL, "%s's government is anarchy but does not finish",
                pplayer->name);
      }
      SANITY_CHECK(pplayer->government != game.info.government_when_anarchy);
    } else if (pplayer->revolution_finishes > game.info.turn) {
      SANITY_CHECK(pplayer->government == game.info.government_when_anarchy);
    } else {
      /* Things may vary in this case depending on when the sanity_check
       * call is made.  No better check is possible. */
    }
  } players_iterate_end;

  /* Sanity checks on living and dead players. */
  for (player_no = 0; player_no < ARRAY_SIZE(game.players); player_no++) {
    struct player *pplayer = &game.players[player_no];

    if (!pplayer->is_alive) {
      /* Dead players' units and cities are disbanded in kill_player(). */
      SANITY_CHECK(unit_list_size(pplayer->units) == 0);
      SANITY_CHECK(city_list_size(pplayer->cities) == 0);
    }

    /* Dying players shouldn't be left around.  But they are. */
    SANITY_CHECK(!pplayer->is_dying);
  }
}

/****************************************************************************
  Sanity checking on teams.
****************************************************************************/
static void check_teams(void)
{
  int count[MAX_NUM_TEAMS], i;

  memset(count, 0, sizeof(count));
  players_iterate(pplayer) {
    /* For the moment, all players (including observers) have teams. */
    SANITY_CHECK(pplayer->team != NULL);
    if (pplayer->team) {
      count[pplayer->team->index]++;
    }
  } players_iterate_end;

  for (i = 0; i < MAX_NUM_TEAMS; i++) {
    SANITY_CHECK(team_get_by_id(i)->players == count[i]);
  }
}

/**************************************************************************
...
**************************************************************************/
void sanity_check(void)
{
  if (!map_is_empty()) {
    /* Don't sanity-check the map if it hasn't been created yet (this
     * happens when loading scenarios). */
    check_specials();
    check_map();
    check_cities();
    check_units();
    check_fow();
  }
  check_misc();
  check_players();
  check_teams();
}

#endif /* SANITY_CHECKING */
