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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "city.h"
#include "combat.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "player.h"
#include "rand.h"
#include "shared.h"
#include "timing.h"
#include "unit.h"

#include "barbarian.h"
#include "citytools.h"
#include "cityturn.h"
#include "diplomats.h"
#include "gotohand.h"
#include "maphand.h"
#include "settlers.h"
#include "unithand.h"
#include "unittools.h"

#include "advmilitary.h"
#include "aiair.h"
#include "aicity.h"
#include "aidiplomat.h"
#include "aihand.h"
#include "aitools.h"
#include "aidata.h"
#include "ailog.h"

#include "aiunit.h"

#define LOGLEVEL_RECOVERY LOG_DEBUG

static void ai_manage_military(struct player *pplayer,struct unit *punit);
static void ai_manage_caravan(struct player *pplayer, struct unit *punit);
static void ai_manage_barbarian_leader(struct player *pplayer,
				       struct unit *leader);

static void ai_military_findjob(struct player *pplayer,struct unit *punit);
static void ai_military_gohome(struct player *pplayer,struct unit *punit);
static void ai_military_attack(struct player *pplayer,struct unit *punit);
static bool ai_military_rampage(struct unit *punit, int threshold);

static int unit_move_turns(struct unit *punit, int x, int y);
static bool unit_role_defender(Unit_Type_id type);
static int unit_vulnerability(struct unit *punit, struct unit *pdef);

/*
 * Cached values. Updated by update_simple_ai_types.
 *
 * This a hack to enable emulation of old loops previously hardwired
 * as 
 *    for (i = U_WARRIORS; i <= U_BATTLESHIP; i++)
 *
 * (Could probably just adjust the loops themselves fairly simply,
 * but this is safer for regression testing.)
 *
 * Not dealing with planes yet.
 *
 * Terminated by U_LAST.
 */
Unit_Type_id simple_ai_types[U_LAST];

/**************************************************************************
  Similar to is_my_zoc(), but with some changes:
  - destination (x0,y0) need not be adjacent?
  - don't care about some directions?
  
  Note this function only makes sense for ground units.

  Fix to bizarre did-not-find bug.  Thanks, Katvrr -- Syela
**************************************************************************/
static bool could_be_my_zoc(struct unit *myunit, int x0, int y0)
{
  assert(is_ground_unit(myunit));
  
  if (same_pos(x0, y0, myunit->x, myunit->y))
    return FALSE; /* can't be my zoc */
  if (is_tiles_adjacent(x0, y0, myunit->x, myunit->y)
      && !is_non_allied_unit_tile(map_get_tile(x0, y0),
				  unit_owner(myunit)))
    return FALSE;

  adjc_iterate(x0, y0, ax, ay) {
    if (!is_ocean(map_get_terrain(ax, ay))
	&& is_non_allied_unit_tile(map_get_tile(ax, ay),
				   unit_owner(myunit))) return FALSE;
  } adjc_iterate_end;
  
  return TRUE;
}

/**************************************************************************
  returns:
    0 if can't move
    1 if zoc_ok
   -1 if zoc could be ok?
**************************************************************************/
int could_unit_move_to_tile(struct unit *punit, int dest_x, int dest_y)
{
  enum unit_move_result result;

  result =
      test_unit_move_to_tile(punit->type, unit_owner(punit),
                             ACTIVITY_IDLE, FALSE, punit->x, punit->y, 
                             dest_x, dest_y, unit_flag(punit, F_IGZOC));
  if (result == MR_OK) {
    return 1;
  }

  if (result == MR_ZOC) {
    if (could_be_my_zoc(punit, punit->x, punit->y)) {
      return -1;
    }
  }
  return 0;
}

/********************************************************************** 
  This is a much simplified form of assess_defense (see advmilitary.c),
  but which doesn't use pcity->ai.wallvalue and just returns a boolean
  value.  This is for use with "foreign" cities, especially non-ai
  cities, where ai.wallvalue may be out of date or uninitialized --dwp
***********************************************************************/
static bool has_defense(struct city *pcity)
{
  unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, punit) {
    if (is_military_unit(punit) && get_defense_power(punit) != 0 && punit->hp != 0) {
      return TRUE;
    }
  }
  unit_list_iterate_end;
  return FALSE;
}

/**********************************************************************
 Precondition: A warmap must already be generated for the punit.

 Returns the minimal amount of turns required to reach the given
 destination position. The actual turn at which the unit will
 reach the given point depends on the movement points it has remaining.

 For example: Unit has a move rate of 3, path cost is 5, unit has 2
 move points left.

 path1 costs: first tile = 3, second tile = 2

 turn 0: points = 2, unit has to wait
 turn 1: points = 3, unit can move, points = 0, has to wait
 turn 2: points = 3, unit can move, points = 1

 path2 costs: first tile = 2, second tile = 3

 turn 0: points = 2, unit can move, points = 0, has to wait
 turn 1: points = 3, unit can move, points = 0

 In spite of the path costs being the same, units that take a path of the
 same length will arrive at different times. This function also does not 
 take into account ZOC or lack of hp affecting movement.
 
 Note: even if a unit has only fractional move points left, there is
 still a possibility it could cross the tile.
**************************************************************************/
static int unit_move_turns(struct unit *punit, int x, int y)
{
  int move_time;
  int move_rate = unit_type(punit)->move_rate;

  switch (unit_type(punit)->move_type) {
  case LAND_MOVING:
  
   /* FIXME: IGTER units should have their move rates multiplied by 
    * igter_speedup. Note: actually, igter units should never have their 
    * move rates multiplied. The correct behaviour is to have every tile 
    * they cross cost 1/3 of a movement point. ---RK */
 
    if (unit_flag(punit, F_IGTER)) {
      move_rate *= 3;
    }
    move_time = warmap.cost[x][y] / move_rate;
    break;
 
  case SEA_MOVING:
   
    if (player_owns_active_wonder(unit_owner(punit), B_LIGHTHOUSE)) {
        move_rate += SINGLE_MOVE;
    }
    
    if (player_owns_active_wonder(unit_owner(punit), B_MAGELLAN)) {
        move_rate += (improvement_variant(B_MAGELLAN) == 1) 
                       ?  SINGLE_MOVE : 2 * SINGLE_MOVE;
    }
      
    if (player_knows_techs_with_flag(unit_owner(punit), TF_BOAT_FAST)) {
        move_rate += SINGLE_MOVE;
    }
      
    move_time = warmap.seacost[x][y] / move_rate;
    break;
 
  case HELI_MOVING:
  case AIR_MOVING:
     move_time = real_map_distance(punit->x, punit->y, x, y) 
                   * SINGLE_MOVE / move_rate;
     break;
 
  default:
    die("ai/aiunit.c:unit_move_turns: illegal move type %d",
	unit_type(punit)->move_type);
  }
  return move_time;
}

/**************************************************************************
  is there any hope of reaching this tile without violating ZOC? 
**************************************************************************/
static bool tile_is_accessible(struct unit *punit, int x, int y)
{
  CHECK_UNIT(punit);

  if (unit_type_really_ignores_zoc(punit->type))
    return TRUE;
  if (is_my_zoc(unit_owner(punit), x, y))
    return TRUE;

  adjc_iterate(x, y, x1, y1) {
    if (!is_ocean(map_get_terrain(x1, y1))
	&& is_my_zoc(unit_owner(punit), x1, y1))
      return TRUE;
  } adjc_iterate_end;

  return FALSE;
}
 
/**************************************************************************
Handle eXplore mode of a unit (explorers are always in eXplore mode for AI) -
explores unknown territory, finds huts.

Returns whether there is any more territory to be explored.
**************************************************************************/
bool ai_manage_explorer(struct unit *punit)
{
  struct player *pplayer = unit_owner(punit);
  /* The position of the unit; updated inside the function */
  int x = punit->x, y = punit->y;
  /* Continent the unit is on */
  int continent;
  /* Unit's speed */
  int move_rate = unit_move_rate(punit);
  /* Range of unit's vision */
  int range;

  CHECK_UNIT(punit);

  /* Get the range */
  /* FIXME: The vision range should NOT take into account watchtower benefit.
   * Now it is done in a wrong way: the watchtower bonus is computed locally,
   * at the point where the unit is, and then it is applied at a faraway
   * location, as if there is a watchtower there too. --gb */

  if (unit_profits_of_watchtower(punit)
      && map_has_special(punit->x, punit->y, S_FORTRESS)) {
    range = get_watchtower_vision(punit);
  } else {
    range = unit_type(punit)->vision_range;
  }

  /* Localize the unit */
  
  if (is_ground_unit(punit)) {
    continent = map_get_continent(x, y, NULL);
  } else {
    continent = 0;
  }

  /*
   * PART 1: Look for huts
   * Non-Barbarian Ground units ONLY.
   */

  if (!is_barbarian(pplayer)
      && is_ground_unit(punit)) {
    /* Maximal acceptable _number_ of moves to the target */
    int maxmoves = pplayer->ai.control ? 2 * THRESHOLD : 3;
    /* Move _cost_ to the best target (=> lower is better) */
    int bestcost = maxmoves * SINGLE_MOVE + 1;
    /* Desired destination */
    int best_x = -1, best_y = -1;

    /* CPU-expensive but worth it -- Syela */
    generate_warmap(map_get_city(x, y), punit);
  
    /* We're iterating outward so that with two tiles with the same movecost
     * the nearest is used. */
    iterate_outward(x, y, maxmoves, x1, y1) {
      if (map_has_special(x1, y1, S_HUT)
          && warmap.cost[x1][y1] < bestcost
          && (!ai_handicap(pplayer, H_HUTS) || map_get_known(x1, y1, pplayer))
          && tile_is_accessible(punit, x1, y1)
          && ai_fuzzy(pplayer, TRUE)) {
        best_x = x1;
        best_y = y1;
        bestcost = warmap.cost[best_x][best_y];
      }
    } iterate_outward_end;
    
    if (bestcost <= maxmoves * SINGLE_MOVE) {
      /* Go there! */
      if (!ai_unit_goto(punit, best_x, best_y)) {
        /* We're dead. */
        return FALSE;
      }

      if (punit->moves_left > 0) {
        /* We can still move on... */

        if (same_pos(punit->x, punit->y, best_x, best_y)) {
          /* ...and got into desired place. */
          return ai_manage_explorer(punit);  
        } else {
          /* Something went wrong. This should almost never happen. */
	  if (!same_pos(punit->x, punit->y, x, y)) {
	    generate_warmap(map_get_city(punit->x, punit->y), punit);
	  }
          
          x = punit->x;
          y = punit->y;
          /* Fallthrough to next part. */
        }

      } else {
        return TRUE;
      }
    }
  }

  /* 
   * PART 2: Move into unexplored territory
   * Move the unit as long as moving will unveil unknown territory
   */
  
  while (punit->moves_left > 0) {
    /* Best (highest) number of unknown tiles adjacent (in vision range) */
    int most_unknown = 0;
    /* Desired destination */
    int best_x = -1, best_y = -1;

    /* Evaluate all adjacent tiles. */
    
    square_iterate(x, y, 1, x1, y1) {
      /* Number of unknown tiles in vision range around this tile */
      int unknown = 0;
      
      square_iterate(x1, y1, range, x2, y2) {
        if (!map_get_known(x2, y2, pplayer))
          unknown++;
      } square_iterate_end;

      if (unknown > most_unknown) {
        if (unit_flag(punit, F_TRIREME)
            && trireme_loss_pct(pplayer, x1, y1) != 0)
          continue;
        
        if (map_get_continent(x1, y1, NULL) != continent)
          continue;
        
        if (could_unit_move_to_tile(punit, x1, y1) == 0) {
          continue;
        }

        /* We won't travel into cities, unless we are able to do so - diplomats
         * and caravans can. */
        /* FIXME/TODO: special flag for this? --pasky */
        /* FIXME: either comment or code is wrong here :-) --pasky */
        if (map_get_city(x1, y1) && (unit_flag(punit, F_DIPLOMAT) 
                                     || unit_flag(punit, F_TRADE_ROUTE)))
          continue;

        if (is_barbarian(pplayer) && map_has_special(x1, y1, S_HUT))
          continue;
          
        most_unknown = unknown;
        best_x = x1;
        best_y = y1;
      }
    } square_iterate_end;

    if (most_unknown > 0) {
      /* We can die because easy AI may stumble on huts and so disappear in the
       * wilderness - unit_id is used to check this */
      int unit_id = punit->id;
      bool broken = TRUE; /* failed movement */
      
      /* Some tile have unexplored territory adjacent, let's move there. */

      /* ai_unit_move for AI players, handle_unit_move_request for humans */
      if ((pplayer->ai.control && ai_unit_move(punit, best_x, best_y))
          || (!pplayer->ai.control 
              && handle_unit_move_request(punit, best_x, best_y, FALSE, FALSE))) {
        x = punit->x;
        y = punit->y;
        broken = FALSE;
      }
      
      if (!player_find_unit_by_id(pplayer, unit_id)) {
        /* We're dead. */
        return FALSE;
      }

      if (broken) { break; } /* a move failed, so danger of endless loop */
    } else {
      /* Everything is already explored. */
      break;
    }
  }

  if (punit->moves_left == 0) {
    /* We can't move on anymore. */
    return TRUE;
  }

  /* 
   * PART 3: Go towards unexplored territory
   * No adjacent squares help us to explore - really slow part follows.
   */

  {
    /* Best (highest) number of unknown tiles adjectent (in vision range) */
    int most_unknown = 0;
    /* Desired destination */
    int best_x = -1, best_y = -1;
  
    generate_warmap(map_get_city(x, y), punit);

    /* XXX: There's some duplicate code here, but it's several tiny pieces,
     * impossible to group together and not worth their own function
     * separately. --pasky */
    
    whole_map_iterate(x1, y1) {
      /* The actual map tile */
      struct tile *ptile = map_get_tile(x1, y1);
      
      if (ptile->continent == continent
          && !is_non_allied_unit_tile(ptile, pplayer)
          && !is_non_allied_city_tile(ptile, pplayer)
          && tile_is_accessible(punit, x1, y1)) {
        /* Number of unknown tiles in vision range around this tile */
        int unknown = 0;
        
        square_iterate(x1, y1, range, x2, y2) {
          if (!map_get_known(x2, y2, pplayer))
            unknown++;
        } square_iterate_end;
        
        if (unknown > 0) {
#define COSTWEIGHT 9
          /* How far it's worth moving away */
          int threshold = THRESHOLD * move_rate;
          
          if (is_sailing_unit(punit))
            unknown += COSTWEIGHT * (threshold - warmap.seacost[x1][y1]);
          else
            unknown += COSTWEIGHT * (threshold - warmap.cost[x1][y1]);
          
          /* FIXME? Why we don't do same tests like in part 2? --pasky */
          if (((unknown > most_unknown) ||
               (unknown == most_unknown && myrand(2) == 1))
              && !(is_barbarian(pplayer) && tile_has_special(ptile, S_HUT))) {
            best_x = x1;
            best_y = y1;
            most_unknown = unknown;
          }
#undef COSTWEIGHT
        }
      }
    } whole_map_iterate_end;

    if (most_unknown > 0) {
      /* Go there! */
      if (!ai_unit_goto(punit, best_x, best_y)) {
        return FALSE;
      }
      
      if (punit->moves_left > 0) {
        /* We can still move on... */

        if (same_pos(punit->x, punit->y, best_x, best_y)) {
          /* ...and got into desired place. */
          return ai_manage_explorer(punit);          
        } else {
          /* Something went wrong. What to do but return? */
          if (punit->activity == ACTIVITY_EXPLORE) {
            handle_unit_activity_request(punit, ACTIVITY_IDLE);
          }
          return FALSE;
        }
        
      } else {
        return TRUE;
      }
    }
    
    /* No candidates; fall-through. */
  }

  /* We have nothing to explore, so we can go idle. */
  UNIT_LOG(LOG_DEBUG, punit, "failed to explore more");
  if (punit->activity == ACTIVITY_EXPLORE) {
    handle_unit_activity_request(punit, ACTIVITY_IDLE);
  }
  
  /* 
   * PART 4: Go home
   * If we are AI controlled _military_ unit (so Explorers don't count, why?
   * --pasky), we will return to our homecity, maybe even to another continent.
   */
  
  if (pplayer->ai.control) {
    /* Unit's homecity */
    struct city *pcity = find_city_by_id(punit->homecity);
    /* No homecity? Find one! */
    if (!pcity) {
      pcity = find_closest_owned_city(pplayer, punit->x, punit->y, FALSE, NULL);
      if (pcity && ai_unit_make_homecity(punit, pcity)) {
        CITY_LOG(LOG_DEBUG, pcity, "we became home to an exploring %s",
                 unit_name(punit->type));
      }
    }

    if (pcity && !same_pos(punit->x, punit->y, pcity->x, pcity->y)) {
      if (map_get_continent(pcity->x, pcity->y, NULL) == continent) {
        UNIT_LOG(LOG_DEBUG, punit, "sending explorer home by foot");
        if (punit->homecity != 0) {
          ai_military_gohome(pplayer, punit);
        } else {
          /* Also try take care of deliberately homeless units */
          (void) ai_unit_goto(punit, pcity->x, pcity->y);
        }
      } else {
        /* Sea travel */
        if (find_boat(pplayer, &x, &y, 0) == 0) {
          punit->ai.ferryboat = -1;
          UNIT_LOG(LOG_DEBUG, punit, "exploring unit wants a boat, going home");
          ai_military_gohome(pplayer, punit); /* until then go home */
        } else {
          UNIT_LOG(LOG_DEBUG, punit, "sending explorer home by boat");
          (void) ai_unit_goto(punit, x, y);
        }
      }
    }
  }

  return FALSE;
}

/*********************************************************************
  In the words of Syela: "Using funky fprime variable instead of f in
  the denom, so that def=1 units are penalized correctly."

  Translation (GB): build_cost_balanced is used in the denominator of
  the want equation (see, e.g.  find_something_to_kill) instead of
  just build_cost to make AI build more balanced units (with def > 1).
*********************************************************************/
int build_cost_balanced(Unit_Type_id type)
{
  struct unit_type *ptype = get_unit_type(type);

  return 2 * ptype->build_cost * ptype->attack_strength /
      (ptype->attack_strength + ptype->defense_strength);
}


/**************************************************************************
  Return first city that contains a wonder being built on the given
  continent.
**************************************************************************/
static struct city *wonder_on_continent(struct player *pplayer, int cont)
{
  city_list_iterate(pplayer->cities, pcity) 
    if (!(pcity->is_building_unit) 
        && is_wonder(pcity->currently_building)
        && map_get_continent(pcity->x, pcity->y, NULL) == cont) {
      return pcity;
  }
  city_list_iterate_end;
  return NULL;
}

/**************************************************************************
  Return whether we should stay and defend a square, usually a city. Will
  protect allied cities temporarily.

  FIXME: We should check for fortresses here.
**************************************************************************/
static bool stay_and_defend(struct unit *punit)
{
  struct city *pcity = map_get_city(punit->x, punit->y);
  bool has_defense = FALSE;

  CHECK_UNIT(punit);

  if (!pcity) {
    return FALSE;
  }

  unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, pdef) {
    if (assess_defense_unit(pcity, punit, FALSE) >= 0
	&& pdef != punit
	&& pdef->homecity == pcity->id) {
      has_defense = TRUE;
    }
  } unit_list_iterate_end;
 
  /* Guess I better stay / you can live at home now */
  if (!has_defense && pcity->ai.danger > 0) {
    /* change homecity to this city */
    if (ai_unit_make_homecity(punit, pcity)) {
      /* Very important, or will not stay -- Syela */
      ai_unit_new_role(punit, AIUNIT_DEFEND_HOME, pcity->x, pcity->y);
      return TRUE;
    }
  }
  return FALSE;
}

/**************************************************************************
...
**************************************************************************/
int base_unit_belligerence_primitive(Unit_Type_id type, bool veteran,
				     int moves_left, int hp)
{
  return (base_get_attack_power(type, veteran, moves_left) * hp *
	  get_unit_type(type)->firepower / POWER_DIVIDER);
}

/********************************************************************** 
  ...
***********************************************************************/
static int unit_belligerence_primitive(struct unit *punit)
{
  return (base_unit_belligerence_primitive(punit->type, punit->veteran,
					   punit->moves_left, punit->hp));
}

/********************************************************************** 
  ...
***********************************************************************/
int unit_belligerence_basic(struct unit *punit)
{
  return (base_unit_belligerence_primitive(punit->type, punit->veteran,
					   SINGLE_MOVE, punit->hp));
}

/********************************************************************** 
  ...
***********************************************************************/
static int unit_belligerence(struct unit *punit)
{
  int v = unit_belligerence_basic(punit);
  return(v * v);
}

/********************************************************************** 
  ...
***********************************************************************/
static int unit_vulnerability_basic(struct unit *punit, struct unit *pdef)
{
  return (get_total_defense_power(punit, pdef) *
	  (punit->id != 0 ? pdef->hp : unit_type(pdef)->hp) *
	  unit_type(pdef)->firepower / POWER_DIVIDER);
}

/********************************************************************** 
  ...
***********************************************************************/
int unit_vulnerability_virtual(struct unit *punit)
{
  int v = base_get_defense_power(punit) * punit->hp / POWER_DIVIDER;

  return v * v;
}

/**************************************************************************
  See get_virtual_defense_power for the arguments att_type, def_type,
  x, y, fortified and veteran. If use_alternative_hp is FALSE the
  (maximum) hps of the given unit type is used. If use_alternative_hp
  is TRUE the given alternative_hp value is used.
**************************************************************************/
int unit_vulnerability_virtual2(Unit_Type_id att_type, Unit_Type_id def_type,
				int x, int y, bool fortified, bool veteran,
				bool use_alternative_hp, int alternative_hp)
{
  int v = (get_virtual_defense_power(att_type, def_type, x, y, fortified,
				     veteran) *
	   (use_alternative_hp ? alternative_hp : unit_types[def_type].
	    hp) * unit_types[def_type].firepower / POWER_DIVIDER);
  return v * v;
}

/********************************************************************** 
  ...
***********************************************************************/
static int unit_vulnerability(struct unit *punit, struct unit *pdef)
{
  int v = unit_vulnerability_basic(punit, pdef);
  return (v * v);
}

/********************************************************************** 
  ...
***********************************************************************/
static int stack_attack_value(int x, int y)
{
  int val = 0;
  struct tile *ptile = map_get_tile(x, y);
  unit_list_iterate(ptile->units, aunit)
    val += unit_belligerence_basic(aunit);
  unit_list_iterate_end;
  return(val);
}

/**************************************************************************
Compute how much we want to kill certain victim we've chosen, counted in
SHIELDs.

FIXME?: The equation is not accurate as the other values can vary for other
victims on same tile (we take values from best defender) - however I believe
it's accurate just enough now and lost speed isn't worth that. --pasky

Benefit is something like 'attractiveness' of the victim, how nice it would be
to destroy it. Larger value, worse loss for enemy.

Attack is the total possible attack power we can throw on the victim. Note that
it usually comes squared.

Loss is the possible loss when we would lose the unit we are attacking with (in
SHIELDs).

Vuln is vulnerability of our unit when attacking the enemy. Note that it
usually comes squared as well.

Victim count is number of victims stacked in the target tile. Note that we
shouldn't treat cities as a stack (despite the code using this function) - the
scaling is probably different. (extremely dodgy usage of it -- GB)
**************************************************************************/
int kill_desire(int benefit, int attack, int loss, int vuln, int victim_count)
{
  int desire;

  /*         attractiveness     danger */ 
  desire = ((benefit * attack - loss * vuln) * victim_count * SHIELD_WEIGHTING
            / (attack + vuln * victim_count));

  return desire;
}

/**************************************************************************
  Calculates the value and cost of nearby allied units to see if we can
  expect any help in our attack. Base function.
**************************************************************************/
static void reinforcements_cost_and_value(struct unit *punit, int x, int y,
                                          int *value, int *cost)
{
  *cost = 0;
  *value = 0;
  square_iterate(x, y, 1, i, j) {
    struct tile *ptile = map_get_tile(i, j);
    unit_list_iterate(ptile->units, aunit) {
      if (aunit != punit
	  && pplayers_allied(unit_owner(punit), unit_owner(aunit))) {
        int val = (unit_belligerence_basic(aunit));
        if (val != 0) {
          *value += val;
          *cost += unit_type(aunit)->build_cost;
        }
      }
    } unit_list_iterate_end;
  } square_iterate_end;
}

/**************************************************************************
  Calculates the value and cost of nearby units to see if we can expect 
  any help in our attack. Note that is includes the units of allied players
  in this calculation.
**************************************************************************/
static int reinforcements_value(struct unit *punit, int x, int y)
{
  int val, cost;

  reinforcements_cost_and_value(punit, x, y, &val, &cost);
  return val;
}

/*************************************************************************
  Is there another unit which really should be doing this attack? Checks
  all adjacent tiles and the tile we stand on for such units.
**************************************************************************/
static bool is_my_turn(struct unit *punit, struct unit *pdef)
{
  int val = unit_belligerence_primitive(punit), cur, d;
  struct tile *ptile;

  CHECK_UNIT(punit);

  square_iterate(pdef->x, pdef->y, 1, i, j) {
    ptile = map_get_tile(i, j);
    unit_list_iterate(ptile->units, aunit) {
      if (aunit == punit || aunit->owner != punit->owner)
	continue;
      if (!can_unit_attack_unit_at_tile(aunit, pdef, pdef->x, pdef->y))
	continue;
      d = get_virtual_defense_power(aunit->type, pdef->type, pdef->x,
				    pdef->y, FALSE, FALSE);
      if (d == 0)
	return TRUE;		/* Thanks, Markus -- Syela */
      cur = unit_belligerence_primitive(aunit) *
	  get_virtual_defense_power(punit->type, pdef->type, pdef->x,
				    pdef->y, FALSE, FALSE) / d;
      if (cur > val && ai_fuzzy(unit_owner(punit), TRUE))
	return FALSE;
    }
    unit_list_iterate_end;
  }
  square_iterate_end;
  return TRUE;
}

/*************************************************************************
This function looks at tiles adjacent to the unit in order to find something
to kill or explore.  It prefers tiles in the following order:

Returns value of the victim which has been chosen:

99999    means empty (undefended) city
99998    means hut
2..99997 means value of enemy unit weaker than our unit
1        means barbarians wanting to pillage
0        means nothing found or error
-2*MORT*TRADE_WEIGHTING
         means nothing found and punit causing unhappiness

If value <= 0 is returned, (dest_x, dest_y) is set to actual punit's position.
**************************************************************************/
static int ai_military_findvictim(struct unit *punit, int *dest_x, int *dest_y)
{
  struct player *pplayer = unit_owner(punit);
  int bellig = unit_belligerence_primitive(punit);
  int x = punit->x, y = punit->y;
  int best = 0;
  int stack_size = unit_list_size(&(map_get_tile(punit->x, punit->y)->units));

  CHECK_UNIT(punit);

  *dest_y = y;
  *dest_x = x;
 
  /* Ferryboats with passengers do not attack. */
  if (punit->ai.passenger > 0) {
    return 0;
  }
 
  if (punit->unhappiness > 0) {
    /* When we're causing unhappiness, we'll set best even lower, 
     * so that we will take even targets which we would ignore otherwise 
     * (in other words -- we're going to commit suicide). */
    best = - 2 * MORT * TRADE_WEIGHTING;
  }

  adjc_iterate(x, y, x1, y1) {
    /* Macro to set the tile with our target as the best (with value new_best) */
#define SET_BEST(new_best) \
    do { best = (new_best); *dest_x = x1; *dest_y = y1; } while (FALSE)

    struct unit *pdef = get_defender(punit, x1, y1);
    
    if (pdef) {
      struct unit *patt = get_attacker(punit, x1, y1);

      if (!can_unit_attack_tile(punit, x1, y1)) {
        continue;
      }

      /* If we are in the city, let's deeply consider defending it - however,
       * horsemen in city refused to attack phalanx just outside that was
       * bodyguarding catapult; thus, we get the best attacker on the tile (x1,
       * y1) as well, for the case when there are multiple different units on
       * the tile (x1, y1). Thus we force punit to attack a stack of units if
       * they're endangering punit seriously, even if they aren't that weak. */
      /* FIXME: The get_total_defense_power(pdef, punit) should probably use
       * patt rather than pdef. There also ought to be a better metric for
       * determining this. */
      if (patt
          && map_get_city(x, y) 
          && get_total_defense_power(pdef, punit) *
             get_total_defense_power(punit, pdef) >=
             get_total_attack_power(patt, punit) *
             get_total_attack_power(punit, pdef)
          && stack_size < 2
          && get_total_attack_power(patt, punit) > 0) {
        freelog(LOG_DEBUG, "%s's %s defending %s from %s's %s at (%d, %d)",
                pplayer->name, unit_type(punit)->name, map_get_city(x, y)->name,
                unit_owner(pdef)->name, unit_type(pdef)->name, punit->x, punit->y);
      } else {
        /* See description of kill_desire() about this variables. */
        int vuln = unit_vulnerability(punit, pdef);
        int attack = reinforcements_value(punit, pdef->x, pdef->y) + bellig;
        int benefit = unit_type(pdef)->build_cost;
        int loss = unit_type(punit)->build_cost;
        
        attack *= attack;
        
        /* If the victim is in the city, we increase the benefit and correct
         * it with our health because there may be more units in the city
         * stacked, and we won't destroy them all at once, so in the next
         * turn they may attack us. So we shouldn't send already injured
         * units to useless suicide. */
        if (map_get_city(x1, y1)) {
          benefit += unit_value(get_role_unit(F_CITIES, 0));
          benefit = (benefit * punit->hp) / unit_type(punit)->hp;
        }
        
        /* If we have non-zero belligerence... */
        if (attack > 0 && is_my_turn(punit, pdef)) {
          int desire;

          /* FIXME? Why we don't use stack_size as victim_count? --pasky */

          desire = kill_desire(benefit, attack, loss, vuln, 1);
          
          /* No need to amortize! We're doing it in one-turn horizon. */
          
          if (desire > best && ai_fuzzy(pplayer, TRUE)) {
            freelog(LOG_DEBUG, "Better than %d is %d (%s)",
                          best, desire, unit_type(pdef)->name);
            SET_BEST(desire);
            
          } else {
            freelog(LOG_DEBUG, "NOT better than %d is %d (%s)",
                    best, desire, unit_type(pdef)->name);
          }
        }
      }
      
    } else {
      struct city *pcity = map_get_city(x1, y1);
      
      /* No defender... */
     
      /* ...and free foreign city waiting for us. Who would resist! */
      if (pcity && pplayers_at_war(pplayer, city_owner(pcity))
          && is_ground_unit(punit)
          && !is_ocean(map_get_terrain(punit->x, punit->y))) {
        SET_BEST(99999);
        continue;
      }

      /* ...or tiny pleasant hut here! */
      if (map_has_special(x1, y1, S_HUT) && best < 99999
          && could_unit_move_to_tile(punit, x1, y1) != 0
          && !is_barbarian(unit_owner(punit))
          && punit->ai.ai_role != AIUNIT_ESCORT
          && punit->ai.charge == BODYGUARD_NONE /* Above line doesn't seem to work. :( */
          && punit->ai.ai_role != AIUNIT_DEFEND_HOME) {
        SET_BEST(99998);
        continue;
      }
      
      /* If we have nothing to do, we can at least pillage something, hmm? */
      if (is_land_barbarian(pplayer) && best == 0
          && get_tile_infrastructure_set(map_get_tile(x1, y1)) != S_NO_SPECIAL
          && could_unit_move_to_tile(punit, x1, y1) != 0) {
        SET_BEST(1);
        continue;
      }
    }
#undef SET_BEST
  } adjc_iterate_end;

  return best;
}

/*************************************************************************
  Find and kill anything adjacent to us that we don't like with a 
  given threshold until we have run out of juicy targets or movement. 
  Wraps ai_military_findvictim().
  Returns TRUE if survived the rampage session.
**************************************************************************/
static bool ai_military_rampage(struct unit *punit, int threshold)
{
  int x, y;
  int count = punit->moves_left + 1; /* break any infinite loops */

  CHECK_UNIT(punit);

  while (punit->moves_left > 0 && count-- > 0
         && ai_military_findvictim(punit, &x, &y) >= threshold) {
    if (!ai_unit_attack(punit, x, y)) {
      /* Died */
      return FALSE;
    }
  }
  assert(count >= 0);
  return TRUE;
}

/*************************************************************************
  If we are not covering our charge's ass, go do it now. Also check if we
  can kick some ass, which is always nice.
**************************************************************************/
static void ai_military_bodyguard(struct player *pplayer, struct unit *punit)
{
  struct unit *aunit = player_find_unit_by_id(pplayer, punit->ai.charge);
  struct city *acity = find_city_by_id(punit->ai.charge);
  int x, y, id = punit->id;

  CHECK_UNIT(punit);

  if (aunit && aunit->owner == punit->owner) { 
    /* protect a unit */
    x = aunit->x; 
    y = aunit->y; 
  } else if (acity && acity->owner == punit->owner) { 
    /* protect a city */
    x = acity->x; 
    y = acity->y; 
  } else { 
    /* should be impossible */
    ai_unit_new_role(punit, AIUNIT_NONE, -1 , -1);
    return;
  }

  if (aunit) {
    freelog(LOG_DEBUG, "%s#%d@(%d,%d) to meet charge %s#%d@(%d,%d)[body=%d]",
	    unit_type(punit)->name, punit->id, punit->x, punit->y,
	    unit_type(aunit)->name, aunit->id, aunit->x, aunit->y,
	    aunit->ai.bodyguard);
  }

  if (!same_pos(punit->x, punit->y, x, y)) {
    if (goto_is_sane(punit, x, y, TRUE)) {
      (void) ai_unit_goto(punit, x, y);
    } else {
      /* can't possibly get there to help */
      ai_unit_new_role(punit, AIUNIT_NONE, -1, -1);
    }
  } else {
    /* I had these guys set to just fortify, which is so dumb. -- Syela */
    (void) ai_military_rampage(punit, 40 * SHIELD_WEIGHTING);
  }

  /* is this insanity supposed to be a sanity check? -- per */
  if (aunit && unit_list_find(&map_get_tile(x, y)->units, id) 
      && aunit->ai.bodyguard != BODYGUARD_NONE) {
    aunit->ai.bodyguard = id;
  }
}

/*************************************************************************
...
**************************************************************************/
int find_beachhead(struct unit *punit, int dest_x, int dest_y, int *x, int *y)
{
  int ok, best = 0;
  enum tile_terrain_type t;

  CHECK_UNIT(punit);

  adjc_iterate(dest_x, dest_y, x1, y1) {
    ok = 0;
    t = map_get_terrain(x1, y1);
    if (warmap.seacost[x1][y1] <= 6 * THRESHOLD && !is_ocean(t)) {
      /* accessible beachhead */
      adjc_iterate(x1, y1, x2, y2) {
	if (is_ocean(map_get_terrain(x2, y2))
	    && is_my_zoc(unit_owner(punit), x2, y2)) {
	  ok++;
	  goto OK;

	  /* FIXME: The above used to be "break; out of adjc_iterate",
	  but this is incorrect since the current adjc_iterate() is
	  implemented as two nested loops.  */
	}
      } adjc_iterate_end;
    OK:

      if (ok > 0) {
	/* accessible beachhead with zoc-ok water tile nearby */
        ok = get_tile_type(t)->defense_bonus;
	if (map_has_special(x1, y1, S_RIVER))
	  ok += (ok * terrain_control.river_defense_bonus) / 100;
        if (get_tile_type(t)->movement_cost * SINGLE_MOVE <
            unit_type(punit)->move_rate)
	  ok *= 8;
        ok += (6 * THRESHOLD - warmap.seacost[x1][y1]);
        if (ok > best) {
	  best = ok;
	  *x = x1;
	  *y = y1;
	}
      }
    }
  } adjc_iterate_end;

  return best;
}

/**************************************************************************
find_beachhead() works only when city is not further that 1 tile from
the sea. But Sea Raiders might want to attack cities inland.
So this finds the nearest land tile on the same continent as the city.
**************************************************************************/
static void find_city_beach( struct city *pc, struct unit *punit, int *x, int *y)
{
  int best_x = punit->x;
  int best_y = punit->y;
  int dist = 100;
  int search_dist = real_map_distance(pc->x, pc->y, punit->x, punit->y) - 1;

  CHECK_UNIT(punit);
  
  square_iterate(punit->x, punit->y, search_dist, xx, yy) {
    if (map_get_continent(xx, yy, NULL) == 
        map_get_continent(pc->x, pc->y, NULL)
        && real_map_distance(punit->x, punit->y, xx, yy) < dist) {

      dist = real_map_distance(punit->x, punit->y, xx, yy);
      best_x = xx;
      best_y = yy;
    }
  } square_iterate_end;

  *x = best_x;
  *y = best_y;
}

/**************************************************************************
...
**************************************************************************/
static int ai_military_gothere(struct player *pplayer, struct unit *punit,
			       int dest_x, int dest_y)
{
  int id, x, y, boatid = 0, bx = 0, by = 0;
  struct unit *ferryboat;
  struct unit *def;
  struct city *dcity;
  struct tile *ptile;
  int d_type, d_val;

  CHECK_UNIT(punit);

  id = punit->id; x = punit->x; y = punit->y;

  if (is_ground_unit(punit)) boatid = find_boat(pplayer, &bx, &by, 2);
  ferryboat = unit_list_find(&(map_get_tile(x, y)->units), boatid);

  if (!same_pos(dest_x, dest_y, x, y)) {

/* do we require protection? */
    d_val = stack_attack_value(dest_x, dest_y);
    if ((dcity = map_get_city(dest_x, dest_y))) {
      d_type = ai_choose_defender_versus(dcity, punit->type);
      d_val += 
	base_unit_belligerence_primitive(d_type, 
                                         do_make_unit_veteran(dcity, d_type), 
                                         SINGLE_MOVE, unit_types[d_type].hp);
    }
    d_val *= POWER_DIVIDER;
    d_val /= (unit_type(punit)->move_rate / SINGLE_MOVE);
    if (unit_flag(punit, F_IGTER)) d_val /= 1.5;
    freelog(LOG_DEBUG,
	    "%s@(%d,%d) looking for bodyguard, d_val=%d, my_val=%d",
	    unit_type(punit)->name, punit->x, punit->y, d_val,
	    (punit->hp * (punit->veteran ? 15 : 10)
	     * unit_type(punit)->defense_strength));
    ptile = map_get_tile(punit->x, punit->y);
    if (d_val >= punit->hp * (punit->veteran ? 15 : 10) *
                unit_type(punit)->defense_strength) {
/*      if (!unit_list_find(&pplayer->units, punit->ai.bodyguard))   Ugggggh */
      if (!unit_list_find(&ptile->units, punit->ai.bodyguard))
        punit->ai.bodyguard = -1;
    } else if (!unit_list_find(&ptile->units, punit->ai.bodyguard))
      punit->ai.bodyguard = BODYGUARD_NONE;

    if (is_barbarian(pplayer))  /* barbarians must have more courage */
      punit->ai.bodyguard = BODYGUARD_NONE;
/* end protection subroutine */

    if (!goto_is_sane(punit, dest_x, dest_y, TRUE) ||
       (ferryboat && goto_is_sane(ferryboat, dest_x, dest_y, TRUE))) { /* Important!! */
      punit->ai.ferryboat = boatid;
      freelog(LOG_DEBUG, "%s: %d@(%d, %d): Looking for BOAT (id=%d).",
		    pplayer->name, punit->id, punit->x, punit->y, boatid);
      if (!same_pos(x, y, bx, by)) {
	if (!ai_unit_goto(punit, bx, by)) {
	  return -1;		/* died */
	}
      }
      ptile = map_get_tile(punit->x, punit->y);
      ferryboat = unit_list_find(&ptile->units, punit->ai.ferryboat);
      if (ferryboat && (ferryboat->ai.passenger == 0 ||
          ferryboat->ai.passenger == punit->id)) {
	freelog(LOG_DEBUG, "We have FOUND BOAT, %d ABOARD %d@(%d,%d)->(%d, %d).",
		punit->id, ferryboat->id, punit->x, punit->y,
		dest_x, dest_y);
        handle_unit_activity_request(punit, ACTIVITY_SENTRY);
        ferryboat->ai.passenger = punit->id;
/* the code that worked for settlers wasn't good for piles of cannons */
        if (find_beachhead(punit, dest_x, dest_y, &ferryboat->goto_dest_x,
               &ferryboat->goto_dest_y) != 0) {
          punit->goto_dest_x = dest_x;
          punit->goto_dest_y = dest_y;
          handle_unit_activity_request(punit, ACTIVITY_SENTRY);
	  if (ground_unit_transporter_capacity(punit->x, punit->y, pplayer)
	      <= 0) {
	    freelog(LOG_DEBUG, "All aboard!");
	    /* perhaps this should only require two passengers */
            unit_list_iterate(ptile->units, mypass) {
              if (mypass->ai.ferryboat == ferryboat->id
                  && punit->owner == mypass->owner) {
                handle_unit_activity_request(mypass, ACTIVITY_SENTRY);
                def = unit_list_find(&ptile->units, mypass->ai.bodyguard);
                if (def) {
                  handle_unit_activity_request(def, ACTIVITY_SENTRY);
                }
              }
            } unit_list_iterate_end; /* passengers are safely stowed away */
	    if (!ai_unit_goto(ferryboat, dest_x, dest_y)) {
	      return -1;	/* died */
	    }
            handle_unit_activity_request(punit, ACTIVITY_IDLE);
          } /* else wait, we can GOTO later. */
        }
      } 
    }
    if (goto_is_sane(punit, dest_x, dest_y, TRUE) && punit->moves_left > 0 &&
       (!ferryboat || 
       (real_map_distance(punit->x, punit->y, dest_x, dest_y) < 3 &&
       (punit->ai.bodyguard == BODYGUARD_NONE || unit_list_find(&(map_get_tile(punit->x,
       punit->y)->units), punit->ai.bodyguard) || (dcity &&
       !has_defense(dcity)))))) {
/* if we are on a boat, disembark only if we are within two tiles of
our target, and either 1) we don't need a bodyguard, 2) we have a
bodyguard, or 3) we are going to an empty city.  Previously, cannons
would disembark before the cruisers arrived and die. -- Syela */

      punit->goto_dest_x = dest_x;
      punit->goto_dest_y = dest_y;

/* The following code block is supposed to stop units from running away from their
bodyguards, and not interfere with those that don't have bodyguards nearby -- Syela */
/* The case where the bodyguard has moves left and could meet us en route is not
handled properly.  There should be a way to do it with dir_ok but I'm tired now. -- Syela */
      if (punit->ai.bodyguard < BODYGUARD_NONE) { 
	adjc_iterate(punit->x, punit->y, i, j) {
	  unit_list_iterate(map_get_tile(i, j)->units, aunit) {
	    if (aunit->ai.charge == punit->id
                && punit->owner == aunit->owner) {
	      freelog(LOG_DEBUG,
		      "Bodyguard at (%d, %d) is adjacent to (%d, %d)",
		      i, j, punit->x, punit->y);
	      if (aunit->moves_left > 0) return(0);
	      else return ai_unit_move(punit, i, j) ? 1 : 0;
	    }
	  } unit_list_iterate_end;
        } adjc_iterate_end;
      } /* end if */
/* end 'short leash' subroutine */

      if (ferryboat) {
	freelog(LOG_DEBUG, "GOTHERE: %s#%d@(%d,%d)->(%d,%d)", 
		unit_type(punit)->name, punit->id,
		punit->x, punit->y, dest_x, dest_y);
      }
      if (!ai_unit_goto(punit, dest_x, dest_y)) {
	return -1;		/* died */
      }
      /* liable to bump into someone that will kill us.  Should avoid? */
    } else {
      freelog(LOG_DEBUG, "%s#%d@(%d,%d) not moving -> (%d, %d)",
		    unit_type(punit)->name, punit->id,
		    punit->x, punit->y, dest_x, dest_y);
    }
  }

  /* Dead unit shouldn't reach this point */
  CHECK_UNIT(punit);
  
  if (!same_pos(punit->x, punit->y, x, y)) {
    return 1;			/* moved */
  } else {
    return 0;			/* didn't move, didn't die */
  }
}

/*************************************************************************
  Does the unit with the id given have the flag L_DEFEND_GOOD?
**************************************************************************/
static bool unit_role_defender(Unit_Type_id type)
{
  if (unit_types[type].move_type != LAND_MOVING) {
    return FALSE; /* temporary kluge */
  }
  return (unit_has_role(type, L_DEFEND_GOOD));
}

/*************************************************************************
  See if we can find something to defend. Called both by wannabe
  bodyguards and building want estimation code. Returns desirability
  for using this unit as a bodyguard or for defending a city.

  Requires an initialized warmap!
**************************************************************************/
int look_for_charge(struct player *pplayer, struct unit *punit, 
                    struct unit **aunit, struct city **acity)
{
  int dist, def, best = 0;
  int toughness = unit_vulnerability_virtual(punit);

  if (toughness == 0) {
    /* useless */
    return 0; 
  }

  unit_list_iterate(pplayer->units, buddy) {
    if (buddy->ai.bodyguard == BODYGUARD_NONE /* should be != BODYGUARD_WANTED */
        || !goto_is_sane(punit, buddy->x, buddy->y, TRUE)
        || unit_type(buddy)->move_rate > unit_type(punit)->move_rate
        || unit_type(buddy)->move_type != unit_type(punit)->move_type) { 
      continue;
    }
    dist = unit_move_turns(punit, buddy->x, buddy->y);
    def = (toughness - unit_vulnerability_virtual(buddy)) >> dist;
    freelog(LOG_DEBUG, "(%d,%d)->(%d,%d), %d turns, def=%d",
	    punit->x, punit->y, buddy->x, buddy->y, dist, def);

    /* sanity check: if we already have a bodyguard, but don't know about it,
       we don't need a new one... this check should be superfluous eventually */
    unit_list_iterate(pplayer->units, body) {
      if (body->ai.charge == buddy->id) { 
        /* if buddy already has a bodyguard, ignore it */
        def = 0;
        /* we should  buddy->bodyguard = body->id;  here */
      }
    } unit_list_iterate_end;
    if (def > best && ai_fuzzy(pplayer, TRUE)) { 
      *aunit = buddy; 
      best = def; 
    }
  } unit_list_iterate_end;

  city_list_iterate(pplayer->cities, mycity) {
    if (!goto_is_sane(punit, mycity->x, mycity->y, TRUE)
        || mycity->ai.urgency == 0) { 
      continue; 
    }
    dist = unit_move_turns(punit, mycity->x, mycity->y);
    def = (mycity->ai.danger - assess_defense_quadratic(mycity)) >> dist;
    if (def > best && ai_fuzzy(pplayer, TRUE)) { 
      *acity = mycity; 
      best = def; 
    }
  } city_list_iterate_end;

  freelog(LOG_DEBUG, "%s: %s (%d@%d,%d) looking for charge; %d/%d",
	  pplayer->name, unit_type(punit)->name, punit->id,
	  punit->x, punit->y, best, best * 100 / toughness);

  return ((best * 100) / toughness);
}

/********************************************************************** 
  Find something to do with a unit. Also, check sanity of existing
  missions.
***********************************************************************/
static void ai_military_findjob(struct player *pplayer,struct unit *punit)
{
  struct city *pcity = NULL, *acity = NULL;
  struct unit *aunit;
  int val, def;
  int q = 0;
  struct unit_type *punittype = get_unit_type(punit->type);

  CHECK_UNIT(punit);

/* tired of AI abandoning its cities! -- Syela */
  if (punit->homecity != 0 && (pcity = find_city_by_id(punit->homecity))) {
    if (pcity->ai.danger != 0) { /* otherwise we can attack */
      def = assess_defense(pcity);
      if (same_pos(punit->x, punit->y, pcity->x, pcity->y)) {
        /* I'm home! */
        val = assess_defense_unit(pcity, punit, FALSE); 
        def -= val; /* old bad kluge fixed 980803 -- Syela */
/* only the least defensive unit may leave home */
/* and only if this does not jeopardize the city */
/* def is the defense of the city without punit */
        if (unit_flag(punit, F_FIELDUNIT)) val = -1;
        unit_list_iterate(map_get_tile(pcity->x, pcity->y)->units, pdef)
          if (is_military_unit(pdef) 
              && pdef != punit 
              && !unit_flag(pdef, F_FIELDUNIT)
              && pdef->owner == punit->owner) {
            if (assess_defense_unit(pcity, pdef, FALSE) >= val) val = 0;
          }
        unit_list_iterate_end; /* was getting confused without the is_military part in */
        if (unit_vulnerability_virtual(punit) == 0) q = 0; /* thanks, JMT, Paul */
        else q = (pcity->ai.danger * 2 - def * unit_type(punit)->attack_strength /
             unit_type(punit)->defense_strength);
/* this was a WAG, but it works, so now it's just good code! -- Syela */
        if (val > 0 || q > 0) { /* Guess I better stay */
          ;
        } else q = 0;
      } /* end if home */
    } /* end if home is in danger */
  } /* end if we have a home */

  /* keep barbarians aggresive and primitive */
  if (is_barbarian(pplayer)) {
    if (can_unit_do_activity(punit, ACTIVITY_PILLAGE)
	&& is_land_barbarian(pplayer))
      /* land barbarians pillage */
      ai_unit_new_role(punit, AIUNIT_PILLAGE, -1, -1);
    else
      ai_unit_new_role(punit, AIUNIT_ATTACK, -1, -1);
    return;
  }

  if (punit->ai.charge != BODYGUARD_NONE) { /* I am a bodyguard */
    aunit = player_find_unit_by_id(pplayer, punit->ai.charge);
    acity = find_city_by_id(punit->ai.charge);

    /* Check if city we are on our way to rescue is still in danger,
     * or unit we should protect is still alive */
    if ((aunit && aunit->ai.bodyguard != BODYGUARD_NONE 
         && unit_vulnerability_virtual(punit) >
         unit_vulnerability_virtual(aunit)) ||
         (acity && acity->owner == punit->owner && acity->ai.urgency != 0 &&
          acity->ai.danger > assess_defense_quadratic(acity))) {
      assert(punit->ai.ai_role == AIUNIT_ESCORT);
      return;
    } else {
      ai_unit_new_role(punit, AIUNIT_NONE, -1, -1);
    }
  }

  /* ok, what if I'm somewhere new? - ugly, kludgy code by Syela */
  if (stay_and_defend(punit)) {
    UNIT_LOG(LOG_DEBUG, punit, "stays to defend %s",
             map_get_city(punit->x, punit->y)->name);
    return;
  }

  if (q > 0 && pcity->ai.urgency > 0) {
    ai_unit_new_role(punit, AIUNIT_DEFEND_HOME, pcity->x, pcity->y);
    return;
  }

  /* Is the unit badly damaged? */
  if ((punit->ai.ai_role == AIUNIT_RECOVER
       && punit->hp < punittype->hp)
      || punit->hp < punittype->hp * 0.25) { /* WAG */
    UNIT_LOG(LOGLEVEL_RECOVERY, punit, "set to hp recovery");
    ai_unit_new_role(punit, AIUNIT_RECOVER, -1, -1);
    return;
  }

/* I'm not 100% sure this is the absolute best place for this... -- Syela */
  generate_warmap(map_get_city(punit->x, punit->y), punit);
/* I need this in order to call unit_move_turns, here and in look_for_charge */

  if (q > 0) {
    q *= 100;
    q /= unit_vulnerability_virtual(punit);
    q >>= unit_move_turns(punit, pcity->x, pcity->y);
  }

  val = 0; acity = NULL; aunit = NULL;
  if (unit_role_defender(punit->type)) {
    /* 
     * This is a defending unit that doesn't need to stay put.
     * It needs to defend something, but not necessarily where it's at.
     * Therefore, it will consider becoming a bodyguard. -- Syela 
     */
    val = look_for_charge(pplayer, punit, &aunit, &acity);
  }
  if (q > val) {
    ai_unit_new_role(punit, AIUNIT_DEFEND_HOME, pcity->x, pcity->y);
    return;
  }
  /* this is bad; riflemen might rather attack if val is low -- Syela */
  if (acity) {
    ai_unit_new_role(punit, AIUNIT_ESCORT, acity->x, acity->y);
    punit->ai.charge = acity->id;
    BODYGUARD_LOG(LOG_DEBUG, punit, "going to defend city");
  } else if (aunit) {
    ai_unit_new_role(punit, AIUNIT_ESCORT, aunit->x, aunit->y);
    punit->ai.charge = aunit->id;
    BODYGUARD_LOG(LOG_DEBUG, punit, "going to defend unit");
  } else if (ai_unit_attack_desirability(punit->type) != 0 ||
      (pcity && !same_pos(pcity->x, pcity->y, punit->x, punit->y))) {
     ai_unit_new_role(punit, AIUNIT_ATTACK, -1, -1);
  } else {
    ai_unit_new_role(punit, AIUNIT_DEFEND_HOME, -1, -1); /* for default */
  }
}

/********************************************************************** 
...
***********************************************************************/
static void ai_military_gohome(struct player *pplayer,struct unit *punit)
{
  struct city *pcity;

  CHECK_UNIT(punit);

  if (punit->homecity != 0){
    pcity=find_city_by_id(punit->homecity);
    freelog(LOG_DEBUG, "GOHOME (%d)(%d,%d)C(%d,%d)",
		 punit->id,punit->x,punit->y,pcity->x,pcity->y); 
    if (same_pos(punit->x, punit->y, pcity->x, pcity->y)) {
      freelog(LOG_DEBUG, "INHOUSE. GOTO AI_NONE(%d)", punit->id);
      ai_unit_new_role(punit, AIUNIT_NONE, -1, -1);
      /* aggro defense goes here -- Syela */
      (void) ai_military_rampage(punit, 2); /* 2 is better than pillage */
    } else {
      UNIT_LOG(LOG_DEBUG, punit, "GOHOME");
      (void) ai_unit_goto(punit, pcity->x, pcity->y);
    }
  }
}

/***************************************************************************
 A rough estimate of time (measured in turns) to get to the enemy city, 
 taking into account ferry transfer.
 If boat == NULL, we will build a boat of type boattype right here, so
 we wouldn't have to walk to it.

 Requires ready warmap(s).  Assumes punit is ground or sailing.
***************************************************************************/
int turns_to_enemy_city(Unit_Type_id our_type,  struct city *acity,
                        int speed, bool go_by_boat, 
                        struct unit *boat, Unit_Type_id boattype)
{
  switch(unit_types[our_type].move_type) {
  case LAND_MOVING:
    if (go_by_boat) {
      int boatspeed = unit_types[boattype].move_rate;
      int move_time = (warmap.seacost[acity->x][acity->y]) / boatspeed;
      
      if (unit_type_flag(boattype, F_TRIREME) && move_time > 2) {
        /* FIXME: Should also check for LIGHTHOUSE */
        /* Return something prohibitive */
        return 999;
      }
      if (boat) {
        /* Time to get to the boat */
        move_time += (warmap.cost[boat->x][boat->y] + speed - 1) / speed;
      }
      
      if (!unit_type_flag(our_type, F_MARINES)) {
        /* Time to get off the boat (Marines do it from the vessel) */
        move_time += 1;
      }
      
      return move_time;
    } else {
      /* We are walking */
      return (warmap.cost[acity->x][acity->y] + speed - 1) / speed;
    }
  case SEA_MOVING:
    /* We are a boat: time to sail */
    return (warmap.seacost[acity->x][acity->y] + speed - 1) / speed;
  default: 
    freelog(LOG_ERROR, "ERROR: Unsupported move_type in time_to_enemy_city");
    /* Return something prohibitive */
    return 999;
  }

}

/************************************************************************
 Rough estimate of time (in turns) to catch up with the enemy unit.  
 FIXME: Take enemy speed into account in a more sensible way

 Requires precomputed warmap.  Assumes punit is ground or sailing. 
************************************************************************/ 
int turns_to_enemy_unit(Unit_Type_id our_type, int speed, int x, int y, 
                        Unit_Type_id enemy_type)
{
  int dist;

  switch(unit_types[our_type].move_type) {
  case LAND_MOVING:
    dist = warmap.cost[x][y];
    break;
  case SEA_MOVING:
    dist = warmap.seacost[x][y];
    break;
  default:
    /* Compiler warning */
    dist = 0; 
    freelog(LOG_ERROR, "ERROR: Unsupported unit_type in time_to_enemy_city");
    /* Return something prohibitive */
    return 999;
  }

  /* if dist <= move_rate, we hit the enemy right now */    
  if (dist > speed) { 
    /* Weird attempt to take into account enemy running away... */
    dist *= unit_types[enemy_type].move_rate;
    if (unit_type_flag(enemy_type, F_IGTER)) {
      dist *= 3;
    }
  }
  
  return (dist + speed - 1) / speed;
}

/*************************************************************************
  Mark invasion possibilities of punit in the surrounding cities. The
  given radius limites the area which is searched for cities. The
  center of the area is either the unit itself (dest == FALSE) or the
  destiniation of the current goto (dest == TRUE). The invasion threat
  is marked in pcity->ai.invasion via ORing the "which" argument (to
  tell attack from sea apart from ground unit attacks). Note that
  "which" should only have one bit set.

  If dest == TRUE then a valid goto is presumed.
**************************************************************************/
static void invasion_funct(struct unit *punit, bool dest, int radius,
			   int which)
{
  int x, y;

  CHECK_UNIT(punit);

  if (dest) {
    x = punit->goto_dest_x;
    y = punit->goto_dest_y;
  } else {
    x = punit->x;
    y = punit->y;
  }

  square_iterate(x, y, radius, x1, y1) {
    struct city *pcity = map_get_city(x1, y1);

    if (pcity && pplayers_at_war(city_owner(pcity), unit_owner(punit))
	&& (pcity->ai.invasion & which) != which
	&& (dest || !has_defense(pcity))) {
      pcity->ai.invasion |= which;
    }
  } square_iterate_end;
}

/*************************************************************************
  Find something to kill! This function is called for units to find 
  targets to destroy and for cities that want to know if they should
  build offensive units. Target location returned in (x, y), want as
  function return value.

  punit->id == 0 means that the unit is virtual (considered to be built).
**************************************************************************/
int find_something_to_kill(struct player *pplayer, struct unit *punit, 
                           int *x, int *y)
{
  /* basic attack */
  int attack_value = unit_belligerence_basic(punit);
  /* Enemy defence rating */
  int vuln;
  /* Benefit from killing the target */
  int benefit;
  /* Number of enemies there */
  int victim_count;
  /* Want (amortized) of the operaton */
  int want;
  /* Best of all wants */
  int best = 0;
  /* Our total attack value with reinforcements */
  int attack;
  int move_time, move_rate;
  int con = map_get_continent(punit->x, punit->y, NULL);
  struct unit *pdef;
  int maxd, needferry;
  /* Do we have access to sea? */
  bool harbor = FALSE;
  int bx = 0, by = 0;
  /* Build cost of the attacker (+adjustments) */
  int bcost, bcost_bal;
  bool handicap = ai_handicap(pplayer, H_TARGETS);
  struct unit *ferryboat = NULL;
  /* Type of our boat (a future one if ferryboat == NULL) */
  Unit_Type_id boattype = U_LAST;
  bool unhap = FALSE;
  struct city *pcity;
  /* this is a kluge, because if we don't set x and y with !punit->id,
   * p_a_w isn't called, and we end up not wanting ironclads and therefore 
   * never learning steam engine, even though ironclads would be very 
   * useful. -- Syela */
  int bk = 0; 

  /*** Very preliminary checks */
  *x = punit->x; 
  *y = punit->y;

  if (!is_ground_unit(punit) && !is_sailing_unit(punit)) {
    /* Don't know what to do with them! */
    return 0;
  }

  if (attack_value == 0) {
    /* A very poor attacker... */
    return 0;
  }

  /*** Part 1: Calculate targets ***/
  /* This horrible piece of code attempts to calculate the attractiveness of
   * enemy cities as targets for our units, by checking how many units are
   * going towards it or are near it already.
   */

  /* First calculate in nearby units */
  players_iterate(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)) {
      continue;
    }
    city_list_iterate(aplayer->cities, acity) {
      reinforcements_cost_and_value(punit, acity->x, acity->y,
                                    &acity->ai.attack, &acity->ai.bcost);
      acity->ai.invasion = 0;
    } city_list_iterate_end;
  } players_iterate_end;

  /* Second, calculate in units on their way there, and mark targets for
   * invasion */
  unit_list_iterate(pplayer->units, aunit) {
    if (aunit == punit) {
      continue;
    }

    /* dealing with invasion stuff */
    if (IS_ATTACKER(aunit)) {
      if (aunit->activity == ACTIVITY_GOTO) {
        invasion_funct(aunit, TRUE, 0, (CAN_OCCUPY(aunit) ? 1 : 2));
        if ((pcity = map_get_city(aunit->goto_dest_x, aunit->goto_dest_y))) {
          pcity->ai.attack += unit_belligerence_basic(aunit);
          pcity->ai.bcost += unit_type(aunit)->build_cost;
        } 
      }
      invasion_funct(aunit, FALSE, unit_type(aunit)->move_rate / SINGLE_MOVE,
                     (CAN_OCCUPY(aunit) ? 1 : 2));
    } else if (aunit->ai.passenger != 0 &&
               !same_pos(aunit->x, aunit->y, punit->x, punit->y)) {
      /* It's a transport with reinforcements */
      if (aunit->activity == ACTIVITY_GOTO) {
        invasion_funct(aunit, TRUE, 1, 1);
      }
      invasion_funct(aunit, FALSE, 2, 1);
    }
  } unit_list_iterate_end;
  /* end horrible initialization subroutine */

  /*** Part 2: Now pick one target ***/
  /* We first iterate through all cities, then all units, looking
   * for a viable target. We also try to gang up on the target
   * to avoid spreading out attacks too widely to be inefficient.
   */

  pcity = map_get_city(punit->x, punit->y);

  if (pcity && (punit->id == 0 || pcity->id == punit->homecity)) {
    /* I would have thought unhappiness should be taken into account 
     * irrespectfully the city in which it will surface...  GB */ 
    unhap = ai_assess_military_unhappiness(pcity, get_gov_pplayer(pplayer));
  }

  move_rate = unit_type(punit)->move_rate;
  if (unit_flag(punit, F_IGTER)) {
    move_rate *= 3;
  }

  maxd = MIN(6, move_rate) * THRESHOLD + 1;

  bcost = unit_type(punit)->build_cost;
  bcost_bal = build_cost_balanced(punit->type);

  /* most flexible but costs milliseconds */
  generate_warmap(map_get_city(*x, *y), punit);

  if (is_ground_unit(punit)) {
    int boatid = find_boat(pplayer, &bx, &by, 2);
    ferryboat = player_find_unit_by_id(pplayer, boatid);
  }

  if (ferryboat) {
    boattype = ferryboat->type;
    really_generate_warmap(map_get_city(ferryboat->x, ferryboat->y),
                           ferryboat, SEA_MOVING);
  } else {
    boattype = best_role_unit_for_player(pplayer, L_FERRYBOAT);
    if (boattype == U_LAST) {
      /* We pretend that we can have the simplest boat -- to stimulate tech */
      boattype = get_role_unit(L_FERRYBOAT, 0);
    }
  }

  if (is_ground_unit(punit) && punit->id == 0 
      && is_ocean_near_tile(punit->x, punit->y)) {
    harbor = TRUE;
  }

  players_iterate(aplayer) {
    if (!pplayers_at_war(pplayer, aplayer)) { 
      /* Not an enemy */
      continue;
    }

    city_list_iterate(aplayer->cities, acity) {
      bool go_by_boat = (is_ground_unit(punit)
                         && !(goto_is_sane(punit, acity->x, acity->y, TRUE) 
                              && warmap.cost[acity->x][acity->y] < maxd));

      if (handicap && !map_get_known(acity->x, acity->y, pplayer)) {
        /* Can't see it */
        continue;
      }

      if (go_by_boat 
          && (!(ferryboat || harbor)
              || warmap.seacost[acity->x][acity->y] > 6 * THRESHOLD)) {
        /* Too far or impossible to go by boat */
        continue;
      }
      
      if (is_sailing_unit(punit) 
          && warmap.seacost[acity->x][acity->y] >= maxd) {
        /* Too far to sail */
        continue;
      }
      
      if ((pdef = get_defender(punit, acity->x, acity->y))) {
        vuln = unit_vulnerability(punit, pdef);
        benefit = unit_type(pdef)->build_cost;
      } else { 
        vuln = 0; 
        benefit = 0; 
      }
      
      move_time = turns_to_enemy_city(punit->type, acity, move_rate, 
                                      go_by_boat, ferryboat, boattype);

      if (move_time > 1) {
        Unit_Type_id def_type = ai_choose_defender_versus(acity, punit->type);
        int v 
          = unit_vulnerability_virtual2(punit->type, def_type, acity->x,
                                        acity->y, FALSE,
                                        do_make_unit_veteran(acity, def_type),
                                        FALSE, 0);
        if (v >= vuln) {
          /* They can build a better defender! */ 
          vuln = v; 
          benefit = unit_types[def_type].build_cost; 
        }
      }

      if (CAN_OCCUPY(punit) || TEST_BIT(acity->ai.invasion, 0)) {
        /* There are units able to occupy the city! */
        benefit += 40;
      }

      attack = (attack_value + acity->ai.attack) 
        * (attack_value + acity->ai.attack);
      /* Avoiding handling upkeep aggregation this way -- Syela */
      
      /* AI was not sending enough reinforcements to totally wipe out a city
       * and conquer it in one turn.  
       * This variable enables total carnage. -- Syela */
      victim_count 
        = unit_list_size(&(map_get_tile(acity->x, acity->y)->units)) + 1;

      if (!CAN_OCCUPY(punit) && !pdef) {
        /* Nothing there to bash and we can't occupy! 
         * Not having this check caused warships yoyoing */
        want = 0;
      } else if (move_time > THRESHOLD) {
        /* Too far! */
        want = 0;
      } else if (CAN_OCCUPY(punit) && acity->ai.invasion == 2) {
        /* Units able to occupy really needed there! */
        want = bcost * SHIELD_WEIGHTING;
      } else {
        int a_squared = acity->ai.attack * acity->ai.attack;
        
        want = kill_desire(benefit, attack, (bcost + acity->ai.bcost), 
                           vuln, victim_count);
        if (benefit * a_squared > acity->ai.bcost * vuln) {
          /* If there're enough units to do the job, we don't need this
           * one. */
          /* FIXME: The problem with ai.bcost is that bigger it is, less is
           * our desire to go help other units.  Now suppose we need five
           * cavalries to take over a city, we have four (which is not
           * enough), then we will be severely discouraged to build the
           * fifth one.  Where is logic in this??!?! --GB */
          want -= kill_desire(benefit, a_squared, acity->ai.bcost, 
                              vuln, victim_count);
        }
      }
      want -= move_time * (unhap ? SHIELD_WEIGHTING + 2 * TRADE_WEIGHTING 
                           : SHIELD_WEIGHTING);
      /* build_cost of ferry */
      needferry = (go_by_boat && !ferryboat ? unit_value(boattype) : 0);
      /* FIXME: add time to build the ferry? */
      want = military_amortize(pplayer, find_city_by_id(punit->homecity),
                               want, MAX(1, move_time), bcost_bal + needferry);

      /* BEGIN STEAM-ENGINES-ARE-OUR-FRIENDS KLUGE */
      if (want <= 0 && punit->id == 0 && best == 0) {
        int bk_e = military_amortize(pplayer, find_city_by_id(punit->homecity),
                                     benefit * SHIELD_WEIGHTING, 
                                     MAX(1, move_time), bcost_bal + needferry);

        if (bk_e > bk) {
          *x = acity->x;
          *y = acity->y;
          bk = bk_e;
        }
      }
      /* END STEAM-ENGINES KLUGE */
      
      if (punit->id != 0 && ferryboat && is_ground_unit(punit)) {
        freelog(LOG_DEBUG, "%s@(%d, %d) -> %s@(%d, %d) -> %s@(%d, %d)"
                " (go_by_boat=%d, move_time=%d, want=%d, best=%d)",
                unit_type(punit)->name, punit->x, punit->y,
                unit_type(ferryboat)->name, bx, by,
                acity->name, acity->x, acity->y, 
                go_by_boat, move_time, want, best);
      }
      
      if (want > best && ai_fuzzy(pplayer, TRUE)) {
        /* Yes, we like this target */
        if (punit->id != 0 && is_ground_unit(punit) 
            && !unit_flag(punit, F_MARINES)
            && map_get_continent(acity->x, acity->y, NULL) != con) {
          /* a non-virtual ground unit is trying to attack something on 
           * another continent.  Need a beachhead which is adjacent 
           * to the city and an available ocean tile */
          int xx, yy;

          if (find_beachhead(punit, acity->x, acity->y, &xx, &yy) != 0) { 
            best = want;
            *x = acity->x;
            *y = acity->y;
            /* the ferryboat needs to target the beachhead, but the unit 
             * needs to target the city itself.  This is a little weird, 
             * but it's the best we can do. -- Syela */
          } /* else do nothing, since we have no beachhead */
        } else {
          best = want;
          *x = acity->x;
          *y = acity->y;
        } /* end need-beachhead else */
      }
    } city_list_iterate_end;

    attack = unit_belligerence(punit);
    /* I'm not sure the following code is good but it seems to be adequate. 
     * I am deliberately not adding ferryboat code to the unit_list_iterate. 
     * -- Syela */
    unit_list_iterate(aplayer->units, aunit) {
      if (map_get_city(aunit->x, aunit->y)) {
        /* already dealt with it */
        continue;
      }

      if (handicap && !map_get_known(aunit->x, aunit->y, pplayer)) {
        /* Can't see the target */
        continue;
      }

      if ((unit_flag(aunit, F_HELP_WONDER) || unit_flag(aunit, F_TRADE_ROUTE))
          && punit->id == 0) {
        /* We will not build units just to chase caravans */
        continue;
      }

      if (!(aunit == get_defender(punit, aunit->x, aunit->y))) {
        /* It's not the main defender */
        continue;
      }

      if (is_ground_unit(punit) 
          && (map_get_continent(aunit->x, aunit->y, NULL) != con 
              || warmap.cost[aunit->x][aunit->y] >= maxd)) {
        /* Impossible or too far to walk */
        continue;
      }

      if (is_sailing_unit(punit)
          && (!goto_is_sane(punit, aunit->x, aunit->y, TRUE)
              || warmap.seacost[aunit->x][aunit->y] >= maxd)) {
        /* Impossible or too far to sail */
        continue;
      }

      vuln = unit_vulnerability(punit, aunit);
      benefit = unit_type(aunit)->build_cost;
 
      move_time = turns_to_enemy_unit(punit->type, move_rate, 
                                      aunit->x, aunit->y, aunit->type);

      if (!CAN_OCCUPY(punit) && vuln == 0) {
        /* FIXME: There is something with defence 0 there, maybe a diplomat.
         * What's wrong in killing a diplomat? -- GB */
        want = 0;
      } else if (move_time > THRESHOLD) {
        /* Too far */
        want = 0;
      } else {
        want = kill_desire(benefit, attack, bcost, vuln, 1);
          /* Take into account maintainance of the unit */
          /* FIXME: Depends on the government */
        want -= move_time * SHIELD_WEIGHTING;
        /* Take into account unhappiness 
         * (costs 2 luxuries to compensate) */
        want -= (unhap ? 2 * move_time * TRADE_WEIGHTING : 0);
      }
      want = military_amortize(pplayer, find_city_by_id(punit->homecity),
                               want, MAX(1, move_time), bcost_bal);
      if (want > best && ai_fuzzy(pplayer, TRUE)) {
        best = want;
        *x = aunit->x;
        *y = aunit->y;
      }
    } unit_list_iterate_end;
  } players_iterate_end;

  return(best);
}

/**********************************************************************
  Find safe city to recover in. An allied player's city is just as 
  good as one of our own, since both replenish our hitpoints and
  reduce unhappiness.

  TODO: Actually check how safe the city is. This is a difficult
  decision not easily taken, since we also want to protect unsafe
  cities, at least most of the time.
***********************************************************************/
static struct city *find_nearest_safe_city(struct unit *punit)
{
  struct player *pplayer = unit_owner(punit);
  struct city *acity = NULL;
  int best = 6 * THRESHOLD + 1, cur;
  bool ground = is_ground_unit(punit);

  CHECK_UNIT(punit);

  generate_warmap(map_get_city(punit->x, punit->y), punit);
  players_iterate(aplayer) {
    if (pplayers_allied(pplayer,aplayer)) {
      city_list_iterate(aplayer->cities, pcity) {
        if (ground) {
          cur = warmap.cost[pcity->x][pcity->y];
          if (city_got_building(pcity, B_BARRACKS)
              || city_got_building(pcity, B_BARRACKS2)
              || city_got_building(pcity, B_BARRACKS3)) {
            cur /= 3;
          }
        } else {
          cur = warmap.seacost[pcity->x][pcity->y];
          if (city_got_building(pcity, B_PORT)) {
            cur /= 3;
          }
        }
        if (cur < best) {
          best = cur;
          acity = pcity;
        }
      } city_list_iterate_end;
    }
  } players_iterate_end;
  if (best > 6 * THRESHOLD) {
    return NULL;
  }
  return acity;
}

/*************************************************************************
  This does the attack until we have used up all our movement, unless we
  should safeguard a city. First we rampage on adjacent tiles, then we go
  looking for trouble elsewhere. If there is nothing to kill, sailing units 
  go home, others explore while barbs go berserk.
**************************************************************************/
static void ai_military_attack(struct player *pplayer, struct unit *punit)
{
  int dest_x, dest_y; 
  int id = punit->id;
  int ct = 10;
  struct city *pcity = NULL;

  CHECK_UNIT(punit);

  /* Main attack loop */
  do {
    /* First find easy adjacent enemies; 2 is better than pillage */
    if (!ai_military_rampage(punit, 2)) {
      return; /* we died */
    }

    if (stay_and_defend(punit)) {
      /* This city needs defending, don't go outside! */
      UNIT_LOG(LOG_DEBUG, punit, "stayed to defend %s", 
               map_get_city(punit->x, punit->y)->name);
      return;
    }

    /* Then find enemies the hard way */
    find_something_to_kill(pplayer, punit, &dest_x, &dest_y);
    if (!same_pos(punit->x, punit->y, dest_x, dest_y)) {
     int repeat;

     for(repeat = 0; repeat < 2; repeat++) {

      if (!is_tiles_adjacent(punit->x, punit->y, dest_x, dest_y)
          || !can_unit_attack_tile(punit, dest_x, dest_y)
          || (could_unit_move_to_tile(punit, dest_x, dest_y) == 0)) {
        /* Can't attack or move usually means we are adjacent but
         * on a ferry. This fixes the problem (usually). */
        UNIT_LOG(LOG_DEBUG, punit, "mil att gothere -> %d, %d", 
                 dest_x, dest_y);
        if (ai_military_gothere(pplayer, punit, dest_x, dest_y) <= 0) {
          /* Died or got stuck */
          return;
        }
      } else {
        /* Close combat. fstk sometimes want us to attack an adjacent
         * enemy that rampage wouldn't */
        UNIT_LOG(LOG_DEBUG, punit, "mil att bash -> %d, %d", dest_x, dest_y);
        if (!ai_unit_attack(punit, dest_x, dest_y)) {
          /* Died */
          return;
        }
      }

     } /* for-loop */
    } else {
      /* FIXME: This happens a bit too often! */
      UNIT_LOG(LOG_DEBUG, punit, "fstk didn't find us a worthy target!");
      /* No worthy enemies found, so abort loop */
      ct = 0;
    }

    ct--; /* infinite loops from railroads must be stopped */
  } while (punit->moves_left > 0 && ct > 0);

  /* Cleanup phase */
  if (punit->moves_left == 0) {
    return;
  }
  pcity = find_nearest_safe_city(punit);
  if (is_sailing_unit(punit) && pcity) {
    /* Sail somewhere */
    (void) ai_unit_goto(punit, pcity->x, pcity->y);
  } else if (!is_barbarian(pplayer)) {
    /* Nothing else to do. Worst case, this function
       will send us back home */
    (void) ai_manage_explorer(punit);
  } else {
    /* You can still have some moves left here, but barbarians should
       not sit helplessly, but advance towards nearest known enemy city */
    struct city *pc;
    int fx, fy;
    if ((pc = dist_nearest_city(pplayer, punit->x, punit->y, FALSE, TRUE))) {
      if (!is_ocean(map_get_terrain(punit->x, punit->y))) {
        UNIT_LOG(LOG_DEBUG, punit, "Barbarian marching to conquer %s", pc->name);
        ai_military_gothere(pplayer, punit, pc->x, pc->y);
      } else {
        /* sometimes find_beachhead is not enough */
        if (find_beachhead(punit, pc->x, pc->y, &fx, &fy) == 0) {
          find_city_beach(pc, punit, &fx, &fy);
          freelog(LOG_DEBUG, "Barbarian sailing to city");
          ai_military_gothere(pplayer, punit, fx, fy);
       }
      }
    }
  }
  if ((punit = find_unit_by_id(id)) && punit->moves_left > 0) {
    struct city *pcity = map_get_city(punit->x, punit->y);
    if (pcity && pcity->id == punit->homecity) {
      /* We're needlessly idle in our homecity */
      UNIT_LOG(LOG_DEBUG, punit, "fstk could not find work for me!");
    }
  }
}

/*************************************************************************
  Use caravans for building wonders, or send caravans to establish
  trade with a city on the same continent, owned by yourself or an
  ally.
**************************************************************************/
static void ai_manage_caravan(struct player *pplayer, struct unit *punit)
{
  struct city *pcity;
  struct packet_unit_request req;
  int tradeval, best_city = -1, best=0;

  CHECK_UNIT(punit);

  if (punit->ai.ai_role == AIUNIT_NONE) {
    if ((pcity = wonder_on_continent(pplayer, 
                             map_get_continent(punit->x, punit->y, NULL))) 
        && unit_flag(punit, F_HELP_WONDER)
        && build_points_left(pcity) > (pcity->shield_surplus * 2)) {
      if (!same_pos(pcity->x, pcity->y, punit->x, punit->y)) {
        if (punit->moves_left == 0) {
          return;
        }
        (void) ai_unit_goto(punit, pcity->x, pcity->y);
      } else {
        /*
         * We really don't want to just drop all caravans in immediately.
         * Instead, we want to aggregate enough caravans to build instantly.
         * -AJS, 990704
         */
        req.unit_id = punit->id;
        req.city_id = pcity->id;
        handle_unit_help_build_wonder(pplayer, &req);
      }
    } else {
       /* A caravan without a home?  Kinda strange, but it might happen.  */
       pcity=player_find_city_by_id(pplayer, punit->homecity);
       players_iterate(aplayer) {
         if (pplayers_at_war(pplayer, aplayer)) continue;
         city_list_iterate(pplayer->cities,pdest) {
           if (pcity && can_establish_trade_route(pcity, pdest)
               && map_get_continent(pcity->x, pcity->y, NULL) 
                    == map_get_continent(pdest->x, pdest->y, NULL)) {
             tradeval=trade_between_cities(pcity, pdest);
             if (tradeval != 0) {
               if (best < tradeval) {
                 best=tradeval;
                 best_city=pdest->id;
               }
             }
           }
         } city_list_iterate_end;
       } players_iterate_end;
       pcity = player_find_city_by_id(pplayer, best_city);

       if (pcity) {
         if (!same_pos(pcity->x, pcity->y, punit->x, punit->y)) {
           if (punit->moves_left == 0) {
             return;
           }
           (void) ai_unit_goto(punit, pcity->x, pcity->y);
         } else {
           req.unit_id = punit->id;
           req.city_id = pcity->id;
           (void) handle_unit_establish_trade(pplayer, &req);
        }
      }
    }
  }
}

/**************************************************************************
This seems to manage the ferryboat. When it carries units on their way
to invade something, it goes there. If it carries other units, it returns home.
When empty, it tries to find some units to carry or goes home or explores.
Military units handled by ai_manage_military()
**************************************************************************/
static void ai_manage_ferryboat(struct player *pplayer, struct unit *punit)
{ /* It's about 12 feet square and has a capacity of almost 1000 pounds.
     It is well constructed of teak, and looks seaworthy. */
  struct city *pcity;
  struct city *port = NULL;
  struct unit *bodyguard;
  struct unit_type *punittype = get_unit_type(punit->type);
  int best = 4 * punittype->move_rate, x = punit->x, y = punit->y;
  int n = 0, p = 0;

  CHECK_UNIT(punit);

  if (!unit_list_find(&map_get_tile(punit->x, punit->y)->units, punit->ai.passenger))
    punit->ai.passenger = 0;

  unit_list_iterate(map_get_tile(punit->x, punit->y)->units, aunit)
    if (punit->owner != aunit->owner) {
      return;
    }
    if (aunit->ai.ferryboat == punit->id) {
      if (punit->ai.passenger == 0) punit->ai.passenger = aunit->id; /* oops */
      p++;
      bodyguard = unit_list_find(&map_get_tile(punit->x, punit->y)->units, aunit->ai.bodyguard);
      pcity = map_get_city(aunit->goto_dest_x, aunit->goto_dest_y);
      if (aunit->ai.bodyguard == BODYGUARD_NONE || bodyguard ||
         (pcity && pcity->ai.invasion >= 2)) {
	if (pcity) {
	  UNIT_LOG(LOG_DEBUG, punit, "Ferrying to %s to %s, invasion = %d, body = %d",
		  unit_name(aunit->type), pcity->name,
		  pcity->ai.invasion, aunit->ai.bodyguard);
	}
        n++;
        handle_unit_activity_request(aunit, ACTIVITY_SENTRY);
        if (bodyguard) {
          handle_unit_activity_request(bodyguard, ACTIVITY_SENTRY);
        }
      }
    }
  unit_list_iterate_end;

  /* we try to recover hitpoints if we are in a city, before we leave */
  if (punit->hp < punittype->hp 
      && (pcity = map_get_city(punit->x, punit->y))) {
    /* Don't do anything, just wait in the city */
    UNIT_LOG(LOG_DEBUG, punit, "waiting in %s to recover hitpoints "
            "before ferrying", pcity->name);
    return;
  }

  if (p != 0) {
    freelog(LOG_DEBUG, "%s#%d@(%d,%d), p=%d, n=%d",
		  unit_name(punit->type), punit->id, punit->x, punit->y, p, n);
    if (punit->moves_left > 0 && n != 0) {
      (void) ai_unit_gothere(punit);
    } else if (n == 0 && !map_get_city(punit->x, punit->y)) { /* rest in a city, for unhap */
      x = punit->goto_dest_x; y = punit->goto_dest_y;
      port = find_nearest_safe_city(punit);
      if (port && !ai_unit_goto(punit, port->x, port->y)) {
        return; /* oops! */
      }
      punit->goto_dest_x = x; punit->goto_dest_y = y;
      send_unit_info(pplayer, punit); /* to get the crosshairs right -- Syela */
    } else {
      UNIT_LOG(LOG_DEBUG, punit, "Ferryboat %d@(%d,%d) stalling.",
		    punit->id, punit->x, punit->y);
      if(is_barbarian(pplayer)) /* just in case */
        (void) ai_manage_explorer(punit);
    }
    return;
  }

  /* check if barbarian boat is empty and so not needed - the crew has landed */
  if( is_barbarian(pplayer) && unit_list_size(&map_get_tile(punit->x, punit->y)->units)<2 ) {
    wipe_unit(punit);
    return;
  }

  /* ok, not carrying anyone, even the ferryman */
  punit->ai.passenger = 0;
  UNIT_LOG(LOG_DEBUG, punit, "Ferryboat is lonely.");
  handle_unit_activity_request(punit, ACTIVITY_IDLE);
  punit->goto_dest_x = 0; /* FIXME: -1 */
  punit->goto_dest_y = 0; /* FIXME: -1 */

  if (IS_ATTACKER(punit)) {
    if (punit->moves_left > 0) ai_manage_military(pplayer, punit);
    return;
  } /* AI used to build frigates to attack and then use them as ferries -- Syela */

  /*** Find work ***/
  CHECK_UNIT(punit);

  generate_warmap(map_get_city(punit->x, punit->y), punit);
  p = 0; /* yes, I know it's already zero. -- Syela */
  best = 9999;
  x = -1; y = -1;
  unit_list_iterate(pplayer->units, aunit) {
    if (aunit->ai.ferryboat != 0 && warmap.seacost[aunit->x][aunit->y] < best &&
	ground_unit_transporter_capacity(aunit->x, aunit->y, pplayer) <= 0
        && is_at_coast(aunit->x, aunit->y)) {
      UNIT_LOG(LOG_DEBUG, punit, "Found a potential pickup %d@(%d, %d)",
		    aunit->id, aunit->x, aunit->y);
      x = aunit->x;
      y = aunit->y;
      best = warmap.seacost[x][y];
    }
    if (is_sailing_unit(aunit)
	&& is_ocean(map_get_terrain(aunit->x, aunit->y))) {
      p++;
    }
  } unit_list_iterate_end;
  if (best < 4 * unit_type(punit)->move_rate) {
    /* Pickup is within 4 turns to grab, so move it! */
    punit->goto_dest_x = x;
    punit->goto_dest_y = y;
    UNIT_LOG(LOG_DEBUG, punit, "Found a friend and going to him @(%d, %d)",
             x, y);
    (void) ai_unit_gothere(punit);
    return;
  }

  /* do cool stuff here */
  CHECK_UNIT(punit);

  if (punit->moves_left == 0) return;
  pcity = find_city_by_id(punit->homecity);
  if (pcity) {
    if (!ai_handicap(pplayer, H_TARGETS) ||
        unit_move_turns(punit, pcity->x, pcity->y) < p) {
      punit->goto_dest_x = pcity->x;
      punit->goto_dest_y = pcity->y;
      UNIT_LOG(LOG_DEBUG, punit, "No friends.  Going home.");
      (void) ai_unit_gothere(punit);
      return;
    }
  }
  if (is_ocean(map_get_terrain(punit->x, punit->y))) {
    /* thanks, Tony */
    (void) ai_manage_explorer(punit);
  }
  return;
}

/*************************************************************************
 This function goes wait a unit in a city for the hitpoints to recover. 
 If something is attacking our city, kill it yeahhh!!!.
**************************************************************************/
static void ai_manage_hitpoint_recovery(struct unit *punit)
{
  struct player *pplayer = unit_owner(punit);
  struct city *pcity = map_get_city(punit->x, punit->y);
  struct city *safe = NULL;
  struct unit_type *punittype = get_unit_type(punit->type);

  CHECK_UNIT(punit);

  if (pcity) {
    /* rest in city until the hitpoints are recovered, but attempt
       to protect city from attack */
    if (ai_military_rampage(punit, 2)) {
      freelog(LOG_DEBUG, "%s's %s(%d) at (%d, %d) recovering hit points.",
              pplayer->name, unit_type(punit)->name, punit->id, punit->x,
              punit->y);
    } else {
      return; /* we died heroically defending our city */
    }
  } else {
    /* goto to nearest city to recover hit points */
    /* just before, check to see if we can occupy at enemy city undefended */
    if (!ai_military_rampage(punit, 99999)) { 
      return; /* oops, we died */
    }

    /* find city to stay and go there */
    safe = find_nearest_safe_city(punit);
    if (safe) {
      UNIT_LOG(LOGLEVEL_RECOVERY, punit, "going to %s to recover", safe->name);
      if (!ai_unit_goto(punit, safe->x, safe->y)) {
        freelog(LOGLEVEL_RECOVERY, "died trying to hide and recover");
        return;
      }
    } else {
      /* oops */
      UNIT_LOG(LOGLEVEL_RECOVERY, punit, "didn't find a city to recover in!");
      ai_unit_new_role(punit, AIUNIT_NONE, -1, -1);
      ai_military_attack(pplayer, punit);
      return;
    }
  }

  /* is the unit still damaged? if true recover hit points, if not idle */
  if (punit->hp == punittype->hp) {
    /* we are ready to go out and kick ass again */
    UNIT_LOG(LOGLEVEL_RECOVERY, punit, "ready to kick ass again!");
    ai_unit_new_role(punit, AIUNIT_NONE, -1, -1);  
    return;
  }
}

/**************************************************************************
decides what to do with a military unit.
**************************************************************************/
static void ai_manage_military(struct player *pplayer, struct unit *punit)
{
  int id = punit->id;

  CHECK_UNIT(punit);

  /* was getting a bad bug where a settlers caused a defender to leave home */
  /* and then all other supported units went on DEFEND_HOME/goto */
  ai_military_findjob(pplayer, punit);

  switch (punit->ai.ai_role) {
  case AIUNIT_AUTO_SETTLER:
  case AIUNIT_BUILD_CITY:
    ai_unit_new_role(punit, AIUNIT_NONE, -1, -1);
    break;
  case AIUNIT_DEFEND_HOME:
    ai_military_gohome(pplayer, punit);
    break;
  case AIUNIT_ATTACK:
    ai_military_attack(pplayer, punit);
    break;
  case AIUNIT_FORTIFY:
    ai_military_gohome(pplayer, punit);
    break;
  case AIUNIT_RUNAWAY: 
    break;
  case AIUNIT_ESCORT: 
    ai_military_bodyguard(pplayer, punit);
    break;
  case AIUNIT_PILLAGE:
    handle_unit_activity_request(punit, ACTIVITY_PILLAGE);
    return; /* when you pillage, you have moves left, avoid later fortify */
  case AIUNIT_EXPLORE:
    (void) ai_manage_explorer(punit);
    break;
  case AIUNIT_RECOVER:
    ai_manage_hitpoint_recovery(punit);
    break;
  default:
    assert(FALSE);
  }

  /* If we are still alive, either sentry or fortify. */
  if ((punit = find_unit_by_id(id))) {
    if (unit_list_find(&(map_get_tile(punit->x, punit->y)->units),
        punit->ai.ferryboat)) {
      handle_unit_activity_request(punit, ACTIVITY_SENTRY);
    } else if (punit->activity == ACTIVITY_IDLE) {
      handle_unit_activity_request(punit, ACTIVITY_FORTIFYING);
    }
  }
}

/**************************************************************************
  Barbarian units may disband spontaneously if their age is more then 5,
  they are not in cities, and they are far from any enemy units. It is to 
  remove barbarians that do not engage into any activity for a long time.
**************************************************************************/
static bool unit_can_be_retired(struct unit *punit)
{
  if (punit->fuel > 0) {	/* fuel abused for barbarian life span */
    punit->fuel--;
    return FALSE;
  }

  if (is_allied_city_tile
      (map_get_tile(punit->x, punit->y), unit_owner(punit))) return FALSE;

  /* check if there is enemy nearby */
  square_iterate(punit->x, punit->y, 3, x, y) {
    if (is_enemy_city_tile(map_get_tile(x, y), unit_owner(punit)) ||
	is_enemy_unit_tile(map_get_tile(x, y), unit_owner(punit)))
      return FALSE;
  }
  square_iterate_end;

  return TRUE;
}

/**************************************************************************
 manage one unit
 Careful: punit may have been destroyed upon return from this routine!

 Gregor: This function is a very limited approach because if a unit has
 several flags the first one in order of appearance in this function
 will be used.
**************************************************************************/
static void ai_manage_unit(struct player *pplayer, struct unit *punit)
{
  CHECK_UNIT(punit);

  /* retire useless barbarian units here, before calling the management
     function */
  if( is_barbarian(pplayer) ) {
    /* Todo: should be configurable */
    if( unit_can_be_retired(punit) && myrand(100) > 90 ) {
      wipe_unit(punit);
      return;
    }
    if( !is_military_unit(punit)
	&& !unit_has_role(punit->type, L_BARBARIAN_LEADER)) {
      freelog(LOG_VERBOSE, "Barbarians picked up non-military unit.");
      return;
    }
  }

  if ((unit_flag(punit, F_DIPLOMAT))
      || (unit_flag(punit, F_SPY))) {
    ai_manage_diplomat(pplayer, punit);
    return;
  } else if (unit_flag(punit, F_SETTLERS)
	     ||unit_flag(punit, F_CITIES)) {
    if (punit->moves_left == 0) return; /* can't do anything with no moves */
    ai_manage_settler(pplayer, punit);
    return;
  } else if (unit_flag(punit, F_TRADE_ROUTE)
             || unit_flag(punit, F_HELP_WONDER)) {
    ai_manage_caravan(pplayer, punit);
    return;
  } else if (unit_has_role(punit->type, L_BARBARIAN_LEADER)) {
    ai_manage_barbarian_leader(pplayer, punit);
    return;
  } else if (get_transporter_capacity(punit) > 0) {
    ai_manage_ferryboat(pplayer, punit);
    return;
  } else if (is_air_unit(punit)) {
    ai_manage_airunit(pplayer, punit);
    return;
  } else if (is_military_unit(punit)) {
    if (punit->moves_left == 0) return; /* can't do anything with no moves */
    ai_manage_military(pplayer,punit); 
    return;
  } else {
    if (punit->moves_left == 0) return; /* can't do anything with no moves */
    (void) ai_manage_explorer(punit); /* what else could this be? -- Syela */
    return;
  }
}

/**************************************************************************
  Master manage unit function.
**************************************************************************/
void ai_manage_units(struct player *pplayer) 
{
  unit_list_iterate_safe(pplayer->units, punit) {
    ai_manage_unit(pplayer, punit);
  } unit_list_iterate_safe_end;
}

/**************************************************************************
 Assign tech wants for techs to get better units with given role/flag.
 Returns the best we can build so far, or U_LAST if none.  (dwp)
**************************************************************************/
Unit_Type_id ai_wants_role_unit(struct player *pplayer, struct city *pcity,
                                int role, int want)
{
  Unit_Type_id iunit;
  Tech_Type_id itech;
  int i, n;

  n = num_role_units(role);
  for (i=n-1; i>=0; i--) {
    iunit = get_role_unit(role, i);
    if (can_build_unit(pcity, iunit)) {
      return iunit;
    } else {
      /* careful; might be unable to build for non-tech reason... */
      itech = get_unit_type(iunit)->tech_requirement;
      if (get_invention(pplayer, itech) != TECH_KNOWN) {
	pplayer->ai.tech_want[itech] += want;
      }
    }
  }
  return U_LAST;
}

/**************************************************************************
 As ai_wants_role_unit, but also set choice->choice if we can build something.
**************************************************************************/
void ai_choose_role_unit(struct player *pplayer, struct city *pcity,
			 struct ai_choice *choice, int role, int want)
{
  Unit_Type_id iunit = ai_wants_role_unit(pplayer, pcity, role, want);
  if (iunit != U_LAST)
    choice->choice = iunit;
}

/**************************************************************************
 Whether unit_type test is on the "upgrade path" of unit_type base,
 even if we can't upgrade now.
**************************************************************************/
bool is_on_unit_upgrade_path(Unit_Type_id test, Unit_Type_id base)
{
  /* This is the real function: */
  do {
    base = unit_types[base].obsoleted_by;
    if (base == test) {
      return TRUE;
    }
  } while (base != -1);
  return FALSE;
}

/*************************************************************************
Barbarian leader tries to stack with other barbarian units, and if it's
not possible it runs away. When on coast, it may disappear with 33% chance.
**************************************************************************/
static void ai_manage_barbarian_leader(struct player *pplayer, struct unit *leader)
{
  int con = map_get_continent(leader->x, leader->y, NULL);
  int safest = 0, safest_x = leader->x, safest_y = leader->y;
  struct unit *closest_unit = NULL;
  int dist, mindist = 10000;

  CHECK_UNIT(leader);

  if (leader->moves_left == 0 || 
      (!is_ocean(map_get_terrain(leader->x, leader->y)) &&
       unit_list_size(&(map_get_tile(leader->x, leader->y)->units)) > 1) ) {
      handle_unit_activity_request(leader, ACTIVITY_SENTRY);
      return;
  }
  /* the following takes much CPU time and could be avoided */
  generate_warmap(map_get_city(leader->x, leader->y), leader);

  /* duck under own units */
  unit_list_iterate(pplayer->units, aunit) {
    if (unit_has_role(aunit->type, L_BARBARIAN_LEADER)
	|| !is_ground_unit(aunit)
	|| map_get_continent(aunit->x, aunit->y, NULL) != con)
      continue;

    if (warmap.cost[aunit->x][aunit->y] < mindist) {
      mindist = warmap.cost[aunit->x][aunit->y];
      closest_unit = aunit;
    }
  } unit_list_iterate_end;

  if (closest_unit
      && !same_pos(closest_unit->x, closest_unit->y, leader->x, leader->y)
      && (map_get_continent(leader->x, leader->y, NULL)
          == map_get_continent(closest_unit->x, closest_unit->y, NULL))) {
    (void) ai_unit_goto(leader, closest_unit->x, closest_unit->y);
    return; /* sticks better to own units with this -- jk */
  }

  freelog(LOG_DEBUG, "Barbarian leader needs to flee");
  mindist = 1000000;
  closest_unit = NULL;

  players_iterate(other_player) {
    unit_list_iterate(other_player->units, aunit) {
      if (is_military_unit(aunit)
	  && is_ground_unit(aunit)
	  && map_get_continent(aunit->x, aunit->y, NULL) == con) {
	/* questionable assumption: aunit needs as many moves to reach us as we
	   need to reach it */
	dist = warmap.cost[aunit->x][aunit->y] - unit_move_rate(aunit);
	if (dist < mindist) {
	  freelog(LOG_DEBUG, "Barbarian leader: closest enemy is %s at %d, %d, dist %d",
                  unit_name(aunit->type), aunit->x, aunit->y, dist);
	  mindist = dist;
	  closest_unit = aunit;
	}
      }
    } unit_list_iterate_end;
  } players_iterate_end;

  /* Disappearance - 33% chance on coast, when older than barbarian life span */
  if (is_at_coast(leader->x, leader->y) && leader->fuel == 0) {
    if(myrand(3) == 0) {
      freelog(LOG_DEBUG, "Barbarian leader disappeared at %d %d", leader->x, leader->y);
      wipe_unit(leader);
      return;
    }
  }

  if (!closest_unit) {
    handle_unit_activity_request(leader, ACTIVITY_IDLE);
    freelog(LOG_DEBUG, "Barbarian leader: No enemy.");
    return;
  }

  generate_warmap(map_get_city(closest_unit->x, closest_unit->y), closest_unit);

  do {
    int last_x, last_y;
    freelog(LOG_DEBUG, "Barbarian leader: moves left: %d\n",
	    leader->moves_left);

    square_iterate(leader->x, leader->y, 1, x, y) {
      if (warmap.cost[x][y] > safest
	  && could_unit_move_to_tile(leader, x, y) == 1) {
	safest = warmap.cost[x][y];
	freelog(LOG_DEBUG,
		"Barbarian leader: safest is %d, %d, safeness %d", x, y,
		safest);
	safest_x = x;
	safest_y = y;
      }
    } 
    square_iterate_end;

    if (same_pos(leader->x, leader->y, safest_x, safest_y)) {
      freelog(LOG_DEBUG, "Barbarian leader reached the safest position.");
      handle_unit_activity_request(leader, ACTIVITY_IDLE);
      return;
    }

    freelog(LOG_DEBUG, "Fleeing to %d, %d.", safest_x, safest_y);
    last_x = leader->x;
    last_y = leader->y;
    (void) ai_unit_goto(leader, safest_x, safest_y);
    if (same_pos(leader->x, leader->y, last_x, last_y)) {
      /* Deep inside the goto handling code, in 
	 server/unithand.c::handle_unite_move_request(), the server
	 may decide that a unit is better off not moving this turn,
	 because the unit doesn't have quite enough movement points
	 remaining.  Unfortunately for us, this favor that the server
	 code does may lead to an endless loop here in the barbarian
	 leader code:  the BL will try to flee to a new location, execute 
	 the goto, find that it's still at its present (unsafe) location,
	 and repeat.  To break this loop, we test for the condition
	 where the goto doesn't do anything, and break if this is
	 the case. */
      break;
    }
  } while (leader->moves_left > 0);
}

/*************************************************************************
  Updates the global array simple_ai_types.
**************************************************************************/
void update_simple_ai_types(void)
{
  int i = 0;

  unit_type_iterate(id) {
    if (unit_type_exists(id) && !unit_type_flag(id, F_NONMIL)
	&& !unit_type_flag(id, F_MISSILE)
	&& !unit_type_flag(id, F_NO_LAND_ATTACK)
        && get_unit_type(id)->move_type != AIR_MOVING
	&& get_unit_type(id)->transport_capacity < 8) {
      simple_ai_types[i] = id;
      i++;
    }
  } unit_type_iterate_end;

  simple_ai_types[i] = U_LAST;
}
