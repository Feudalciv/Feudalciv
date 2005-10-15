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
#include "game.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "movement.h"
#include "packets.h"
#include "player.h"
#include "shared.h"
#include "support.h"
#include "tech.h"

#include "city.h"
#include "unit.h"


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

  if (!is_tiles_adjacent(pdiplomat->tile, ptile)
      && !same_pos(pdiplomat->tile, ptile)) {
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
  struct city *pcity=tile_get_city(ptile);

  if (action!=DIPLOMAT_MOVE
      && is_ocean(tile_get_terrain(pdiplomat->tile))) {
    return FALSE;
  }

  if (pcity) {
    if (pcity->owner != pdiplomat->owner
       && real_map_distance(pdiplomat->tile, pcity->tile) <= 1) {
      if(action==DIPLOMAT_SABOTAGE)
	return pplayers_at_war(unit_owner(pdiplomat), city_owner(pcity));
      if(action==DIPLOMAT_MOVE)
        return pplayers_allied(unit_owner(pdiplomat), city_owner(pcity));
      if (action == DIPLOMAT_EMBASSY
          && !get_player_bonus(city_owner(pcity), EFT_NO_DIPLOMACY)
          && !player_has_embassy(unit_owner(pdiplomat), city_owner(pcity))) {
	return TRUE;
      }
      if(action==SPY_POISON &&
	 pcity->size>1 &&
	 unit_flag(pdiplomat, F_SPY))
	return pplayers_at_war(unit_owner(pdiplomat), city_owner(pcity));
      if(action==DIPLOMAT_INVESTIGATE)
        return TRUE;
      if (action == DIPLOMAT_STEAL && !is_barbarian(city_owner(pcity))) {
	return TRUE;
      }
      if(action==DIPLOMAT_INCITE)
        return !pplayers_allied(city_owner(pcity), unit_owner(pdiplomat));
      if(action==DIPLOMAT_ANY_ACTION)
        return TRUE;
      if (action==SPY_GET_SABOTAGE_LIST && unit_flag(pdiplomat, F_SPY))
	return pplayers_at_war(unit_owner(pdiplomat), city_owner(pcity));
    }
  } else { /* Action against a unit at a tile */
    /* If it is made possible to do action against allied units
       handle_unit_move_request() should be changed so that pdefender
       is also set to allied units */
    struct unit *punit;

    if ((action == SPY_SABOTAGE_UNIT || action == DIPLOMAT_ANY_ACTION) 
        && unit_list_size(ptile->units) == 1
        && unit_flag(pdiplomat, F_SPY)) {
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

/**************************************************************************
FIXME: Maybe we should allow airlifts between allies
**************************************************************************/
bool unit_can_airlift_to(const struct unit *punit, const struct city *pcity)
{
  if(punit->moves_left == 0)
    return FALSE;
  if (!punit->tile->city) {
    return FALSE;
  }
  if (punit->tile->city == pcity) {
    return FALSE;
  }
  if (punit->tile->city->owner != pcity->owner) {
    return FALSE;
  }
  if (!punit->tile->city->airlift || !punit->tile->city->airlift) {
    return FALSE;
  }
  if (!is_ground_unit(punit))
    return FALSE;

  return TRUE;
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
  if (!is_tiles_adjacent(punit->tile, pcity->tile)
      && !same_pos(punit->tile, pcity->tile)) {
    return FALSE;
  }

  return (unit_flag(punit, F_HELP_WONDER)
	  && punit->owner == pcity->owner
	  && !pcity->production.is_unit
	  && is_wonder(pcity->production.value)
	  && (pcity->shield_stock
	      < impr_build_shield_cost(pcity->production.value)));
}


/**************************************************************************
  Return TRUE iff this unit can be disbanded at its current position to
  get full shields for building a wonder.
**************************************************************************/
bool unit_can_help_build_wonder_here(const struct unit *punit)
{
  struct city *pcity = tile_get_city(punit->tile);

  return pcity && unit_can_help_build_wonder(punit, pcity);
}


/**************************************************************************
  Return TRUE iff this unit can be disbanded at its current location to
  provide a trade route from the homecity to the target city.
**************************************************************************/
bool unit_can_est_traderoute_here(const struct unit *punit)
{
  struct city *phomecity, *pdestcity;

  return (unit_flag(punit, F_TRADE_ROUTE)
	  && (pdestcity = tile_get_city(punit->tile))
	  && (phomecity = find_city_by_id(punit->homecity))
	  && can_cities_trade(phomecity, pdestcity));
}


/**************************************************************************
Returns the number of free spaces for ground units. Can be 0 or negative.
**************************************************************************/
int ground_unit_transporter_capacity(const struct tile *ptile,
				     const struct player *pplayer)
{
  int availability = 0;

  unit_list_iterate(ptile->units, punit) {
    if (unit_owner(punit) == pplayer
        || pplayers_allied(unit_owner(punit), pplayer)) {
      if (is_ground_units_transport(punit)
	  && !(is_ground_unit(punit) && is_ocean(ptile->terrain))) {
	availability += get_transporter_capacity(punit);
      } else if (is_ground_unit(punit)) {
	availability--;
      }
    }
  }
  unit_list_iterate_end;

  return availability;
}

/**************************************************************************
  Return the number of units the transporter can hold (or 0).
**************************************************************************/
int get_transporter_capacity(const struct unit *punit)
{
  return unit_type(punit)->transport_capacity;
}

/**************************************************************************
  Return TRUE iff the unit is a transporter of ground units.
**************************************************************************/
bool is_ground_units_transport(const struct unit *punit)
{
  return (get_transporter_capacity(punit) > 0
	  && !unit_flag(punit, F_MISSILE_CARRIER)
	  && !unit_flag(punit, F_CARRIER));
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
  and heli units can occupy empty cities -- see COULD_OCCUPY(punit).
  Some military units, like the Galleon, have no attack strength.
**************************************************************************/
bool is_military_unit(const struct unit *punit)
{
  return !unit_flag(punit, F_NONMIL);
}

/**************************************************************************
  Return TRUE iff this unit is a diplomat (spy) unit.  Diplomatic units
  can do diplomatic actions (not to be confused with diplomacy).
**************************************************************************/
bool is_diplomat_unit(const struct unit *punit)
{
  return (unit_flag(punit, F_DIPLOMAT));
}

/**************************************************************************
  Return TRUE iff the player should consider this unit to be a threat on
  the ground.
**************************************************************************/
static bool is_ground_threat(const struct player *pplayer,
			     const struct unit *punit)
{
  return (pplayers_at_war(pplayer, unit_owner(punit))
	  && (unit_flag(punit, F_DIPLOMAT)
	      || (is_ground_unit(punit) && is_military_unit(punit))));
}

/**************************************************************************
  Return TRUE iff this tile is threatened from any threatening ground unit
  within 2 tiles.
**************************************************************************/
bool is_square_threatened(const struct player *pplayer,
			  const struct tile *ptile)
{
  square_iterate(ptile, 2, ptile1) {
    unit_list_iterate(ptile1->units, punit) {
      if (is_ground_threat(pplayer, punit)) {
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
  return unit_flag(punit, F_FIELDUNIT);
}


/**************************************************************************
  Is the unit one that is invisible on the map. A unit is invisible if
  it has the F_PARTIAL_INVIS flag or if it transported by a unit with
  this flag.
**************************************************************************/
bool is_hiding_unit(const struct unit *punit)
{
  struct unit *transporter = find_unit_by_id(punit->transported_by);

  return (unit_flag(punit, F_PARTIAL_INVIS)
	  || (transporter && unit_flag(transporter, F_PARTIAL_INVIS)));
}

/**************************************************************************
  Return TRUE iff an attack from this unit would kill a citizen in a city
  (city walls protect against this).
**************************************************************************/
bool kills_citizen_after_attack(const struct unit *punit)
{
  return TEST_BIT(game.info.killcitizen, 
                  (int) (unit_type(punit)->move_type) - 1);
}

/**************************************************************************
  Return TRUE iff this unit may be disbanded to add its pop_cost to a
  city at its current location.
**************************************************************************/
bool can_unit_add_to_city(const struct unit *punit)
{
  return (test_unit_add_or_build_city(punit) == AB_ADD_OK);
}

/**************************************************************************
  Return TRUE iff this unit is capable of building a new city at its
  current location.
**************************************************************************/
bool can_unit_build_city(const struct unit *punit)
{
  return (test_unit_add_or_build_city(punit) == AB_BUILD_OK);
}

/**************************************************************************
  Return TRUE iff this unit can add to a current city or build a new city
  at its current location.
**************************************************************************/
bool can_unit_add_or_build_city(const struct unit *punit)
{
  enum add_build_city_result r = test_unit_add_or_build_city(punit);

  return (r == AB_BUILD_OK || r == AB_ADD_OK);
}

/**************************************************************************
  See if the unit can add to an existing city or build a new city at
  its current location, and return a 'result' value telling what is
  allowed.
**************************************************************************/
enum add_build_city_result test_unit_add_or_build_city(const struct unit *
						       punit)
{
  struct city *pcity = tile_get_city(punit->tile);
  bool is_build = unit_flag(punit, F_CITIES);
  bool is_add = unit_flag(punit, F_ADD_TO_CITY);
  int new_pop;

  /* See if we can build */
  if (!pcity) {
    if (!is_build)
      return AB_NOT_BUILD_UNIT;
    if (punit->moves_left == 0)
      return AB_NO_MOVES_BUILD;
    if (!city_can_be_built_here(punit->tile, punit)) {
      return AB_NOT_BUILD_LOC;
    }
    return AB_BUILD_OK;
  }
  
  /* See if we can add */

  if (!is_add)
    return AB_NOT_ADDABLE_UNIT;
  if (punit->moves_left == 0)
    return AB_NO_MOVES_ADD;

  assert(unit_pop_value(punit->type) > 0);
  new_pop = pcity->size + unit_pop_value(punit->type);

  if (new_pop > game.info.add_to_size_limit)
    return AB_TOO_BIG;
  if (pcity->owner != punit->owner)
    return AB_NOT_OWNER;
  if (!city_can_grow_to(pcity, new_pop))
    return AB_NO_SPACE;
  return AB_ADD_OK;
}

/**************************************************************************
  Return TRUE iff the unit can change homecity at its current location.
**************************************************************************/
bool can_unit_change_homecity(const struct unit *punit)
{
  /* Requirements to change homecity:
   *
   * 1. Homeless cities can't change homecity (this is a feature since
   *    being homeless is a big benefit).
   * 2. The unit must be inside the city it is rehoming to.
   * 3. Of course you can only have your own cities as homecity. */
  return (punit->homecity != -1
	  && punit->tile->city
	  && punit->tile->city->owner == punit->owner);
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
  int fact = punit->type->veteran[punit->veteran].power_fact;

  /* The speed of the settler depends on its base move_rate, not on
   * the number of moves actually remaining or the adjusted move rate.
   * This means sea formers won't have their activity rate increased by
   * Magellan's, and it means injured units work just as fast as
   * uninjured ones.  Note the value is never less than SINGLE_MOVE. */
  int move_rate = unit_type(punit)->move_rate;

  /* All settler actions are multiplied by ACTIVITY_COUNT. */
  return ACTIVITY_FACTOR * fact * move_rate / SINGLE_MOVE;
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
  worker units are helping out.
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
  Return whether the unit can be put in auto-settler mode.

  NOTE: we used to have "auto" mode including autosettlers and auto-attack.
  This was bad because the two were indestinguishable even though they
  are very different.  Now auto-attack is done differently so we just have
  auto-settlers.  If any new auto modes are introduced they should be
  handled separately.
**************************************************************************/
bool can_unit_do_autosettlers(const struct unit *punit) 
{
  return unit_flag(punit, F_SETTLERS);
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
  case ACTIVITY_ROAD:
    return _("Road");
  case ACTIVITY_MINE:
    return _("Mine");
  case ACTIVITY_IRRIGATE:
    return _("Irrigation");
  case ACTIVITY_FORTIFYING:
    return _("Fortifying");
  case ACTIVITY_FORTIFIED:
    return _("Fortified");
  case ACTIVITY_FORTRESS:
    return _("Fortress");
  case ACTIVITY_SENTRY:
    return _("Sentry");
  case ACTIVITY_RAILROAD:
    return _("Railroad");
  case ACTIVITY_PILLAGE:
    return _("Pillage");
  case ACTIVITY_GOTO:
    return _("Goto");
  case ACTIVITY_EXPLORE:
    return _("Explore");
  case ACTIVITY_TRANSFORM:
    return _("Transform");
  case ACTIVITY_AIRBASE:
    return _("Airbase");
  case ACTIVITY_FALLOUT:
    return _("Fallout");
  case ACTIVITY_UNKNOWN:
  case ACTIVITY_PATROL_UNUSED:
  case ACTIVITY_LAST:
    break;
  }

  assert(0);
  return _("Unknown");
}

/****************************************************************************
  Return TRUE iff the given unit can be loaded into the transporter.
****************************************************************************/
bool can_unit_load(const struct unit *pcargo, const struct unit *ptrans)
{
  /* This function needs to check EVERYTHING. */

  if (!pcargo || !ptrans) {
    return FALSE;
  }

  /* Check positions of the units.  Of course you can't load a unit onto
   * a transporter on a different tile... */
  if (!same_pos(pcargo->tile, ptrans->tile)) {
    return FALSE;
  }

  /* Double-check ownership of the units: you can load into an allied unit
   * (of course only allied units can be on the same tile). */
  if (!pplayers_allied(unit_owner(pcargo), unit_owner(ptrans))) {
    return FALSE;
  }

  /* Only top-level transporters may be loaded or loaded into. */
  if (pcargo->transported_by != -1 || ptrans->transported_by != -1) {
    return FALSE;
  }

  /* Recursive transporting is not allowed (for now). */
  if (get_transporter_occupancy(pcargo) > 0) {
    return FALSE;
  }

  /* Make sure this transporter can carry this type of unit. */
  if(!can_unit_transport(ptrans, pcargo)) {
    return FALSE;
  }

  /* Make sure there's room in the transporter. */
  return (get_transporter_occupancy(ptrans)
	  < get_transporter_capacity(ptrans));
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
  if (pcargo->transported_by != ptrans->id) {
    return FALSE;
  }

  /* Only top-level transporters may be unloaded.  However the unit being
   * unloaded may be transporting other units (well, at least it's allowed
   * here: elsewhere this may be disallowed). */
  if (ptrans->transported_by != -1) {
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

  if (!unit_flag(punit, F_PARATROOPERS))
    return FALSE;

  if(punit->paradropped)
    return FALSE;

  utype = unit_type(punit);

  if(punit->moves_left < utype->paratroopers_mr_req)
    return FALSE;

  if (tile_has_special(punit->tile, S_AIRBASE)) {
    return TRUE;
  }

  if (!tile_get_city(punit->tile)) {
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
  if (!unit_flag(punit, F_BOMBARDER)) {
    return FALSE;
  }

  if (punit->transported_by != -1) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
  Check if the unit's current activity is actually legal.
**************************************************************************/
bool can_unit_continue_current_activity(struct unit *punit)
{
  enum unit_activity current = punit->activity;
  enum tile_special_type target = punit->activity_target;
  enum unit_activity current2 = 
              (current == ACTIVITY_FORTIFIED) ? ACTIVITY_FORTIFYING : current;
  bool result;

  punit->activity = ACTIVITY_IDLE;
  punit->activity_target = S_LAST;

  result = can_unit_do_activity_targeted(punit, current2, target);

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
  return can_unit_do_activity_targeted(punit, activity, S_LAST);
}

/**************************************************************************
  Return whether the unit can do the targeted activity at its current
  location.
**************************************************************************/
bool can_unit_do_activity_targeted(const struct unit *punit,
				   enum unit_activity activity,
				   enum tile_special_type target)
{
  return can_unit_do_activity_targeted_at(punit, activity, target,
					  punit->tile);
}

/**************************************************************************
  Return TRUE if the unit can do the targeted activity at the given
  location.

  Note that if you make changes here you should also change the code for
  autosettlers in server/settler.c. The code there does not use this
  function as it would be a major CPU hog.
**************************************************************************/
bool can_unit_do_activity_targeted_at(const struct unit *punit,
				      enum unit_activity activity,
				      enum tile_special_type target,
				      const struct tile *ptile)
{
  struct player *pplayer = unit_owner(punit);
  struct terrain *pterrain = ptile->terrain;

  switch(activity) {
  case ACTIVITY_IDLE:
  case ACTIVITY_GOTO:
    return TRUE;

  case ACTIVITY_POLLUTION:
    return (unit_flag(punit, F_SETTLERS)
	    && tile_has_special(ptile, S_POLLUTION));

  case ACTIVITY_FALLOUT:
    return (unit_flag(punit, F_SETTLERS)
	    && tile_has_special(ptile, S_FALLOUT));

  case ACTIVITY_ROAD:
    return (terrain_control.may_road
	    && unit_flag(punit, F_SETTLERS)
	    && !tile_has_special(ptile, S_ROAD)
	    && pterrain->road_time != 0
	    && (!tile_has_special(ptile, S_RIVER)
		|| player_knows_techs_with_flag(pplayer, TF_BRIDGE)));

  case ACTIVITY_MINE:
    /* Don't allow it if someone else is irrigating this tile.
     * *Do* allow it if they're transforming - the mine may survive */
    if (terrain_control.may_mine
	&& unit_flag(punit, F_SETTLERS)
	&& ((ptile->terrain == pterrain->mining_result
	     && !tile_has_special(ptile, S_MINE))
	    || (ptile->terrain != pterrain->mining_result
		&& pterrain->mining_result != T_NONE
		&& (!is_ocean(ptile->terrain)
		    || is_ocean(pterrain->mining_result)
		    || can_reclaim_ocean(ptile))
		&& (is_ocean(ptile->terrain)
		    || !is_ocean(pterrain->mining_result)
		    || can_channel_land(ptile))
		&& (!is_ocean(pterrain->mining_result)
		    || !tile_get_city(ptile))))) {
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
    if (terrain_control.may_irrigate
	&& unit_flag(punit, F_SETTLERS)
	&& (!tile_has_special(ptile, S_IRRIGATION)
	    || (!tile_has_special(ptile, S_FARMLAND)
		&& player_knows_techs_with_flag(pplayer, TF_FARMLAND)))
	&& ((ptile->terrain == pterrain->irrigation_result
	     && is_water_adjacent_to_tile(ptile))
	    || (ptile->terrain != pterrain->irrigation_result
		&& pterrain->irrigation_result != T_NONE
		&& (!is_ocean(ptile->terrain)
		    || is_ocean(pterrain->irrigation_result)
		    || can_reclaim_ocean(ptile))
		&& (is_ocean(ptile->terrain)
		    || !is_ocean(pterrain->irrigation_result)
		    || can_channel_land(ptile))
		&& (!is_ocean(pterrain->irrigation_result)
		    || !tile_get_city(ptile))))) {
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
    return (is_ground_unit(punit)
	    && punit->activity != ACTIVITY_FORTIFIED
	    && !unit_flag(punit, F_SETTLERS)
	    && !is_ocean(ptile->terrain));

  case ACTIVITY_FORTIFIED:
    return FALSE;

  case ACTIVITY_FORTRESS:
    return (unit_flag(punit, F_SETTLERS)
	    && !tile_get_city(ptile)
	    && player_knows_techs_with_flag(pplayer, TF_FORTRESS)
	    && !tile_has_special(ptile, S_FORTRESS)
	    && !is_ocean(ptile->terrain));

  case ACTIVITY_AIRBASE:
    return (unit_flag(punit, F_AIRBASE)
	    && player_knows_techs_with_flag(pplayer, TF_AIRBASE)
	    && !tile_has_special(ptile, S_AIRBASE)
	    && !is_ocean(ptile->terrain));

  case ACTIVITY_SENTRY:
    if (!can_unit_survive_at_tile(punit, punit->tile)
	&& punit->transported_by == -1) {
      /* Don't let units sentry on tiles they will die on. */
      return FALSE;
    }
    return TRUE;

  case ACTIVITY_RAILROAD:
    /* if the tile has road, the terrain must be ok.. */
    return (terrain_control.may_road
	    && unit_flag(punit, F_SETTLERS)
	    && tile_has_special(ptile, S_ROAD)
	    && !tile_has_special(ptile, S_RAILROAD)
	    && player_knows_techs_with_flag(pplayer, TF_RAILROAD));

  case ACTIVITY_PILLAGE:
    {
      int numpresent;
      bv_special pspresent = get_tile_infrastructure_set(ptile, &numpresent);

      if (numpresent > 0 && is_ground_unit(punit)) {
	bv_special psworking;
	int i;

	if (ptile->city && (target == S_ROAD || target == S_RAILROAD)) {
	  return FALSE;
	}
	psworking = get_unit_tile_pillage_set(ptile);
	if (target == S_LAST) {
	  for (i = 0; infrastructure_specials[i] != S_LAST; i++) {
	    enum tile_special_type spe = infrastructure_specials[i];

	    if (ptile->city && (spe == S_ROAD || spe == S_RAILROAD)) {
	      /* Can't pillage this. */
	      continue;
	    }
	    if (BV_ISSET(pspresent, spe) && !BV_ISSET(psworking, spe)) {
	      /* Can pillage this! */
	      return TRUE;
	    }
	  }
	} else if (!game.info.pillage_select
		   && target != get_preferred_pillage(pspresent)) {
	  return FALSE;
	} else {
	  return BV_ISSET(pspresent, target) && !BV_ISSET(psworking, target);
	}
      } else {
	return FALSE;
      }
    }

  case ACTIVITY_EXPLORE:
    return (is_ground_unit(punit) || is_sailing_unit(punit));

  case ACTIVITY_TRANSFORM:
    return (terrain_control.may_transform
	    && pterrain->transform_result != T_NONE
	    && ptile->terrain != pterrain->transform_result
	    && (!is_ocean(ptile->terrain)
		|| is_ocean(pterrain->transform_result)
		|| can_reclaim_ocean(ptile))
	    && (is_ocean(ptile->terrain)
		|| !is_ocean(pterrain->transform_result)
		|| can_channel_land(ptile))
	    && (!terrain_has_flag(pterrain->transform_result, TER_NO_CITIES)
		|| !(tile_get_city(ptile)))
	    && unit_flag(punit, F_TRANSFORM));

  case ACTIVITY_PATROL_UNUSED:
  case ACTIVITY_LAST:
  case ACTIVITY_UNKNOWN:
    break;
  }
  freelog(LOG_ERROR,
	  "Unknown activity %d in can_unit_do_activity_targeted_at()",
	  activity);
  return FALSE;
}

/**************************************************************************
  assign a new task to a unit.
**************************************************************************/
void set_unit_activity(struct unit *punit, enum unit_activity new_activity)
{
  punit->activity=new_activity;
  punit->activity_count=0;
  punit->activity_target = S_LAST;
  if (new_activity == ACTIVITY_IDLE && punit->moves_left > 0) {
    /* No longer done. */
    punit->done_moving = FALSE;
  }
}

/**************************************************************************
  assign a new targeted task to a unit.
**************************************************************************/
void set_unit_activity_targeted(struct unit *punit,
				enum unit_activity new_activity,
				enum tile_special_type new_target)
{
  set_unit_activity(punit, new_activity);
  punit->activity_target = new_target;
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
	&& punit->activity_target != S_LAST) {
      assert(punit->activity_target < S_LAST);
      BV_SET(tgt_ret, punit->activity_target);
    }
  } unit_list_iterate_end;

  return tgt_ret;
}

/**************************************************************************
  Return text describing the current unit's activity.
**************************************************************************/
const char *unit_activity_text(const struct unit *punit)
{
  static char text[64];
  const char *moves_str;
   
  switch(punit->activity) {
   case ACTIVITY_IDLE:
     moves_str = _("Moves");
     if (is_air_unit(punit) && unit_type(punit)->fuel > 0) {
       int rate,f;
       rate=unit_type(punit)->move_rate/SINGLE_MOVE;
       f=((punit->fuel)-1);
      if ((punit->moves_left % SINGLE_MOVE) != 0) {
	 if(punit->moves_left/SINGLE_MOVE>0) {
	   my_snprintf(text, sizeof(text), "%s: (%d)%d %d/%d", moves_str,
		       ((rate*f)+(punit->moves_left/SINGLE_MOVE)),
		       punit->moves_left/SINGLE_MOVE, punit->moves_left%SINGLE_MOVE,
		       SINGLE_MOVE);
	 } else {
	   my_snprintf(text, sizeof(text), "%s: (%d)%d/%d", moves_str,
		       ((rate*f)+(punit->moves_left/SINGLE_MOVE)),
		       punit->moves_left%SINGLE_MOVE, SINGLE_MOVE);
	 }
       } else {
	 my_snprintf(text, sizeof(text), "%s: (%d)%d", moves_str,
		     rate*f+punit->moves_left/SINGLE_MOVE,
		     punit->moves_left/SINGLE_MOVE);
       }
     } else {
      if ((punit->moves_left % SINGLE_MOVE) != 0) {
	 if(punit->moves_left/SINGLE_MOVE>0) {
	   my_snprintf(text, sizeof(text), "%s: %d %d/%d", moves_str,
		       punit->moves_left/SINGLE_MOVE, punit->moves_left%SINGLE_MOVE,
		       SINGLE_MOVE);
	 } else {
	   my_snprintf(text, sizeof(text),
		       "%s: %d/%d", moves_str, punit->moves_left%SINGLE_MOVE,
		       SINGLE_MOVE);
	 }
       } else {
	 my_snprintf(text, sizeof(text),
		     "%s: %d", moves_str, punit->moves_left/SINGLE_MOVE);
       }
     }
     return text;
   case ACTIVITY_POLLUTION:
   case ACTIVITY_FALLOUT:
   case ACTIVITY_ROAD:
   case ACTIVITY_RAILROAD:
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
     return get_activity_text (punit->activity);
   case ACTIVITY_PILLAGE:
     if (punit->activity_target == S_LAST) {
       return get_activity_text (punit->activity);
     } else {
       bv_special pset;

       BV_CLR_ALL(pset);
       BV_SET(pset, punit->activity_target);
       my_snprintf(text, sizeof(text), "%s: %s",
		   get_activity_text (punit->activity),
		   get_infrastructure_text(pset));
       return (text);
     }
   default:
    die("Unknown unit activity %d in unit_activity_text()", punit->activity);
  }
  return NULL;
}

/**************************************************************************
  Look for a unit with the given ID in the unit list.
**************************************************************************/
struct unit *unit_list_find(const struct unit_list *This, int id)
{
  unit_list_iterate(This, punit) {
    if (punit->id == id) {
      return punit;
    }
  } unit_list_iterate_end;

  return NULL;
}

/**************************************************************************
 Comparison function for genlist_sort, sorting by ord_map:
 The indirection is a bit gory:
 Read from the right:
   1. cast arg "a" to "ptr to void*"   (we're sorting a list of "void*"'s)
   2. dereference to get the "void*"
   3. cast that "void*" to a "struct unit*"
**************************************************************************/
static int compar_unit_ord_map(const void *a, const void *b)
{
  const struct unit *ua, *ub;
  ua = (const struct unit*) *(const void**)a;
  ub = (const struct unit*) *(const void**)b;
  return ua->ord_map - ub->ord_map;
}

/**************************************************************************
 Comparison function for genlist_sort, sorting by ord_city: see above.
**************************************************************************/
static int compar_unit_ord_city(const void *a, const void *b)
{
  const struct unit *ua, *ub;
  ua = (const struct unit*) *(const void**)a;
  ub = (const struct unit*) *(const void**)b;
  return ua->ord_city - ub->ord_city;
}

/**************************************************************************
...
**************************************************************************/
void unit_list_sort_ord_map(struct unit_list *This)
{
  if (unit_list_size(This) > 1) {
    unit_list_sort(This, compar_unit_ord_map);
  }
}

/**************************************************************************
...
**************************************************************************/
void unit_list_sort_ord_city(struct unit_list *This)
{
  if (unit_list_size(This) > 1) {
    unit_list_sort(This, compar_unit_ord_city);
  }
}

/**************************************************************************
  Return the unit's owner.
**************************************************************************/
struct player *unit_owner(const struct unit *punit)
{
  return punit->owner;
}

/****************************************************************************
  Measure the carrier (missile + airplane) capacity of the given tile for
  a player.

  In the future this should probably look at the actual occupancy of the
  transporters.  However for now we only look at the potential capacity and
  leave loading up to the caller.
****************************************************************************/
static void count_carrier_capacity(int *airall, int *misonly,
				   const struct tile *ptile,
				   const struct player *pplayer,
				   bool count_units_with_extra_fuel)
{
  *airall = *misonly = 0;

  unit_list_iterate(ptile->units, punit) {
    if (unit_owner(punit) == pplayer) {
      if (unit_flag(punit, F_CARRIER)
	  && !(is_ground_unit(punit) && is_ocean(ptile->terrain))) {
	*airall += get_transporter_capacity(punit);
	continue;
      }
      if (unit_flag(punit, F_MISSILE_CARRIER)
	  && !(is_ground_unit(punit) && is_ocean(ptile->terrain))) {
	*misonly += get_transporter_capacity(punit);
	continue;
      }

      /* Don't count units which have enough fuel (>1) */
      if (is_air_unit(punit)
	  && (count_units_with_extra_fuel || punit->fuel <= 1)) {
	if (unit_flag(punit, F_MISSILE)) {
	  (*misonly)--;
	} else {
	  (*airall)--;
	}
      }
    }
  } unit_list_iterate_end;
}

/**************************************************************************
  Returns the number of free spaces for missiles for the given player on
  the given tile. Can be 0 or negative.
**************************************************************************/
int missile_carrier_capacity(const struct tile *ptile,
			     const struct player *pplayer,
			     bool count_units_with_extra_fuel)
{
  int airall, misonly;

  count_carrier_capacity(&airall, &misonly, ptile, pplayer,
			 count_units_with_extra_fuel);

  /* Any extra air spaces can be used by missles, but if there aren't enough
   * air spaces this doesn't bother missiles. */
  return MAX(airall, 0) + misonly;
}

/**************************************************************************
  Returns the number of free spaces for airunits (includes missiles) for
  the given player on the given tile.  Can be 0 or negative.
**************************************************************************/
int airunit_carrier_capacity(const struct tile *ptile,
			     const struct player *pplayer,
			     bool count_units_with_extra_fuel)
{
  int airall, misonly;

  count_carrier_capacity(&airall, &misonly, ptile, pplayer,
			 count_units_with_extra_fuel);

  /* Any extra missile spaces are useless to air units, but if there aren't
   * enough missile spaces the missles must take up airunit capacity. */
  return airall + MIN(misonly, 0);
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
    if (is_ocean(ptile->terrain)) {
      continue;
    }
    if (is_non_allied_unit_tile(ptile, pplayer)) {
      /* Note: in the client, the above function will return NULL 
       * if there is a city there, even if the city is occupied */
      return FALSE;
    }
    
    if (!is_server) {
      struct city *pcity = is_non_allied_city_tile(ptile, pplayer);

      if (pcity 
          && (pcity->client.occupied 
              || tile_get_known(ptile, pplayer) == TILE_KNOWN_FOGGED)) {
        /* If the city is fogged, we assume it's occupied */
        return FALSE;
      }
    }
  } square_iterate_end;

  return TRUE;
}

/**************************************************************************
  Takes into account unit move_type as well as IGZOC
**************************************************************************/
bool unit_type_really_ignores_zoc(const struct unit_type *punittype)
{
  return (!is_ground_unittype(punittype)
	  || unit_type_flag(punittype, F_IGZOC));
}

/**************************************************************************
  Calculate the chance of losing (as a percentage) if it were to spend a
  turn at the given location.

  Note this function isn't really useful for AI planning, since it needs
  to know more.  The AI code uses base_trireme_loss_pct and
  base_unsafe_terrain_loss_pct directly.
**************************************************************************/
int unit_loss_pct(const struct player *pplayer, const struct tile *ptile,
		  const struct unit *punit)
{
  int loss_pct = 0;

  /* Units are never lost if they're inside cities. */
  if (tile_get_city(ptile)) {
    return 0; 
  }

  /* Trireme units may be lost if they stray from coastline. */
  if (unit_flag(punit, F_TRIREME)) {
    if (!is_safe_ocean(ptile)) {
      loss_pct = base_trireme_loss_pct(pplayer, punit);
    }
  }

  /* All units may be lost on unsafe terrain.  (Actually air units are
   * exempt; see base_unsafe_terrain_loss_pct.) */
  if (terrain_has_flag(tile_get_terrain(ptile), TER_UNSAFE)) {
    return loss_pct + base_unsafe_terrain_loss_pct(pplayer, punit);
  }

  return loss_pct;
}

/**************************************************************************
  Triremes have a varying loss percentage based on tech and veterancy
  level.
**************************************************************************/
int base_trireme_loss_pct(const struct player *pplayer,
			  const struct unit *punit)
{
  if (get_unit_bonus(punit, EFT_NO_SINK_DEEP) > 0) {
    return 0;
  } else if (player_knows_techs_with_flag(pplayer, TF_REDUCE_TRIREME_LOSS2)) {
    return game.trireme_loss_chance[punit->veteran] / 4;
  } else if (player_knows_techs_with_flag(pplayer, TF_REDUCE_TRIREME_LOSS1)) {
    return game.trireme_loss_chance[punit->veteran] / 2;
  } else {
    return game.trireme_loss_chance[punit->veteran];
  }
}

/**************************************************************************
  All units except air units have a flat 15% chance of being lost.
**************************************************************************/
int base_unsafe_terrain_loss_pct(const struct player *pplayer,
				 const struct unit *punit)
{
  return (is_air_unit(punit) || is_heli_unit(punit)) ? 0 : 15;
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
  if (tile_get_city(punit->tile)) {
    return FALSE;
  }
  if (game.info.borders > 0
      && game.info.happyborders
      && tile_get_owner(punit->tile) == unit_owner(punit)) {
    return FALSE;
  }
  if (is_ground_unit(punit) &&
      tile_has_special(punit->tile, S_FORTRESS)) {
    return !is_unit_near_a_friendly_city (punit);
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
  case ACTIVITY_ROAD:
  case ACTIVITY_MINE:
  case ACTIVITY_IRRIGATE:
  case ACTIVITY_FORTRESS:
  case ACTIVITY_RAILROAD:
  case ACTIVITY_TRANSFORM:
  case ACTIVITY_AIRBASE:
  case ACTIVITY_FALLOUT:
    return TRUE;
  default:
    return FALSE;
  }
}

/**************************************************************************
  Create a virtual unit skeleton. pcity can be NULL, but then you need
  to set x, y and homecity yourself.
**************************************************************************/
struct unit *create_unit_virtual(struct player *pplayer, struct city *pcity,
                                 struct unit_type *punittype,
				 int veteran_level)
{
  struct unit *punit = fc_calloc(1, sizeof(struct unit));

  CHECK_UNIT_TYPE(punittype); /* No untyped units! */
  punit->type = punittype;
  assert(pplayer != NULL); /* No unowned units! */
  punit->owner = pplayer;
  if (pcity) {
    punit->tile = pcity->tile;
    punit->homecity = pcity->id;
  } else {
    punit->tile = NULL;
    punit->homecity = 0;
  }
  punit->goto_tile = NULL;
  punit->veteran = veteran_level;
  memset(punit->upkeep, 0, O_COUNT * sizeof(*punit->upkeep));
  punit->unhappiness = 0;
  /* A unit new and fresh ... */
  punit->foul = FALSE;
  punit->debug = FALSE;
  punit->fuel = unit_type(punit)->fuel;
  punit->hp = unit_type(punit)->hp;
  punit->moves_left = unit_move_rate(punit);
  punit->moved = FALSE;
  punit->paradropped = FALSE;
  punit->done_moving = FALSE;
  if (is_barbarian(pplayer)) {
    punit->fuel = BARBARIAN_LIFE;
  }
  punit->ai.done = FALSE;
  punit->ai.cur_pos = NULL;
  punit->ai.prev_pos = NULL;
  punit->ai.target = 0;
  punit->ai.hunted = 0;
  punit->ai.control = FALSE;
  punit->ai.ai_role = AIUNIT_NONE;
  punit->ai.ferryboat = 0;
  punit->ai.passenger = 0;
  punit->ai.bodyguard = 0;
  punit->ai.charge = 0;
  punit->bribe_cost = -1; /* flag value */
  punit->transported_by = -1;
  punit->focus_status = FOCUS_AVAIL;
  punit->ord_map = 0;
  punit->ord_city = 0;
  set_unit_activity(punit, ACTIVITY_IDLE);
  punit->occupy = 0;
  punit->client.colored = FALSE;
  punit->server.vision = NULL; /* No vision. */
  punit->has_orders = FALSE;

  return punit;
}

/**************************************************************************
  Free the memory used by virtual unit. By the time this function is
  called, you should already have unregistered it everywhere.
**************************************************************************/
void destroy_unit_virtual(struct unit *punit)
{
  free_unit_orders(punit);
  free(punit);
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
  Expensive function to check how many units are in the transport.
****************************************************************************/
int get_transporter_occupancy(const struct unit *ptrans)
{
  int occupied = 0;

  unit_list_iterate(ptrans->tile->units, pcargo) {
    if (pcargo->transported_by == ptrans->id) {
      occupied++;
    }
  } unit_list_iterate_end;

  return occupied;
}

/****************************************************************************
  Find a transporter at the given location for the unit.
****************************************************************************/
struct unit *find_transporter_for_unit(const struct unit *pcargo,
				       const struct tile *ptile)
{ 
  unit_list_iterate(ptile->units, ptrans) {
    if (can_unit_load(pcargo, ptrans)) {
      return ptrans;
    }
  } unit_list_iterate_end;

  return NULL;
}

/***************************************************************************
  Tests if the unit could be updated. Returns UR_OK if is this is
  possible.

  is_free should be set if the unit upgrade is "free" (e.g., Leonardo's).
  Otherwise money is needed and the unit must be in an owned city.

  Note that this function is strongly tied to unittools.c:upgrade_unit().
***************************************************************************/
enum unit_upgrade_result test_unit_upgrade(const struct unit *punit,
					   bool is_free)
{
  struct player *pplayer = unit_owner(punit);
  struct unit_type *to_unittype = can_upgrade_unittype(pplayer, punit->type);
  struct city *pcity;
  int cost;

  if (!to_unittype) {
    return UR_NO_UNITTYPE;
  }

  if (!is_free) {
    cost = unit_upgrade_price(pplayer, punit->type, to_unittype);
    if (pplayer->economic.gold < cost) {
      return UR_NO_MONEY;
    }

    pcity = tile_get_city(punit->tile);
    if (!pcity) {
      return UR_NOT_IN_CITY;
    }
    if (city_owner(pcity) != pplayer) {
      /* TODO: should upgrades in allied cities be possible? */
      return UR_NOT_CITY_OWNER;
    }
  }

  if (get_transporter_occupancy(punit) > to_unittype->transport_capacity) {
    /* TODO: allow transported units to be reassigned.  Check for
     * ground_unit_transporter_capacity here and make changes to
     * upgrade_unit. */
    return UR_NOT_ENOUGH_ROOM;
  }

  return UR_OK;
}

/**************************************************************************
  Find the result of trying to upgrade the unit, and a message that
  most callers can use directly.
**************************************************************************/
enum unit_upgrade_result get_unit_upgrade_info(char *buf, size_t bufsz,
					       const struct unit *punit)
{
  struct player *pplayer = unit_owner(punit);
  enum unit_upgrade_result result = test_unit_upgrade(punit, FALSE);
  int upgrade_cost;
  struct unit_type *from_unittype = punit->type;
  struct unit_type *to_unittype = can_upgrade_unittype(pplayer,
						  punit->type);

  switch (result) {
  case UR_OK:
    upgrade_cost = unit_upgrade_price(pplayer, from_unittype, to_unittype);
    /* This message is targeted toward the GUI callers. */
    my_snprintf(buf, bufsz, _("Upgrade %s to %s for %d gold?\n"
			      "Treasury contains %d gold."),
		from_unittype->name, to_unittype->name,
		upgrade_cost, pplayer->economic.gold);
    break;
  case UR_NO_UNITTYPE:
    my_snprintf(buf, bufsz,
		_("Sorry, cannot upgrade %s (yet)."),
		from_unittype->name);
    break;
  case UR_NO_MONEY:
    upgrade_cost = unit_upgrade_price(pplayer, from_unittype, to_unittype);
    my_snprintf(buf, bufsz,
		_("Upgrading %s to %s costs %d gold.\n"
		  "Treasury contains %d gold."),
		from_unittype->name, to_unittype->name,
		upgrade_cost, pplayer->economic.gold);
    break;
  case UR_NOT_IN_CITY:
  case UR_NOT_CITY_OWNER:
    my_snprintf(buf, bufsz,
		_("You can only upgrade units in your cities."));
    break;
  case UR_NOT_ENOUGH_ROOM:
    my_snprintf(buf, bufsz,
		_("Upgrading this %s would strand units it transports."),
		from_unittype->name);
    break;
  }

  return result;
}
