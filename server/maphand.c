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

#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "rand.h"
#include "support.h"

#include "base.h"
#include "events.h"
#include "game.h"
#include "map.h"
#include "movement.h"
#include "nation.h"
#include "packets.h"
#include "unit.h"
#include "unitlist.h"
#include "vision.h"

#include "citytools.h"
#include "cityturn.h"
#include "maphand.h"
#include "plrhand.h"           /* notify_player */
#include "sernet.h"
#include "srv_main.h"
#include "unithand.h"
#include "unittools.h"

#define MAXIMUM_CLAIMED_OCEAN_SIZE (20)

/* These arrays are indexed by continent number (or negative of the
 * ocean number) so the 0th element is unused and the array is 1 element
 * larger than you'd expect.
 *
 * The lake surrounders array tells how many land continents surround each
 * ocean (or -1 if the ocean touches more than one continent).
 *
 * The _sizes arrays give the sizes (in tiles) of each continent and
 * ocean.
 */
static Continent_id *lake_surrounders;
static int *continent_sizes, *ocean_sizes;

/* Suppress send_tile_info() during game_load() */
static bool send_tile_suppressed = FALSE;


/**************************************************************************
  Number this tile and nearby tiles (recursively) with the specified
  continent number nr, using a flood-fill algorithm.

  is_land tells us whether we are assigning continent numbers or ocean 
  numbers.
**************************************************************************/
static void assign_continent_flood(struct tile *ptile, bool is_land, int nr)
{
  const struct terrain *pterrain = tile_terrain(ptile);

  if (tile_continent(ptile) != 0) {
    return;
  }

  if (T_UNKNOWN == pterrain) {
    return;
  }

  if (!XOR(is_land, terrain_has_flag(pterrain, TER_OCEANIC))) {
    return;
  }

  tile_set_continent(ptile, nr);
  
  /* count the tile */
  if (nr < 0) {
    ocean_sizes[-nr]++;
  } else {
    continent_sizes[nr]++;
  }

  adjc_iterate(ptile, tile1) {
    assign_continent_flood(tile1, is_land, nr);
  } adjc_iterate_end;
}

/**************************************************************************
  Calculate lake_surrounders[] array
**************************************************************************/
static void recalculate_lake_surrounders(void)
{
  const size_t size = (map.num_oceans + 1) * sizeof(*lake_surrounders);

  lake_surrounders = fc_realloc(lake_surrounders, size);
  memset(lake_surrounders, 0, size);
  
  whole_map_iterate(ptile) {
    const struct terrain *pterrain = tile_terrain(ptile);
    Continent_id cont = tile_continent(ptile);

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      adjc_iterate(ptile, tile2) {
        Continent_id cont2 = tile_continent(tile2);
	if (is_ocean_tile(tile2)) {
	  if (lake_surrounders[-cont2] == 0) {
	    lake_surrounders[-cont2] = cont;
	  } else if (lake_surrounders[-cont2] != cont) {
	    lake_surrounders[-cont2] = -1;
	  }
	}
      } adjc_iterate_end;
    }
  } whole_map_iterate_end;
}

/**************************************************************************
  Assigns continent and ocean numbers to all tiles, and set
  map.num_continents and map.num_oceans.  Recalculates continent and
  ocean sizes, and lake_surrounders[] arrays.

  Continents have numbers 1 to map.num_continents _inclusive_.
  Oceans have (negative) numbers -1 to -map.num_oceans _inclusive_.
**************************************************************************/
void assign_continent_numbers(void)
{
  /* Initialize */
  map.num_continents = 0;
  map.num_oceans = 0;

  whole_map_iterate(ptile) {
    tile_set_continent(ptile, 0);
  } whole_map_iterate_end;

  /* Assign new numbers */
  whole_map_iterate(ptile) {
    const struct terrain *pterrain = tile_terrain(ptile);

    if (tile_continent(ptile) != 0) {
      /* Already assigned. */
      continue;
    }

    if (T_UNKNOWN == pterrain) {
      continue; /* Can't assign this. */
    }

    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      map.num_continents++;
      continent_sizes = fc_realloc(continent_sizes,
		       (map.num_continents + 1) * sizeof(*continent_sizes));
      continent_sizes[map.num_continents] = 0;
      assign_continent_flood(ptile, TRUE, map.num_continents);
    } else {
      map.num_oceans++;
      ocean_sizes = fc_realloc(ocean_sizes,
		       (map.num_oceans + 1) * sizeof(*ocean_sizes));
      ocean_sizes[map.num_oceans] = 0;
      assign_continent_flood(ptile, FALSE, -map.num_oceans);
    }
  } whole_map_iterate_end;

  recalculate_lake_surrounders();

  freelog(LOG_VERBOSE, "Map has %d continents and %d oceans", 
	  map.num_continents, map.num_oceans);
}

/**************************************************************************
  Regenerate all oceanic tiles with coasts, lakes, and deeper oceans.
  Assumes assign_continent_numbers() and recalculate_lake_surrounders()
  have already been done!
  FIXME: insufficiently generalized, use terrain property.
**************************************************************************/
void map_regenerate_water(void)
{
#define DEFAULT_LAKE_SEA_SIZE (4)  /* should be configurable */
#define DEFAULT_NEAR_COAST (6)
  struct terrain *lake = find_terrain_by_identifier(TERRAIN_LAKE_IDENTIFIER);
  struct terrain *sea = find_terrain_by_identifier(TERRAIN_SEA_IDENTIFIER);
  struct terrain *coast = find_terrain_by_identifier(TERRAIN_COAST_IDENTIFIER);
  struct terrain *shelf = find_terrain_by_identifier(TERRAIN_SHELF_IDENTIFIER);
  struct terrain *floor = find_terrain_by_identifier(TERRAIN_FLOOR_IDENTIFIER);
  int coast_depth = coast->property[MG_OCEAN_DEPTH];
  int coast_count = 0;
  int shelf_count = 0;
  int floor_count = 0;

  /* coasts, lakes, and seas */
  whole_map_iterate(ptile) {
    struct terrain *pterrain = tile_terrain(ptile);
    Continent_id here = tile_continent(ptile);

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      continue;
    }
    if (0 < lake_surrounders[-here]) {
      if (DEFAULT_LAKE_SEA_SIZE < ocean_sizes[-here]) {
        tile_change_terrain(ptile, lake);
      } else {
        tile_change_terrain(ptile, sea);
      }
      update_tile_knowledge(ptile);
      continue;
    }
    /* leave any existing deep features in place */
    if (pterrain->property[MG_OCEAN_DEPTH] > coast_depth) {
      continue;
    }

    /* default to shelf */
    tile_change_terrain(ptile, shelf);
    update_tile_knowledge(ptile);
    shelf_count++;

    adjc_iterate(ptile, tile2) {
      struct terrain *pterrain2 = tile_terrain(tile2);
      if (T_UNKNOWN == pterrain2) {
        continue;
      }
      /* glacier not otherwise near land floats */
      if (TERRAIN_GLACIER_IDENTIFIER == terrain_identifier(pterrain2)) {
        continue;
      }
      /* any land makes coast */
      if (!terrain_has_flag(pterrain2, TER_OCEANIC)) {
        tile_change_terrain(ptile, coast);
        update_tile_knowledge(ptile);
        coast_count++;
        shelf_count--;
        break;
      }
    } adjc_iterate_end;
  } whole_map_iterate_end;

  /* continental shelf */
  whole_map_iterate(ptile) {
    struct terrain *pterrain = tile_terrain(ptile);
    int shallow = 0;

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      continue;
    }
    /* leave any other existing features in place */
    if (pterrain != shelf) {
      continue;
    }

    adjc_iterate(ptile, tile2) {
      struct terrain *pterrain2 = tile_terrain(tile2);
      if (T_UNKNOWN == pterrain2)
        continue;

      switch (terrain_identifier(pterrain2)) {
      case TERRAIN_COAST_IDENTIFIER:
        shallow++;
        break;
      default:
        break;
      };
    } adjc_iterate_end;

    if (DEFAULT_NEAR_COAST < shallow) {
      /* smooth with neighbors */
      tile_change_terrain(ptile, coast);
      update_tile_knowledge(ptile);
      coast_count++;
      shelf_count--;
    } else if (0 == shallow) {
      tile_change_terrain(ptile, floor);
      update_tile_knowledge(ptile);
      floor_count++;
      shelf_count--;
    }
  } whole_map_iterate_end;

  /* deep ocean floor */
  whole_map_iterate(ptile) {
    struct terrain *pterrain = tile_terrain(ptile);
    int shallow = 0;

    if (T_UNKNOWN == pterrain) {
      continue;
    }
    if (!terrain_has_flag(pterrain, TER_OCEANIC)) {
      continue;
    }
    /* leave any other existing features in place */
    if (pterrain != floor) {
      continue;
    }

    adjc_iterate(ptile, tile2) {
      struct terrain *pterrain2 = tile_terrain(tile2);
      if (T_UNKNOWN == pterrain2)
        continue;

      switch (terrain_identifier(pterrain2)) {
      case TERRAIN_GLACIER_IDENTIFIER:
      case TERRAIN_COAST_IDENTIFIER:
      case TERRAIN_SHELF_IDENTIFIER:
        shallow++;
        break;
      default:
        break;
      };
    } adjc_iterate_end;

    if (DEFAULT_NEAR_COAST < shallow) {
      /* smooth with neighbors */
      tile_change_terrain(ptile, shelf);
      update_tile_knowledge(ptile);
      floor_count--;
      shelf_count++;
    }
  } whole_map_iterate_end;

  freelog(LOG_VERBOSE, "Map has %d coast, %d shelf, and %d floor tiles", 
          coast_count,
          shelf_count,
          floor_count);
}

static void player_tile_init(struct tile *ptile, struct player *pplayer);
static void give_tile_info_from_player_to_player(struct player *pfrom,
						 struct player *pdest,
						 struct tile *ptile);
static void shared_vision_change_seen(struct tile *ptile,
				      struct player *pplayer, int change,
				      enum vision_layer vlayer);
static int map_get_seen(const struct tile *ptile,
			const struct player *pplayer,
			enum vision_layer vlayer);
static void map_change_own_seen(struct tile *ptile, struct player *pplayer,
				int change, enum vision_layer vlayer);

/**************************************************************************
Used only in global_warming() and nuclear_winter() below.
**************************************************************************/
static bool is_terrain_ecologically_wet(struct tile *ptile)
{
  return (tile_has_special(ptile, S_RIVER)
	  || is_ocean_near_tile(ptile)
	  || is_special_near_tile(ptile, S_RIVER));
}

/**************************************************************************
...
**************************************************************************/
void global_warming(int effect)
{
  int k;

  freelog(LOG_VERBOSE, "Global warming: %d", game.info.heating);

  k = map_num_tiles();
  while(effect > 0 && (k--) > 0) {
    struct terrain *old, *new;
    struct tile *ptile;

    ptile = rand_map_pos();
    old = tile_terrain(ptile);
    if (is_terrain_ecologically_wet(ptile)) {
      new = old->warmer_wetter_result;
    } else {
      new = old->warmer_drier_result;
    }
    if (new != T_NONE && old != new) {
      effect--;
      tile_change_terrain(ptile, new);
      check_terrain_change(ptile, old);
      update_tile_knowledge(ptile);
      unit_list_iterate(ptile->units, punit) {
	if (!can_unit_continue_current_activity(punit)) {
	  unit_activity_handling(punit, ACTIVITY_IDLE);
	}
      } unit_list_iterate_end;
    } else if (old == new) {
      /* This counts toward warming although nothing is changed. */
      effect--;
    }
  }

  notify_player(NULL, NULL, E_GLOBAL_ECO,
		   _("Global warming has occurred!"));
  notify_player(NULL, NULL, E_GLOBAL_ECO,
		_("Coastlines have been flooded and vast "
		  "ranges of grassland have become deserts."));
}

/**************************************************************************
...
**************************************************************************/
void nuclear_winter(int effect)
{
  int k;

  freelog(LOG_VERBOSE, "Nuclear winter: %d", game.info.cooling);

  k = map_num_tiles();
  while(effect > 0 && (k--) > 0) {
    struct terrain *old, *new;
    struct tile *ptile;

    ptile = rand_map_pos();
    old = tile_terrain(ptile);
    if (is_terrain_ecologically_wet(ptile)) {
      new = old->cooler_wetter_result;
    } else {
      new = old->cooler_drier_result;
    }
    if (new != T_NONE && old != new) {
      effect--;
      tile_change_terrain(ptile, new);
      check_terrain_change(ptile, old);
      update_tile_knowledge(ptile);
      unit_list_iterate(ptile->units, punit) {
	if (!can_unit_continue_current_activity(punit)) {
	  unit_activity_handling(punit, ACTIVITY_IDLE);
	}
      } unit_list_iterate_end;
    } else if (old == new) {
      /* This counts toward winter although nothing is changed. */
      effect--;
    }
  }

  notify_player(NULL, NULL, E_GLOBAL_ECO,
		   _("Nuclear winter has occurred!"));
  notify_player(NULL, NULL, E_GLOBAL_ECO,
		_("Wetlands have dried up and vast "
		  "ranges of grassland have become tundra."));
}

/***************************************************************
To be called when a player gains the Railroad tech for the first
time.  Sends a message, and then upgrade all city squares to
railroads.  "discovery" just affects the message: set to
   1 if the tech is a "discovery",
   0 if otherwise acquired (conquer/trade/GLib).        --dwp
***************************************************************/
void upgrade_city_rails(struct player *pplayer, bool discovery)
{
  if (!(terrain_control.may_road)) {
    return;
  }

  conn_list_do_buffer(pplayer->connections);

  if (discovery) {
    notify_player(pplayer, NULL, E_TECH_GAIN,
		  _("New hope sweeps like fire through the country as "
		    "the discovery of railroad is announced.\n"
		    "      Workers spontaneously gather and upgrade all "
		    "cities with railroads."));
  } else {
    notify_player(pplayer, NULL, E_TECH_GAIN,
		  _("The people are pleased to hear that your "
		    "scientists finally know about railroads.\n"
		    "      Workers spontaneously gather and upgrade all "
		    "cities with railroads."));
  }
  
  city_list_iterate(pplayer->cities, pcity) {
    tile_set_special(pcity->tile, S_RAILROAD);
    update_tile_knowledge(pcity->tile);
  }
  city_list_iterate_end;

  conn_list_do_unbuffer(pplayer->connections);
}

/**************************************************************************
Return TRUE iff the player me really gives shared vision to player them.
**************************************************************************/
static bool really_gives_vision(struct player *me, struct player *them)
{
  return TEST_BIT(me->really_gives_vision, player_index(them));
}

/**************************************************************************
...
**************************************************************************/
static void buffer_shared_vision(struct player *pplayer)
{
  players_iterate(pplayer2) {
    if (really_gives_vision(pplayer, pplayer2))
      conn_list_do_buffer(pplayer2->connections);
  } players_iterate_end;
  conn_list_do_buffer(pplayer->connections);
}

/**************************************************************************
...
**************************************************************************/
static void unbuffer_shared_vision(struct player *pplayer)
{
  players_iterate(pplayer2) {
    if (really_gives_vision(pplayer, pplayer2))
      conn_list_do_unbuffer(pplayer2->connections);
  } players_iterate_end;
  conn_list_do_unbuffer(pplayer->connections);
}

/**************************************************************************
...
**************************************************************************/
void give_map_from_player_to_player(struct player *pfrom, struct player *pdest)
{
  buffer_shared_vision(pdest);
  whole_map_iterate(ptile) {
    give_tile_info_from_player_to_player(pfrom, pdest, ptile);
  } whole_map_iterate_end;
  unbuffer_shared_vision(pdest);
}

/**************************************************************************
...
**************************************************************************/
void give_seamap_from_player_to_player(struct player *pfrom, struct player *pdest)
{
  buffer_shared_vision(pdest);
  whole_map_iterate(ptile) {
    if (is_ocean_tile(ptile)) {
      give_tile_info_from_player_to_player(pfrom, pdest, ptile);
    }
  } whole_map_iterate_end;
  unbuffer_shared_vision(pdest);
}

/**************************************************************************
...
**************************************************************************/
void give_citymap_from_player_to_player(struct city *pcity,
					struct player *pfrom, struct player *pdest)
{
  buffer_shared_vision(pdest);
  map_city_radius_iterate(pcity->tile, ptile) {
    give_tile_info_from_player_to_player(pfrom, pdest, ptile);
  } map_city_radius_iterate_end;
  unbuffer_shared_vision(pdest);
}

/**************************************************************************
  Send all tiles known to specified clients.
  If dest is NULL means game.est_connections.
  
  Note for multiple connections this may change "sent" multiple times
  for single player.  This is ok, because "sent" data is just optimised
  calculations, so it will be correct before this, for each connection
  during this, and at end.
**************************************************************************/
void send_all_known_tiles(struct conn_list *dest)
{
  int tiles_sent;

  if (!dest) {
    dest = game.est_connections;
  }

  /* send whole map piece by piece to each player to balance the load
     of the send buffers better */
  tiles_sent = 0;
  conn_list_do_buffer(dest);

  whole_map_iterate(ptile) {
    tiles_sent++;
    if ((tiles_sent % map.xsize) == 0) {
      conn_list_do_unbuffer(dest);
      flush_packets();
      conn_list_do_buffer(dest);
    }

    send_tile_info(dest, ptile, FALSE);
  } whole_map_iterate_end;

  conn_list_do_unbuffer(dest);
  flush_packets();
}

/**************************************************************************
  Suppress send_tile_info() during game_load()
**************************************************************************/
bool send_tile_suppression(bool now)
{
  bool formerly = send_tile_suppressed;

  send_tile_suppressed = now;
  return formerly;
}

/**************************************************************************
  Send tile information to all the clients in dest which know and see
  the tile. If dest is NULL, sends to all clients (game.est_connections)
  which know and see tile.

  Note that this function does not update the playermap.  For that call
  update_tile_knowledge().
**************************************************************************/
void send_tile_info(struct conn_list *dest, struct tile *ptile,
                    bool send_unknown)
{
  struct packet_tile_info info;

  if (send_tile_suppressed) {
    return;
  }

  if (!dest) {
    dest = game.est_connections;
  }

  info.x = ptile->x;
  info.y = ptile->y;
  info.owner = tile_owner(ptile) ? player_number(tile_owner(ptile)) : MAP_TILE_OWNER_NULL;
  if (ptile->spec_sprite) {
    sz_strlcpy(info.spec_sprite, ptile->spec_sprite);
  } else {
    info.spec_sprite[0] = '\0';
  }

  conn_list_iterate(dest, pconn) {
    struct player *pplayer = pconn->player;

    if (!pplayer && !pconn->observer) {
      continue;
    }
    if (!pplayer || map_is_known_and_seen(ptile, pplayer, V_MAIN)) {
      info.known = TILE_KNOWN;
      info.type = tile_terrain(ptile) ? terrain_number(tile_terrain(ptile)) : -1;

      tile_special_type_iterate(spe) {
	info.special[spe] = BV_ISSET(ptile->special, spe);
      } tile_special_type_iterate_end;

      info.resource = tile_resource(ptile) ? resource_number(tile_resource(ptile)) : -1;
      info.continent = tile_continent(ptile);
      send_packet_tile_info(pconn, &info);
    } else if (pplayer && map_is_known(ptile, pplayer)
	       && map_get_seen(ptile, pplayer, V_MAIN) == 0) {
      struct player_tile *plrtile = map_get_player_tile(ptile, pplayer);

      info.known = TILE_KNOWN_FOGGED;
      info.type = plrtile->terrain ? terrain_number(plrtile->terrain) : -1;

      tile_special_type_iterate(spe) {
	info.special[spe] = BV_ISSET(plrtile->special, spe);
      } tile_special_type_iterate_end;

      info.resource = plrtile->resource ? resource_number(plrtile->resource) : -1;
      info.continent = tile_continent(ptile);
      send_packet_tile_info(pconn, &info);
    } else if (send_unknown) {
      info.known = TILE_UNKNOWN;
      info.type  = -1;

      tile_special_type_iterate(spe) {
        info.special[spe] = FALSE;
      } tile_special_type_iterate_end;

      info.resource  = -1;
      info.continent = 0;
      send_packet_tile_info(pconn, &info);
    }
  }
  conn_list_iterate_end;
}

/****************************************************************************
  Assumption: Each unit type is visible on only one layer.
****************************************************************************/
static bool unit_is_visible_on_layer(const struct unit *punit,
				     enum vision_layer vlayer)
{
  return XOR(vlayer == V_MAIN, is_hiding_unit(punit));
}

/****************************************************************************
  This is a backend function that marks a tile as unfogged.  Call this when
  map_unfog_tile adds the first point of visibility to the tile, or when
  shared vision changes cause a tile to become unfogged.
****************************************************************************/
static void really_unfog_tile(struct player *pplayer, struct tile *ptile,
			      enum vision_layer vlayer)
{
  struct city *pcity;
  bool old_known = map_is_known(ptile, pplayer);

  freelog(LOG_DEBUG, "really unfogging %d,%d\n", TILE_XY(ptile));

  map_set_known(ptile, pplayer);

  if (vlayer == V_MAIN) {
    /* send info about the tile itself 
     * It has to be sent first because the client needs correct
     * continent number before it can handle following packets
     */
    update_player_tile_knowledge(pplayer, ptile);
    send_tile_info(pplayer->connections, ptile, FALSE);
    /* NOTE: because the V_INVIS case doesn't fall into this if statement,
     * changes to V_INVIS fogging won't send a new info packet to the client
     * and the client's tile_seen[V_INVIS] bitfield may end up being out
     * of date. */
  }

  /* discover units */
  unit_list_iterate(ptile->units, punit) {
    if (unit_is_visible_on_layer(punit, vlayer)) {
      send_unit_info(pplayer, punit);
    }
  } unit_list_iterate_end;

  if (vlayer == V_MAIN) {
    /* discover cities */ 
    reality_check_city(pplayer, ptile);
    if ((pcity=tile_city(ptile)))
      send_city_info(pplayer, pcity);

    /* If the tile was not known before we need to refresh the cities that
       can use the tile. */
    if (!old_known) {
      map_city_radius_iterate(ptile, tile1) {
	pcity = tile_city(tile1);
	if (pcity && city_owner(pcity) == pplayer) {
	  update_city_tile_status_map(pcity, ptile);
	}
      } map_city_radius_iterate_end;
      sync_cities();
    }
  }
}

/****************************************************************************
  Add an extra point of visibility to the given tile.  pplayer may not be
  NULL.  The caller may wish to buffer_shared_vision if calling this
  function multiple times.
****************************************************************************/
static void map_unfog_tile(struct player *pplayer, struct tile *ptile,
			   bool can_reveal_tiles,
			   enum vision_layer vlayer)
{
  /* Increase seen count. */
  shared_vision_change_seen(ptile, pplayer, +1, vlayer);

  /* And then give the vision.  Did the tile just become visible?
   * Then send info about units and cities and the tile itself. */
  players_iterate(pplayer2) {
    if (pplayer2 == pplayer || really_gives_vision(pplayer, pplayer2)) {
      bool known = map_is_known(ptile, pplayer2);

      if ((!known && can_reveal_tiles)
	  || (known && map_get_seen(ptile, pplayer2, vlayer) == 1)) {
	really_unfog_tile(pplayer2, ptile, vlayer);
      }
    }
  } players_iterate_end;
}

/****************************************************************************
  This is a backend function that marks a tile as fogged.  Call this when
  map_fog_tile removes the last point of visibility from the tile, or when
  shared vision changes cause a tile to become fogged.
****************************************************************************/
static void really_fog_tile(struct player *pplayer, struct tile *ptile,
			    enum vision_layer vlayer)
{
  freelog(LOG_DEBUG, "Fogging %i,%i. Previous fog: %i.",
	  TILE_XY(ptile), map_get_seen(ptile, pplayer, vlayer));
 
  assert(map_get_seen(ptile, pplayer, vlayer) == 0);

  unit_list_iterate(ptile->units, punit)
    if (unit_is_visible_on_layer(punit, vlayer)) {
      unit_goes_out_of_sight(pplayer,punit);
    }
  unit_list_iterate_end;  

  if (vlayer == V_MAIN) {
    update_player_tile_last_seen(pplayer, ptile);
    send_tile_info(pplayer->connections, ptile, FALSE);
  }
}

/**************************************************************************
  Remove a point of visibility from the given tile.  pplayer may not be
  NULL.  The caller may wish to buffer_shared_vision if calling this
  function multiple times.
**************************************************************************/
static void map_fog_tile(struct player *pplayer, struct tile *ptile,
			 enum vision_layer vlayer)
{
  shared_vision_change_seen(ptile, pplayer, -1, vlayer);

  if (map_is_known(ptile, pplayer)) {
    players_iterate(pplayer2) {
      if (pplayer2 == pplayer || really_gives_vision(pplayer, pplayer2)) {
	if (map_get_seen(ptile, pplayer2, vlayer) == 0) {
	  really_fog_tile(pplayer2, ptile, vlayer);
	}
      }
    } players_iterate_end;
  }
}

/**************************************************************************
  Send basic map information: map size, topology, and is_earth.
**************************************************************************/
void send_map_info(struct conn_list *dest)
{
  struct packet_map_info minfo;

  minfo.xsize=map.xsize;
  minfo.ysize=map.ysize;
  minfo.topology_id = map.topology_id;
 
  lsend_packet_map_info(dest, &minfo);
}

/**************************************************************************
...
**************************************************************************/
static void shared_vision_change_seen(struct tile *ptile,
				      struct player *pplayer, int change,
				      enum vision_layer vlayer)
{
  map_change_seen(ptile, pplayer, change, vlayer);
  map_change_own_seen(ptile, pplayer, change, vlayer);

  players_iterate(pplayer2) {
    if (really_gives_vision(pplayer, pplayer2))
      map_change_seen(ptile, pplayer2, change, vlayer);
  } players_iterate_end;
}

/**************************************************************************
There doesn't have to be a city.
**************************************************************************/
static void map_refog_circle(struct player *pplayer, struct tile *ptile,
			     int old_radius_sq, int new_radius_sq,
			     bool can_reveal_tiles,
			     enum vision_layer vlayer)
{
  if (old_radius_sq != new_radius_sq) {
    int max_radius = MAX(old_radius_sq, new_radius_sq);

    freelog(LOG_DEBUG, "Refogging circle at %d,%d from %d to %d",
	    TILE_XY(ptile), old_radius_sq, new_radius_sq);

    buffer_shared_vision(pplayer);
    circle_dxyr_iterate(ptile, max_radius, tile1, dx, dy, dr) {
      if (dr > old_radius_sq && dr <= new_radius_sq) {
	map_unfog_tile(pplayer, tile1, can_reveal_tiles, vlayer);
      } else if (dr > new_radius_sq && dr <= old_radius_sq) {
	map_fog_tile(pplayer, tile1, vlayer);
      }
    } circle_dxyr_iterate_end;
    unbuffer_shared_vision(pplayer);
  }
}

/****************************************************************************
  Shows the area to the player.  Unless the tile is "seen", it will remain
  fogged and units will be hidden.

  Callers may wish to buffer_shared_vision before calling this function.
****************************************************************************/
void map_show_tile(struct player *src_player, struct tile *ptile)
{
  static int recurse = 0;
  freelog(LOG_DEBUG, "Showing %i,%i to %s",
	  TILE_XY(ptile), player_name(src_player));

  assert(recurse == 0);
  recurse++;

  players_iterate(pplayer) {
    if (pplayer == src_player || really_gives_vision(pplayer, src_player)) {
      struct city *pcity;
      bool old_known = map_is_known(ptile, pplayer);

      if (!map_is_known_and_seen(ptile, pplayer, V_MAIN)) {
	map_set_known(ptile, pplayer);

	/* as the tile may be fogged send_tile_info won't always do this for us */
	update_player_tile_knowledge(pplayer, ptile);
	update_player_tile_last_seen(pplayer, ptile);

	send_tile_info(pplayer->connections, ptile, FALSE);

	/* remove old cities that exist no more */
	reality_check_city(pplayer, ptile);
	if ((pcity = tile_city(ptile))) {
	  /* as the tile may be fogged send_city_info won't do this for us */
	  update_dumb_city(pplayer, pcity);
	  send_city_info(pplayer, pcity);
	}

	vision_layer_iterate(v) {
	  if (map_get_seen(ptile, pplayer, v) != 0) {
	    unit_list_iterate(ptile->units, punit)
	      if (unit_is_visible_on_layer(punit, v)) {
		send_unit_info(pplayer, punit);
	      }
	    unit_list_iterate_end;
	  }
	} vision_layer_iterate_end;

	/* If the tile was not known before we need to refresh the cities that
	   can use the tile. */
	if (!old_known) {
	  map_city_radius_iterate(ptile, tile1) {
	    pcity = tile_city(tile1);
	    if (pcity && city_owner(pcity) == pplayer) {
	      update_city_tile_status_map(pcity, ptile);
	    }
	  } map_city_radius_iterate_end;
	  sync_cities();
	}
      }
    }
  } players_iterate_end;

  recurse--;
}

/****************************************************************************
  Shows the area to the player.  Unless the tile is "seen", it will remain
  fogged and units will be hidden.
****************************************************************************/
void map_show_circle(struct player *pplayer, struct tile *ptile, int radius_sq)
{
  buffer_shared_vision(pplayer);

  circle_iterate(ptile, radius_sq, tile1) {
    map_show_tile(pplayer, tile1);
  } circle_iterate_end;

  unbuffer_shared_vision(pplayer);
}

/****************************************************************************
  Shows the area to the player.  Unless the tile is "seen", it will remain
  fogged and units will be hidden.
****************************************************************************/
void map_show_all(struct player *pplayer)
{
  buffer_shared_vision(pplayer);

  whole_map_iterate(ptile) {
    map_show_tile(pplayer, ptile);
  } whole_map_iterate_end;

  unbuffer_shared_vision(pplayer);
}

/****************************************************************************
  Return whether the player knows the tile.  Knowing a tile means you've
  seen it once (as opposed to seeing a tile which means you can see it now).
****************************************************************************/
bool map_is_known(const struct tile *ptile, const struct player *pplayer)
{
  return BV_ISSET(ptile->tile_known, player_index(pplayer));
}

/***************************************************************
...
***************************************************************/
bool map_is_known_and_seen(const struct tile *ptile, struct player *pplayer,
			   enum vision_layer vlayer)
{
  assert(!game.info.fogofwar
	 || (BV_ISSET(ptile->tile_seen[vlayer], player_index(pplayer))
	     == (map_get_player_tile(ptile, pplayer)->seen_count[vlayer]
		 > 0)));
  return (BV_ISSET(ptile->tile_known, player_index(pplayer))
	  && BV_ISSET(ptile->tile_seen[vlayer], player_index(pplayer)));
}

/****************************************************************************
  Return whether the player can see the tile.  Seeing a tile means you have
  vision of it now (as opposed to knowing a tile which means you've seen it
  before).  Note that a tile can be seen but not known (currently this only
  happens when a city is founded with some unknown tiles in its radius); in
  this case the tile is unknown (but map_get_seen will still return TRUE).
****************************************************************************/
static int map_get_seen(const struct tile *ptile,
			const struct player *pplayer,
			enum vision_layer vlayer)
{
  assert(!game.info.fogofwar
	 || (BV_ISSET(ptile->tile_seen[vlayer], player_index(pplayer))
	     == (map_get_player_tile(ptile, pplayer)->seen_count[vlayer]
		 > 0)));
  return map_get_player_tile(ptile, pplayer)->seen_count[vlayer];
}

/***************************************************************
...
***************************************************************/
void map_change_seen(struct tile *ptile, struct player *pplayer, int change,
		     enum vision_layer vlayer)
{
  struct player_tile *plrtile = map_get_player_tile(ptile, pplayer);

  /* assert to avoid underflow */
  assert(0 <= change || -change <= plrtile->seen_count[vlayer]);

  plrtile->seen_count[vlayer] += change;
  if (plrtile->seen_count[vlayer] != 0) {
    BV_SET(ptile->tile_seen[vlayer], player_index(pplayer));
  } else {
    BV_CLR(ptile->tile_seen[vlayer], player_index(pplayer));
  }
  freelog(LOG_DEBUG, "%d,%d, p: %d, change %d, result %d\n", TILE_XY(ptile),
	  player_number(pplayer), change, plrtile->seen_count[vlayer]);
}

/***************************************************************
...
***************************************************************/
static int map_get_own_seen(struct tile *ptile, struct player *pplayer,
			    enum vision_layer vlayer)
{
  return map_get_player_tile(ptile, pplayer)->own_seen[vlayer];
}

/***************************************************************
...
***************************************************************/
static void map_change_own_seen(struct tile *ptile, struct player *pplayer,
				int change,
				enum vision_layer vlayer)
{
  map_get_player_tile(ptile, pplayer)->own_seen[vlayer] += change;
}

/***************************************************************
...
***************************************************************/
void map_set_known(struct tile *ptile, struct player *pplayer)
{
  BV_SET(ptile->tile_known, player_index(pplayer));
  vision_layer_iterate(v) {
    if (map_get_player_tile(ptile, pplayer)->seen_count[v] > 0) {
      BV_SET(ptile->tile_seen[v], player_index(pplayer));
    }
  } vision_layer_iterate_end;
}

/***************************************************************
...
***************************************************************/
void map_clear_known(struct tile *ptile, struct player *pplayer)
{
  BV_CLR(ptile->tile_known, player_index(pplayer));
}

/****************************************************************************
  Call this function to unfog all tiles.  This should only be called when
  a player dies or at the end of the game as it will result in permanent
  vision of the whole map.
****************************************************************************/
void map_know_and_see_all(struct player *pplayer)
{
  buffer_shared_vision(pplayer);

  whole_map_iterate(ptile) {
    vision_layer_iterate(v) {
      map_unfog_tile(pplayer, ptile, TRUE, v);
    } vision_layer_iterate_end;
  } whole_map_iterate_end;

  unbuffer_shared_vision(pplayer);
}

/**************************************************************************
  Unfogs all tiles for all players.  See map_know_and_see_all.
**************************************************************************/
void show_map_to_all(void)
{
  players_iterate(pplayer) {
    map_know_and_see_all(pplayer);
  } players_iterate_end;
}

/***************************************************************
  Allocate space for map, and initialise the tiles.
  Uses current map.xsize and map.ysize.
****************************************************************/
void player_map_allocate(struct player *pplayer)
{
  pplayer->private_map
    = fc_malloc(MAP_INDEX_SIZE * sizeof(*pplayer->private_map));

  whole_map_iterate(ptile) {
    player_tile_init(ptile, pplayer);
  } whole_map_iterate_end;
}

/***************************************************************
 frees a player's private map.
***************************************************************/
void player_map_free(struct player *pplayer)
{
  if (!pplayer->private_map) {
    return;
  }

  /* removing borders */
  whole_map_iterate(ptile) {
    struct player_tile *playtile = map_get_player_tile(ptile, pplayer);

    /* cleverly uses return that is NULL for non-site tile */
    playtile->site = map_get_player_base(ptile, pplayer);
  } whole_map_iterate_end;

  /* only after removing borders! */
  whole_map_iterate(ptile) {
    struct vision_site *psite = map_get_player_base(ptile, pplayer);

    if (NULL != psite) {
      free_vision_site(psite);
    }
  } whole_map_iterate_end;

  free(pplayer->private_map);
  pplayer->private_map = NULL;
}

/***************************************************************
  We need to use fogofwar_old here, so the player's tiles get
  in the same state as the other players' tiles.
***************************************************************/
static void player_tile_init(struct tile *ptile, struct player *pplayer)
{
  struct player_tile *plrtile = map_get_player_tile(ptile, pplayer);

  plrtile->terrain = T_UNKNOWN;
  clear_all_specials(&plrtile->special);
  plrtile->resource = NULL;
  plrtile->site = NULL;

  vision_layer_iterate(v) {
    plrtile->seen_count[v] = 0;
    BV_CLR(ptile->tile_seen[v], player_index(pplayer));
  } vision_layer_iterate_end;

  if (!game.fogofwar_old) {
    plrtile->seen_count[V_MAIN] = 1;
    if (map_is_known(ptile, pplayer)) {
      BV_SET(ptile->tile_seen[V_MAIN], player_index(pplayer));
    }
  }

  plrtile->last_updated = GAME_START_YEAR;
  vision_layer_iterate(v) {
    plrtile->own_seen[v] = plrtile->seen_count[v];
  } vision_layer_iterate_end;
}

/****************************************************************************
  ...
****************************************************************************/
struct vision_site *map_get_player_base(const struct tile *ptile,
					const struct player *pplayer)
{
  struct vision_site *psite = map_get_player_site(ptile, pplayer);

  if (NULL != psite && ptile == psite->location) {
    return psite;
  }
  return NULL;
}

/****************************************************************************
  ...
****************************************************************************/
struct vision_site *map_get_player_city(const struct tile *ptile,
					const struct player *pplayer)
{
  struct vision_site *psite = map_get_player_site(ptile, pplayer);

  if (NULL != psite && ptile == psite->location
   && IDENTITY_NUMBER_ZERO < psite->identity) {
    return psite;
  }
  return NULL;
}

/****************************************************************************
  ...
****************************************************************************/
struct vision_site *map_get_player_site(const struct tile *ptile,
					const struct player *pplayer)
{
  return map_get_player_tile(ptile, pplayer)->site;
}

/****************************************************************************
  Players' information of tiles is tracked so that fogged area can be kept
  consistent even when the client disconnects.  This function returns the
  player tile information for the given tile and player.
****************************************************************************/
struct player_tile *map_get_player_tile(const struct tile *ptile,
					const struct player *pplayer)
{
  return pplayer->private_map + tile_index(ptile);
}

/****************************************************************************
  Give pplayer the correct knowledge about tile; return TRUE iff
  knowledge changed.

  Note that unlike update_tile_knowledge, this function will not send any
  packets to the client.  Callers may want to call send_tile_info() if this
  function returns TRUE.
****************************************************************************/
bool update_player_tile_knowledge(struct player *pplayer, struct tile *ptile)
{
  struct player_tile *plrtile = map_get_player_tile(ptile, pplayer);

  if (plrtile->terrain != ptile->terrain
      || !BV_ARE_EQUAL(plrtile->special, ptile->special)
      || plrtile->resource != ptile->resource) {
    plrtile->terrain = ptile->terrain;
    plrtile->special = ptile->special;
    plrtile->resource = ptile->resource;
    return TRUE;
  }
  return FALSE;
}

/****************************************************************************
  Update playermap knowledge for everybody who sees the tile, and send a
  packet to everyone whose info is changed.

  Note this only checks for changing of the terrain, special, or resource
  for the tile, since these are the only values held in the playermap.

  A tile's owner always can see terrain changes in his or her territory.
****************************************************************************/
void update_tile_knowledge(struct tile *ptile)
{
  /* Players */
  players_iterate(pplayer) {
    if (map_is_known_and_seen(ptile, pplayer, V_MAIN)) {
      if (update_player_tile_knowledge(pplayer, ptile)) {
        send_tile_info(pplayer->connections, ptile, FALSE);
      }
    }
  } players_iterate_end;

  /* Global observers */
  conn_list_iterate(game.est_connections, pconn) {
    struct player *pplayer = pconn->player;
    if (!pplayer && pconn->observer) {
      send_tile_info(pconn->self, ptile, FALSE);
    }
  } conn_list_iterate_end;
}

/***************************************************************
...
***************************************************************/
void update_player_tile_last_seen(struct player *pplayer, struct tile *ptile)
{
  map_get_player_tile(ptile, pplayer)->last_updated = game.info.year;
}

/***************************************************************
...
***************************************************************/
static void really_give_tile_info_from_player_to_player(struct player *pfrom,
							struct player *pdest,
							struct tile *ptile)
{
  struct player_tile *from_tile, *dest_tile;
  if (!map_is_known_and_seen(ptile, pdest, V_MAIN)) {
    /* I can just hear people scream as they try to comprehend this if :).
     * Let me try in words:
     * 1) if the tile is seen by pfrom the info is sent to pdest
     *  OR
     * 2) if the tile is known by pfrom AND (he has more recent info
     *     OR it is not known by pdest)
     */
    if (map_is_known_and_seen(ptile, pfrom, V_MAIN)
	|| (map_is_known(ptile, pfrom)
	    && (((map_get_player_tile(ptile, pfrom)->last_updated
		 > map_get_player_tile(ptile, pdest)->last_updated))
	        || !map_is_known(ptile, pdest)))) {
      from_tile = map_get_player_tile(ptile, pfrom);
      dest_tile = map_get_player_tile(ptile, pdest);
      /* Update and send tile knowledge */
      map_set_known(ptile, pdest);
      dest_tile->terrain = from_tile->terrain;
      dest_tile->special = from_tile->special;
      dest_tile->resource = from_tile->resource;
      dest_tile->last_updated = from_tile->last_updated;
      send_tile_info(pdest->connections, ptile, FALSE);
	
      /* update and send city knowledge */
      /* remove outdated cities */
      if (dest_tile->site && dest_tile->site->location == ptile) {
	if (!from_tile->site || from_tile->site->location != ptile) {
	  /* As the city was gone on the newer from_tile
	     it will be removed by this function */
	  reality_check_city(pdest, ptile);
	} else /* We have a dest_city. update */
	  if (from_tile->site->identity
	   != dest_tile->site->identity)
	    /* As the city was gone on the newer from_tile
	       it will be removed by this function */
	    reality_check_city(pdest, ptile);
      }
      /* Set and send new city info */
      if (from_tile->site && from_tile->site->location == ptile) {
	if (!dest_tile->site || dest_tile->site->location != ptile) {
	  dest_tile->site = fc_calloc(1, sizeof(*dest_tile->site));
	}
	/* struct assignment copy */
	*dest_tile->site = *from_tile->site;
	send_city_info_at_tile(pdest, pdest->connections, NULL, ptile);
      }

      map_city_radius_iterate(ptile, tile1) {
	struct city *pcity = tile_city(tile1);
	if (pcity && city_owner(pcity) == pdest) {
	  update_city_tile_status_map(pcity, ptile);
	}
      } map_city_radius_iterate_end;
      sync_cities();
    }
  }
}

/***************************************************************
...
***************************************************************/
static void give_tile_info_from_player_to_player(struct player *pfrom,
						 struct player *pdest,
						 struct tile *ptile)
{
  really_give_tile_info_from_player_to_player(pfrom, pdest, ptile);

  players_iterate(pplayer2) {
    if (!really_gives_vision(pdest, pplayer2))
      continue;
    really_give_tile_info_from_player_to_player(pfrom, pplayer2, ptile);
  } players_iterate_end;
}

/***************************************************************
This updates all players' really_gives_vision field.
If p1 gives p2 shared vision and p2 gives p3 shared vision p1
should also give p3 shared vision.
***************************************************************/
static void create_vision_dependencies(void)
{
  int added;

  players_iterate(pplayer) {
    pplayer->really_gives_vision = pplayer->gives_shared_vision;
  } players_iterate_end;

  /* In words: This terminates when it has run a round without adding
     a dependency. One loop only propagates dependencies one level deep,
     which is why we keep doing it as long as changes occur. */
  do {
    added = 0;
    players_iterate(pplayer) {
      players_iterate(pplayer2) {
	if (really_gives_vision(pplayer, pplayer2)
	    && pplayer != pplayer2) {
	  players_iterate(pplayer3) {
	    if (really_gives_vision(pplayer2, pplayer3)
		&& !really_gives_vision(pplayer, pplayer3)
		&& pplayer != pplayer3) {
	      pplayer->really_gives_vision |= (1<<player_index(pplayer3));
	      added++;
	    }
	  } players_iterate_end;
	}
      } players_iterate_end;
    } players_iterate_end;
  } while (added > 0);
}

/***************************************************************
...
***************************************************************/
void give_shared_vision(struct player *pfrom, struct player *pto)
{
  int save_vision[MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS];
  if (pfrom == pto) return;
  if (gives_shared_vision(pfrom, pto)) {
    freelog(LOG_ERROR, "Trying to give shared vision from %s to %s, "
	    "but that vision is already given!",
	    player_name(pfrom),
	    player_name(pto));
    return;
  }

  players_iterate(pplayer) {
    save_vision[player_index(pplayer)] = pplayer->really_gives_vision;
  } players_iterate_end;

  pfrom->gives_shared_vision |= 1<<player_index(pto);
  create_vision_dependencies();
  freelog(LOG_DEBUG, "giving shared vision from %s to %s\n",
	  player_name(pfrom),
	  player_name(pto));

  players_iterate(pplayer) {
    buffer_shared_vision(pplayer);
    players_iterate(pplayer2) {
      if (really_gives_vision(pplayer, pplayer2)
	  && !TEST_BIT(save_vision[player_index(pplayer)], player_index(pplayer2))) {
	freelog(LOG_DEBUG, "really giving shared vision from %s to %s\n",
	       player_name(pplayer),
	       player_name(pplayer2));
	whole_map_iterate(ptile) {
	  vision_layer_iterate(v) {
	    int change = map_get_own_seen(ptile, pplayer, v);

	    if (change != 0) {
	      map_change_seen(ptile, pplayer2, change, v);
	      if (map_get_seen(ptile, pplayer2, v) == change
		  && map_is_known(ptile, pplayer)) {
		really_unfog_tile(pplayer2, ptile, v);
	      }
	    }
	  } vision_layer_iterate_end;
	} whole_map_iterate_end;

	/* squares that are not seen, but which pfrom may have more recent
	   knowledge of */
	give_map_from_player_to_player(pplayer, pplayer2);
      }
    } players_iterate_end;
    unbuffer_shared_vision(pplayer);
  } players_iterate_end;

  if (S_S_RUNNING == server_state()) {
    send_player_info(pfrom, NULL);
  }
}

/***************************************************************
...
***************************************************************/
void remove_shared_vision(struct player *pfrom, struct player *pto)
{
  int save_vision[MAX_NUM_PLAYERS+MAX_NUM_BARBARIANS];
  assert(pfrom != pto);
  if (!gives_shared_vision(pfrom, pto)) {
    freelog(LOG_ERROR, "Tried removing the shared vision from %s to %s, "
	    "but it did not exist in the first place!",
	    player_name(pfrom),
	    player_name(pto));
    return;
  }

  players_iterate(pplayer) {
    save_vision[player_index(pplayer)] = pplayer->really_gives_vision;
  } players_iterate_end;

  freelog(LOG_DEBUG, "removing shared vision from %s to %s\n",
	  player_name(pfrom),
	  player_name(pto));

  pfrom->gives_shared_vision &= ~(1<<player_index(pto));
  create_vision_dependencies();

  players_iterate(pplayer) {
    buffer_shared_vision(pplayer);
    players_iterate(pplayer2) {
      if (!really_gives_vision(pplayer, pplayer2)
	  && TEST_BIT(save_vision[player_index(pplayer)], player_index(pplayer2))) {
	freelog(LOG_DEBUG, "really removing shared vision from %s to %s\n",
	       player_name(pplayer),
	       player_name(pplayer2));
	whole_map_iterate(ptile) {
	  vision_layer_iterate(v) {
	    int change = map_get_own_seen(ptile, pplayer, v);

	    if (change > 0) {
	      map_change_seen(ptile, pplayer2, -change, v);
	      if (map_get_seen(ptile, pplayer2, v) == 0)
		really_fog_tile(pplayer2, ptile, v);
	    }
	  } vision_layer_iterate_end;
	} whole_map_iterate_end;
      }
    } players_iterate_end;
    unbuffer_shared_vision(pplayer);
  } players_iterate_end;

  if (S_S_RUNNING == server_state()) {
    send_player_info(pfrom, NULL);
  }
}

/*************************************************************************
...
*************************************************************************/
static void enable_fog_of_war_player(struct player *pplayer)
{
  buffer_shared_vision(pplayer);
  whole_map_iterate(ptile) {
    map_fog_tile(pplayer, ptile, V_MAIN);
  } whole_map_iterate_end;
  unbuffer_shared_vision(pplayer);
}

/*************************************************************************
...
*************************************************************************/
void enable_fog_of_war(void)
{
  players_iterate(pplayer) {
    enable_fog_of_war_player(pplayer);
  } players_iterate_end;
}

/*************************************************************************
...
*************************************************************************/
static void disable_fog_of_war_player(struct player *pplayer)
{
  buffer_shared_vision(pplayer);
  whole_map_iterate(ptile) {
    map_unfog_tile(pplayer, ptile, FALSE, V_MAIN);
  } whole_map_iterate_end;
  unbuffer_shared_vision(pplayer);
}

/*************************************************************************
...
*************************************************************************/
void disable_fog_of_war(void)
{
  players_iterate(pplayer) {
    disable_fog_of_war_player(pplayer);
  } players_iterate_end;
}

/**************************************************************************
  Set the tile to be a river if required.
  It's required if one of the tiles nearby would otherwise be part of a
  river to nowhere.
  For simplicity, I'm assuming that this is the only exit of the river,
  so I don't need to trace it across the continent.  --CJM
  Also, note that this only works for R_AS_SPECIAL type rivers.  --jjm
**************************************************************************/
static void ocean_to_land_fix_rivers(struct tile *ptile)
{
  /* clear the river if it exists */
  tile_clear_special(ptile, S_RIVER);

  cardinal_adjc_iterate(ptile, tile1) {
    if (tile_has_special(tile1, S_RIVER)) {
      bool ocean_near = FALSE;
      cardinal_adjc_iterate(tile1, tile2) {
        if (is_ocean_tile(tile2))
          ocean_near = TRUE;
      } cardinal_adjc_iterate_end;
      if (!ocean_near) {
        tile_set_special(ptile, S_RIVER);
        return;
      }
    }
  } cardinal_adjc_iterate_end;
}

/****************************************************************************
  A helper function for check_terrain_change that moves units off of invalid
  terrain after it's been changed.
****************************************************************************/
static void bounce_units_on_terrain_change(struct tile *ptile)
{
  unit_list_iterate_safe(ptile->units, punit) {
    bool unit_alive = TRUE;

    if (punit->tile == ptile
	&& punit->transported_by == -1
	&& !can_unit_exist_at_tile(punit, ptile)) {
      /* look for a nearby safe tile */
      adjc_iterate(ptile, ptile2) {
	if (can_unit_exist_at_tile(punit, ptile2)
	    && !is_non_allied_unit_tile(ptile2, unit_owner(punit))) {
	  freelog(LOG_VERBOSE,
		  "Moved %s %s due to changing terrain at (%d,%d).",
		  nation_rule_name(nation_of_unit(punit)),
		  unit_rule_name(punit),
		  TILE_XY(punit->tile));
	  notify_player(unit_owner(punit),
			   punit->tile, E_UNIT_RELOCATED,
			   _("Moved your %s due to changing terrain."),
			   unit_name_translation(punit));
	  unit_alive = move_unit(punit, ptile2, 0);
	  if (unit_alive && punit->activity == ACTIVITY_SENTRY) {
	    unit_activity_handling(punit, ACTIVITY_IDLE);
	  }
	  break;
	}
      } adjc_iterate_end;
      if (unit_alive && punit->tile == ptile) {
	/* if we get here we could not move punit */
	freelog(LOG_VERBOSE,
		"Disbanded %s %s due to changing land to sea at (%d,%d).",
		nation_rule_name(nation_of_unit(punit)),
		unit_rule_name(punit),
		TILE_XY(punit->tile));
	notify_player(unit_owner(punit),
			 punit->tile, E_UNIT_LOST,
			 _("Disbanded your %s due to changing terrain."),
			 unit_name_translation(punit));
	wipe_unit(punit);
      }
    }
  } unit_list_iterate_safe_end;
}

/****************************************************************************
  Handles global side effects for a terrain change.  Call this in the
  server immediately after calling tile_change_terrain.
****************************************************************************/
void check_terrain_change(struct tile *ptile, struct terrain *oldter)
{
  struct terrain *newter = tile_terrain(ptile);
  bool ocean_toggled = FALSE;

  if (is_ocean(oldter) && !is_ocean(newter)) {
    /* ocean to land ... */
    ocean_to_land_fix_rivers(ptile);
    city_landlocked_sell_coastal_improvements(ptile);
    ocean_toggled = TRUE;
  } else if (!is_ocean(oldter) && is_ocean(newter)) {
    /* land to ocean ... */
    ocean_toggled = TRUE;
  }

  if (ocean_toggled) {
    bounce_units_on_terrain_change(ptile);
    assign_continent_numbers();

    /* New continent numbers for all tiles to all players */
    send_all_known_tiles(NULL);
  }
}

/*************************************************************************
  Ocean tile can be claimed iff one of the following conditions stands:
  a) it is an inland lake not larger than MAXIMUM_OCEAN_SIZE
  b) it is adjacent to only one continent and not more than two ocean tiles
  c) It is one tile away from a city
  The source which claims the ocean has to be placed on the correct continent.
  in case a) The continent which surrounds the inland lake
  in case b) The only continent which is adjacent to the tile
*************************************************************************/
static bool is_claimable_ocean(struct tile *ptile, struct tile *source)
{
  Continent_id cont = tile_continent(ptile);
  Continent_id source_cont = tile_continent(source);
  Continent_id cont2;
  int ocean_tiles;

  if (get_ocean_size(-cont) <= MAXIMUM_CLAIMED_OCEAN_SIZE
      && lake_surrounders[-cont] == source_cont) {
    return TRUE;
  }
  
  ocean_tiles = 0;
  adjc_iterate(ptile, tile2) {
    cont2 = tile_continent(tile2);
    if (tile2 == source) {
      return TRUE;
    }
    if (cont2 == cont) {
      ocean_tiles++;
    } else if (cont2 != source_cont) {
      return FALSE; /* two land continents adjacent, punt! */
    }
  } adjc_iterate_end;
  if (ocean_tiles <= 2) {
    return TRUE;
  } else {
    return FALSE;
  }
}

#ifdef OWNER_SOURCE
/*************************************************************************
  Add any unique home city not found in list but found on tile to the 
  list.
*************************************************************************/
static void add_unique_homecities(struct city_list *cities_to_refresh, 
                           struct tile *tile1)
{
  /* Update happiness */
 unit_list_iterate(tile1->units, unit) {
   struct city* homecity = game_find_city_by_number(unit->homecity);
   bool already_listed = FALSE;

    if (!homecity) {
      continue;
    }
    city_list_iterate(cities_to_refresh, city2) {
      if (city2 == homecity) {
        already_listed = TRUE;
        break;
      }
      if (!already_listed) {
        city_list_prepend(cities_to_refresh, homecity);
      }
    } city_list_iterate_end;
  } unit_list_iterate_end;
}
#endif

/*************************************************************************
  Claim ownership of a single tile.
*************************************************************************/
void map_claim_ownership(struct tile *ptile, struct player *powner,
                         struct tile *psource)
{
  struct player *ploser = tile_owner(ptile);

  if (NULL != ploser) {
    struct player_tile *playtile = map_get_player_tile(ptile, ploser);

    /* cleverly uses return that is NULL for non-site tile */
    playtile->site = map_get_player_base(ptile, ploser);

    if (NULL != playtile->site && ptile == psource) {
      /* has new owner */
      playtile->site->owner = powner;
    }
  }

  if (NULL != powner /* assume && NULL != psource */) {
    struct city *pcity = tile_city(ptile);
    struct player_tile *playtile = map_get_player_tile(psource, powner);

    if (NULL != playtile->site) {
      if (ptile != psource) {
        map_get_player_tile(ptile, powner)->site = playtile->site;
      } else if (NULL != pcity) {
        playtile->site = update_vision_site_from_city(playtile->site, pcity);
      } else {
        /* has new owner */
        playtile->site->owner = powner;
      }
    } else {
      assert(ptile == psource);
      if (NULL != pcity) {
        playtile->site = create_vision_site_from_city(pcity);
      } else {
        playtile->site = create_vision_site(-1/*FIXME*/, psource, powner);
      }
    }
  }

  tile_set_owner(ptile, powner);
  send_tile_info(NULL, ptile, FALSE);

  /* This implementation is somewhat inefficient.  By design, it's not
   * called often.  Does not send updates to client.
   */
  if (NULL != ploser && ploser != powner) {
    city_list_iterate(ploser->cities, pcity) {
      update_city_tile_status_map(pcity, ptile);
    } city_list_iterate_end;
  }
  if (NULL != powner && ploser != powner) {
    city_list_iterate(powner->cities, pcity) {
      update_city_tile_status_map(pcity, ptile);
    } city_list_iterate_end;
  }
}

/*************************************************************************
  Update borders for this source.  Call this for each new source.
*************************************************************************/
void map_claim_border(struct tile *ptile, struct player *powner)
{
  struct vision_site *psite = map_get_player_site(ptile, powner);
  int range = game.info.borders;

  if (0 == range) {
    /* no borders */
    return;
  }
  if (IDENTITY_NUMBER_ZERO == psite->identity) {
    /* should never be called! */
    freelog(LOG_ERROR, "Warning: border source (%d,%d) is unknown!",
            TILE_XY(ptile));
    return;
  }
  if (IDENTITY_NUMBER_ZERO < psite->identity) {
    /* city expansion */
    range = MIN(psite->size + 1, game.info.borders);
    if (psite->size > game.info.borders) {
      range += (psite->size - game.info.borders) / 2;
    }
  }
  range *= range; /* due to sq dist */

  freelog(LOG_VERBOSE, "border source (%d,%d) range %d",
          TILE_XY(ptile), range);

  circle_dxyr_iterate(ptile, range, dtile, dx, dy, dr) {
    struct player *downer = tile_owner(dtile);

    if (!map_is_known(dtile, powner)) {
      /* border tile never seen */
      continue;
    }
    if (NULL != downer && downer != powner) {
      struct vision_site *dsite = map_get_player_site(dtile, downer);
      int r = sq_map_distance(dsite->location, dtile);

      /* border tile claimed by another */
      if (IDENTITY_NUMBER_ZERO == dsite->identity) {
        /* ruins don't keep their borders */
        dsite->owner = powner;
        tile_set_owner(dtile, powner);
        continue;
      } else if (r < dr) {
        /* nearest shall prevail */
        continue;
      } else if (r == dr) {
        if (dsite->identity < psite->identity) {
          /* lower shall prevail: airport/fortress/city */
          continue;
        } else if (dsite->identity == psite->identity) {
          /* neither shall prevail */
          map_claim_ownership(dtile, NULL, NULL);
          continue;
        }
      }
    }

    if (is_ocean_tile(dtile)) {
      if (is_claimable_ocean(dtile, ptile)) {
        map_claim_ownership(dtile, powner, ptile);
      }
    } else {
      if (tile_continent(dtile) == tile_continent(ptile)) {
        map_claim_ownership(dtile, powner, ptile);
      }
    }
  } circle_dxyr_iterate_end;
}

/*************************************************************************
  Update borders for all sources.  Call this on turn end.

  We will remove claim to land whose source is gone, and claim
  more land to sources in range, unless there are enemy units within
  this range.
*************************************************************************/
void map_calculate_borders(void)
{
  struct city_list *cities_to_refresh = NULL;

  if (game.info.borders == 0) {
    return;
  }

  if (game.info.happyborders > 0) {
    cities_to_refresh = city_list_new();
  }

  /* base sites are done first, as they may be thorn in city side. */
  sites_iterate(psite) {
    map_claim_border(psite->location, vision_owner(psite));
  } sites_iterate_end;

  cities_iterate(pcity) {
    map_claim_border(pcity->tile, city_owner(pcity));
  } cities_iterate_end;

#ifdef OWNER_SOURCE
  /* First transfer ownership for sources that have changed hands. */
  whole_map_iterate(ptile) {
    if (tile_owner(ptile) 
        && ptile->owner_source
        && ptile->owner_source->owner != tile_owner(ptile)
        && (tile_city(ptile->owner_source)
            || tile_has_base_flag(ptile->owner_source,
                                  BF_CLAIM_TERRITORY))) {
      /* Claim ownership of tiles previously owned by someone else */
      map_claim_ownership(ptile, ptile->owner_source->owner, 
                          ptile->owner_source);
    }
  } whole_map_iterate_end;

  /* Second transfer ownership to city closer than current source 
   * but with the same owner. */
  whole_map_iterate(ptile) {
    if (tile_owner(ptile)) {
      city_list_iterate(tile_owner(ptile)->cities, pcity) {
        int r_curr, r_city = sq_map_distance(ptile, pcity->tile);
        int max_range = map_border_range(pcity->tile);

        /* Repair tile ownership */
        if (!ptile->owner_source) {
          assert(FALSE);
          ptile->owner_source = pcity->tile;
        }
        r_curr = sq_map_distance(ptile, ptile->owner_source);
        max_range *= max_range; /* we are dealing with square distances */
        /* Transfer tile to city if closer than current source */
        if (r_curr > r_city && max_range >= r_city) {
          freelog(LOG_DEBUG, "%s %s(%d,%d) acquired tile (%d,%d) from "
                  "(%d,%d)",
                  nation_rule_name(nation_of_player(tile_owner(ptile))),
                  city_name(pcity),
                  TILE_XY(pcity->tile),
                  TILE_XY(ptile),
                  TILE_XY(ptile->owner_source));
          ptile->owner_source = pcity->tile;
        }
      } city_list_iterate_end;
    }
  } whole_map_iterate_end;

  /* Third remove undue ownership. */
  whole_map_iterate(ptile) {
    if (tile_owner(ptile)
        && (!ptile->owner_source
            || !tile_owner(ptile)->is_alive
            || tile_owner(ptile) != ptile->owner_source->owner
            || (!tile_city(ptile->owner_source)
                && !tile_has_base_flag(ptile->owner_source,
                                       BF_CLAIM_TERRITORY)))) {
      /* Ownership source gone */
      map_claim_ownership(ptile, NULL, NULL);
    }
  } whole_map_iterate_end;

  /* Now claim ownership of unclaimed tiles for all sources; we
   * grab one circle each turn as long as we have range left
   * to better visually display expansion. */
  whole_map_iterate(ptile) {
    if (tile_owner(ptile)
        && (tile_city(ptile)
            || tile_has_base_flag(ptile, BF_CLAIM_TERRITORY))) {
      /* We have an ownership source */
      int expand_range = 99;
      int found_unclaimed = 99;
      int range = map_border_range(ptile);

      freelog(LOG_DEBUG, "source at %d,%d", TILE_XY(ptile));
      range *= range; /* due to sq dist */
      freelog(LOG_DEBUG, "borders range for source is %d", range);

      circle_dxyr_iterate(ptile, range, atile, dx, dy, dist) {
        if (expand_range > dist) {
          unit_list_iterate(atile->units, punit) {
            if (!pplayers_allied(unit_owner(punit), tile_owner(ptile))) {
              /* We cannot expand borders further when enemy units are
               * standing in the way. */
              expand_range = dist - 1;
            }
          } unit_list_iterate_end;
        }
        if (found_unclaimed > dist
            && tile_owner(atile) == NULL
            && map_is_known(atile, tile_owner(ptile))
            && (!is_ocean_tile(atile)
                || is_claimable_ocean(atile, ptile))) {
          found_unclaimed = dist;
        }
      } circle_dxyr_iterate_end;
      freelog(LOG_DEBUG, "expand_range=%d found_unclaimed=%d", expand_range,
              found_unclaimed);

      circle_dxyr_iterate(ptile, range, atile, dx, dy, dist) {
        if (dist > expand_range || dist > found_unclaimed) {
          continue; /* only expand one extra circle radius each turn */
        }
        if (map_is_known(atile, tile_owner(ptile))
            && tile_owner(atile) == NULL
            && ((!is_ocean_tile(atile) 
                 && tile_continent(atile) == tile_continent(ptile))
                || (is_ocean_tile(atile)
                    && is_claimable_ocean(atile, ptile)))) {
          map_claim_ownership(atile, tile_owner(ptile), ptile);
          atile->owner_source = ptile;
          if (game.info.happyborders > 0) {
            add_unique_homecities(cities_to_refresh, atile);
          }
        }
      } circle_dxyr_iterate_end;
    }
  } whole_map_iterate_end;
#endif

  /* Update happiness in all homecities we have collected */ 
  if (game.info.happyborders > 0) {
    city_list_iterate(cities_to_refresh, to_refresh) {
      city_refresh(to_refresh);
      send_city_info(city_owner(to_refresh), to_refresh);
    } city_list_iterate_end;
    
    city_list_unlink_all(cities_to_refresh);
    city_list_free(cities_to_refresh);
  }
}

/*************************************************************************
  Return size in tiles of the given continent(not ocean)
*************************************************************************/
int get_continent_size(Continent_id id)
{
  assert(id > 0);
  return continent_sizes[id];
}

/*************************************************************************
  Return size in tiles of the given ocean. You should use positive ocean
  number.
*************************************************************************/
int get_ocean_size(Continent_id id) 
{
  assert(id > 0);
  return ocean_sizes[id];
}


/****************************************************************************
  Change the sight points for the vision source, fogging or unfogging tiles
  as needed.

  See documentation in vision.h.
****************************************************************************/
void vision_change_sight(struct vision *vision, enum vision_layer vlayer,
			 int radius_sq)
{
  map_refog_circle(vision->player, vision->tile,
		   vision->radius_sq[vlayer], radius_sq,
		   vision->can_reveal_tiles, vlayer);
  vision->radius_sq[vlayer] = radius_sq;
}

/****************************************************************************
  Clear all sight points from this vision source.

  See documentation in vision.h.
****************************************************************************/
void vision_clear_sight(struct vision *vision)
{
  /* We don't use vision_layer_iterate because we have to go in reverse
   * order. */
  vision_change_sight(vision, V_INVIS, -1);
  vision_change_sight(vision, V_MAIN, -1);
}
