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
#include "combat.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "movement.h"
#include "packets.h"
#include "player.h"
#include "shared.h"
#include "unit.h"

#include "citymap.h"
#include "path_finding.h"
#include "pf_tools.h"

#include "airgoto.h"
#include "barbarian.h"
#include "citytools.h"
#include "cityturn.h"
#include "gotohand.h"
#include "maphand.h"
#include "plrhand.h"
#include "score.h"
#include "settlers.h"
#include "unithand.h"
#include "unittools.h"

#include "advmilitary.h"
#include "aicity.h"
#include "aidata.h"
#include "aiferry.h"
#include "aiguard.h"
#include "ailog.h"
#include "aitech.h"
#include "aiunit.h"

#include "aitools.h"

/**************************************************************************
  Return a string describing a unit's AI role.
**************************************************************************/
const char *get_ai_role_str(enum ai_unit_task task)
{
  switch(task) {
   case AIUNIT_NONE:
     return "None";
   case AIUNIT_AUTO_SETTLER:
     return "Auto settler";
   case AIUNIT_BUILD_CITY:
     return "Build city";
   case AIUNIT_DEFEND_HOME:
     return "Defend home";
   case AIUNIT_ATTACK:
     return "Attack";
   case AIUNIT_ESCORT:
     return "Escort";
   case AIUNIT_EXPLORE:
     return "Explore";
   case AIUNIT_RECOVER:
     return "Recover";
   case AIUNIT_HUNTER:
     return "Hunter";
  }
  assert(FALSE);
  return NULL;
}

/**************************************************************************
  Amortize a want modified by the shields (build_cost) we risk losing.
  We add the build time of the unit(s) we risk to amortize delay.  The
  build time is claculated as the build cost divided by the production
  output of the unit's homecity or the city where we want to produce
  the unit. If the city has less than average shield output, we
  instead use the average, to encourage long-term thinking.
**************************************************************************/
int military_amortize(struct player *pplayer, struct city *pcity,
                      int value, int delay, int build_cost)
{
  struct ai_data *ai = ai_data_get(pplayer);
  int city_output = (pcity ? pcity->surplus[O_SHIELD] : 1);
  int output = MAX(city_output, ai->stats.average_production);
  int build_time = build_cost / MAX(output, 1);

  if (value <= 0) {
    return 0;
  }

  return amortize(value, delay + build_time);
}

/**********************************************************************
  There are some signs that a player might be dangerous: We are at 
  war with him, he has done lots of ignoble things to us, he is an 
  ally of one of our enemies (a ticking bomb to be sure), or he is 
  our war target.
***********************************************************************/
bool is_player_dangerous(struct player *pplayer, struct player *aplayer)
{
  struct ai_data *ai = ai_data_get(pplayer);
  struct ai_dip_intel *adip = &ai->diplomacy.player_intel[aplayer->player_no];
  int reason = pplayer->diplstates[aplayer->player_no].has_reason_to_cancel;

  return (pplayer != aplayer
          && (pplayers_at_war(pplayer, aplayer)
              || adip->countdown >= 0
              || reason != 0
              || adip->is_allied_with_enemy));
}

/*************************************************************************
  This is a function to execute paths returned by the path-finding engine,
  for AI units and units (such as auto explorers) temporarily controlled
  by the AI.

  Brings our bodyguard along.
  Returns FALSE only if died.
*************************************************************************/
bool ai_unit_execute_path(struct unit *punit, struct pf_path *path)
{
  const bool is_ai = unit_owner(punit)->ai.control;
  int i;

  /* We start with i = 1 for i = 0 is our present position */
  for (i = 1; i < path->length; i++) {
    struct tile *ptile = path->positions[i].tile;
    int id = punit->id;

    if (same_pos(punit->tile, ptile)) {
      UNIT_LOG(LOG_DEBUG, punit, "execute_path: waiting this turn");
      return TRUE;
    }

    /* We use ai_unit_move() for everything but the last step
     * of the way so that we abort if unexpected opposition
     * shows up. Any enemy on the target tile is expected to
     * be our target and any attack there intentional.
     * However, do not annoy human players by automatically attacking
     * using units temporarily under AI control (such as auto-explorers)
     */
    if (is_ai && i == path->length - 1) {
      (void) ai_unit_attack(punit, ptile);
    } else {
      (void) ai_unit_move(punit, ptile);
    }
    if (!find_unit_by_id(id)) {
      /* Died... */
      return FALSE;
    }

    if (!same_pos(punit->tile, ptile) || punit->moves_left <= 0) {
      /* Stopped (or maybe fought) or ran out of moves */
      return TRUE;
    }
  }

  return TRUE;
}

/****************************************************************************
  A helper function for ai_gothere.  Estimates the dangers we will
  be facing at our destination and tries to find/request a bodyguard if 
  needed.
****************************************************************************/
static void ai_gothere_bodyguard(struct unit *punit, struct tile *dest_tile)
{
  struct player *pplayer = unit_owner(punit);
  struct ai_data *ai = ai_data_get(pplayer);
  unsigned int danger = 0;
  struct city *dcity;
  struct tile *ptile;
  struct unit *guard = aiguard_guard_of(punit);
  
  if (is_barbarian(unit_owner(punit))) {
    /* barbarians must have more courage (ie less brains) */
    aiguard_clear_guard(punit);
    return;
  }

  /* Estimate enemy attack power. */
  unit_list_iterate(dest_tile->units, aunit) {
    if (HOSTILE_PLAYER(pplayer, ai, unit_owner(aunit))) {
      danger += unit_att_rating(aunit);
    }
  } unit_list_iterate_end;
  dcity = tile_get_city(dest_tile);
  if (dcity && HOSTILE_PLAYER(pplayer, ai, city_owner(dcity))) {
    /* Assume enemy will build another defender, add it's attack strength */
    struct unit_type *d_type = ai_choose_defender_versus(dcity, punit->type);

    danger += 
      unittype_att_rating(d_type, do_make_unit_veteran(dcity, d_type), 
                          SINGLE_MOVE, d_type->hp);
  }
  danger *= POWER_DIVIDER;

  /* If we are fast, there is less danger.
   * FIXME: that assumes that most units have move_rate == SINGLE_MOVE;
   * not true for all rule-sets */
  danger /= (unit_type(punit)->move_rate / SINGLE_MOVE);
  if (unit_flag(punit, F_IGTER)) {
    danger /= 1.5;
  }

  ptile = punit->tile;
  /* We look for the bodyguard where we stand. */
  if (guard == NULL || guard->tile != punit->tile) {
    int my_def = (punit->hp 
                  * unit_type(punit)->veteran[punit->veteran].power_fact
		  * unit_type(punit)->defense_strength
                  * POWER_FACTOR);
    
    if (danger >= my_def) {
      UNIT_LOG(LOGLEVEL_BODYGUARD, punit, 
               "want bodyguard @(%d, %d) danger=%d, my_def=%d", 
               TILE_XY(dest_tile), danger, my_def);
      aiguard_request_guard(punit);
    } else {
      aiguard_clear_guard(punit);
    }
  }

  /* What if we have a bodyguard, but don't need one? */
}

#define LOGLEVEL_GOTHERE LOG_DEBUG
/****************************************************************************
  This is ferry-enabled goto.  Should not normally be used for non-ferried 
  units (i.e. planes or ships), use ai_unit_goto instead.

  Return values: TRUE if got to or next to our destination, FALSE otherwise. 

  TODO: A big one is rendezvous points.  When this is implemented, we won't
  have to be at the coast to ask for a boat to come to us.
****************************************************************************/
bool ai_gothere(struct player *pplayer, struct unit *punit,
                struct tile *dest_tile)
{
  CHECK_UNIT(punit);

  if (same_pos(dest_tile, punit->tile) || punit->moves_left <= 0) {
    /* Nowhere to go */
    return TRUE;
  }

  /* See if we need a bodyguard at our destination */
  /* FIXME: If bodyguard is _really_ necessary, don't go anywhere */
  ai_gothere_bodyguard(punit, dest_tile);

  if (punit->transported_by > 0 
      || !goto_is_sane(punit, dest_tile, TRUE)) {
    /* Must go by boat, call an aiferryboat function */
    if (!aiferry_gobyboat(pplayer, punit, dest_tile)) {
      return FALSE;
    }
  }

  /* Go where we should be going if we can, and are at our destination 
   * if we are on a ferry */
  if (goto_is_sane(punit, dest_tile, TRUE) && punit->moves_left > 0) {
    punit->goto_tile = dest_tile;
    UNIT_LOG(LOGLEVEL_GOTHERE, punit, "Walking to (%d,%d)",
	     dest_tile->x, dest_tile->y);
    if (!ai_unit_goto(punit, dest_tile)) {
      /* died */
      return FALSE;
    }
    /* liable to bump into someone that will kill us.  Should avoid? */
  } else {
    UNIT_LOG(LOGLEVEL_GOTHERE, punit, "Not moving");
    return FALSE;
  }

  if (punit->ai.ferryboat > 0 && punit->transported_by <= 0) {
    /* We probably just landed, release our boat */
    aiferry_clear_boat(punit);
  }
  
  /* Dead unit shouldn't reach this point */
  CHECK_UNIT(punit);

  return (same_pos(punit->tile, dest_tile) 
          || is_tiles_adjacent(punit->tile, dest_tile));
}

/**************************************************************************
  Returns the destination for a unit moving towards a given final destination.
  That is, it gives a suitable way-point, if necessary.
  For example, aircraft need these way-points to refuel.
**************************************************************************/
struct tile *immediate_destination(struct unit *punit,
				   struct tile *dest_tile)
{
  if (!same_pos(punit->tile, dest_tile) && is_air_unit(punit)) {
    struct tile *waypoint_tile = punit->goto_tile;

    if (find_air_first_destination(punit, &waypoint_tile)) {
      return waypoint_tile;
    } else {
      struct player *pplayer = unit_owner(punit);

      freelog(LOG_VERBOSE, "Did not find an air-route for "
	      "%s's %s at (%d, %d) -> (%d, %d)",
	      pplayer->name, unit_type(punit)->name,
	      TILE_XY(punit->tile), TILE_XY(dest_tile));
      /* Prevent take off */
      return punit->tile;
    }
  }
  /* else does not need way-points */
  return dest_tile;
}

/**************************************************************************
  Move a unit along a path without disturbing its activity, role
  or assigned destination
  Return FALSE iff we died.
**************************************************************************/
bool ai_follow_path(struct unit *punit, struct pf_path *path,
		    struct tile *ptile)
{
  struct tile *old_tile = punit->goto_tile;
  enum unit_activity activity = punit->activity;
  bool alive;

  if (punit->moves_left <= 0) {
    return TRUE;
  }
  punit->goto_tile = ptile;
  handle_unit_activity_request(punit, ACTIVITY_GOTO);
  alive = ai_unit_execute_path(punit, path);
  if (alive) {
    handle_unit_activity_request(punit, ACTIVITY_IDLE);
    send_unit_info(NULL, punit);
    handle_unit_activity_request(punit, activity);
    punit->goto_tile = old_tile; /* May be NULL. */
    send_unit_info(NULL, punit);
  }
  return alive;
}

/**************************************************************************
  Log the cost of travelling a path.
**************************************************************************/
void ai_log_path(struct unit *punit,
		 struct pf_path *path, struct pf_parameter *parameter)
{
  struct pf_position *last = pf_last_position(path);
  const int cc = PF_TURN_FACTOR * last->total_MC
                 + parameter->move_rate * last->total_EC;
  const int tc = cc / (PF_TURN_FACTOR *parameter->move_rate); 

  UNIT_LOG(LOG_DEBUG, punit, "path L=%d T=%d(%d) MC=%d EC=%d CC=%d",
	   path->length - 1, last->turn, tc,
	   last->total_MC, last->total_EC, cc);
}

/**************************************************************************
  Go to specified destination, subject to given PF constraints,
  but do not disturb existing role or activity
  and do not clear the role's destination. Return FALSE iff we died.

  parameter: the PF constraints on the computed path. The unit will move
  as far along the computed path is it can; the movement code will impose
  all the real constraints (ZoC, etc).
**************************************************************************/
bool ai_unit_goto_constrained(struct unit *punit, struct tile *ptile,
			      struct pf_parameter *parameter)
{
  bool alive = TRUE;
  struct pf_map *map = NULL;
  struct pf_path *path = NULL;

  UNIT_LOG(LOG_DEBUG, punit, "constrained goto to %d,%d",
	   ptile->x, ptile->y);

  ptile = immediate_destination(punit, ptile);

  UNIT_LOG(LOG_DEBUG, punit, "constrained goto: let's go to %d,%d",
	   ptile->x, ptile->y);

  if (same_pos(punit->tile, ptile)) {
    /* Not an error; sometimes immediate_destination instructs the unit
     * to stay here. For example, to refuel.*/
    UNIT_LOG(LOG_DEBUG, punit, "constrained goto: already there!");
    send_unit_info(NULL, punit);
    return TRUE;
  } else if (!goto_is_sane(punit, ptile, FALSE)) {
    UNIT_LOG(LOG_DEBUG, punit, "constrained goto: 'insane' goto!");
    punit->activity = ACTIVITY_IDLE;
    send_unit_info(NULL, punit);
    return TRUE;
  } else if(punit->moves_left == 0) {
    UNIT_LOG(LOG_DEBUG, punit, "constrained goto: no moves left!");
    send_unit_info(NULL, punit);
    return TRUE;
  }

  map = pf_create_map(parameter);
  path = pf_get_path(map, ptile);

  if (path) {
    ai_log_path(punit, path, parameter);
    UNIT_LOG(LOG_DEBUG, punit, "constrained goto: following path.");
    alive = ai_follow_path(punit, path, ptile);
  } else {
    UNIT_LOG(LOG_DEBUG, punit, "no path to destination");
  }

  pf_destroy_path(path);
  pf_destroy_map(map);

  return alive;
}


/*********************************************************************
  The value of the units belonging to a given player on a given tile.
*********************************************************************/
static int stack_value(const struct tile *ptile,
		       const struct player *pplayer)
{
  int cost = 0;

  if (is_stack_vulnerable(ptile)) {
    unit_list_iterate(ptile->units, punit) {
      if (unit_owner(punit) == pplayer) {
	cost += unit_build_shield_cost(punit->type);
      }
    } unit_list_iterate_end;
  }

  return cost;
}

/*********************************************************************
  How dangerous would it be stop on a particular tile,
  because of enemy attacks,
  expressed as the probability of being killed.

  TODO: This implementation is a kludge until we compute a more accurate
  probability using the movemap.
  Also, we should take into account the reduced probability of death
  if we have a bodyguard travelling with us.
*********************************************************************/
static double chance_killed_at(const struct tile *ptile,
			       struct ai_risk_cost *risk_cost,
			       struct pf_parameter *param)
{
  double db;
  /* Compute the basic probability */
  /* WAG */
  /* In the early stages of a typical game, ferries
   * are effectively invulnerable (not until Frigates set sail),
   * so we make seas appear safer.
   * If we don't do this, the amphibious movement code has too strong a
   * desire to minimise the length of the path,
   * leading to poor choice for landing beaches */
  double p = is_ocean(ptile->terrain)? 0.05: 0.15;

  /* If we are on defensive terrain, we are more likely to survive */
  db = 10 + ptile->terrain->defense_bonus / 10;
  if (tile_has_special(ptile, S_RIVER)) {
    db += (db * terrain_control.river_defense_bonus) / 100;
  }
  p *= 10.0 / db;

  return p;
}

/*********************************************************************
  PF stack risk cost. How undesirable is passing through a tile
  because of risks?
  Weight by the cost of destruction, for risks that can kill the unit.

  Why use the build cost when assessing the cost of destruction?
  The reasoning is thus.
  - Assume that all our units are doing necessary jobs;
    none are surplus to requirements.
    If that is not the case, we have problems elsewhere :-)
  - Then any units that are destroyed will have to be replaced.
  - The cost of replacing them will be their build cost.
  - Therefore the total (re)build cost is a good representation of the
    the cost of destruction.
*********************************************************************/
static int stack_risk(const struct tile *ptile,
		      struct ai_risk_cost *risk_cost,
		      struct pf_parameter *param)
{
  double risk = 0;
  /* Compute the risk of destruction, assuming we will stop at this tile */
  const double value = risk_cost->base_value
                       + stack_value(ptile, param->owner);
  const double p_killed = chance_killed_at(ptile, risk_cost, param);
  double danger = value * p_killed;

  if (terrain_has_flag(ptile->terrain, TER_UNSAFE)) {
    danger += risk_cost->unsafe_terrain_cost;
  }
  if (is_ocean(ptile->terrain) && !is_safe_ocean(ptile)) {
    danger += risk_cost->ocean_cost;
  }

  /* Adjust for the fact that we might not stop at this tile,
   * and for our fearfulness */
  risk += danger * risk_cost->fearfulness;

  /* Adjust for the risk that we might become stuck (for an indefinite period)
   * if we enter or try to enter the tile. */
  if (risk_cost->enemy_zoc_cost != 0
      && (is_non_allied_city_tile(ptile, param->owner)
	  || !is_my_zoc(param->owner, ptile)
	  || is_non_allied_unit_tile(ptile, param->owner))) {
    /* We could become stuck. */
    risk += risk_cost->enemy_zoc_cost;
  }

  return risk;
}

/*********************************************************************
  PF extra cost call back to avoid creating tall stacks or
  crossing dangerous tiles.
  By setting this as an extra-cost call-back, paths will avoid tall stacks.
  Avoiding tall stacks *all* along a path is useful because a unit following a
  path might have to stop early because of ZoCs.
*********************************************************************/
static int prefer_short_stacks(const struct tile *ptile,
			       enum known_type known,
			       struct pf_parameter *param)
{
  return stack_risk(ptile, (struct ai_risk_cost *)param->data, param);
}

/**********************************************************************
  Set PF call-backs to favour paths that do not create tall stacks
  or cross dangerous tiles.
***********************************************************************/
void ai_avoid_risks(struct pf_parameter *parameter,
		    struct ai_risk_cost *risk_cost,
		    struct unit *punit,
		    const double fearfulness)
{
  const struct player *pplayer = unit_owner(punit);
  /* If we stay a short time on each tile, the danger of each individual tile
   * is reduced. If we do not do this,
   * we will not favour longer but faster routs. */
  const double linger_fraction = (double)SINGLE_MOVE / parameter->move_rate;

  parameter->data = risk_cost;
  parameter->get_EC = prefer_short_stacks;
  parameter->turn_mode = TM_WORST_TIME;
  risk_cost->base_value = unit_build_shield_cost(punit->type);
  risk_cost->fearfulness = fearfulness * linger_fraction;

  if (unit_flag(punit, F_TRIREME)) {
    risk_cost->ocean_cost = risk_cost->base_value
      * (double)base_trireme_loss_pct(pplayer, punit)
      / 100.0;
  } else {
    risk_cost->ocean_cost = 0;
  }
  risk_cost->unsafe_terrain_cost = risk_cost->base_value
    * (double)base_unsafe_terrain_loss_pct(pplayer, punit) / 100.0;
  risk_cost->enemy_zoc_cost = PF_TURN_FACTOR * 20;
}

/*
 * The length of time, in turns, which is long enough to be optimistic
 * that enemy units will have moved from their current position.
 * WAG
 */
#define LONG_TIME 4
/**************************************************************************
  Set up the constraints on a path for an AI unit,
  of for a unit (such as an auto-explorer) temporarily under AI control.

  For non-AI units, take care to prevent cheats, because the AI is 
  omniscient but the players are not. (Ideally, this code should not
  be used by non-AI units at all, though.)

  parameter:
     constraints (output)
  risk_cost:
     auxiliary data used by the constraints (output)
  ptile:
     the destination of the unit.
     For ferries, the destination may be a coastal land tile,
     in which case the ferry should stop on an adjacent tile.
**************************************************************************/
void ai_fill_unit_param(struct pf_parameter *parameter,
			struct ai_risk_cost *risk_cost,
			struct unit *punit, struct tile *ptile)
{
  const bool is_ferry = get_transporter_capacity(punit) > 0
                        && !unit_flag(punit, F_MISSILE_CARRIER)
                        && punit->ai.ai_role != AIUNIT_HUNTER;
  const bool is_air = is_air_unit(punit)
                      && punit->ai.ai_role != AIUNIT_ESCORT;
  const bool long_path = LONG_TIME < (map_distance(punit->tile, punit->tile)
				      * SINGLE_MOVE
				      / unit_type(punit)->move_rate);
  const bool barbarian = is_barbarian(unit_owner(punit));
  const bool is_ai = unit_owner(punit)->ai.control;

  if (is_ferry) {
    /* The destination may be a coastal land tile,
     * in which case the ferry should stop on an adjacent tile. */
    pft_fill_unit_overlap_param(parameter, punit);
  } else if (is_ai && !is_air && is_military_unit(punit)
	     && (punit->ai.ai_role == AIUNIT_DEFEND_HOME
		 || punit->ai.ai_role == AIUNIT_ATTACK
		 || punit->ai.ai_role ==  AIUNIT_ESCORT
		 || punit->ai.ai_role == AIUNIT_HUNTER)) {
    /* Use attack movement for defenders and escorts so they can
     * make defensive attacks */
    pft_fill_unit_attack_param(parameter, punit);
  } else {
    pft_fill_unit_parameter(parameter, punit);
  }

  /* Should we use the risk avoidance code?
   * The risk avoidance code uses omniscience, so do not use for
   * human-player units under temporary AI control.
   * Air units are immune to most risks, especially dangerous terrain.
   * Barbarians bravely/stupidly ignore risks
   */
  if (is_ai && !is_air && !barbarian) {
    ai_avoid_risks(parameter, risk_cost, punit, NORMAL_STACKING_FEARFULNESS);
  }

  /* Should we absolutely forbid ending a turn on a dangerous tile?
   * Do not annoy human players by killing their units for them.
   * For AI units be optimistic; allows attacks across dangerous terrain,
   * and polar settlements.
   * TODO: This is compatible with old code,
   * but probably ought to be more cautious for non military units
   */
  if (is_ai && !is_ferry && !is_air) {
    parameter->is_pos_dangerous = NULL;
  }

  if (is_ai && long_path) {
    /* Move as far along the path to the destination as we can;
     * that is, ignore the presence of enemy units when computing the
     * path.
     * Hopefully, ai_avoid_risks will have produced a path that avoids enemy
     * ZoCs. Ignoring ZoCs allows us to move closer to a destination
     * for which there is not yet a clear path.
     * That is good if the destination is several turns away,
     * so we can reasonably expect blocking enemy units to move or
     * be destroyed. But it can be bad if the destination is one turn away
     * or our destination is far but there are enemy units near us and on the
     * shortest path to the destination.
     */
    parameter->get_zoc = NULL;
  }

  if (!is_ai) {
    /* Do not annoy human players by killing their units for them.
     * Do not cheat by using information about tiles unknown to the player.
     */
    parameter->get_TB = no_fights_or_unknown;
  } else if ((unit_flag(punit, F_DIPLOMAT))
      || (unit_flag(punit, F_SPY))) {
    /* Default tile behaviour */
  } else if (unit_flag(punit, F_SETTLERS)) {
    parameter->get_TB = no_fights;
  } else if (long_path && unit_flag(punit, F_CITIES)) {
    /* Default tile behaviour;
     * move as far along the path to the destination as we can;
     * that is, ignore the presence of enemy units when computing the
     * path.
     */
  } else if (unit_flag(punit, F_CITIES)) {
    /* Short path */
    parameter->get_TB = no_fights;
  } else if (unit_flag(punit, F_TRADE_ROUTE)
             || unit_flag(punit, F_HELP_WONDER)) {
    parameter->get_TB = no_fights;
  } else if (unit_has_role(punit->type, L_BARBARIAN_LEADER)) {
    /* Avoid capture */
    parameter->get_TB = no_fights;
  } else if (is_ferry) {
    /* Ferries are not warships */
    parameter->get_TB = no_fights;
  } else if (is_air) {
    /* Default tile behaviour */
  } else if (is_heli_unit(punit)) {
    /* Default tile behaviour */
  } else if (is_military_unit(punit)) {
    switch (punit->ai.ai_role) {
    case AIUNIT_AUTO_SETTLER:
    case AIUNIT_BUILD_CITY:
      /* Strange, but not impossible */
      parameter->get_TB = no_fights;
      break;
    case AIUNIT_DEFEND_HOME:
    case AIUNIT_ATTACK:
    case AIUNIT_ESCORT:
    case AIUNIT_HUNTER:
      parameter->get_TB = no_intermediate_fights;
      break;
    case AIUNIT_EXPLORE:
    case AIUNIT_RECOVER:
      parameter->get_TB = no_fights;
      break;
    default:
      /* Default tile behaviour */
      break;
    }
  } else {
    /* Probably an explorer */
    parameter->get_TB = no_fights;
  }

  if (is_ferry) {
    /* Must use TM_WORST_TIME, so triremes move safely */
    parameter->turn_mode = TM_WORST_TIME;
    /* Show the destination in the client when watching an AI: */
    punit->goto_tile = ptile;
  }
}

/**************************************************************************
  Go to specified destination but do not disturb existing role or activity
  and do not clear the role's destination. Return FALSE iff we died.
**************************************************************************/
bool ai_unit_goto(struct unit *punit, struct tile *ptile)
{
  struct pf_parameter parameter;
  struct ai_risk_cost risk_cost;

  UNIT_LOG(LOG_DEBUG, punit, "ai_unit_goto to %d,%d", ptile->x, ptile->y);
  ai_fill_unit_param(&parameter, &risk_cost, punit, ptile);
  return ai_unit_goto_constrained(punit, ptile, &parameter);
}

/**************************************************************************
  Ensure unit sanity by telling charge that we won't bodyguard it anymore,
  tell bodyguard it can roam free if our job is done, add and remove city 
  spot reservation, and set destination. If we set a unit to hunter, also
  reserve its target, and try to load it with cruise missiles or nukes
  to bring along.
**************************************************************************/
void ai_unit_new_role(struct unit *punit, enum ai_unit_task task,
		      struct tile *ptile)
{
  struct unit *bodyguard = aiguard_guard_of(punit);

  /* If the unit is under (human) orders we shouldn't control it. */
  assert(!unit_has_orders(punit));

  UNIT_LOG(LOG_DEBUG, punit, "changing role from %s to %s",
           get_ai_role_str(punit->ai.ai_role), get_ai_role_str(task));

  /* Free our ferry.  Most likely it has been done already. */
  if (task == AIUNIT_NONE || task == AIUNIT_DEFEND_HOME) {
    aiferry_clear_boat(punit);
  }

  if (punit->activity == ACTIVITY_GOTO) {
    /* It would indicate we're going somewhere otherwise */
    handle_unit_activity_request(punit, ACTIVITY_IDLE);
  }

  if (punit->ai.ai_role == AIUNIT_BUILD_CITY) {
    citymap_free_city_spot(punit->goto_tile, punit->id);
  }

  if (punit->ai.ai_role == AIUNIT_HUNTER) {
    /* Clear victim's hunted bit - we're no longer chasing. */
    struct unit *target = find_unit_by_id(punit->ai.target);

    if (target) {
      target->ai.hunted &= ~(1 << unit_owner(punit)->player_no);
      UNIT_LOG(LOGLEVEL_HUNT, target, "no longer hunted (new role %d, old %d)",
               task, punit->ai.ai_role);
    }
  }

  aiguard_clear_charge(punit);
  /* Record the city to defend; our goto may be to transport. */
  if (task == AIUNIT_DEFEND_HOME && ptile && ptile->city) {
    aiguard_assign_guard_city(ptile->city, punit);
  }

  punit->ai.ai_role = task;

  /* Verify and set the goto destination.  Eventually this can be a lot more
   * stringent, but for now we don't want to break things too badly. */
  punit->goto_tile = ptile; /* May be NULL. */

  if (punit->ai.ai_role == AIUNIT_NONE && bodyguard) {
    ai_unit_new_role(bodyguard, AIUNIT_NONE, NULL);
  }

  /* Reserve city spot, _unless_ we want to add ourselves to a city. */
  if (punit->ai.ai_role == AIUNIT_BUILD_CITY && !tile_get_city(ptile)) {
    citymap_reserve_city_spot(ptile, punit->id);
  }
  if (punit->ai.ai_role == AIUNIT_HUNTER) {
    /* Set victim's hunted bit - the hunt is on! */
    struct unit *target = find_unit_by_id(punit->ai.target);

    assert(target != NULL);
    target->ai.hunted |= (1 << unit_owner(punit)->player_no);
    UNIT_LOG(LOGLEVEL_HUNT, target, "is being hunted");

    /* Grab missiles lying around and bring them along */
    if (unit_flag(punit, F_MISSILE_CARRIER)
        || unit_flag(punit, F_CARRIER)) {
      unit_list_iterate(punit->tile->units, missile) {
        if (missile->ai.ai_role != AIUNIT_ESCORT
            && missile->transported_by == -1
            && missile->owner == punit->owner
            && unit_flag(missile, F_MISSILE)
            && can_unit_load(missile, punit)) {
          UNIT_LOG(LOGLEVEL_HUNT, missile, "loaded on hunter");
          ai_unit_new_role(missile, AIUNIT_ESCORT, target->tile);
          load_unit_onto_transporter(missile, punit);
        }
      } unit_list_iterate_end;
    }
  }
}

/**************************************************************************
  Try to make pcity our new homecity. Fails if we can't upkeep it. Assumes
  success from server.
**************************************************************************/
bool ai_unit_make_homecity(struct unit *punit, struct city *pcity)
{
  CHECK_UNIT(punit);
  assert(punit->owner == pcity->owner);

  if (punit->homecity == 0 && !unit_has_role(punit->type, L_EXPLORER)) {
    /* This unit doesn't pay any upkeep while it doesn't have a homecity,
     * so it would be stupid to give it one. There can also be good reasons
     * why it doesn't have a homecity. */
    /* However, until we can do something more useful with them, we
       will assign explorers to a city so that they can be disbanded for 
       the greater good -- Per */
    return FALSE;
  }
  if (pcity->surplus[O_SHIELD] >= unit_type(punit)->upkeep[O_SHIELD]
      && pcity->surplus[O_FOOD] >= unit_type(punit)->upkeep[O_FOOD]) {
    handle_unit_change_homecity(unit_owner(punit), punit->id, pcity->id);
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
  Move a bodyguard along with another unit. We assume that unit has already
  been moved to ptile which is a valid, safe tile, and that our
  bodyguard has not. This is an ai_unit_* auxiliary function, do not use 
  elsewhere.
**************************************************************************/
static void ai_unit_bodyguard_move(struct unit *bodyguard, struct tile *ptile)
{
  struct unit *punit;
  struct player *pplayer;

  assert(bodyguard != NULL);
  pplayer = unit_owner(bodyguard);
  assert(pplayer != NULL);
  punit = aiguard_charge_unit(bodyguard);
  assert(punit != NULL);

  CHECK_GUARD(bodyguard);
  CHECK_CHARGE_UNIT(punit);

  if (!is_tiles_adjacent(ptile, bodyguard->tile)) {
    return;
  }

  if (bodyguard->moves_left <= 0) {
    /* should generally should not happen */
    BODYGUARD_LOG(LOG_DEBUG, bodyguard, "was left behind by charge");
    return;
  }

  handle_unit_activity_request(bodyguard, ACTIVITY_IDLE);
  (void) ai_unit_move(bodyguard, ptile);
}

/**************************************************************************
  Move and attack with an ai unit. We do not wait for server reply.
**************************************************************************/
bool ai_unit_attack(struct unit *punit, struct tile *ptile)
{
  struct unit *bodyguard = aiguard_guard_of(punit);
  int sanity = punit->id;
  bool alive;

  CHECK_UNIT(punit);
  assert(unit_owner(punit)->ai.control);
  assert(is_tiles_adjacent(punit->tile, ptile));

  handle_unit_activity_request(punit, ACTIVITY_IDLE);
  (void) handle_unit_move_request(punit, ptile, FALSE, FALSE);
  alive = (find_unit_by_id(sanity) != NULL);

  if (alive && same_pos(ptile, punit->tile)
      && bodyguard != NULL  && bodyguard->ai.charge == punit->id) {
    ai_unit_bodyguard_move(bodyguard, ptile);
    /* Clumsy bodyguard might trigger an auto-attack */
    alive = (find_unit_by_id(sanity) != NULL);
  }

  return alive;
}

/**************************************************************************
  Move a unit. Do not attack. Do not leave bodyguard.
  For AI units and units (such as auto explorers) temporarily controlled
  by the AI.

  This function returns only when we have a reply from the server and
  we can tell the calling function what happened to the move request.
  (Right now it is not a big problem, since we call the server directly.)
**************************************************************************/
bool ai_unit_move(struct unit *punit, struct tile *ptile)
{
  struct unit *bodyguard;
  int sanity = punit->id;
  struct player *pplayer = unit_owner(punit);
  const bool is_ai = pplayer->ai.control;

  CHECK_UNIT(punit);
  assert(is_tiles_adjacent(punit->tile, ptile));

  /* if enemy, stop and give a chance for the ai attack function
   * or the human player to handle this case */
  if (is_enemy_unit_tile(ptile, pplayer)
      || is_enemy_city_tile(ptile, pplayer)) {
    UNIT_LOG(LOG_DEBUG, punit, "movement halted due to enemy presence");
    return FALSE;
  }

  /* barbarians shouldn't enter huts */
  if (is_barbarian(pplayer) && tile_has_special(ptile, S_HUT)) {
    return FALSE;
  }

  /* don't leave bodyguard behind */
  if (is_ai
      && (bodyguard = aiguard_guard_of(punit))
      && same_pos(punit->tile, bodyguard->tile)
      && bodyguard->moves_left == 0) {
    UNIT_LOG(LOGLEVEL_BODYGUARD, punit, "does not want to leave "
             "its bodyguard");
    return FALSE;
  }

  /* Try not to end move next to an enemy if we can avoid it by waiting */
  if (punit->moves_left <= map_move_cost(punit, ptile)
      && unit_move_rate(punit) > map_move_cost(punit, ptile)
      && enemies_at(punit, ptile)
      && !enemies_at(punit, punit->tile)) {
    UNIT_LOG(LOG_DEBUG, punit, "ending move early to stay out of trouble");
    return FALSE;
  }

  /* go */
  handle_unit_activity_request(punit, ACTIVITY_IDLE);
  (void) handle_unit_move_request(punit, ptile, FALSE, TRUE);

  /* handle the results */
  if (find_unit_by_id(sanity) && same_pos(ptile, punit->tile)) {
    struct unit *bodyguard = aiguard_guard_of(punit);
    if (is_ai && bodyguard != NULL && bodyguard->ai.charge == punit->id) {
      ai_unit_bodyguard_move(bodyguard, ptile);
    }
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
This looks for the nearest city:
If (x,y) is the land, it looks for cities only on the same continent
unless (everywhere != 0)
If (enemy != 0) it looks only for enemy cities
If (pplayer != NULL) it looks for cities known to pplayer
**************************************************************************/
struct city *dist_nearest_city(struct player *pplayer, struct tile *ptile,
                               bool everywhere, bool enemy)
{ 
  struct city *pc=NULL;
  int best_dist = -1;
  Continent_id con = tile_get_continent(ptile);

  players_iterate(pplay) {
    /* If "enemy" is set, only consider cities whose owner we're at
     * war with. */
    if (enemy && pplayer && !pplayers_at_war(pplayer, pplay)) {
      continue;
    }

    city_list_iterate(pplay->cities, pcity) {
      int city_dist = real_map_distance(ptile, pcity->tile);

      /* Find the closest city known to the player with a matching
       * continent. */
      if ((best_dist == -1 || city_dist < best_dist)
	  && (everywhere || con == 0
	      || con == tile_get_continent(pcity->tile))
	  && (!pplayer || map_is_known(pcity->tile, pplayer))) {
	best_dist = city_dist;
        pc = pcity;
      }
    } city_list_iterate_end;
  } players_iterate_end;

  return(pc);
}


/**************************************************************************
  Calculate the value of the target unit including the other units which
  will die in a successful attack
**************************************************************************/
int stack_cost(struct unit *pdef)
{
  int victim_cost = 0;

  if (is_stack_vulnerable(pdef->tile)) {
    /* lotsa people die */
    unit_list_iterate(pdef->tile->units, aunit) {
      victim_cost += unit_build_shield_cost(aunit->type);
    } unit_list_iterate_end;
  } else {
    /* Only one unit dies if attack is successful */
    victim_cost = unit_build_shield_cost(pdef->type);
  }
  
  return victim_cost;
}

/**************************************************************************
  Change government, pretty fast...
**************************************************************************/
void ai_government_change(struct player *pplayer, struct government *gov)
{
  if (gov == pplayer->government) {
    return;
  }

  handle_player_change_government(pplayer, gov->index);

  city_list_iterate(pplayer->cities, pcity) {
    auto_arrange_workers(pcity); /* update cities */
  } city_list_iterate_end;
}

/**************************************************************************
  Credits the AI wants to have in reserves. We need some gold to bribe
  and incite cities.

  "I still don't trust this function" -- Syela
**************************************************************************/
int ai_gold_reserve(struct player *pplayer)
{
  int i = total_player_citizens(pplayer)*2;
  return MAX(pplayer->ai.maxbuycost, i);
}

/**************************************************************************
  Sets the values of the choice to initial values.
**************************************************************************/
void init_choice(struct ai_choice *choice)
{
  choice->choice = A_UNSET;
  choice->want = 0;
  choice->type = CT_NONE;
  choice->need_boat = FALSE;
}

/**************************************************************************
...
**************************************************************************/
void adjust_choice(int value, struct ai_choice *choice)
{
  choice->want = (choice->want *value)/100;
}

/**************************************************************************
...
**************************************************************************/
void copy_if_better_choice(struct ai_choice *cur, struct ai_choice *best)
{
  if (cur->want > best->want) {
    best->choice =cur->choice;
    best->want = cur->want;
    best->type = cur->type;
    best->need_boat = cur->need_boat;
  }
}

/**************************************************************************
  Calls ai_wants_role_unit to choose the best unit with the given role and 
  set tech wants.  Sets choice->choice if we can build something.
**************************************************************************/
void ai_choose_role_unit(struct player *pplayer, struct city *pcity,
			 struct ai_choice *choice, int role, int want)
{
  struct unit_type *iunit = ai_wants_role_unit(pplayer, pcity, role, want);

  if (iunit != NULL) {
    choice->choice = iunit->index;
  }
}

/**************************************************************************
  Choose improvement we like most and put it into ai_choice.

 "I prefer the ai_choice as a return value; gcc prefers it as an arg" 
  -- Syela 
**************************************************************************/
void ai_advisor_choose_building(struct city *pcity, struct ai_choice *choice)
{
  Impr_type_id id = B_LAST;
  int want = 0;
  struct player *plr = city_owner(pcity);

  impr_type_iterate(i) {
    if (!plr->ai.control && is_wonder(i)) {
      continue; /* Humans should not be advised to build wonders or palace */
    }
    if (pcity->ai.building_want[i] > want
        && can_build_improvement(pcity, i)) {
      want = pcity->ai.building_want[i];
      id = i;
    }
  } impr_type_iterate_end;

  choice->want = want;
  choice->choice = id;
  choice->type = CT_BUILDING;
  CITY_LOG(LOG_DEBUG, pcity, "wants most to build %s at %d",
           get_improvement_name(id), want);
}

/**********************************************************************
  "The following evaluates the unhappiness caused by military units
  in the field (or aggressive) at a city when at Republic or 
  Democracy.

  Now generalised somewhat for government rulesets, though I'm not 
  sure whether it is fully general for all possible parameters/
  combinations." --dwp
**********************************************************************/
bool ai_assess_military_unhappiness(struct city *pcity,
                                    struct government *g)
{
  int free_happy;
  int unhap = 0;

  /* bail out now if happy_cost is 0 */
  if (g->unit_happy_cost_factor == 0) {
    return FALSE;
  }
  
  free_happy  = citygov_free_happy(pcity, g);

  /* ??  This does the right thing for normal Republic and Democ -- dwp */
  free_happy += get_city_bonus(pcity, EFT_MAKE_CONTENT_MIL);

  unit_list_iterate(pcity->units_supported, punit) {
    int happy_cost = utype_happy_cost(unit_type(punit), g);

    if (happy_cost <= 0) {
      continue;
    }

    /* See discussion/rules in common/city.c:city_support() */
    if (!unit_being_aggressive(punit)) {
      if (is_field_unit(punit)) {
	happy_cost = 1;
      } else {
	happy_cost = 0;
      }
    }
    if (happy_cost <= 0) {
      continue;
    }

    if (get_city_bonus(pcity, EFT_MAKE_CONTENT_MIL_PER) > 0) {
      happy_cost--;
    }
    adjust_city_free_cost(&free_happy, &happy_cost);
    
    if (happy_cost > 0) {
      unhap += happy_cost;
    }
  } unit_list_iterate_end;
 
  if (unhap < 0) {
    unhap = 0;
  }
  return (unhap > 0);
}

/**************************************************************************
  AI doesn't want the score for future techs.
**************************************************************************/
bool ai_wants_no_science(struct player *pplayer)
{
  return ai_data_get(pplayer)->wants_no_science;
}

/**************************************************************************
  Clear all the AI information for a unit, placing the unit in a blank
  state.

  This is a suitable action for when a unit is about to change owners;
  the new owner can not use (and should not know) what the previous owner was
  using the unit for.
**************************************************************************/
void ai_reinit(struct unit *punit)
{
  punit->ai.control = false;
  punit->ai.ai_role = AIUNIT_NONE;
  aiguard_clear_charge(punit);
  aiguard_clear_guard(punit);
  aiferry_clear_boat(punit);
  punit->ai.target = 0;
  punit->ai.hunted = 0;
  punit->ai.done = FALSE;
}

