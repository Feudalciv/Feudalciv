/**********************************************************************
 Freeciv - Copyright (C) 2003 - The Freeciv Team
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
#include "map.h"
#include "movement.h"
#include "log.h"
#include "pf_tools.h"
#include "player.h"
#include "unit.h"

#include "citytools.h"
#include "settlers.h"
#include "unittools.h"

#include "aidata.h"
#include "ailog.h"
#include "aitools.h"
#include "aiunit.h"

#include "aihunt.h"

/**************************************************************************
  We don't need a hunter in this city if we already have one. Return 
  existing hunter if any.
**************************************************************************/
static struct unit *ai_hunter_find(struct player *pplayer, 
                                   struct city *pcity)
{
  unit_list_iterate(pcity->units_supported, punit) {
    if (ai_hunter_qualify(pplayer, punit)) {
      return punit;
    }
  } unit_list_iterate_end;
  unit_list_iterate(pcity->tile->units, punit) {
    if (ai_hunter_qualify(pplayer, punit)) {
      return punit;
    }
  } unit_list_iterate_end;

  return NULL;
}

/**************************************************************************
  Guess best hunter unit type.
**************************************************************************/
static struct unit_type *ai_hunter_guess_best(struct city *pcity,
					      enum unit_move_type umt)
{
  struct unit_type *bestid = NULL;
  int best = 0;

  unit_type_iterate(ut) {
    int desire;

    if (ut->move_type != umt || !can_build_unit(pcity, ut)
        || ut->attack_strength < ut->transport_capacity) {
      continue;
    }

    desire = (ut->hp
              * ut->attack_strength
              * ut->firepower
              * ut->move_rate
              + ut->defense_strength) / MAX(UNITTYPE_COSTS(ut), 1);

    if (unit_type_flag(ut, F_CARRIER)
        || unit_type_flag(ut, F_MISSILE_CARRIER)) {
      desire += desire / 6;
    }
    if (unit_type_flag(ut, F_IGTER)) {
      desire += desire / 2;
    }
    if (unit_type_flag(ut, F_IGTIRED)) {
      desire += desire / 8;
    }
    if (unit_type_flag(ut, F_PARTIAL_INVIS)) {
      desire += desire / 4;
    }
    if (unit_type_flag(ut, F_NO_LAND_ATTACK)) {
      desire -= desire / 4; /* less flexibility */
    }
    /* Causes continual unhappiness */
    if (unit_type_flag(ut, F_FIELDUNIT)) {
      desire /= 2;
    }

    desire = amortize(desire,
		      (unit_build_shield_cost(ut)
		       / MAX(pcity->surplus[O_SHIELD], 1)));

    if (desire > best) {
        best = desire;
        bestid = ut;
    }
  } unit_type_iterate_end;

  return bestid;
}

/**************************************************************************
  Check if we want to build a missile for our hunter.
**************************************************************************/
static void ai_hunter_missile_want(struct player *pplayer,
                                   struct city *pcity,
                                   struct ai_choice *choice)
{
  int best = -1;
  struct unit_type *best_unit_type = NULL;
  bool have_hunter = FALSE;

  unit_list_iterate(pcity->tile->units, punit) {
    if (ai_hunter_qualify(pplayer, punit)
        && (unit_flag(punit, F_MISSILE_CARRIER)
            || unit_flag(punit, F_CARRIER))) {
      /* There is a potential hunter in our city which we can equip 
       * with a missile. Do it. */
      have_hunter = TRUE;
      break;
    }
  } unit_list_iterate_end;

  if (!have_hunter) {
    return;
  }

  unit_type_iterate(ut) {
    int desire;

    if (!BV_ISSET(ut->flags, F_MISSILE) || !can_build_unit(pcity, ut)) {
      continue;
    }

    /* FIXME: We need to store some data that can tell us if
     * enemy transports are protected by anti-missile technology. 
     * In this case, want nuclear much more! */
    desire = (ut->hp
              * MIN(ut->attack_strength, 30) /* nuke fix */
              * ut->firepower
              * ut->move_rate) / UNITTYPE_COSTS(ut) + 1;

    /* Causes continual unhappiness */
    if (unit_type_flag(ut, F_FIELDUNIT)) {
      desire /= 2;
    }

    desire = amortize(desire,
		      (unit_build_shield_cost(ut)
		       / MAX(pcity->surplus[O_SHIELD], 1)));

    if (desire > best) {
        best = desire;
        best_unit_type = ut;
    }
  } unit_type_iterate_end;

  if (best > choice->want) {
    CITY_LOG(LOGLEVEL_HUNT, pcity, "pri missile w/ want %d", best);
    choice->choice = best_unit_type->index;
    choice->want = best;
    choice->type = CT_ATTACKER;
  } else if (best != -1) {
    CITY_LOG(LOGLEVEL_HUNT, pcity, "not pri missile w/ want %d"
             "(old want %d)", best, choice->want);
  }
}

/**************************************************************************
  Support function for ai_hunter_choice()
**************************************************************************/
static void eval_hunter_want(struct player *pplayer, struct city *pcity,
                             struct ai_choice *choice,
			     struct unit_type *best_type,
                             int veteran)
{
  struct unit *virtualunit;
  int want = 0;

  virtualunit = create_unit_virtual(pplayer, pcity, best_type, veteran);
  want = ai_hunter_manage(pplayer, virtualunit);
  destroy_unit_virtual(virtualunit);
  if (want > choice->want) {
    CITY_LOG(LOGLEVEL_HUNT, pcity, "pri hunter w/ want %d", want);
    choice->choice = best_type->index;
    choice->want = want;
    choice->type = CT_ATTACKER;
  }
}

/**************************************************************************
  Check if we want to build a hunter.
**************************************************************************/
void ai_hunter_choice(struct player *pplayer, struct city *pcity,
                      struct ai_choice *choice)
{
  struct unit_type *best_land_hunter
    = ai_hunter_guess_best(pcity, LAND_MOVING);
  struct unit_type *best_sea_hunter
    = ai_hunter_guess_best(pcity, SEA_MOVING);
  struct unit *hunter = ai_hunter_find(pplayer, pcity);

  if ((!best_land_hunter && !best_sea_hunter)
      || is_barbarian(pplayer) || !pplayer->is_alive
      || ai_handicap(pplayer, H_TARGETS)) {
    return; /* None available */
  }
  if (hunter) {
    /* Maybe want missiles to go with a hunter instead? */
    ai_hunter_missile_want(pplayer, pcity, choice);
    return;
  }

  if (best_sea_hunter) {
    eval_hunter_want(pplayer, pcity, choice, best_sea_hunter, 
                     do_make_unit_veteran(pcity, best_sea_hunter));
  }
  if (best_land_hunter) {
    eval_hunter_want(pplayer, pcity, choice, best_land_hunter, 
                     do_make_unit_veteran(pcity, best_land_hunter));
  }
}

/**************************************************************************
  Does this unit qualify as a hunter?
**************************************************************************/
bool ai_hunter_qualify(struct player *pplayer, struct unit *punit)
{
  if (is_barbarian(pplayer) || punit->owner != pplayer) {
    return FALSE;
  }
  if (unit_has_role(punit->type, L_HUNTER)) {
    return TRUE;
  }
  return FALSE;
}

/**************************************************************************
  Try to shoot our target with a missile. Also shoot down anything that
  might attempt to intercept _us_. We assign missiles to a hunter in
  ai_unit_new_role().
**************************************************************************/
static void ai_hunter_try_launch(struct player *pplayer,
                                 struct unit *punit,
                                 struct unit *target)
{
  int target_sanity = target->id;
  struct pf_parameter parameter;
  struct pf_map *map;

  unit_list_iterate(punit->tile->units, missile) {
    struct unit *sucker = NULL;

    if (missile->owner == pplayer && unit_flag(missile, F_MISSILE)) {
      UNIT_LOG(LOGLEVEL_HUNT, missile, "checking for hunt targets");
      pft_fill_unit_parameter(&parameter, punit);
      map = pf_create_map(&parameter);

      pf_iterator(map, pos) {
        if (pos.total_MC > missile->moves_left / SINGLE_MOVE) {
          break;
        }
        if (tile_get_city(pos.tile)
            || !can_unit_attack_tile(punit, pos.tile)) {
          continue;
        }
        unit_list_iterate(pos.tile->units, victim) {
          struct unit_type *ut = unit_type(victim);
          enum diplstate_type ds = pplayer_get_diplstate(pplayer, 
                                                         unit_owner(victim))->type;

          if (ds != DS_WAR) {
            continue;
          }
          if (victim == target) {
            sucker = victim;
            UNIT_LOG(LOGLEVEL_HUNT, missile, "found primary target %d(%d, %d)"
                     " dist %d", victim->id, TILE_XY(victim->tile), 
                     pos.total_MC);
            break; /* Our target! Get him!!! */
          }
          if (ut->move_rate + victim->moves_left > pos.total_MC
              && ATTACK_POWER(victim) > DEFENCE_POWER(punit)
              && (ut->move_type == SEA_MOVING
                  || ut->move_type == AIR_MOVING)) {
            /* Threat to our carrier. Kill it. */
            sucker = victim;
            UNIT_LOG(LOGLEVEL_HUNT, missile, "found aux target %d(%d, %d)",
                     victim->id, TILE_XY(victim->tile));
            break;
          }
        } unit_list_iterate_end;
        if (sucker) {
          break; /* found something - kill it! */
        }
      } pf_iterator_end;
      pf_destroy_map(map);
      if (sucker) {
        if (find_unit_by_id(missile->transported_by)) {
          unload_unit_from_transporter(missile);
        }
        ai_unit_goto(missile, sucker->tile);
        sucker = find_unit_by_id(target_sanity); /* Sanity */
        if (sucker && is_tiles_adjacent(sucker->tile, missile->tile)) {
          ai_unit_attack(missile, sucker->tile);
        }
        target = find_unit_by_id(target_sanity); /* Sanity */
        break; /* try next missile, if any */
      }
    } /* if */
  } unit_list_iterate_end;
}

/**************************************************************************
  Calculate desire to crush this target.
**************************************************************************/
static void ai_hunter_juiciness(struct player *pplayer, struct unit *punit,
                                struct unit *target, int *stackthreat,
                                int *stackcost)
{
  *stackthreat = 0;
  *stackcost = 0;

  unit_list_iterate(target->tile->units, sucker) {
    *stackthreat += ATTACK_POWER(sucker);
    if (unit_flag(sucker, F_GAMELOSS)) {
      *stackcost += 1000;
      *stackthreat += 5000;
    }
    if (unit_flag(sucker, F_DIPLOMAT)) {
      *stackthreat += 500; /* extra threatening */
    }
    *stackcost += unit_build_shield_cost(unit_type(sucker));
  } unit_list_iterate_end;

  *stackthreat *= 9; /* WAG - reduced by distance later */
  *stackthreat += *stackcost;
}

/**************************************************************************
  Manage a (possibly virtual) hunter. Return the want for building a 
  hunter like this. If we return 0, then we have nothing to do with
  the hunter. If we return -1, then we succeeded, and can try again.
  If we return > 0 then we are hunting but ran out of moves (this is
  also used for construction want).

  We try to keep track of our original target, but also opportunistically
  snatch up closer targts if they are better.

  We set punit->ai.target to target's id.
**************************************************************************/
int ai_hunter_manage(struct player *pplayer, struct unit *punit)
{
  bool is_virtual = (punit->id == 0);
  struct pf_parameter parameter;
  struct pf_map *map;
  int limit = unit_move_rate(punit) * 6;
  struct unit *original_target = find_unit_by_id(punit->ai.target);
  int original_threat = 0, original_cost = 0;

  assert(!is_barbarian(pplayer));
  assert(pplayer->is_alive);

  pft_fill_unit_parameter(&parameter, punit);
  map = pf_create_map(&parameter);

  if (original_target) {
    ai_hunter_juiciness(pplayer, punit, original_target, 
                        &original_threat, &original_cost);
  }

  pf_iterator(map, pos) {
    /* End faster if we have a target */
    if (pos.total_MC > limit) {
      UNIT_LOG(LOGLEVEL_HUNT, punit, "gave up finding hunt target");
      pf_destroy_map(map);
      return 0;
    }
    unit_list_iterate_safe(pos.tile->units, target) {
      struct player *aplayer = unit_owner(target);
      int dist1, dist2, stackthreat = 0, stackcost = 0;
      int sanity_target = target->id;

      /* Note that we need not (yet) be at war with aplayer */
      if (!is_player_dangerous(pplayer, aplayer)) {
        continue;
      }
      if (pos.tile->city
          || !can_unit_attack_tile(punit, pos.tile)
          || TEST_BIT(target->ai.hunted, pplayer->player_no)) {
        /* Can't hunt this one.  The bit is cleared in the beginning
         * of each turn. */
        continue;
      }
      if (!unit_flag(target, F_DIPLOMAT)
          && get_transporter_capacity(target) == 0
          && !unit_flag(target, F_GAMELOSS)) {
        /* Won't hunt this one. */
        continue;
      }

      /* Figure out whether unit is coming closer */
      if (target->ai.cur_pos && target->ai.prev_pos) {
        dist1 = real_map_distance(punit->tile, *target->ai.cur_pos);
        dist2 = real_map_distance(punit->tile, *target->ai.prev_pos);
      } else {
        dist1 = dist2 = 0;
      }
      UNIT_LOG(LOGLEVEL_HUNT, punit, "considering chasing %s(%d, %d) id %d "
               "dist1 %d dist2 %d",
	       unit_type(target)->name, TILE_XY(target->tile),
               target->id, dist1, dist2);

      /* We can't chase if we aren't faster or on intercept vector */
      if (unit_type(punit)->move_rate < unit_type(target)->move_rate
          && dist1 >= dist2) {
        UNIT_LOG(LOGLEVEL_HUNT, punit, "giving up racing %s (%d, %d)->(%d, %d)",
                 unit_type(target)->name,
		 target->ai.prev_pos ? (*target->ai.prev_pos)->x : -1,
                 target->ai.prev_pos ? (*target->ai.prev_pos)->y : -1,
                 TILE_XY(target->tile));
        continue;
      }

      /* Calculate juiciness of target, compare with existing target,
       * if any. */
      ai_hunter_juiciness(pplayer, punit, target, &stackthreat, &stackcost);
      stackcost *= unit_win_chance(punit, get_defender(punit, target->tile));
      if (stackcost < unit_build_shield_cost(unit_type(punit))) {
        UNIT_LOG(LOGLEVEL_HUNT, punit, "%d is too expensive (it %d vs us %d)", 
                 target->id, stackcost,
		 unit_build_shield_cost(unit_type(punit)));
        continue; /* Too expensive */
      }
      stackthreat /= pos.total_MC + 1;
      if (!is_virtual 
          && original_target != target
          && original_threat > stackthreat) {
        UNIT_LOG(LOGLEVEL_HUNT, punit, "Unit %d is not worse than %d", 
                 target->id, original_target->id);
        continue; /* The threat we found originally was worse than this! */
      }
      if (stackthreat < unit_build_shield_cost(unit_type(punit))) {
        UNIT_LOG(LOGLEVEL_HUNT, punit, "%d is not worth it", target->id);
        continue; /* Not worth it */
      }

      UNIT_LOG(LOGLEVEL_HUNT, punit, "hunting %s's %s(%d, %d) "
               "id %d with want %d, dist1 %d, dist2 %d", 
               unit_owner(target)->name, unit_type(target)->name, 
               TILE_XY(target->tile), target->id, stackthreat, dist1,
               dist2);
      /* Ok, now we FINALLY have a target worth destroying! */
      punit->ai.target = target->id;
      if (is_virtual) {
        pf_destroy_map(map);
        return stackthreat;
      }

      /* This assigns missiles to us */
      ai_unit_new_role(punit, AIUNIT_HUNTER, target->tile);

      /* Check if we can nuke it */
      ai_hunter_try_launch(pplayer, punit, target);

      /* Check if we have nuked it */
      if (target != find_unit_by_id(sanity_target)) {
        UNIT_LOG(LOGLEVEL_HUNT, punit, "mission accomplished by cargo (pre)");
        ai_unit_new_role(punit, AIUNIT_NONE, NULL);
        pf_destroy_map(map);
        return -1; /* try again */
      }

      /* Go towards it. */
      if (!ai_unit_execute_path(punit, pf_get_path(map, target->tile))) {
        pf_destroy_map(map);
        return 0;
      }

      /* Check if we can nuke it now */
      ai_hunter_try_launch(pplayer, punit, target);
      if (target != find_unit_by_id(sanity_target)) {
        UNIT_LOG(LOGLEVEL_HUNT, punit, "mission accomplished by cargo (post)");
        ai_unit_new_role(punit, AIUNIT_NONE, NULL);
        pf_destroy_map(map);
        return -1; /* try again */
      }

      pf_destroy_map(map);
      punit->ai.done = TRUE;
      return stackthreat; /* still have work to do */
    } unit_list_iterate_safe_end;
  } pf_iterator_end;

  UNIT_LOG(LOGLEVEL_HUNT, punit, "ran out of map finding hunt target");
  pf_destroy_map(map);
  return 0; /* found nothing */
}
