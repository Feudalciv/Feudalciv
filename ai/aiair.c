/********************************************************************** 
 Freeciv - Copyright (C) 2002 - The Freeciv Team
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

#include "combat.h"
#include "log.h"
#include "map.h"
#include "player.h"
#include "unit.h"

#include "airgoto.h"
#include "citytools.h"
#include "gotohand.h"
#include "maphand.h"
#include "unithand.h"
#include "unittools.h"

#include "aitools.h"
#include "aiunit.h"

#include "aiair.h"

/**************************************************************************
 * Looks for nearest airbase for punit.
 * Returns 0 if not found.
 * TODO: 1. Use proper refuel_iterate, like in ai_find_strategic_airbase
 *       2. Special handicaps for planes running out of fuel
 *       IMO should be less restrictive than general H_MAP, H_FOG
 *************************************************************************/
static bool find_nearest_airbase(int x, int y, struct unit *punit, 
				 int *xref, int *yref)
{
  struct player *pplayer = unit_owner(punit);
  int moves_left = punit->moves_left / SINGLE_MOVE;

  iterate_outward(x, y, moves_left, x1, y1) {
    if (is_airunit_refuel_point(x1, y1, pplayer, punit->type, FALSE)
	&& (air_can_move_between (moves_left, x, y, x1, y1, pplayer) >= 0)) {
      *xref = x1;
      *yref = y1;
      return TRUE;
    }
  } iterate_outward_end;

  return FALSE;
}

/**********************************************************************
 * Very preliminary estimate for our intent to attack the tile (x, y).
 * Used by bombers only.
 **********************************************************************/
static bool ai_should_we_air_attack_tile(struct unit *punit, int x, int y)
{
  struct city *acity = map_get_city(x, y);

  /* For a virtual unit (punit->id == 0), all targets are good */
  /* TODO: There is a danger of producing too many units that will not 
   * attack anything.  Production should not happen if there is an idle 
   * unit of the same type nearby */
  if (acity && !TEST_BIT(acity->ai.invasion, 0) && punit->id != 0) {
    /* No ground troups are invading */
    freelog(LOG_DEBUG, "Don't want to attack %s, although we could", 
            acity->name);
    return FALSE;
  }

  return TRUE;
}

/**********************************************************************
 * Returns an estimate for the profit gained through attack.
 * Assumes that the victim is within one day's flight
 **********************************************************************/
static int ai_evaluate_tile_for_air_attack(struct unit *punit, 
                                    int dest_x, int dest_y)
{
  struct unit *pdefender = get_defender(punit, dest_x, dest_y);
  /* unit costs in shields */
  int balanced_cost, unit_cost, victim_cost = 0;
  /* unit stats */
  int unit_attack, victim_defence;
  /* final answer */
  int profit;
  /* time spent in the air */
  int sortie_time;
#define PROB_MULTIPLIER 100 /* should unify with those in combat.c */

  if ((pdefender == NULL) 
      || !can_unit_attack_all_at_tile(punit, dest_x, dest_y)) {
    return 0;
  }

  /* Ok, we can attack, but is it worth it? */

  /* Cost of our unit */
  unit_cost = unit_type(punit)->build_cost;
  /* This is to say "wait, ill unit will get better!" */
  unit_cost = unit_cost * unit_type(punit)->hp / punit->hp; 

  /* Determine cost of enemy units */
  victim_cost = stack_cost(pdefender);

  /* Missile would die 100% so we adjust the victim_cost -- GB */
  if (unit_flag(punit, F_MISSILE)) {
    victim_cost -= unit_type(punit)->build_cost;
  }

  unit_attack = (int) (PROB_MULTIPLIER 
                       * unit_win_chance(punit, pdefender));

  victim_defence = PROB_MULTIPLIER - unit_attack;

  balanced_cost = build_cost_balanced(punit->type);

  sortie_time = (unit_flag(punit, F_ONEATTACK) ? 1 : 0);

  profit = kill_desire(victim_cost, unit_attack, unit_cost, victim_defence, 1) 
    - SHIELD_WEIGHTING + 2 * TRADE_WEIGHTING;
  if (profit > 0) {
    profit = military_amortize(unit_owner(punit), 
                               find_city_by_id(punit->homecity),
                               profit, sortie_time, balanced_cost);
    freelog(LOG_DEBUG, 
	    "%s at (%d, %d) is a worthy target with profit %d", 
	    unit_type(pdefender)->name, dest_x, dest_y, profit);
  } else {
    freelog(LOG_DEBUG, 
	    "%s(%d, %d): %s at (%d, %d) is unworthy with profit %d",
	    unit_type(punit)->name, punit->x, punit->y,
	    unit_type(pdefender)->name, dest_x, dest_y, profit);
    profit = 0;
  }

  return profit;
}
  

/**********************************************************************
 * Find something to bomb
 * Air-units specific victim search
 * Returns the want for the best target, records target in punit's goto_dest
 * TODO: take counterattack dangers into account
 * TODO: make separate handicaps for air units seeing targets
 *       IMO should be more restrictive than general H_MAP, H_FOG
 *********************************************************************/
static int find_something_to_bomb(struct unit *punit, int x, int y)
{
  struct player *pplayer = unit_owner(punit);
  int max_dist = punit->moves_left / SINGLE_MOVE;
  int best = 0;

  if ((punit->x == x) && (punit->y == y)) {
    /* Unit is attacking from here */
    max_dist = punit->moves_left / SINGLE_MOVE;
  } else {
    /* Unit will be attacking from another airbase */
    max_dist = unit_type(punit)-> move_rate / SINGLE_MOVE;
  }

  /* Adjust the max distance so Fighters can attack safely */
  if (punit->fuel < 2) {
    /* -1 is to take into account the attack itself */
    max_dist = (max_dist - 1) / 2;
  }
  
  /* Let's find something to bomb */
  iterate_outward(x, y, max_dist, x1, y1) {

    if (ai_handicap(pplayer, H_MAP) && !map_is_known(x1, y1, pplayer)) {
      /* The target tile is unknown */
      continue;
    }
    if (ai_handicap(pplayer, H_FOG) 
        && !map_is_known_and_seen(x1, y1, pplayer)) {
      /* The tile is fogged */
      continue;
    }

    if (is_enemy_unit_tile(map_get_tile(x1, y1), pplayer)
        && ai_should_we_air_attack_tile(punit, x1, y1)
	&& (air_can_move_between (max_dist, x, y, x1, y1, pplayer) >= 0)){
      int new_best = ai_evaluate_tile_for_air_attack(punit, x1, y1);
      if (new_best > best) {
	set_goto_dest(punit, x1, y1);
	best = new_best;
	freelog(LOG_DEBUG, "%s wants to attack tile (%d, %d)", 
		unit_type(punit)->name, x1, y1);
      }
    }

  } iterate_outward_end;

  return best;
} 

/***********************************************************************
 * Iterates through reachable cities and appraises them as a possible 
 * base for air operations by (air)unit punit.
 **********************************************************************/
static bool ai_find_strategic_airbase(struct unit *punit, 
                                      int *dest_x, int *dest_y)
{
  struct refuel *airbase;
  struct pqueue *airbase_iterator;
  int turns_to_dest = 0;
  int best_worth = 0;
  bool found = FALSE;

  airbase_iterator 
    = refuel_iterate_init(unit_owner(punit), punit->x, punit->y,
                          punit->x, punit->y, TRUE, 
                          punit->moves_left / SINGLE_MOVE, 
                          unit_move_rate(punit) / SINGLE_MOVE, 
                          unit_type(punit)->fuel);

  while( (airbase = refuel_iterate_next(airbase_iterator)) != NULL) {
    int target_worth;

    if (turns_to_dest > 0 
        && turns_to_dest < get_turns_to_refuel(airbase)){
      /* We had found something already and it's closer than 
       * anything else -- so take it */
      break;
    }

    if ((target_worth = find_something_to_bomb(punit, get_refuel_x(airbase), 
                                               get_refuel_y(airbase))) > 0) {
      struct city *base_city 
        = map_get_city(get_refuel_x(airbase), get_refuel_y(airbase));
     
      if (base_city && base_city->ai.grave_danger != 0) {
        /* Fly there immediately!! */
        *dest_x = get_refuel_x(airbase);
        *dest_y = get_refuel_y(airbase);
        found = TRUE;
        break;
      } else if (target_worth > best_worth) {
        /* It's either a first find or it's better than the previous */
        *dest_x = get_refuel_x(airbase);
        *dest_y = get_refuel_y(airbase);
        turns_to_dest = get_turns_to_refuel(airbase);
        best_worth = target_worth;
        found = TRUE;
        /* We can still look for something better */
      }
    }

    refuel_iterate_process(airbase_iterator, airbase);
  }

  return found;
}

/************************************************************************
 * Trying to manage bombers and stuff.
 * If we are in the open {
 *   if moving intelligently on a valid GOTO, {
 *     carry on doing it.
 *   } else {
 *     go refuel
 *   }
 * } else {
 *   try to attack something
 * } 
 * TODO: distant target selection, support for fuel > 2
 ***********************************************************************/
void ai_manage_airunit(struct player *pplayer, struct unit *punit)
{
  int dest_x = punit->x, dest_y = punit->y;
  /* Loop prevention */
  int moves = punit->moves_left;
  int id = punit->id;


  CHECK_UNIT(punit);

  if (!is_airunit_refuel_point(punit->x, punit->y, 
			       pplayer, punit->type, FALSE)) {
    /* We are out in the open, what shall we do? */
    int refuel_x, refuel_y;

    if (punit->activity == ACTIVITY_GOTO
      /* We are on a GOTO.  Check if it will get us anywhere */
	&& is_airunit_refuel_point(goto_dest_x(punit), goto_dest_y(punit), 
				   pplayer, punit->type, FALSE)
	&& air_can_move_between (punit->moves_left/SINGLE_MOVE, 
                                 punit->x, punit->y, 
				 goto_dest_x(punit), goto_dest_y(punit),
				 pplayer) >= 0) {
      /* It's an ok GOTO, just go there */
      ai_unit_gothere(punit);
    } else if (find_nearest_airbase(punit->x, punit->y, punit, 
			     &refuel_x, &refuel_y)) {
      /* Go refuelling */
      set_goto_dest(punit, refuel_x, refuel_y);
      freelog(LOG_DEBUG, "Sent %s to refuel", unit_type(punit)->name);
      ai_unit_gothere(punit);
    } else {
      if (punit->fuel == 1) {
	freelog(LOG_DEBUG, "Oops, %s is fallin outta sky", 
		unit_type(punit)->name);
      }
      return;
    }

  } else if (punit->fuel == unit_type(punit)->fuel) {
    /* We only leave a refuel point when we are on full fuel */

    if (find_something_to_bomb(punit, punit->x, punit->y) > 0) {
      /* Found target, coordinates are in punit's goto_dest.
       * TODO: separate attacking into a function, check for the best 
       * tile to attack from */
      assert(is_goto_dest_set(punit));
      if (!ai_unit_gothere(punit)) {
        return; /* died */
      }

      /* goto would be aborted: "Aborting GOTO for AI attack procedures"
       * now actually need to attack */
      /* We could use ai_military_findvictim here, but I don't trust it... */
      set_unit_activity(punit, ACTIVITY_IDLE);
      if (is_tiles_adjacent(punit->x, punit->y,
                            goto_dest_x(punit), goto_dest_y(punit))) {
        (void) handle_unit_move_request(punit, goto_dest_x(punit),
                                        goto_dest_y(punit), TRUE, FALSE);
      }
    } else if (ai_find_strategic_airbase(punit, &dest_x, &dest_y)) {
      freelog(LOG_DEBUG, "%s will fly to (%i, %i) (%s) to fight there",
              unit_type(punit)->name, dest_x, dest_y, 
              (map_get_city(dest_x, dest_y) ? 
               map_get_city(dest_x, dest_y)->name : ""));
      set_goto_dest(punit, dest_x, dest_y);
      ai_unit_gothere(punit);
    } else {
      freelog(LOG_DEBUG, "%s cannot find anything to kill and is staying put", 
              unit_type(punit)->name);
      set_unit_activity(punit, ACTIVITY_IDLE);
    }
  }

  if ((punit = find_unit_by_id(id)) != NULL && punit->moves_left > 0
      && punit->moves_left != moves) {
    /* We have moved this turn, might have ended up stuck out in the fields
     * so, as a safety measure, let's manage again */
    ai_manage_airunit(pplayer, punit);
  }

}

/*******************************************************************
 * Chooses the best available and usable air unit and records it in 
 * choice, if it's better than previous choice
 * The interface is somewhat different from other ai_choose, but
 * that's what it should be like, I believe -- GB
 ******************************************************************/
bool ai_choose_attacker_air(struct player *pplayer, struct city *pcity, 
			    struct ai_choice *choice)
{
  bool want_something = FALSE;

  /* This AI doesn't know to build planes */
  if (ai_handicap(pplayer, H_NOPLANES)) {
    return FALSE;
  }

  /* military_advisor_choose_build does something idiotic, 
   * this function should not be called if there is danger... */
  if (choice->want >= 100 && choice->type != CT_ATTACKER) {
    return FALSE;
  }

  if (!player_knows_techs_with_flag(pplayer, TF_BUILD_AIRBORNE)) {
    return FALSE;
  }

  unit_type_iterate(u_type) {
    if (get_unit_type(u_type)->move_type != AIR_MOVING) continue;
    if (can_build_unit(pcity, u_type)) {
      struct unit *virtual_unit = 
	create_unit_virtual(pplayer, pcity, u_type, TRUE);
      int profit = find_something_to_bomb(virtual_unit, pcity->x, pcity->y);
      if (profit > choice->want){
	/* Update choice */
	choice->want = profit;
	choice->choice = u_type;
	choice->type = CT_ATTACKER;
	want_something = TRUE;
	freelog(LOG_DEBUG, "%s wants to build %s (want=%d)",
		pcity->name, get_unit_type(u_type)->name, profit);
      } else {
      freelog(LOG_DEBUG, "%s doesn't want to build %s (want=%d)",
		pcity->name, get_unit_type(u_type)->name, profit);
      }
      destroy_unit_virtual(virtual_unit);
    }
  } unit_type_iterate_end;

  return want_something;
}
