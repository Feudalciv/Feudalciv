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
#include <fc_config.h>
#endif

/* utility */
#include "astring.h"
#include "bitvector.h"
#include "fcintl.h"
#include "mem.h"
#include "shared.h"
#include "support.h"

/* common */
#include "ai.h"
#include "base.h"
#include "city.h"
#include "game.h"
#include "log.h"
#include "map.h"
#include "movement.h"
#include "packets.h"
#include "player.h"
#include "road.h"
#include "tech.h"
#include "traderoutes.h"
#include "unitlist.h"

#include "unit.h"

static bool is_real_activity(enum unit_activity activity);

Activity_type_id real_activities[ACTIVITY_LAST];

struct cargo_iter {
  struct iterator vtable;
  const struct unit_list_link *links[GAME_TRANSPORT_MAX_RECURSIVE];
  int depth;
};
#define CARGO_ITER(iter) ((struct cargo_iter *) (iter))

/**************************************************************************
bribe unit
investigate
poison
make revolt
establish embassy
sabotage city
**************************************************************************/

/**************************************************************************
Whether a diplomat can move to a particular tile and perform a
particular action there.
**************************************************************************/
bool diplomat_can_do_action(const struct unit *pdiplomat,
			    enum diplomat_actions action, 
			    const struct tile *ptile)
{
  if (!is_diplomat_action_available(pdiplomat, action, ptile)) {
    return FALSE;
  }

  if (!is_tiles_adjacent(unit_tile(pdiplomat), ptile)
      && !same_pos(unit_tile(pdiplomat), ptile)) {
    return FALSE;
  }

  if(pdiplomat->moves_left == 0)
    return FALSE;

  return TRUE;
}

/**************************************************************************
Whether a diplomat can perform a particular action at a particular
tile.  This does _not_ check whether the diplomat can move there.
If the action is DIPLOMAT_ANY_ACTION, checks whether there is any
action the diplomat can perform at the tile.
**************************************************************************/
bool is_diplomat_action_available(const struct unit *pdiplomat,
				  enum diplomat_actions action, 
				  const struct tile *ptile)
{
  struct city *pcity=tile_city(ptile);

  if (action != DIPLOMAT_MOVE
      && !can_unit_exist_at_tile(pdiplomat, unit_tile(pdiplomat))) {
    return FALSE;
  }

  if (pcity) {
    if (city_owner(pcity) != unit_owner(pdiplomat)
       && real_map_distance(unit_tile(pdiplomat), pcity->tile) <= 1) {
      if(action==DIPLOMAT_SABOTAGE)
	return pplayers_at_war(unit_owner(pdiplomat), city_owner(pcity));
      if(action==DIPLOMAT_MOVE)
        return pplayers_allied(unit_owner(pdiplomat), city_owner(pcity));
      if (action == DIPLOMAT_EMBASSY
          && !get_player_bonus(city_owner(pcity), EFT_NO_DIPLOMACY)
          && !player_has_real_embassy(unit_owner(pdiplomat),
                                      city_owner(pcity))) {
        return TRUE;
      }
      if(action == SPY_POISON
         && city_size_get(pcity) > 1
         && unit_has_type_flag(pdiplomat, UTYF_SPY)) {
        return pplayers_at_war(unit_owner(pdiplomat), city_owner(pcity));
      }
      if(action==DIPLOMAT_INVESTIGATE)
        return TRUE;
      if (action == DIPLOMAT_STEAL && !is_barbarian(city_owner(pcity))) {
	return TRUE;
      }
      if(action==DIPLOMAT_INCITE)
        return !pplayers_allied(city_owner(pcity), unit_owner(pdiplomat));
      if(action==DIPLOMAT_ANY_ACTION)
        return TRUE;
    }
  } else { /* Action against a unit at a tile */
    /* If it is made possible to do action against allied units
       unit_move_handling() should be changed so that pdefender
       is also set to allied units */
    struct unit *punit;

    if ((action == SPY_SABOTAGE_UNIT || action == DIPLOMAT_ANY_ACTION) 
        && unit_list_size(ptile->units) == 1
        && unit_has_type_flag(pdiplomat, UTYF_SPY)) {
      punit = unit_list_get(ptile->units, 0);
      if (pplayers_at_war(unit_owner(pdiplomat), unit_owner(punit))) {
        return TRUE;
      }
    }

    if ((action == DIPLOMAT_BRIBE || action == DIPLOMAT_ANY_ACTION)
        && unit_list_size(ptile->units) == 1) {
      punit = unit_list_get(ptile->units, 0);
      if (!pplayers_allied(unit_owner(punit), unit_owner(pdiplomat))) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

/****************************************************************************
  Determines if punit can be airlifted to dest_city now!  So punit needs
  to be in a city now.
  If pdest_city is NULL, just indicate whether it's possible for the unit
  to be airlifted at all from its current position.
  The 'restriction' parameter specifies which player's knowledge this is
  based on -- one player can't see whether another's cities are currently
  able to airlift.  (Clients other than global observers should only call
  this with a non-NULL 'restriction'.)
****************************************************************************/
enum unit_airlift_result
    test_unit_can_airlift_to(const struct player *restriction,
                             const struct unit *punit,
                             const struct city *pdest_city)
{
  const struct city *psrc_city = tile_city(unit_tile(punit));
  const struct player *punit_owner;
  enum unit_airlift_result ok_result = AR_OK;

  if (0 == punit->moves_left) {
    /* No moves left. */
    return AR_NO_MOVES;
  }

  if (!is_ground_unit(punit)) {
    /* Only land units can be airlifted currently. */
    return AR_WRONG_UNITTYPE;
  }

  if (0 < get_transporter_occupancy(punit)) {
    /* Units with occupants can't be airlifted currently. */
    return AR_OCCUPIED;
  }

  if (NULL == psrc_city) {
    /* No city there. */
    return AR_NOT_IN_CITY;
  }

  if (psrc_city == pdest_city) {
    /* Airlifting to our current position doesn't make sense. */
    return AR_BAD_DST_CITY;
  }

  punit_owner = unit_owner(punit);

  /* Check validity of both source and destination before checking capacity,
   * to avoid misleadingly optimistic returns. */

  if (punit_owner != city_owner(psrc_city)
      && !(game.info.airlifting_style & AIRLIFTING_ALLIED_SRC
           && pplayers_allied(punit_owner, city_owner(psrc_city)))) {
    /* Not allowed to airlift from this source. */
    return AR_BAD_SRC_CITY;
  }

  if (pdest_city &&
      punit_owner != city_owner(pdest_city)
      && !(game.info.airlifting_style & AIRLIFTING_ALLIED_DEST
           && pplayers_allied(punit_owner, city_owner(pdest_city)))) {
    /* Not allowed to airlift to this destination. */
    return AR_BAD_DST_CITY;
  }

  if (NULL == restriction || city_owner(psrc_city) == restriction) {
    /* We know for sure whether or not src can airlift this turn. */
    if (0 >= psrc_city->airlift) {
      /* The source cannot airlift for this turn (maybe already airlifted
       * or no airport).
       *
       * Note that (game.info.airlifting_style & AIRLIFTING_UNLIMITED_SRC)
       * is not handled here because it always needs an airport to airlift.
       * See also do_airline() in server/unittools.h. */
      return AR_SRC_NO_FLIGHTS;
    } /* else, there is capacity; continue to other checks */
  } else {
    /* We don't have access to the 'airlift' field. Assume it's OK; can
     * only find out for sure by trying it. */
    ok_result = AR_OK_SRC_UNKNOWN;
  }

  if (pdest_city) {
    if (NULL == restriction || city_owner(pdest_city) == restriction) {
      if (0 >= pdest_city->airlift
          && !(game.info.airlifting_style & AIRLIFTING_UNLIMITED_DEST)) {
        /* The destination cannot support airlifted units for this turn
         * (maybe already airlifed or no airport).
         * See also do_airline() in server/unittools.h. */
        return AR_DST_NO_FLIGHTS;
      } /* else continue */
    } else {
      ok_result = AR_OK_DST_UNKNOWN;
    }
  }

  return ok_result;
}

/****************************************************************************
  Encapsulates whether a return from test_unit_can_airlift_to() should be
  treated as a successful result.
****************************************************************************/
bool is_successful_airlift_result(enum unit_airlift_result result)
{
  switch (result) {
  case AR_OK:
  case AR_OK_SRC_UNKNOWN:
  case AR_OK_DST_UNKNOWN:
    return TRUE;
  default:  /* everything else is failure */
    return FALSE;
  }
}

/****************************************************************************
  Determines if punit can be airlifted to dest_city now!  So punit needs
  to be in a city now.
  On the server this gives correct information; on the client it errs on the
  side of saying airlifting is possible even if it's not certain given
  player knowledge.
****************************************************************************/
bool unit_can_airlift_to(const struct unit *punit,
                         const struct city *pdest_city)
{
  /* FIXME: really we want client_player(), not unit_owner(). */
  struct player *restriction = is_server() ? NULL : unit_owner(punit);
  fc_assert_ret_val(pdest_city, FALSE);
  return is_successful_airlift_result(
      test_unit_can_airlift_to(restriction, punit, pdest_city));
}

/****************************************************************************
  Return TRUE iff the unit is following client-side orders.
****************************************************************************/
bool unit_has_orders(const struct unit *punit)
{
  return punit->has_orders;
}

/**************************************************************************
  Return TRUE iff this unit can be disbanded at the given city to get full
  shields for building a wonder.
**************************************************************************/
bool unit_can_help_build_wonder(const struct unit *punit,
				const struct city *pcity)
{
  if (!is_tiles_adjacent(unit_tile(punit), pcity->tile)
      && !same_pos(unit_tile(punit), pcity->tile)) {
    return FALSE;
  }

  return (unit_has_type_flag(punit, UTYF_HELP_WONDER)
	  && unit_owner(punit) == city_owner(pcity)
	  && VUT_IMPROVEMENT == pcity->production.kind
	  && is_wonder(pcity->production.value.building)
	  && (pcity->shield_stock
	      < impr_build_shield_cost(pcity->production.value.building)));
}


/**************************************************************************
  Return TRUE iff this unit can be disbanded at its current position to
  get full shields for building a wonder.
**************************************************************************/
bool unit_can_help_build_wonder_here(const struct unit *punit)
{
  struct city *pcity = tile_city(unit_tile(punit));

  return pcity && unit_can_help_build_wonder(punit, pcity);
}


/**************************************************************************
  Return TRUE iff this unit can be disbanded at its current location to
  provide a trade route from the homecity to the target city.
**************************************************************************/
bool unit_can_est_trade_route_here(const struct unit *punit)
{
  struct city *phomecity, *pdestcity;

  return (unit_has_type_flag(punit, UTYF_TRADE_ROUTE)
          && (pdestcity = tile_city(unit_tile(punit)))
          && (phomecity = game_city_by_number(punit->homecity))
          && can_cities_trade(phomecity, pdestcity));
}

/**************************************************************************
  Return the number of units the transporter can hold (or 0).
**************************************************************************/
int get_transporter_capacity(const struct unit *punit)
{
  return unit_type(punit)->transport_capacity;
}

/**************************************************************************
  Is the unit capable of attacking?
**************************************************************************/
bool is_attack_unit(const struct unit *punit)
{
  return (unit_type(punit)->attack_strength > 0);
}

/**************************************************************************
  Military units are capable of enforcing martial law. Military ground
  and heli units can occupy empty cities -- see unit_can_take_over(punit).
  Some military units, like the Galleon, have no attack strength.
**************************************************************************/
bool is_military_unit(const struct unit *punit)
{
  return !unit_has_type_flag(punit, UTYF_CIVILIAN);
}

/**************************************************************************
  Return TRUE iff this unit is a diplomat (spy) unit.  Diplomatic units
  can do diplomatic actions (not to be confused with diplomacy).
**************************************************************************/
bool is_diplomat_unit(const struct unit *punit)
{
  return (unit_has_type_flag(punit, UTYF_DIPLOMAT));
}

/**************************************************************************
  Return TRUE iff this tile is threatened from any unit within 2 tiles.
**************************************************************************/
bool is_square_threatened(const struct player *pplayer,
			  const struct tile *ptile)
{
  square_iterate(ptile, 2, ptile1) {
    unit_list_iterate(ptile1->units, punit) {
      if (((pplayer->ai_controlled
            && !ai_handicap(pplayer, H_FOG))
           || can_player_see_unit(pplayer, punit))
          && pplayers_at_war(pplayer, unit_owner(punit))
          && (is_diplomat_unit(punit)
              || (is_military_unit(punit) && is_attack_unit(punit)))
          && (is_native_tile(unit_type(punit), ptile)
              || (can_attack_non_native(unit_type(punit))
                  && is_native_near_tile(unit_class(punit), ptile)))) {
	return TRUE;
      }
    } unit_list_iterate_end;
  } square_iterate_end;

  return FALSE;
}

/**************************************************************************
  This checks the "field unit" flag on the unit.  Field units cause
  unhappiness (under certain governments) even when they aren't abroad.
**************************************************************************/
bool is_field_unit(const struct unit *punit)
{
  return unit_has_type_flag(punit, UTYF_FIELDUNIT);
}


/**************************************************************************
  Is the unit one that is invisible on the map. A unit is invisible if
  it has the UTYF_PARTIAL_INVIS flag or if it transported by a unit with
  this flag.
**************************************************************************/
bool is_hiding_unit(const struct unit *punit)
{
  return (unit_has_type_flag(punit, UTYF_PARTIAL_INVIS)
          || (unit_transported(punit)
              && unit_has_type_flag(unit_transport_get(punit),
                                    UTYF_PARTIAL_INVIS)));
}

/**************************************************************************
  Return TRUE iff an attack from this unit would kill a citizen in a city
  (city walls protect against this).
**************************************************************************/
bool kills_citizen_after_attack(const struct unit *punit)
{
  return game.info.killcitizen
    && uclass_has_flag(unit_class(punit), UCF_KILLCITIZEN);
}

/****************************************************************************
  Return TRUE iff this unit may be disbanded to add its pop_cost to a
  city at its current location.
****************************************************************************/
bool unit_can_add_to_city(const struct unit *punit)
{
  return (UAB_ADD_OK == unit_add_or_build_city_test(punit));
}

/****************************************************************************
  Return TRUE iff this unit is capable of building a new city at its
  current location.
****************************************************************************/
bool unit_can_build_city(const struct unit *punit)
{
  return (UAB_BUILD_OK == unit_add_or_build_city_test(punit));
}

/****************************************************************************
  Return TRUE iff this unit can add to a current city or build a new city
  at its current location.
****************************************************************************/
bool unit_can_add_or_build_city(const struct unit *punit)
{
  enum unit_add_build_city_result res = unit_add_or_build_city_test(punit);

  return (UAB_BUILD_OK == res || UAB_ADD_OK == res);
}

/****************************************************************************
  See if the unit can add to an existing city or build a new city at
  its current location, and return a 'result' value telling what is
  allowed.
****************************************************************************/
enum unit_add_build_city_result
unit_add_or_build_city_test(const struct unit *punit)
{
  struct tile *ptile = unit_tile(punit);
  struct city *pcity = tile_city(ptile);
  bool is_build = unit_has_type_flag(punit, UTYF_CITIES);
  bool is_add = unit_has_type_flag(punit, UTYF_ADD_TO_CITY);
  int new_pop;

  /* Test if we can build. */
  if (NULL == pcity) {
    if (!is_build) {
      return UAB_NOT_BUILD_UNIT;
    }
    if (punit->moves_left == 0) {
      return UAB_NO_MOVES_BUILD;
    }
    switch (city_build_here_test(ptile, punit)) {
    case CB_OK:
      return UAB_BUILD_OK;
    case CB_BAD_CITY_TERRAIN:
      return UAB_BAD_CITY_TERRAIN;
    case CB_BAD_UNIT_TERRAIN:
      return UAB_BAD_UNIT_TERRAIN;
    case CB_BAD_BORDERS:
      return UAB_BAD_BORDERS;
    case CB_NO_MIN_DIST:
      return UAB_NO_MIN_DIST;
    }
    log_error("%s(): Internal error.", __FUNCTION__);
    return UAB_NO_MOVES_BUILD; /* Returns something prohibitive. */
  }

  /* Test if we can add. */
  if (!is_add) {
    return UAB_NOT_ADDABLE_UNIT;
  }
  if (punit->moves_left == 0) {
    return UAB_NO_MOVES_ADD;
  }

  fc_assert(unit_pop_value(punit) > 0);
  new_pop = city_size_get(pcity) + unit_pop_value(punit);

  if (new_pop > game.info.add_to_size_limit) {
    return UAB_TOO_BIG;
  }
  if (city_owner(pcity) != unit_owner(punit)) {
    return UAB_NOT_OWNER;
  }
  if (!city_can_grow_to(pcity, new_pop)) {
    return UAB_NO_SPACE;
  }
  return UAB_ADD_OK;
}

/**************************************************************************
  Return TRUE iff the unit can change homecity to the given city.
**************************************************************************/
bool can_unit_change_homecity_to(const struct unit *punit,
				 const struct city *pcity)
{
  struct city *acity = tile_city(unit_tile(punit));

  /* Requirements to change homecity:
   *
   * 1. Homeless units can't change homecity (this is a feature since
   *    being homeless is a big benefit).
   * 2. The unit must be inside the city it is rehoming to.
   * 3. Of course you can only have your own cities as homecity.
   * 4. You can't rehome to the current homecity. */
  return (punit && pcity
	  && punit->homecity > 0
	  && acity
	  && city_owner(acity) == unit_owner(punit)
	  && punit->homecity != acity->id);
}

/**************************************************************************
  Return TRUE iff the unit can change homecity at its current location.
**************************************************************************/
bool can_unit_change_homecity(const struct unit *punit)
{
  return can_unit_change_homecity_to(punit, tile_city(unit_tile(punit)));
}

/**************************************************************************
  Returns the speed of a unit doing an activity.  This depends on the
  veteran level and the base move_rate of the unit (regardless of HP or
  effects).  Usually this is just used for settlers but the value is also
  used for military units doing fortify/pillage activities.

  The speed is multiplied by ACTIVITY_COUNT.
**************************************************************************/
int get_activity_rate(const struct unit *punit)
{
  const struct veteran_level *vlevel;

  fc_assert_ret_val(punit != NULL, 0);

  vlevel = utype_veteran_level(unit_type(punit), punit->veteran);
  fc_assert_ret_val(vlevel != NULL, 0);

  /* The speed of the settler depends on its base move_rate, not on
   * the number of moves actually remaining or the adjusted move rate.
   * This means sea formers won't have their activity rate increased by
   * Magellan's, and it means injured units work just as fast as
   * uninjured ones.  Note the value is never less than SINGLE_MOVE. */
  int move_rate = unit_type(punit)->move_rate;

  /* All settler actions are multiplied by ACTIVITY_COUNT. */
  return ACTIVITY_FACTOR
         * (float)vlevel->power_fact / 100
         * move_rate / SINGLE_MOVE;
}

/**************************************************************************
  Returns the amount of work a unit does (will do) on an activity this
  turn.  Units that have no MP do no work.

  The speed is multiplied by ACTIVITY_COUNT.
**************************************************************************/
int get_activity_rate_this_turn(const struct unit *punit)
{
  /* This logic is also coded in client/goto.c. */
  if (punit->moves_left > 0) {
    return get_activity_rate(punit);
  } else {
    return 0;
  }
}

/**************************************************************************
  Return the estimated number of turns for the worker unit to start and
  complete the activity at the given location.  This assumes no other
  worker units are helping out, and doesn't take account of any work
  already done by this unit.
**************************************************************************/
int get_turns_for_activity_at(const struct unit *punit,
			      enum unit_activity activity,
			      const struct tile *ptile)
{
  /* FIXME: This is just an approximation since we don't account for
   * get_activity_rate_this_turn. */
  int speed = get_activity_rate(punit);
  int time = tile_activity_time(activity, ptile);

  if (time >= 0 && speed >= 0) {
    return (time - 1) / speed + 1; /* round up */
  } else {
    return FC_INFINITY;
  }
}

/**************************************************************************
  Return the estimated number of turns for the worker unit to start and
  complete the road at the given location.  This assumes no other
  worker units are helping out, and doesn't take account of any work
  already done by this unit.
**************************************************************************/
int get_turns_for_road_at(const struct unit *punit,
			  const struct road_type *proad,
			  const struct tile *ptile)
{
  /* FIXME: This is just an approximation since we don't account for
   * get_activity_rate_this_turn. */
  int speed = get_activity_rate(punit);
  int time = tile_activity_road_time(ptile, road_number(proad));

  if (time >= 0 && speed >= 0) {
    return (time - 1) / speed + 1; /* round up */
  } else {
    return FC_INFINITY;
  }
}

/**************************************************************************
  Return the estimated number of turns for the worker unit to start and
  complete the base road at the given location.  This assumes no other
  worker units are helping out, and doesn't take account of any work
  already done by this unit.
**************************************************************************/
int get_turns_for_base_at(const struct unit *punit,
                          const struct base_type *pbase,
                          const struct tile *ptile)
{
  /* FIXME: This is just an approximation since we don't account for
   * get_activity_rate_this_turn. */
  int speed = get_activity_rate(punit);
  int time = tile_activity_base_time(ptile, base_number(pbase));

  if (time >= 0 && speed >= 0) {
    return (time - 1) / speed + 1; /* round up */
  } else {
    return FC_INFINITY;
  }
}

/**************************************************************************
  Return TRUE if activity requires some sort of target to be specified.
**************************************************************************/
bool activity_requires_target(enum unit_activity activity)
{
  switch (activity) {
  case ACTIVITY_PILLAGE:
  case ACTIVITY_BASE:
  case ACTIVITY_GEN_ROAD:
    return TRUE;
  case ACTIVITY_IDLE:
  case ACTIVITY_POLLUTION:
  case ACTIVITY_MINE:
  case ACTIVITY_IRRIGATE:
  case ACTIVITY_FORTIFIED:
  case ACTIVITY_SENTRY:
  case ACTIVITY_GOTO:
  case ACTIVITY_EXPLORE:
  case ACTIVITY_TRANSFORM:
  case ACTIVITY_FORTIFYING:
  case ACTIVITY_FALLOUT:
  case ACTIVITY_CONVERT:
    return FALSE;
  /* These shouldn't be kicking around internally. */
  case ACTIVITY_FORTRESS:
  case ACTIVITY_AIRBASE:
  case ACTIVITY_PATROL_UNUSED:
  default:
    fc_assert_ret_val(FALSE, FALSE);
  }

  return FALSE;
}

/**************************************************************************
  Return whether the unit can be put in auto-settler mode.

  NOTE: we used to have "auto" mode including autosettlers and auto-attack.
  This was bad because the two were indestinguishable even though they
  are very different.  Now auto-attack is done differently so we just have
  auto-settlers.  If any new auto modes are introduced they should be
  handled separately.
**************************************************************************/
bool can_unit_do_autosettlers(const struct unit *punit) 
{
  return unit_has_type_flag(punit, UTYF_SETTLERS);
}

/**************************************************************************
  Setup array of real activities
**************************************************************************/
void setup_real_activities_array(void)
{
  Activity_type_id act;
  int i = 0;

  for (act = 0; act < ACTIVITY_LAST; act++) {
    if (is_real_activity(act)) {
      real_activities[i++] = act;
    }
  }

  real_activities[i] = ACTIVITY_LAST;
}

/**************************************************************************
  Return if given activity really is in game. For savegame compatibility
  activity enum cannot be reordered and there is holes in it.
**************************************************************************/
static bool is_real_activity(enum unit_activity activity)
{
  /* ACTIVITY_FORTRESS, ACTIVITY_AIRBASE, ACTIVITY_OLD_ROAD, and
   * ACTIVITY_OLD_RAILROAD are deprecated */
  return (0 <= activity && activity < ACTIVITY_LAST)
          && activity != ACTIVITY_FORTRESS
          && activity != ACTIVITY_AIRBASE
          && activity != ACTIVITY_OLD_ROAD
          && activity != ACTIVITY_OLD_RAILROAD
          && activity != ACTIVITY_UNKNOWN
          && activity != ACTIVITY_PATROL_UNUSED;
}

/**************************************************************************
  Return the name of the activity in a static buffer.
**************************************************************************/
const char *get_activity_text(enum unit_activity activity)
{
  /* The switch statement has just the activities listed with no "default"
   * handling.  This enables the compiler to detect missing entries
   * automatically, and still handles everything correctly. */
  switch (activity) {
  case ACTIVITY_IDLE:
    return _("Idle");
  case ACTIVITY_POLLUTION:
    return _("Pollution");
  case ACTIVITY_MINE:
    return _("Mine");
  case ACTIVITY_IRRIGATE:
    return _("Irrigation");
  case ACTIVITY_FORTIFYING:
    return _("Fortifying");
  case ACTIVITY_FORTIFIED:
    return _("Fortified");
  case ACTIVITY_SENTRY:
    return _("Sentry");
  case ACTIVITY_PILLAGE:
    return _("Pillage");
  case ACTIVITY_GOTO:
    return _("Goto");
  case ACTIVITY_EXPLORE:
    return _("Explore");
  case ACTIVITY_TRANSFORM:
    return _("Transform");
  case ACTIVITY_FALLOUT:
    return _("Fallout");
  case ACTIVITY_BASE:
    return _("Base");
  case ACTIVITY_GEN_ROAD:
    return _("Road");
  case ACTIVITY_CONVERT:
    return _("Convert");
  case ACTIVITY_OLD_ROAD:
  case ACTIVITY_OLD_RAILROAD:
  case ACTIVITY_FORTRESS:
  case ACTIVITY_AIRBASE:
  case ACTIVITY_UNKNOWN:
  case ACTIVITY_PATROL_UNUSED:
  case ACTIVITY_LAST:
    break;
  }

  fc_assert(FALSE);
  return _("Unknown");
}

/****************************************************************************
  Return TRUE iff the given unit could be loaded into the transporter
  if we moved there.
****************************************************************************/
bool could_unit_load(const struct unit *pcargo, const struct unit *ptrans)
{
  if (!pcargo || !ptrans || pcargo == ptrans) {
    return FALSE;
  }

  /* Double-check ownership of the units: you can load into an allied unit
   * (of course only allied units can be on the same tile). */
  if (!pplayers_allied(unit_owner(pcargo), unit_owner(ptrans))) {
    return FALSE;
  }

  /* Make sure this transporter can carry this type of unit. */
  if (!can_unit_transport(ptrans, pcargo)) {
    return FALSE;
  }

  /* Un-embarkable transport must be in city or base to load cargo. */
  if (!utype_can_freely_load(unit_type(pcargo), unit_type(ptrans))
      && !tile_city(unit_tile(ptrans))
      && !tile_has_native_base(unit_tile(ptrans), unit_type(ptrans))) {
    return FALSE;
  }

  /* Make sure there's room in the transporter. */
  if (get_transporter_occupancy(ptrans)
      >= get_transporter_capacity(ptrans)) {
    return FALSE;
  }

  /* Check iff this is a valid transport. */
  if (!unit_transport_check(pcargo, ptrans)) {
    return FALSE;
  }

  /* Check transport depth. */
  if (GAME_TRANSPORT_MAX_RECURSIVE
      < 1 + unit_transport_depth(ptrans) + unit_cargo_depth(pcargo)) {
    return FALSE;
  }

  return TRUE;
}

/****************************************************************************
  Return TRUE iff the given unit can be loaded into the transporter.
****************************************************************************/
bool can_unit_load(const struct unit *pcargo, const struct unit *ptrans)
{
  /* This function needs to check EVERYTHING. */

  /* Check positions of the units.  Of course you can't load a unit onto
   * a transporter on a different tile... */
  if (!same_pos(unit_tile(pcargo), unit_tile(ptrans))) {
    return FALSE;
  }

  /* Cannot load if cargo is already loaded onto something else. */
  if (unit_transported(pcargo)) {
    return FALSE;
  }

  return could_unit_load(pcargo, ptrans);
}

/****************************************************************************
  Return TRUE iff the given unit can be unloaded from its current
  transporter.

  This function checks everything *except* the legality of the position
  after the unloading.  The caller may also want to call
  can_unit_exist_at_tile() to check this, unless the unit is unloading and
  moving at the same time.
****************************************************************************/
bool can_unit_unload(const struct unit *pcargo, const struct unit *ptrans)
{
  if (!pcargo || !ptrans) {
    return FALSE;
  }

  /* Make sure the unit's transporter exists and is known. */
  if (unit_transport_get(pcargo) != ptrans) {
    return FALSE;
  }

  /* Un-disembarkable transport must be in city or base to unload cargo. */
  if (!utype_can_freely_unload(unit_type(pcargo), unit_type(ptrans))
      && !tile_city(unit_tile(ptrans))
      && !tile_has_native_base(unit_tile(ptrans), unit_type(ptrans))) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
  Return whether the unit can be paradropped - that is, if the unit is in
  a friendly city or on an airbase special, has enough movepoints left, and
  has not paradropped yet this turn.
**************************************************************************/
bool can_unit_paradrop(const struct unit *punit)
{
  struct unit_type *utype;

  if (!unit_has_type_flag(punit, UTYF_PARATROOPERS))
    return FALSE;

  if(punit->paradropped)
    return FALSE;

  utype = unit_type(punit);

  if(punit->moves_left < utype->paratroopers_mr_req)
    return FALSE;

  if (tile_has_base_flag(unit_tile(punit), BF_PARADROP_FROM)) {
    /* Paradrop has to be possible from non-native base.
     * Paratroopers are "Land" units, but they can paradrom from Airbase. */
    return TRUE;
  }

  if (!tile_city(unit_tile(punit))) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
  Return whether the unit can bombard.
  Basically if it is a bombarder, isn't being transported, and hasn't 
  moved this turn.
**************************************************************************/
bool can_unit_bombard(const struct unit *punit)
{
  if (!unit_has_type_flag(punit, UTYF_BOMBARDER)) {
    return FALSE;
  }

  if (unit_transported(punit)) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
  Compare if action targets are identical
**************************************************************************/
bool cmp_act_tgt(struct act_tgt *act1, struct act_tgt *act2)
{
  if (act1->type != act2->type) {
    return FALSE;
  }

  switch (act1->type) {
    case ATT_SPECIAL:
      return act1->obj.spe == act2->obj.spe;
    case ATT_BASE:
      return act1->obj.base == act2->obj.base;
    case ATT_ROAD:
      return act1->obj.road == act2->obj.road;
  }

  fc_assert(FALSE);
  return FALSE;
}

/**************************************************************************
  Check if the unit's current activity is actually legal.
**************************************************************************/
bool can_unit_continue_current_activity(struct unit *punit)
{
  enum unit_activity current = punit->activity;
  struct act_tgt target = punit->activity_target;
  enum unit_activity current2 = 
              (current == ACTIVITY_FORTIFIED) ? ACTIVITY_FORTIFYING : current;
  bool result;

  punit->activity = ACTIVITY_IDLE;
  punit->activity_target.type = ATT_SPECIAL;
  punit->activity_target.obj.spe = S_LAST;

  result = can_unit_do_activity_targeted(punit, current2, &target);

  punit->activity = current;
  punit->activity_target = target;

  return result;
}

/**************************************************************************
  Return TRUE iff the unit can do the given untargeted activity at its
  current location.

  Note that some activities must be targeted; see
  can_unit_do_activity_targeted.
**************************************************************************/
bool can_unit_do_activity(const struct unit *punit,
                          enum unit_activity activity)
{
  struct act_tgt target = { .type = ATT_SPECIAL, .obj.spe = S_LAST };

  return can_unit_do_activity_targeted(punit, activity, &target);
}

/**************************************************************************
  Return TRUE iff the unit can do the given base building activity at its
  current location.
**************************************************************************/
bool can_unit_do_activity_base(const struct unit *punit,
                               Base_type_id base)
{
  struct act_tgt target = { .type = ATT_BASE, .obj.base = base };

  return can_unit_do_activity_targeted(punit, ACTIVITY_BASE, &target);
}

/**************************************************************************
  Return TRUE iff the unit can do the given road building activity at its
  current location.
**************************************************************************/
bool can_unit_do_activity_road(const struct unit *punit,
                               Road_type_id road)
{
  struct act_tgt target = { .type = ATT_ROAD, .obj.road = road };

  return can_unit_do_activity_targeted(punit, ACTIVITY_GEN_ROAD, &target);
}

/**************************************************************************
  Return whether the unit can do the targeted activity at its current
  location.
**************************************************************************/
bool can_unit_do_activity_targeted(const struct unit *punit,
				   enum unit_activity activity,
                                   struct act_tgt *target)
{
  return can_unit_do_activity_targeted_at(punit, activity, target,
					  unit_tile(punit));
}

/**************************************************************************
  Return TRUE if the unit can do the targeted activity at the given
  location.
**************************************************************************/
bool can_unit_do_activity_targeted_at(const struct unit *punit,
				      enum unit_activity activity,
                                      struct act_tgt *target,
				      const struct tile *ptile)
{
  struct player *pplayer = unit_owner(punit);
  struct terrain *pterrain = tile_terrain(ptile);
  struct unit_class *pclass = unit_class(punit);

  switch(activity) {
  case ACTIVITY_IDLE:
  case ACTIVITY_GOTO:
    return TRUE;

  case ACTIVITY_POLLUTION:
    return (unit_has_type_flag(punit, UTYF_SETTLERS)
	    && tile_has_special(ptile, S_POLLUTION));

  case ACTIVITY_FALLOUT:
    return (unit_has_type_flag(punit, UTYF_SETTLERS)
	    && tile_has_special(ptile, S_FALLOUT));

  case ACTIVITY_MINE:
    /* Don't allow it if someone else is irrigating this tile.
     * *Do* allow it if they're transforming - the mine may survive */
    if (unit_has_type_flag(punit, UTYF_SETTLERS)
	&& ((pterrain == pterrain->mining_result
	     && !tile_has_special(ptile, S_MINE)
             && get_tile_bonus(ptile, punit, EFT_MINING_POSSIBLE) > 0)
	    || (pterrain != pterrain->mining_result
		&& pterrain->mining_result != T_NONE
                && get_tile_bonus(ptile, punit, EFT_MINING_TF_POSSIBLE) > 0
		&& (!is_ocean(pterrain)
		    || is_ocean(pterrain->mining_result)
		    || can_reclaim_ocean(ptile))
		&& (is_ocean(pterrain)
		    || !is_ocean(pterrain->mining_result)
		    || can_channel_land(ptile))
                && (!terrain_has_flag(pterrain->mining_result, TER_NO_CITIES)
		    || !tile_city(ptile))))) {
      unit_list_iterate(ptile->units, tunit) {
	if (tunit->activity == ACTIVITY_IRRIGATE) {
	  return FALSE;
	}
      } unit_list_iterate_end;
      return TRUE;
    } else {
      return FALSE;
    }

  case ACTIVITY_IRRIGATE:
    /* Don't allow it if someone else is mining this tile.
     * *Do* allow it if they're transforming - the irrigation may survive */
    if (unit_has_type_flag(punit, UTYF_SETTLERS)
	&& (!tile_has_special(ptile, S_IRRIGATION)
	    || (!tile_has_special(ptile, S_FARMLAND)
		&& player_knows_techs_with_flag(pplayer, TF_FARMLAND)))
	&& ((pterrain == pterrain->irrigation_result
             && can_be_irrigated(ptile, punit))
	    || (pterrain != pterrain->irrigation_result
		&& pterrain->irrigation_result != T_NONE
                && get_tile_bonus(ptile, punit, EFT_IRRIG_TF_POSSIBLE) > 0
		&& (!is_ocean(pterrain)
		    || is_ocean(pterrain->irrigation_result)
		    || can_reclaim_ocean(ptile))
		&& (is_ocean(pterrain)
		    || !is_ocean(pterrain->irrigation_result)
		    || can_channel_land(ptile))
                && (!terrain_has_flag(pterrain->irrigation_result, TER_NO_CITIES)
		    || !tile_city(ptile))))) {
      unit_list_iterate(ptile->units, tunit) {
	if (tunit->activity == ACTIVITY_MINE) {
	  return FALSE;
	}
      } unit_list_iterate_end;
      return TRUE;
    } else {
      return FALSE;
    }

  case ACTIVITY_FORTIFYING:
    return (uclass_has_flag(pclass, UCF_CAN_FORTIFY)
	    && punit->activity != ACTIVITY_FORTIFIED
	    && !unit_has_type_flag(punit, UTYF_SETTLERS)
	    && (!is_ocean(pterrain) || tile_city(ptile)));

  case ACTIVITY_FORTIFIED:
    return FALSE;

  case ACTIVITY_BASE:
    return can_build_base(punit, base_by_number(target->obj.base), ptile);

  case ACTIVITY_GEN_ROAD:
    return can_build_road(road_by_number(target->obj.road), punit, ptile);

  case ACTIVITY_SENTRY:
    if (!can_unit_survive_at_tile(punit, unit_tile(punit))
        && !unit_transported(punit)) {
      /* Don't let units sentry on tiles they will die on. */
      return FALSE;
    }
    return TRUE;

  case ACTIVITY_PILLAGE:
    {
      if (uclass_has_flag(pclass, UCF_CAN_PILLAGE)) {
        bv_special pspresent = get_tile_infrastructure_set(ptile, NULL);
        bv_bases bspresent = get_tile_pillageable_base_set(ptile, NULL);
	bv_roads rspresent = get_tile_pillageable_road_set(ptile, NULL);
        bv_special psworking = get_unit_tile_pillage_set(ptile);
        bv_bases bsworking = get_unit_tile_pillage_base_set(ptile);
	bv_roads rsworking = get_unit_tile_pillage_road_set(ptile);
        bv_special pspossible;
        bv_bases bspossible;
        bv_roads rspossible;

        BV_CLR_ALL(pspossible);
        tile_special_type_iterate(spe) {
          /* Only one unit can pillage a given improvement at a time */
          if (BV_ISSET(pspresent, spe) && !BV_ISSET(psworking, spe)) {
            BV_SET(pspossible, spe);
          }
        } tile_special_type_iterate_end;
        tile_special_type_iterate(spe) {
          enum tile_special_type prereq = get_infrastructure_prereq(spe);
          /* If an improvement is present, we can't pillage its prerequisite */
          /* (FIXME: Could in principle allow simultaneous pillaging of
           * an improvement and its prerequisite, but this would require care
           * to ensure that the unit pillaging the topmost improvement
           * finished first.) */
          if (prereq != S_LAST && BV_ISSET(pspresent, spe)) {
            BV_CLR(pspossible, prereq);
          }
        } tile_special_type_iterate_end;

        BV_CLR_ALL(bspossible);
        base_type_iterate(bp) {
          struct city *pcity = tile_city(ptile);
          bool cannot_pillage = FALSE;

          /* Cannot pillage BF_ALWAYS_ON_CITY_CENTER bases from city center */
          if (pcity != NULL) {
            if (base_has_flag(bp, BF_ALWAYS_ON_CITY_CENTER)) {
              cannot_pillage = TRUE;
            } else if (base_has_flag(bp, BF_AUTO_ON_CITY_CENTER)) {
              struct tile *vtile = tile_virtual_new(ptile);

              /* Would base get rebuilt if removed */ 
              tile_remove_base(vtile, bp);
              if (player_can_build_base(bp, city_owner(pcity), vtile)) {
                /* No need to worry about conflicting bases - base would had
                 * not been here if conflicting one is. */
                cannot_pillage = TRUE;
              }

              tile_virtual_destroy(vtile);
            }
          }

          if (!cannot_pillage) {
            Base_type_id b = base_index(bp);

            if (BV_ISSET(bspresent, b) && !BV_ISSET(bsworking, b)) {
              BV_SET(bspossible, b);
            }
          }
        } base_type_iterate_end;

        BV_CLR_ALL(rspossible);
        road_type_iterate(pr) {
          struct city *pcity = tile_city(ptile);
          bool cannot_pillage = FALSE;

          /* Cannot pillage RF_ALWAYS_ON_CITY_CENTER roads from city center */
          if (pcity != NULL) {
            if (road_has_flag(pr, RF_ALWAYS_ON_CITY_CENTER)) {
              cannot_pillage = TRUE;
            } else if (road_has_flag(pr, RF_AUTO_ON_CITY_CENTER)) {
              struct tile *vtile = tile_virtual_new(ptile);

              /* Would road get rebuilt if removed */ 
              tile_remove_road(vtile, pr);
              if (player_can_build_road(pr, city_owner(pcity), vtile)) {
                /* No need to worry about conflicting roads - road would had
                 * not been here if conflicting one is. */
                cannot_pillage = TRUE;
              }

              tile_virtual_destroy(vtile);
            }
          }

          if (!cannot_pillage) {
            Road_type_id r = road_index(pr);

            if (BV_ISSET(rspresent, r) && !BV_ISSET(rsworking, r)) {
              BV_SET(rspossible, r);
            }
          }
        } road_type_iterate_end;

        if (!BV_ISSET_ANY(pspossible)
            && !BV_ISSET_ANY(bspossible)
            && !BV_ISSET_ANY(rspossible)) {
          /* Nothing available to pillage */
          return FALSE;
        }

        if (target->type == ATT_SPECIAL && target->obj.spe == S_LAST) {
          /* Undirected pillaging. If we've got this far, then there's
           * *something* we can pillage; work out what when we come to it */
          return TRUE;
        } else {
          if (!game.info.pillage_select) {
            /* Hobson's choice (this case mostly exists for old clients) */
            /* Needs to match what unit_activity_assign_target chooses */
            struct act_tgt tgt;
            bool found = get_preferred_pillage(&tgt, pspossible, bspossible, rspossible);

            fc_assert_ret_val(found, FALSE);

            if (!cmp_act_tgt(&tgt, target)) {
              /* Only one target allowed, which wasn't the requested one */
              return FALSE;
            }
          }
          if (target->type == ATT_SPECIAL) {
            return BV_ISSET(pspossible, target->obj.spe);
          } else if (target->type == ATT_BASE) {
            return BV_ISSET(bspossible, target->obj.base);
          } else {
            fc_assert(target->type == ATT_ROAD);
            return BV_ISSET(rspossible, target->obj.road);
          }
        }
      } else {
        /* Unit is not a type that can pillage at all */
        return FALSE;
      }
    }

  case ACTIVITY_EXPLORE:
    return (!unit_type(punit)->fuel && !is_losing_hp(punit));

  case ACTIVITY_TRANSFORM:
    return (pterrain->transform_result != T_NONE
	    && pterrain != pterrain->transform_result
	    && (!is_ocean(pterrain)
		|| is_ocean(pterrain->transform_result)
		|| can_reclaim_ocean(ptile))
	    && (is_ocean(pterrain)
		|| !is_ocean(pterrain->transform_result)
		|| can_channel_land(ptile))
	    && (!terrain_has_flag(pterrain->transform_result, TER_NO_CITIES)
		|| !(tile_city(ptile)))
            && get_tile_bonus(ptile, punit, EFT_TRANSFORM_POSSIBLE) > 0);

  case ACTIVITY_CONVERT:
    return unit_can_convert(punit);

  case ACTIVITY_OLD_ROAD:
  case ACTIVITY_OLD_RAILROAD:
  case ACTIVITY_FORTRESS:
  case ACTIVITY_AIRBASE:
  case ACTIVITY_PATROL_UNUSED:
  case ACTIVITY_LAST:
  case ACTIVITY_UNKNOWN:
    break;
  }
  log_error("can_unit_do_activity_targeted_at() unknown activity %d",
            activity);
  return FALSE;
}

/**************************************************************************
  Assign a new task to a unit. Doesn't account for changed_from.
**************************************************************************/
static void set_unit_activity_internal(struct unit *punit,
                                       enum unit_activity new_activity)
{
  fc_assert_ret(new_activity != ACTIVITY_FORTRESS
                && new_activity != ACTIVITY_AIRBASE);

  punit->activity = new_activity;
  punit->activity_count = 0;
  punit->activity_target.type = ATT_SPECIAL;
  punit->activity_target.obj.spe = S_LAST;
  if (new_activity == ACTIVITY_IDLE && punit->moves_left > 0) {
    /* No longer done. */
    punit->done_moving = FALSE;
  }
}

/**************************************************************************
  assign a new untargeted task to a unit.
**************************************************************************/
void set_unit_activity(struct unit *punit, enum unit_activity new_activity)
{
  fc_assert_ret(!activity_requires_target(new_activity));

  if (new_activity == ACTIVITY_FORTIFYING
      && punit->changed_from == ACTIVITY_FORTIFIED) {
    new_activity = ACTIVITY_FORTIFIED;
  }
  set_unit_activity_internal(punit, new_activity);
  if (new_activity == punit->changed_from) {
    punit->activity_count = punit->changed_from_count;
  }
}

/**************************************************************************
  assign a new targeted task to a unit.
**************************************************************************/
void set_unit_activity_targeted(struct unit *punit,
				enum unit_activity new_activity,
				struct act_tgt *new_target)
{
  fc_assert_ret(activity_requires_target(new_activity));

  set_unit_activity_internal(punit, new_activity);
  punit->activity_target = *new_target;
  if (new_activity == punit->changed_from
      && cmp_act_tgt(new_target, &punit->changed_from_target)) {
    punit->activity_count = punit->changed_from_count;
  }
}

/**************************************************************************
  Assign a new base building task to unit
**************************************************************************/
void set_unit_activity_base(struct unit *punit,
                            Base_type_id base)
{
  set_unit_activity_internal(punit, ACTIVITY_BASE);
  punit->activity_target.type = ATT_BASE;
  punit->activity_target.obj.base = base;
  if (ACTIVITY_BASE == punit->changed_from
      && cmp_act_tgt(&punit->activity_target, &punit->changed_from_target)) {
    punit->activity_count = punit->changed_from_count;
  }
}

/**************************************************************************
  Assign a new road building task to unit
**************************************************************************/
void set_unit_activity_road(struct unit *punit,
                            Road_type_id road)
{
  set_unit_activity_internal(punit, ACTIVITY_GEN_ROAD);
  punit->activity_target.type = ATT_ROAD;
  punit->activity_target.obj.road = road;
  if (ACTIVITY_GEN_ROAD == punit->changed_from
      && cmp_act_tgt(&punit->activity_target, &punit->changed_from_target)) {
    punit->activity_count = punit->changed_from_count;
  }
}

/**************************************************************************
  Return whether any units on the tile are doing this activity.
**************************************************************************/
bool is_unit_activity_on_tile(enum unit_activity activity,
			      const struct tile *ptile)
{
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == activity) {
      return TRUE;
    }
  } unit_list_iterate_end;
  return FALSE;
}

/****************************************************************************
  Return a mask of the specials which are actively (currently) being
  pillaged on the given tile.
****************************************************************************/
bv_special get_unit_tile_pillage_set(const struct tile *ptile)
{
  bv_special tgt_ret;

  BV_CLR_ALL(tgt_ret);
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == ACTIVITY_PILLAGE
        && punit->activity_target.type == ATT_SPECIAL) {
      fc_assert_action(punit->activity_target.obj.spe < S_LAST, continue);
      BV_SET(tgt_ret, punit->activity_target.obj.spe);
    }
  } unit_list_iterate_end;

  return tgt_ret;
}

/****************************************************************************
  Return a mask of the bases which are actively (currently) being
  pillaged on the given tile.
****************************************************************************/
bv_bases get_unit_tile_pillage_base_set(const struct tile *ptile)
{
  bv_bases tgt_ret;

  BV_CLR_ALL(tgt_ret);
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == ACTIVITY_PILLAGE
        && punit->activity_target.type == ATT_BASE) {
      fc_assert(punit->activity_target.obj.base < base_count());
      BV_SET(tgt_ret, punit->activity_target.obj.base);
    }
  } unit_list_iterate_end;

  return tgt_ret;
}

/****************************************************************************
  Return a mask of the roads which are actively (currently) being
  pillaged on the given tile.
****************************************************************************/
bv_roads get_unit_tile_pillage_road_set(const struct tile *ptile)
{
  bv_roads tgt_ret;

  BV_CLR_ALL(tgt_ret);
  unit_list_iterate(ptile->units, punit) {
    if (punit->activity == ACTIVITY_PILLAGE
        && punit->activity_target.type == ATT_ROAD) {
      fc_assert(punit->activity_target.obj.road < road_count());
      BV_SET(tgt_ret, punit->activity_target.obj.road);
    }
  } unit_list_iterate_end;

  return tgt_ret;
}

/**************************************************************************
  Return text describing the unit's current activity as a static string.

  FIXME: Convert all callers of this function to unit_activity_astr()
  because this function is not re-entrant.
**************************************************************************/
const char *unit_activity_text(const struct unit *punit) {
  static struct astring str = ASTRING_INIT;

  astr_clear(&str);
  unit_activity_astr(punit, &str);

  return astr_str(&str);
}

/**************************************************************************
  Append text describing the unit's current activity to the given astring.
**************************************************************************/
void unit_activity_astr(const struct unit *punit, struct astring *astr)
{
  if (!punit || !astr) {
    return;
  }

  switch (punit->activity) {
  case ACTIVITY_IDLE:
    if (utype_fuel(unit_type(punit))) {
      int rate, f;
      rate = unit_type(punit)->move_rate;
      f = ((punit->fuel) - 1);

      /* Add in two parts as move_points_text() returns ptr to static
       * End result: "Moves: (fuel)moves_left" */
      astr_add_line(astr, "%s: (%s)", _("Moves"),
                    move_points_text((rate * f) + punit->moves_left, FALSE));
      astr_add(astr, "%s",
               move_points_text(punit->moves_left, FALSE));
    } else {
      astr_add_line(astr, "%s: %s", _("Moves"),
                    move_points_text(punit->moves_left, FALSE));
    }
    return;
  case ACTIVITY_POLLUTION:
  case ACTIVITY_FALLOUT:
  case ACTIVITY_OLD_ROAD:
  case ACTIVITY_OLD_RAILROAD:
  case ACTIVITY_MINE:
  case ACTIVITY_IRRIGATE:
  case ACTIVITY_TRANSFORM:
  case ACTIVITY_FORTIFYING:
  case ACTIVITY_FORTIFIED:
  case ACTIVITY_AIRBASE:
  case ACTIVITY_FORTRESS:
  case ACTIVITY_SENTRY:
  case ACTIVITY_GOTO:
  case ACTIVITY_EXPLORE:
  case ACTIVITY_CONVERT:
    astr_add_line(astr, "%s", get_activity_text(punit->activity));
    return;
  case ACTIVITY_PILLAGE:
    switch (punit->activity_target.type) {
      case ATT_SPECIAL:
        if (punit->activity_target.obj.spe == S_LAST) {
          astr_add_line(astr, "%s", get_activity_text(punit->activity));
        } else {
          bv_special pset;
          bv_bases bases;
          bv_roads roads;

          BV_CLR_ALL(pset);
          BV_CLR_ALL(bases);
          BV_CLR_ALL(roads);
          BV_SET(pset, punit->activity_target.obj.spe);
          astr_add_line(astr, "%s: %s", get_activity_text(punit->activity),
                        get_infrastructure_text(pset, bases, roads));
        }
        break;
     case ATT_BASE:
        {
          bv_special pset;
          bv_bases bases;
          bv_roads roads;

          BV_CLR_ALL(pset);
          BV_CLR_ALL(bases);
          BV_CLR_ALL(roads);
          BV_SET(bases, punit->activity_target.obj.base);
          astr_add_line(astr, "%s: %s", get_activity_text(punit->activity),
                        get_infrastructure_text(pset, bases, roads));
        }
        break;
     case ATT_ROAD:
        {
          bv_special pset;
          bv_bases bases;
          bv_roads roads;

          BV_CLR_ALL(pset);
          BV_CLR_ALL(bases);
          BV_CLR_ALL(roads);
          BV_SET(roads, punit->activity_target.obj.road);
          astr_add_line(astr, "%s: %s", get_activity_text(punit->activity),
                        get_infrastructure_text(pset, bases, roads));
        }
        break;
    } 
    return;
  case ACTIVITY_BASE:
    {
      struct base_type *pbase;
      pbase = base_by_number(punit->activity_target.obj.base);
      astr_add_line(astr, "%s: %s", get_activity_text(punit->activity),
                    base_name_translation(pbase));
    }
    return;
  case ACTIVITY_GEN_ROAD:
    {
      struct road_type *proad;
      proad = road_by_number(punit->activity_target.obj.road);
      astr_add_line(astr, "%s: %s", get_activity_text(punit->activity),
                    road_name_translation(proad));
    }
    return;
  case ACTIVITY_UNKNOWN:
  case ACTIVITY_PATROL_UNUSED:
  case ACTIVITY_LAST:
    break;
  }

  log_error("Unknown unit activity %d for %s (nb %d) in %s()",
            punit->activity, unit_rule_name(punit), punit->id, __FUNCTION__);
}

/**************************************************************************
  Append a line of text describing the unit's upkeep to the astring.

  NB: In the client it is assumed that this information is only available
  for units owned by the client's player; the caller must check this.
**************************************************************************/
void unit_upkeep_astr(const struct unit *punit, struct astring *astr)
{
  if (!punit || !astr) {
    return;
  }

  astr_add_line(astr, "%s %d/%d/%d", _("Food/Shield/Gold:"),
                punit->upkeep[O_FOOD], punit->upkeep[O_SHIELD],
                punit->upkeep[O_GOLD]);
}

/**************************************************************************
  Return the nationality of the unit.
**************************************************************************/
struct player *unit_nationality(const struct unit *punit)
{
  fc_assert_ret_val(NULL != punit, NULL);
  return punit->nationality;
}

/*****************************************************************************
  Set the tile location of the unit. Tile can be NULL (for transported units.
*****************************************************************************/
void unit_tile_set(struct unit *punit, struct tile *ptile)
{
  fc_assert_ret(NULL != punit);
  punit->tile = ptile;
}

/**************************************************************************
Returns true if the tile contains an allied unit and only allied units.
(ie, if your nation A is allied with B, and B is allied with C, a tile
containing units from B and C will return false)
**************************************************************************/
struct unit *is_allied_unit_tile(const struct tile *ptile,
				 const struct player *pplayer)
{
  struct unit *punit = NULL;

  unit_list_iterate(ptile->units, cunit) {
    if (pplayers_allied(pplayer, unit_owner(cunit)))
      punit = cunit;
    else
      return NULL;
  }
  unit_list_iterate_end;

  return punit;
}

/****************************************************************************
  Is there an enemy unit on this tile?  Returns the unit or NULL if none.

  This function is likely to fail if used at the client because the client
  doesn't see all units.  (Maybe it should be moved into the server code.)
****************************************************************************/
struct unit *is_enemy_unit_tile(const struct tile *ptile,
				const struct player *pplayer)
{
  unit_list_iterate(ptile->units, punit) {
    if (pplayers_at_war(unit_owner(punit), pplayer))
      return punit;
  } unit_list_iterate_end;

  return NULL;
}

/**************************************************************************
 is there an non-allied unit on this tile?
**************************************************************************/
struct unit *is_non_allied_unit_tile(const struct tile *ptile,
				     const struct player *pplayer)
{
  unit_list_iterate(ptile->units, punit) {
    if (!pplayers_allied(unit_owner(punit), pplayer))
      return punit;
  }
  unit_list_iterate_end;

  return NULL;
}

/**************************************************************************
 is there an unit we have peace or ceasefire with on this tile?
**************************************************************************/
struct unit *is_non_attack_unit_tile(const struct tile *ptile,
				     const struct player *pplayer)
{
  unit_list_iterate(ptile->units, punit) {
    if (pplayers_non_attack(unit_owner(punit), pplayer))
      return punit;
  }
  unit_list_iterate_end;

  return NULL;
}

/****************************************************************************
  Is there an occupying unit on this tile?

  Intended for both client and server; assumes that hiding units are not
  sent to client.  First check tile for known and seen.

  called by city_can_work_tile().
****************************************************************************/
struct unit *unit_occupies_tile(const struct tile *ptile,
				const struct player *pplayer)
{
  unit_list_iterate(ptile->units, punit) {
    if (!is_military_unit(punit)) {
      continue;
    }

    if (uclass_has_flag(unit_class(punit), UCF_DOESNT_OCCUPY_TILE)) {
      continue;
    }

    if (pplayers_at_war(unit_owner(punit), pplayer)) {
      return punit;
    }
  } unit_list_iterate_end;

  return NULL;
}

/**************************************************************************
  Is this square controlled by the pplayer?

  Here "is_my_zoc" means essentially a square which is *not* adjacent to an
  enemy unit on a land tile.

  Note this function only makes sense for ground units.

  Since this function is also used in the client, it has to deal with some
  client-specific features, like FoW and the fact that the client cannot 
  see units inside enemy cities.
**************************************************************************/
bool is_my_zoc(const struct player *pplayer, const struct tile *ptile0)
{
  square_iterate(ptile0, 1, ptile) {
    if (is_ocean_tile(ptile)) {
      continue;
    }
    if (is_non_allied_unit_tile(ptile, pplayer)) {
      /* Note: in the client, the above function will return NULL 
       * if there is a city there, even if the city is occupied */
      return FALSE;
    }
    
    if (!is_server()) {
      struct city *pcity = is_non_allied_city_tile(ptile, pplayer);

      if (pcity 
          && (pcity->client.occupied 
              || TILE_KNOWN_UNSEEN == tile_get_known(ptile, pplayer))) {
        /* If the city is fogged, we assume it's occupied */
        return FALSE;
      }
    }
  } square_iterate_end;

  return TRUE;
}

/**************************************************************************
  Takes into account unit class flag UCF_ZOC as well as IGZOC
**************************************************************************/
bool unit_type_really_ignores_zoc(const struct unit_type *punittype)
{
  return (!uclass_has_flag(utype_class(punittype), UCF_ZOC)
	  || utype_has_flag(punittype, UTYF_IGZOC));
}

/**************************************************************************
An "aggressive" unit is a unit which may cause unhappiness
under a Republic or Democracy.
A unit is *not* aggressive if one or more of following is true:
- zero attack strength
- inside a city
- ground unit inside a fortress within 3 squares of a friendly city
**************************************************************************/
bool unit_being_aggressive(const struct unit *punit)
{
  if (!is_attack_unit(punit)) {
    return FALSE;
  }
  if (tile_city(unit_tile(punit))) {
    return FALSE;
  }
  if (BORDERS_DISABLED != game.info.borders
      && game.info.happyborders
      && tile_owner(unit_tile(punit)) == unit_owner(punit)) {
    return FALSE;
  }
  if (tile_has_base_flag_for_unit(unit_tile(punit),
                                  unit_type(punit),
                                  BF_NOT_AGGRESSIVE)) {
    return !is_unit_near_a_friendly_city(punit);
  }

  return TRUE;
}

/**************************************************************************
  Returns true if given activity is some kind of building/cleaning.
**************************************************************************/
bool is_build_or_clean_activity(enum unit_activity activity)
{
  switch (activity) {
  case ACTIVITY_POLLUTION:
  case ACTIVITY_MINE:
  case ACTIVITY_IRRIGATE:
  case ACTIVITY_TRANSFORM:
  case ACTIVITY_FALLOUT:
  case ACTIVITY_BASE:
    return TRUE;
  default:
    return FALSE;
  }
}

/**************************************************************************
  Create a virtual unit skeleton. pcity can be NULL, but then you need
  to set tile and homecity yourself.
**************************************************************************/
struct unit *unit_virtual_create(struct player *pplayer, struct city *pcity,
                                 struct unit_type *punittype,
                                 int veteran_level)
{
  /* Make sure that contents of unit structure are correctly initialized,
   * if you ever allocate it by some other mean than fc_calloc() */
  struct unit *punit = fc_calloc(1, sizeof(*punit));
  int max_vet_lvl;

  /* It does not register the unit so the id is set to 0. */
  punit->id = IDENTITY_NUMBER_ZERO;

  fc_assert_ret_val(NULL != punittype, NULL);   /* No untyped units! */
  punit->utype = punittype;

  fc_assert_ret_val(NULL != pplayer, NULL);     /* No unowned units! */
  punit->owner = pplayer;
  punit->nationality = pplayer;

  punit->facing = rand_direction();

  if (pcity) {
    unit_tile_set(punit, pcity->tile);
    punit->homecity = pcity->id;
  } else {
    unit_tile_set(punit, NULL);
    punit->homecity = IDENTITY_NUMBER_ZERO;
  }

  memset(punit->upkeep, 0, O_LAST * sizeof(*punit->upkeep));
  punit->goto_tile = NULL;
  max_vet_lvl = utype_veteran_levels(punittype) - 1;
  punit->veteran = MIN(veteran_level, max_vet_lvl);
  /* A unit new and fresh ... */
  punit->fuel = utype_fuel(unit_type(punit));
  punit->hp = unit_type(punit)->hp;
  punit->moves_left = unit_move_rate(punit);
  punit->moved = FALSE;

  punit->ai_controlled = FALSE;
  punit->paradropped = FALSE;
  punit->done_moving = FALSE;

  punit->transporter = NULL;
  punit->transporting = unit_list_new();

  set_unit_activity(punit, ACTIVITY_IDLE);
  punit->battlegroup = BATTLEGROUP_NONE;
  punit->has_orders = FALSE;

  if (is_server()) {
    punit->server.debug = FALSE;
    punit->server.birth_turn = game.info.turn;

    punit->server.dying = FALSE;

    punit->server.ord_map = 0;
    punit->server.ord_city = 0;

    punit->server.vision = NULL; /* No vision. */
    punit->server.action_timestamp = 0;
    /* Must be an invalid turn number, and an invalid previous turn
     * number. */
    punit->server.action_turn = -2;
    /* punit->server.moving = NULL; set by fc_calloc(). */

    punit->server.adv = fc_calloc(1, sizeof(*punit->server.adv));

    CALL_FUNC_EACH_AI(unit_alloc, punit);
  } else {
    punit->client.focus_status = FOCUS_AVAIL;
    punit->client.transported_by = -1;
    punit->client.colored = FALSE;
  }

  return punit;
}

/**************************************************************************
  Free the memory used by virtual unit. By the time this function is
  called, you should already have unregistered it everywhere.
**************************************************************************/
void unit_virtual_destroy(struct unit *punit)
{
  free_unit_orders(punit);

  /* Unload unit if transported. */
  unit_transport_unload(punit);
  fc_assert(!unit_transported(punit));

  /* Check for transported units. Use direct access to the list. */
  if (unit_list_size(punit->transporting) != 0) {
    /* Unload all units. */
    unit_list_iterate_safe(punit->transporting, pcargo) {
      unit_transport_unload(pcargo);
    } unit_list_iterate_safe_end;
  }
  fc_assert(unit_list_size(punit->transporting) == 0);

  if (punit->transporting) {
    unit_list_destroy(punit->transporting);
  }

  CALL_FUNC_EACH_AI(unit_free, punit);

  if (is_server() && punit->server.adv) {
    FC_FREE(punit->server.adv);
  }

  FC_FREE(punit);
}

/**************************************************************************
  Free and reset the unit's goto route (punit->pgr).  Only used by the
  server.
**************************************************************************/
void free_unit_orders(struct unit *punit)
{
  if (punit->has_orders) {
    punit->goto_tile = NULL;
    free(punit->orders.list);
    punit->orders.list = NULL;
  }
  punit->has_orders = FALSE;
}

/****************************************************************************
  Return how many units are in the transport.
****************************************************************************/
int get_transporter_occupancy(const struct unit *ptrans)
{
  fc_assert_ret_val(ptrans, -1);

  return unit_list_size(ptrans->transporting);
}

/****************************************************************************
  Find the best transporter at the given location for the unit. See also
  transport_from_tile() to test if a transport might be suitable for
  'pcargo'.
****************************************************************************/
struct unit *transporter_for_unit(const struct unit *pcargo)
{
  struct unit_list *tile_units = unit_tile(pcargo)->units;
  struct unit *best_trans = NULL;
  struct {
    bool has_orders, is_idle, can_freely_unload;
    int depth, outermost_moves_left, total_moves;
  } cur, best = { FALSE };

  unit_list_iterate(tile_units, ptrans) {
    if (!can_unit_load(pcargo, ptrans)) {
      continue;
    } else if (best_trans == NULL) {
      best_trans = ptrans;
    }

    /* Gather data from transport stack in a single pass, for use in
     * various conditions below. */
    cur.has_orders = unit_has_orders(ptrans);
    cur.outermost_moves_left = ptrans->moves_left;
    cur.total_moves = ptrans->moves_left + unit_move_rate(ptrans);
    {
      const struct unit *ptranstrans = unit_transport_get(ptrans);

      while (NULL != ptranstrans) {
        if (unit_has_orders(ptranstrans)) {
          cur.has_orders = TRUE;
        }
        cur.outermost_moves_left = ptranstrans->moves_left;
        cur.total_moves += ptranstrans->moves_left
                           + unit_move_rate(ptranstrans);
        ptranstrans = unit_transport_get(ptranstrans);
      }
    }

    /* Criteria for deciding the 'best' transport to load onto.
     * The following tests are applied in order; earlier ones have
     * lexicographically greater significance than later ones. */

    /* Transports which have orders, or are on transports with orders,
     * are less preferable to transport stacks without orders (to
     * avoid loading on units that are just passing through). */
    if (best_trans != ptrans) {
      if (!cur.has_orders && best.has_orders) {
        best_trans = ptrans;
      } else if (cur.has_orders && !best.has_orders) {
        continue;
      }
    }

    /* Else, transports which are idle are preferable (giving players
     * some control over loading) -- this does not check transports
     * of transports. */
    cur.is_idle = (ptrans->activity == ACTIVITY_IDLE);
    if (best_trans != ptrans) {
      if (cur.is_idle && !best.is_idle) {
        best_trans = ptrans;
      } else if (!cur.is_idle && best.is_idle) {
        continue;
      }
    }

    /* Else, transports from which the cargo could unload at any time
     * are preferable to those where the cargo can only disembark in
     * cities/bases. */
    cur.can_freely_unload = utype_can_freely_unload(unit_type(pcargo),
                                                    unit_type(ptrans));
    if (best_trans != ptrans) {
      if (cur.can_freely_unload && !best.can_freely_unload) {
        best_trans = ptrans;
      } else if (!cur.can_freely_unload && best.can_freely_unload) {
        continue;
      }
    }

    /* Else, transports which are less deeply nested are preferable. */
    cur.depth = unit_transport_depth(ptrans);
    if (best_trans != ptrans) {
      if (cur.depth < best.depth) {
        best_trans = ptrans;
      } else if (cur.depth > best.depth) {
        continue;
      }
    }

    /* Else, transport stacks where the outermost transport has more
     * moves left are preferable (on the assumption that it's the
     * outermost transport that's about to move). */
    if (best_trans != ptrans) {
      if (cur.outermost_moves_left > best.outermost_moves_left) {
        best_trans = ptrans;
      } else if (cur.outermost_moves_left < best.outermost_moves_left) {
        continue;
      }
    }

    /* All other things being equal, as a tie-breaker, compare the total
     * moves left (this turn) and move rate (future turns) for the whole
     * stack, to take into account total potential movement for both
     * short and long journeys (we don't know which the cargo intends to
     * make). Doesn't try to account for whether transports can unload,
     * etc. */
    if (best_trans != ptrans) {
      if (cur.total_moves > best.total_moves) {
        best_trans = ptrans;
      } else {
        continue;
      }
    }

    fc_assert(best_trans == ptrans);
    best = cur;
  } unit_list_iterate_end;

  return best_trans;
}

/****************************************************************************
  Check if unit of given type would be able to transport all of transport's
  cargo.
****************************************************************************/
static bool can_type_transport_units_cargo(const struct unit_type *utype,
                                           const struct unit *punit)
{
  if (get_transporter_occupancy(punit) > utype->transport_capacity) {
    return FALSE;
  }

  unit_list_iterate(punit->transporting, pcargo) {
    if (!can_unit_type_transport(utype, unit_class(pcargo))) {
      return FALSE;
    }
  } unit_list_iterate_end;

  return TRUE;
}

/****************************************************************************
  Tests if the unit could be updated. Returns UU_OK if is this is
  possible.

  is_free should be set if the unit upgrade is "free" (e.g., Leonardo's).
  Otherwise money is needed and the unit must be in an owned city.

  Note that this function is strongly tied to unittools.c:upgrade_unit().
****************************************************************************/
enum unit_upgrade_result unit_upgrade_test(const struct unit *punit,
                                           bool is_free)
{
  struct player *pplayer = unit_owner(punit);
  struct unit_type *to_unittype = can_upgrade_unittype(pplayer, unit_type(punit));
  struct city *pcity;
  int cost;

  if (!to_unittype) {
    return UU_NO_UNITTYPE;
  }

  if (!is_free) {
    cost = unit_upgrade_price(pplayer, unit_type(punit), to_unittype);
    if (pplayer->economic.gold < cost) {
      return UU_NO_MONEY;
    }

    pcity = tile_city(unit_tile(punit));
    if (!pcity) {
      return UU_NOT_IN_CITY;
    }
    if (city_owner(pcity) != pplayer) {
      /* TODO: should upgrades in allied cities be possible? */
      return UU_NOT_CITY_OWNER;
    }
  }

  if (!can_type_transport_units_cargo(to_unittype, punit)) {
    /* TODO: allow transported units to be reassigned.  Check here
     * and make changes to upgrade_unit. */
    return UU_NOT_ENOUGH_ROOM;
  }

  if (punit->transporter != NULL) {
    if (!can_unit_type_transport(unit_type(punit->transporter),
                                 unit_class(punit))) {
      return UU_UNSUITABLE_TRANSPORT;
    }
  } else if (!can_exist_at_tile(to_unittype, unit_tile(punit))) {
    /* The new unit type can't survive on this terrain. */
    return UU_NOT_TERRAIN;
  }

  return UU_OK;
}

/**************************************************************************
  Tests if unit can be converted to another type.
**************************************************************************/
bool unit_can_convert(const struct unit *punit)
{
  struct unit_type *tgt = unit_type(punit)->converted_to;

  if (tgt == NULL) {
    return FALSE;
  }

  if (!can_type_transport_units_cargo(tgt, punit)) {
    return FALSE;
  }

  if (!can_exist_at_tile(tgt, unit_tile(punit))) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
  Find the result of trying to upgrade the unit, and a message that
  most callers can use directly.
**************************************************************************/
enum unit_upgrade_result unit_upgrade_info(const struct unit *punit,
                                           char *buf, size_t bufsz)
{
  struct player *pplayer = unit_owner(punit);
  enum unit_upgrade_result result = unit_upgrade_test(punit, FALSE);
  int upgrade_cost;
  struct unit_type *from_unittype = unit_type(punit);
  struct unit_type *to_unittype = can_upgrade_unittype(pplayer,
                                                       unit_type(punit));
  char tbuf[MAX_LEN_MSG];

  fc_snprintf(tbuf, ARRAY_SIZE(tbuf), PL_("Treasury contains %d gold.",
                                          "Treasury contains %d gold.",
                                          pplayer->economic.gold),
              pplayer->economic.gold);

  switch (result) {
  case UU_OK:
    upgrade_cost = unit_upgrade_price(pplayer, from_unittype, to_unittype);
    /* This message is targeted toward the GUI callers. */
    /* TRANS: Last %s is pre-pluralised "Treasury contains %d gold." */
    fc_snprintf(buf, bufsz, PL_("Upgrade %s to %s for %d gold?\n%s",
                                "Upgrade %s to %s for %d gold?\n%s",
                                upgrade_cost),
                utype_name_translation(from_unittype),
                utype_name_translation(to_unittype),
                upgrade_cost, tbuf);
    break;
  case UU_NO_UNITTYPE:
    fc_snprintf(buf, bufsz,
                _("Sorry, cannot upgrade %s (yet)."),
                utype_name_translation(from_unittype));
    break;
  case UU_NO_MONEY:
    upgrade_cost = unit_upgrade_price(pplayer, from_unittype, to_unittype);
    /* TRANS: Last %s is pre-pluralised "Treasury contains %d gold." */
    fc_snprintf(buf, bufsz, PL_("Upgrading %s to %s costs %d gold.\n%s",
                                "Upgrading %s to %s costs %d gold.\n%s",
                                upgrade_cost),
                utype_name_translation(from_unittype),
                utype_name_translation(to_unittype),
                upgrade_cost, tbuf);
    break;
  case UU_NOT_IN_CITY:
  case UU_NOT_CITY_OWNER:
    fc_snprintf(buf, bufsz,
                _("You can only upgrade units in your cities."));
    break;
  case UU_NOT_ENOUGH_ROOM:
    fc_snprintf(buf, bufsz,
                _("Upgrading this %s would strand units it transports."),
                utype_name_translation(from_unittype));
    break;
  case UU_NOT_TERRAIN:
    fc_snprintf(buf, bufsz,
                _("Upgrading this %s would result in a %s which can not "
                  "survive at this place."),
                utype_name_translation(from_unittype),
                utype_name_translation(to_unittype));
    break;
  case UU_UNSUITABLE_TRANSPORT:
    fc_snprintf(buf, bufsz,
                _("Upgrading this %s would result in a %s which its "
                  "current transport, %s, could not transport."),
                utype_name_translation(from_unittype),
                utype_name_translation(to_unittype),
                unit_name_translation(punit->transporter));
    break;
  }

  return result;
}

/**************************************************************************
  Does unit lose hitpoints each turn?
**************************************************************************/
bool is_losing_hp(const struct unit *punit)
{
  struct unit_type *punittype = unit_type(punit);

  return get_unit_bonus(punit, EFT_UNIT_RECOVER)
    < (punittype->hp *
       utype_class(punittype)->hp_loss_pct / 100);
}

/**************************************************************************
  Does unit lose hitpoints each turn?
**************************************************************************/
bool unit_type_is_losing_hp(const struct player *pplayer,
                            const struct unit_type *punittype)
{
  return get_unittype_bonus(pplayer, NULL, punittype, EFT_UNIT_RECOVER)
    < (punittype->hp *
       utype_class(punittype)->hp_loss_pct / 100);
}

/**************************************************************************
  Check if unit with given id is still alive. Use this before using
  old unit pointers when unit might have dead.
**************************************************************************/
bool unit_alive(int id)
{
  /* Check if unit exist in game */
  if (game_unit_by_number(id)) {
    return TRUE;
  }

  return FALSE;
}

/**************************************************************************
  Return TRUE if this is a valid unit pointer but does not correspond to
  any unit that exists in the game.

  NB: A return value of FALSE implies that either the pointer is NULL or
  that the unit exists in the game.
**************************************************************************/
bool unit_is_virtual(const struct unit *punit)
{
  if (!punit) {
    return FALSE;
  }

  return punit != game_unit_by_number(punit->id);
}

/**************************************************************************
  Return pointer to ai data of given unit and ai type.
**************************************************************************/
void *unit_ai_data(const struct unit *punit, const struct ai_type *ai)
{
  return punit->server.ais[ai_type_number(ai)];
}

/**************************************************************************
  Attach ai data to unit
**************************************************************************/
void unit_set_ai_data(struct unit *punit, const struct ai_type *ai,
                      void *data)
{
  punit->server.ais[ai_type_number(ai)] = data;
}

/*****************************************************************************
  Calculate how expensive it is to bribe the unit. The cost depends on the
  distance to the capital, the owner's treasury, and the build cost of the
  unit. For a damaged unit the price is reduced. For a veteran unit, it is
  increased.

  The bribe cost for settlers are halved.
**************************************************************************/
int unit_bribe_cost(struct unit *punit)
{
  int cost, default_hp, dist = 0;
  struct city *capital;

  fc_assert_ret_val(punit != NULL, 0);

  default_hp = unit_type(punit)->hp;
  cost = unit_owner(punit)->economic.gold + game.info.base_bribe_cost;
  capital = player_capital(unit_owner(punit));

  /* Consider the distance to the capital. */
  if (capital != NULL) {
    dist = MIN(GAME_UNIT_BRIBE_DIST_MAX,
               map_distance(capital->tile, unit_tile(punit)));
  } else {
    dist = GAME_UNIT_BRIBE_DIST_MAX;
  }
  cost /= dist + 2;

  /* Consider the build cost. */
  cost *= unit_build_shield_cost(punit) / 10;

  /* FIXME: This is a weird one - should be replaced. */
  if (unit_has_type_flag(punit, UTYF_CITIES)) {
    cost /= 2;
  }

  /* Veterans are not cheap. */
  {
    const struct veteran_level *vlevel
      = utype_veteran_level(unit_type(punit), punit->veteran);
    fc_assert_ret_val(vlevel != NULL, 0);
    cost = cost * vlevel->power_fact / 100;
    if (unit_type(punit)->move_rate > 0) {
      cost += cost * vlevel->move_bonus / unit_type(punit)->move_rate;
    } else {
      cost += cost * vlevel->move_bonus / SINGLE_MOVE;
    }
  }

  /* Cost now contains the basic bribe cost.  We now reduce it by:
   *    bribecost = cost/2 + cost/2 * damage/hp
   *              = cost/2 * (1 + damage/hp) */
  return ((float)cost / 2 * (1.0 + punit->hp / default_hp));
}

/*****************************************************************************
  Load pcargo onto ptrans. Returns TRUE on success.
*****************************************************************************/
bool unit_transport_load(struct unit *pcargo, struct unit *ptrans, bool force)
{
  fc_assert_ret_val(ptrans != NULL, FALSE);
  fc_assert_ret_val(pcargo != NULL, FALSE);

  fc_assert_ret_val(!unit_list_search(ptrans->transporting, pcargo), FALSE);

  if (force || can_unit_load(pcargo, ptrans)) {
    pcargo->transporter = ptrans;
    unit_list_append(ptrans->transporting, pcargo);

    return TRUE;
  }

  return FALSE;
}

/*****************************************************************************
  Unload pcargo from ptrans. Returns TRUE on success.
*****************************************************************************/
bool unit_transport_unload(struct unit *pcargo)
{
  struct unit *ptrans;

  fc_assert_ret_val(pcargo != NULL, FALSE);

  if (!unit_transported(pcargo)) {
    /* 'pcargo' is not transported. */
    return FALSE;
  }

  /* Get the transporter; must not be defined on the client! */
  ptrans = unit_transport_get(pcargo);
  if (ptrans) {
    bool success;

    /* 'pcargo' and 'ptrans' should be on the same tile. */
    fc_assert(same_pos(unit_tile(pcargo), unit_tile(ptrans)));
    /* It is an error if 'pcargo' can not be removed from the 'ptrans'. */
    success = unit_list_remove(ptrans->transporting, pcargo);
    fc_assert(success);
  }

  /* For the server (also safe for the client). */
  pcargo->transporter = NULL;

  return TRUE;
}

/*****************************************************************************
  Returns TRUE iff the unit is transported.
*****************************************************************************/
bool unit_transported(const struct unit *pcargo)
{
  fc_assert_ret_val(pcargo != NULL, FALSE);

  /* The unit is transported if a transporter unit is set or, (for the client)
   * if the transported_by field is set. */
  if (pcargo->transporter != NULL
      || (!is_server() && pcargo->client.transported_by != -1)) {
    return TRUE;
  }

  return FALSE;
}

/*****************************************************************************
  Returns the transporter of the unit or NULL if it is not transported.
*****************************************************************************/
struct unit *unit_transport_get(const struct unit *pcargo)
{
  fc_assert_ret_val(pcargo != NULL, NULL);

  return pcargo->transporter;
}

/*****************************************************************************
  Returns the list of cargo units.
*****************************************************************************/
struct unit_list *unit_transport_cargo(const struct unit *ptrans)
{
  fc_assert_ret_val(ptrans != NULL, NULL);
  fc_assert_ret_val(ptrans->transporting != NULL, NULL);

  return ptrans->transporting;
}

/****************************************************************************
  Helper for unit_transport_check().
****************************************************************************/
static inline bool
unit_transport_check_one(const struct unit_type *cargo_utype,
                         const struct unit_type *trans_utype)
{
  return (trans_utype != cargo_utype
          && !can_unit_type_transport(cargo_utype,
                                      utype_class(trans_utype)));
}

/****************************************************************************
  Returns whether 'pcargo' in 'ptrans' is a valid transport. Note that
  'pcargo' can already be (but doesn't need) loaded into 'ptrans'.

  It may fail if one of the cargo unit has the same type of one of the
  transporter unit or if one of the cargo unit can transport one of
  the transporters.
****************************************************************************/
bool unit_transport_check(const struct unit *pcargo,
                          const struct unit *ptrans)
{
  const struct unit_type *cargo_utype = unit_type(pcargo);

  /* Check 'pcargo' against 'ptrans'. */
  if (!unit_transport_check_one(cargo_utype, unit_type(ptrans))) {
    return FALSE;
  }

  /* Check 'pcargo' against 'ptrans' parents. */
  unit_transports_iterate(ptrans, pparent) {
    if (!unit_transport_check_one(cargo_utype, unit_type(pparent))) {
      return FALSE;
    }
  } unit_transports_iterate_end;

  /* Check cargo children... */
  unit_cargo_iterate(pcargo, pchild) {
    cargo_utype = unit_type(pchild);

    /* ...against 'ptrans'. */
    if (!unit_transport_check_one(cargo_utype, unit_type(ptrans))) {
      return FALSE;
    }

    /* ...and against 'ptrans' parents. */
    unit_transports_iterate(ptrans, pparent) {
      if (!unit_transport_check_one(cargo_utype, unit_type(pparent))) {
        return FALSE;
      }
    } unit_transports_iterate_end;
  } unit_cargo_iterate_end;

  return TRUE;
}

/****************************************************************************
  Returns whether 'pcargo' is transported by 'ptrans', either directly
  or indirectly.
****************************************************************************/
bool unit_contained_in(const struct unit *pcargo, const struct unit *ptrans)
{
  unit_transports_iterate(pcargo, plevel) {
    if (ptrans == plevel) {
      return TRUE;
    }
  } unit_transports_iterate_end;
  return FALSE;
}

/****************************************************************************
  Returns the number of unit cargo layers within transport 'ptrans'.
****************************************************************************/
int unit_cargo_depth(const struct unit *ptrans)
{
  struct cargo_iter iter;
  struct iterator *it;
  int depth = 0;

  for (it = cargo_iter_init(&iter, ptrans); iterator_valid(it);
       iterator_next(it)) {
    if (iter.depth > depth) {
      depth = iter.depth;
    }
  }
  return depth;
}

/****************************************************************************
  Returns the number of unit transport layers which carry unit 'pcargo'.
****************************************************************************/
int unit_transport_depth(const struct unit *pcargo)
{
  int level = 0;

  unit_transports_iterate(pcargo, plevel) {
    level++;
  } unit_transports_iterate_end;
  return level;
}

/****************************************************************************
  Returns the size of the unit cargo iterator.
****************************************************************************/
size_t cargo_iter_sizeof(void)
{
  return sizeof(struct cargo_iter);
}

/****************************************************************************
  Get the unit of the cargo iterator.
****************************************************************************/
static void *cargo_iter_get(const struct iterator *it)
{
  const struct cargo_iter *iter = CARGO_ITER(it);

  return unit_list_link_data(iter->links[iter->depth - 1]);
}

/****************************************************************************
  Try to find next unit for the cargo iterator.
****************************************************************************/
static void cargo_iter_next(struct iterator *it)
{
  struct cargo_iter *iter = CARGO_ITER(it);
  const struct unit_list_link *piter = iter->links[iter->depth - 1];
  const struct unit_list_link *pnext;

  /* Variant 1: unit has cargo. */
  pnext = unit_list_head(unit_transport_cargo(unit_list_link_data(piter)));
  if (NULL != pnext) {
    fc_assert(iter->depth < ARRAY_SIZE(iter->links));
    iter->links[iter->depth++] = pnext;
    return;
  }

  do {
    /* Variant 2: there are other cargo units at same level. */
    pnext = unit_list_link_next(piter);
    if (NULL != pnext) {
      iter->links[iter->depth - 1] = pnext;
      return;
    }

    /* Variant 3: return to previous level, and do same tests. */
    piter = iter->links[iter->depth-- - 2];
  } while (0 < iter->depth);
}

/****************************************************************************
  Return whether the iterator is still valid.
****************************************************************************/
static bool cargo_iter_valid(const struct iterator *it)
{
  return (0 < CARGO_ITER(it)->depth);
}

/****************************************************************************
  Initialize the cargo iterator.
****************************************************************************/
struct iterator *cargo_iter_init(struct cargo_iter *iter,
                                 const struct unit *ptrans)
{
  struct iterator *it = ITERATOR(iter);

  it->get = cargo_iter_get;
  it->next = cargo_iter_next;
  it->valid = cargo_iter_valid;
  iter->links[0] = unit_list_head(unit_transport_cargo(ptrans));
  iter->depth = (NULL != iter->links[0] ? 1 : 0);

  return it;
}
