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

/* common */
#include "city.h"
#include "map.h"
#include "player.h"
#include "tile.h"

/* server */
#include "citytools.h"
#include "maphand.h"

/* ai */
#include "aicity.h"

#include "infracache.h"

/* cache activities within the city map */
struct ai_activity_cache {
  int act[ACTIVITY_LAST];
};

static bool is_wet_or_is_wet_cardinal_around(struct player *pplayer,
					     struct tile *ptile);

/**************************************************************************
  Calculate the benefit of irrigating the given tile.

    (map_x, map_y) is the map position of the tile.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the irrigation.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_irrigate(struct city *pcity, struct player *pplayer,
                            struct tile *ptile)
{
  int goodness;
  /* FIXME: Isn't an other way to know the goodness of the transformation? */
  struct terrain *old_terrain = tile_terrain(ptile);
  bv_special old_special = ptile->special;
  bv_bases old_bases = ptile->bases;
  struct terrain *new_terrain = old_terrain->irrigation_result;

  if (old_terrain != new_terrain && new_terrain != T_NONE) {
    /* Irrigation would change the terrain type, clearing the mine
     * in the process.  Calculate the benefit of doing so. */
    if (tile_city(ptile) && terrain_has_flag(new_terrain, TER_NO_CITIES)) {
      return -1;
    }
    tile_change_terrain(ptile, new_terrain);
    tile_clear_special(ptile, S_MINE);
    goodness = city_tile_value(pcity, ptile, 0, 0);
    tile_set_terrain(ptile, old_terrain);
    ptile->special = old_special;
    ptile->bases = old_bases;
    return goodness;
  } else if (old_terrain == new_terrain
	     && !tile_has_special(ptile, S_IRRIGATION)
	     && is_wet_or_is_wet_cardinal_around(pplayer, ptile)) {
    /* The tile is currently unirrigated; irrigating it would put an
     * S_IRRIGATE on it replacing any S_MINE already there.  Calculate
     * the benefit of doing so. */
    tile_clear_special(ptile, S_MINE);
    tile_set_special(ptile, S_IRRIGATION);
    goodness = city_tile_value(pcity, ptile, 0, 0);
    ptile->special = old_special;
    ptile->bases = old_bases;
    fc_assert(tile_terrain(ptile) == old_terrain);
    return goodness;
  } else if (old_terrain == new_terrain
	     && tile_has_special(ptile, S_IRRIGATION)
	     && !tile_has_special(ptile, S_FARMLAND)
	     && player_knows_techs_with_flag(pplayer, TF_FARMLAND)
	     && is_wet_or_is_wet_cardinal_around(pplayer, ptile)) {
    /* The tile is currently irrigated; irrigating it more puts an
     * S_FARMLAND on it.  Calculate the benefit of doing so. */
    fc_assert(!tile_has_special(ptile, S_MINE));
    tile_set_special(ptile, S_FARMLAND);
    goodness = city_tile_value(pcity, ptile, 0, 0);
    tile_clear_special(ptile, S_FARMLAND);
    fc_assert(tile_terrain(ptile) == old_terrain
              && memcmp(&ptile->special, &old_special,
                        sizeof(old_special)) == 0);
    return goodness;
  } else {
    return -1;
  }
}

/**************************************************************************
  Calculate the benefit of mining the given tile.

    (map_x, map_y) is the map position of the tile.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the mining.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_mine(struct city *pcity, struct tile *ptile)
{
  int goodness;
  /* FIXME: Isn't an other way to know the goodness of the transformation? */
  struct terrain *old_terrain = tile_terrain(ptile);
  bv_special old_special = ptile->special;
  bv_bases old_bases = ptile->bases;
  struct terrain *new_terrain = old_terrain->mining_result;

  if (old_terrain != new_terrain && new_terrain != T_NONE) {
    /* Mining would change the terrain type, clearing the irrigation
     * in the process.  Calculate the benefit of doing so. */
    if (tile_city(ptile) && terrain_has_flag(new_terrain, TER_NO_CITIES)) {
      return -1;
    }
    tile_change_terrain(ptile, new_terrain);
    tile_clear_special(ptile, S_IRRIGATION);
    tile_clear_special(ptile, S_FARMLAND);
    goodness = city_tile_value(pcity, ptile, 0, 0);
    tile_set_terrain(ptile, old_terrain);
    ptile->special = old_special;
    ptile->bases = old_bases;
    return goodness;
  } else if (old_terrain == new_terrain
	     && !tile_has_special(ptile, S_MINE)) {
    /* The tile is currently unmined; mining it would put an S_MINE on it
     * replacing any S_IRRIGATION/S_FARMLAND already there.  Calculate
     * the benefit of doing so. */
    tile_clear_special(ptile, S_IRRIGATION);
    tile_clear_special(ptile, S_FARMLAND);
    tile_set_special(ptile, S_MINE);
    goodness = city_tile_value(pcity, ptile, 0, 0);
    ptile->special = old_special;
    ptile->bases = old_bases;
    fc_assert(tile_terrain(ptile) == old_terrain);
    return goodness;
  } else {
    return -1;
  }
  return goodness;
}

/**************************************************************************
  Calculate the benefit of transforming the given tile.

    (ptile) is the map position of the tile.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the transform.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_transform(struct city *pcity, struct tile *ptile)
{
  int goodness;
  /* FIXME: Isn't an other way to know the goodness of the transformation? */
  struct terrain *old_terrain = tile_terrain(ptile);
  bv_special old_special = ptile->special;
  bv_bases old_bases = ptile->bases;
  struct terrain *new_terrain = old_terrain->transform_result;

  if (old_terrain == new_terrain || new_terrain == T_NONE) {
    return -1;
  }

  if (is_ocean(old_terrain) && !is_ocean(new_terrain)
      && !can_reclaim_ocean(ptile)) {
    /* Can't change ocean into land here. */
    return -1;
  }
  if (is_ocean(new_terrain) && !is_ocean(old_terrain)
      && !can_channel_land(ptile)) {
    /* Can't change land into ocean here. */
    return -1;
  }

  if (tile_city(ptile) && terrain_has_flag(new_terrain, TER_NO_CITIES)) {
    return -1;
  }

  tile_change_terrain(ptile, new_terrain);
  goodness = city_tile_value(pcity, ptile, 0, 0);

  /* FIXME: Very ugly hacking */
  tile_set_terrain(ptile, old_terrain);
  ptile->special = old_special;
  ptile->bases = old_bases;

  return goodness;
}

/**************************************************************************
  Calculates the value of removing pollution at the given tile.

    (map_x, map_y) is the map position of the tile.

  The return value is the goodness of the tile after the cleanup.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_pollution(struct city *pcity, int best,
                             struct tile *ptile)
{
  int goodness;

  if (!tile_has_special(ptile, S_POLLUTION)) {
    return -1;
  }
  tile_clear_special(ptile, S_POLLUTION);
  goodness = city_tile_value(pcity, ptile, 0, 0);
  tile_set_special(ptile, S_POLLUTION);

  /* FIXME: need a better way to guarantee pollution is cleaned up. */
  goodness = (goodness + best + 50) * 2;

  return goodness;
}

/**************************************************************************
  Calculates the value of removing fallout at the given tile.

    (map_x, map_y) is the map position of the tile.

  The return value is the goodness of the tile after the cleanup.  This
  should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).
**************************************************************************/
static int ai_calc_fallout(struct city *pcity, struct player *pplayer,
                           int best, struct tile *ptile)
{
  int goodness;

  if (!tile_has_special(ptile, S_FALLOUT)) {
    return -1;
  }
  tile_clear_special(ptile, S_FALLOUT);
  goodness = city_tile_value(pcity, ptile, 0, 0);
  tile_set_special(ptile, S_FALLOUT);

  /* FIXME: need a better way to guarantee fallout is cleaned up. */
  if (!pplayer->ai_controlled) {
    goodness = (goodness + best + 50) * 2;
  }

  return goodness;
}


/**************************************************************************
  Calculate the benefit of building a road at the given tile.

    (map_x, map_y) is the map position of the tile.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the road is built.
  This should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).

  This function does not calculate the benefit of being able to quickly
  move units (i.e., of connecting the civilization).  See road_bonus() for
  that calculation.
**************************************************************************/
static int ai_calc_road(struct city *pcity, struct player *pplayer, 
                        struct tile *ptile)
{
  int goodness;

  if (!is_ocean_tile(ptile)
      && (!tile_has_special(ptile, S_RIVER)
	  || player_knows_techs_with_flag(pplayer, TF_BRIDGE))
      && !tile_has_special(ptile, S_ROAD)) {

    /* HACK: calling tile_set_special here will have side effects, so we
     * have to set it manually. */
    fc_assert(!tile_has_special(ptile, S_ROAD));
    set_special(&ptile->special, S_ROAD);

    goodness = city_tile_value(pcity, ptile, 0, 0);

    clear_special(&ptile->special, S_ROAD);

    return goodness;
  } else {
    return -1;
  }
}

/**************************************************************************
  Calculate the benefit of building a railroad at the given tile.

    (ptile) is the map position of the tile.
    pplayer is the player under consideration.

  The return value is the goodness of the tile after the railroad is built.
  This should be compared to the goodness of the tile currently (see
  city_tile_value(); note that this depends on the AI's weighting
  values).

  This function does not calculate the benefit of being able to quickly
  move units (i.e., of connecting the civilization).  See road_bonus() for
  that calculation.
**************************************************************************/
static int ai_calc_railroad(struct city *pcity, struct player *pplayer,
                            struct tile *ptile)
{
  int goodness;
  bv_special old_special;

  if (!is_ocean_tile(ptile)
      && player_knows_techs_with_flag(pplayer, TF_RAILROAD)
      && !tile_has_special(ptile, S_RAILROAD)) {
    old_special = ptile->special;

    /* HACK: calling tile_set_special here will have side effects, so we
     * have to set it manually. */
    set_special(&ptile->special, S_ROAD);
    set_special(&ptile->special, S_RAILROAD);

    goodness = city_tile_value(pcity, ptile, 0, 0);

    ptile->special = old_special;

    return goodness;
  } else {
    return -1;
  }
}

/**************************************************************************
  Returns TRUE if tile at (map_x,map_y) is useful as a source of
  irrigation.  This takes player vision into account, but allows the AI
  to cheat.

  This function should probably only be used by
  is_wet_or_is_wet_cardinal_around, below.
**************************************************************************/
static bool is_wet(struct player *pplayer, struct tile *ptile)
{
  /* FIXME: this should check a handicap. */
  if (!pplayer->ai_controlled && !map_is_known(ptile, pplayer)) {
    return FALSE;
  }

  if (is_ocean_tile(ptile)) {
    /* TODO: perhaps salt water should not be usable for irrigation? */
    return TRUE;
  }

  if (tile_has_special(ptile, S_RIVER)
      || tile_has_special(ptile, S_IRRIGATION)) {
    return TRUE;
  }

  return FALSE;
}

/**************************************************************************
  Returns TRUE if there is an irrigation source adjacent to the given x, y
  position.  This takes player vision into account, but allows the AI to
  cheat. (See is_wet() for the definition of an irrigation source.)

  This function exactly mimics is_water_adjacent_to_tile, except that it
  checks vision.
**************************************************************************/
static bool is_wet_or_is_wet_cardinal_around(struct player *pplayer,
					     struct tile *ptile)
{
  if (is_wet(pplayer, ptile)) {
    return TRUE;
  }

  cardinal_adjc_iterate(ptile, tile1) {
    if (is_wet(pplayer, tile1)) {
      return TRUE;
    }
  } cardinal_adjc_iterate_end;

  return FALSE;
}

/**************************************************************************
  Returns city_tile_value of the best tile worked by or available to pcity.
**************************************************************************/
static int best_worker_tile_value(struct city *pcity)
{
  struct tile *pcenter = city_tile(pcity);
  int best = 0;

  city_tile_iterate(city_map_radius_sq_get(pcity), pcenter, ptile) {
    if (is_free_worked(pcity, ptile)
	|| tile_worked(ptile) == pcity /* quick test */
	|| city_can_work_tile(pcity, ptile)) {
      int tmp = city_tile_value(pcity, ptile, 0, 0);

      if (best < tmp) {
	best = tmp;
      }
    }
  } city_tile_iterate_end;

  return best;
}

/**************************************************************************
  Do all tile improvement calculations and cache them for later.

  These values are used in settler_evaluate_improvements() so this function
  must be called before doing that.  Currently this is only done when handling
  auto-settlers or when the AI contemplates building worker units.
**************************************************************************/
void initialize_infrastructure_cache(struct player *pplayer)
{
  city_list_iterate(pplayer->cities, pcity) {
    struct tile *pcenter = city_tile(pcity);
    int radius_sq = city_map_radius_sq_get(pcity);
    int best = best_worker_tile_value(pcity);

    city_map_iterate(radius_sq, city_index, city_x, city_y) {
      activity_type_iterate(act) {
        ai_city_worker_act_set(pcity, city_index, act, -1);
      } activity_type_iterate_end;
    } city_map_iterate_end;

    city_tile_iterate_index(radius_sq, pcenter, ptile, cindex) {
#ifndef NDEBUG
      struct terrain *old_terrain = tile_terrain(ptile);
      bv_special old_special = ptile->special;
#endif

      ai_city_worker_act_set(pcity, cindex, ACTIVITY_POLLUTION,
        ai_calc_pollution(pcity, best, ptile));
      ai_city_worker_act_set(pcity, cindex, ACTIVITY_FALLOUT,
        ai_calc_fallout(pcity, pplayer, best, ptile));
      ai_city_worker_act_set(pcity, cindex, ACTIVITY_MINE,
        ai_calc_mine(pcity, ptile));
      ai_city_worker_act_set(pcity, cindex, ACTIVITY_IRRIGATE,
        ai_calc_irrigate(pcity, pplayer, ptile));
      ai_city_worker_act_set(pcity, cindex, ACTIVITY_TRANSFORM,
        ai_calc_transform(pcity, ptile));

      /* road_bonus() is handled dynamically later; it takes into
       * account settlers that have already been assigned to building
       * roads this turn. */
      ai_city_worker_act_set(pcity, cindex, ACTIVITY_ROAD,
        ai_calc_road(pcity, pplayer, ptile));
      ai_city_worker_act_set(pcity, cindex, ACTIVITY_RAILROAD,
        ai_calc_railroad(pcity, pplayer, ptile));

      /* Make sure nothing was accidentally changed by these calculations. */
      fc_assert(old_terrain == tile_terrain(ptile)
                && memcmp(&ptile->special, &old_special,
                          sizeof(old_special)) == 0);
    } city_tile_iterate_index_end;
  } city_list_iterate_end;
}

/**************************************************************************
  Returns a measure of goodness of a tile to pcity.

  FIXME: foodneed and prodneed are always 0.
**************************************************************************/
int city_tile_value(struct city *pcity, struct tile *ptile,
                    int foodneed, int prodneed)
{
  int food = city_tile_output_now(pcity, ptile, O_FOOD);
  int shield = city_tile_output_now(pcity, ptile, O_SHIELD);
  int trade = city_tile_output_now(pcity, ptile, O_TRADE);
  int value = 0;

  /* Each food, trade, and shield gets a certain weighting.  We also benefit
   * tiles that have at least one of an item - this promotes balance and 
   * also accounts for INC_TILE effects. */
  value += food * FOOD_WEIGHTING;
  if (food > 0) {
    value += FOOD_WEIGHTING / 2;
  }
  value += shield * SHIELD_WEIGHTING;
  if (shield > 0) {
    value += SHIELD_WEIGHTING / 2;
  }
  value += trade * TRADE_WEIGHTING;
  if (trade > 0) {
    value += TRADE_WEIGHTING / 2;
  }

  return value;
}

/**************************************************************************
  Set the value for activity 'doing' on tile 'city_tile_index' of
  city 'pcity'.
**************************************************************************/
void ai_city_worker_act_set(struct city *pcity, int city_tile_index,
                            enum unit_activity act_id, int value)
{
  if (pcity->server.ai->act_cache_radius_sq
      != city_map_radius_sq_get(pcity)) {
    log_debug("update activity cache for %s: radius_sq changed from "
              "%d to %d", city_name(pcity),
              pcity->server.ai->act_cache_radius_sq,
              city_map_radius_sq_get(pcity));
    ai_city_update(pcity);
  }

  fc_assert_ret(NULL != pcity);
  fc_assert_ret(NULL != pcity->server.ai);
  fc_assert_ret(NULL != pcity->server.ai->act_cache);
  fc_assert_ret(pcity->server.ai->act_cache_radius_sq
                == city_map_radius_sq_get(pcity));
  fc_assert_ret(city_tile_index < city_map_tiles_from_city(pcity));

  (pcity->server.ai->act_cache[city_tile_index]).act[act_id] = value;
}

/**************************************************************************
  Return the value for activity 'doing' on tile 'city_tile_index' of
  city 'pcity'.
**************************************************************************/
int ai_city_worker_act_get(const struct city *pcity, int city_tile_index,
                           enum unit_activity act_id)
{
  fc_assert_ret_val(NULL != pcity, 0);
  fc_assert_ret_val(NULL != pcity->server.ai, 0);
  fc_assert_ret_val(NULL != pcity->server.ai->act_cache, 0);
  fc_assert_ret_val(pcity->server.ai->act_cache_radius_sq
                     == city_map_radius_sq_get(pcity), 0);
  fc_assert_ret_val(city_tile_index < city_map_tiles_from_city(pcity), 0);

  return (pcity->server.ai->act_cache[city_tile_index]).act[act_id];
}

/**************************************************************************
  Update the memory allocated for AI city handling.
**************************************************************************/
void ai_city_update(struct city *pcity)
{
  int radius_sq = city_map_radius_sq_get(pcity);

  fc_assert_ret(NULL != pcity);
  fc_assert_ret(NULL != pcity->server.ai);

  /* initialize act_cache if needed */
  if (pcity->server.ai->act_cache == NULL
      || pcity->server.ai->act_cache_radius_sq == -1
      || pcity->server.ai->act_cache_radius_sq != radius_sq) {
    pcity->server.ai->act_cache
      = fc_realloc(pcity->server.ai->act_cache,
                   city_map_tiles(radius_sq)
                   * sizeof(*(pcity->server.ai->act_cache)));
    /* initialize with 0 */
    memset(pcity->server.ai->act_cache, 0,
           city_map_tiles(radius_sq)
           * sizeof(*(pcity->server.ai->act_cache)));
    pcity->server.ai->act_cache_radius_sq = radius_sq;
  }
}
