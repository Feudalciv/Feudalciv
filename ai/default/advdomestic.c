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

#include <string.h>
#include <math.h> /* pow */

/* utility */
#include "log.h"
#include "mem.h"

/* common */
#include "city.h"
#include "game.h"
#include "government.h"
#include "map.h"
#include "movement.h"
#include "traderoutes.h"
#include "unit.h"
#include "unitlist.h"
#include "unittype.h"

/* common/aicore */
#include "pf_tools.h"

/* server */
#include "citytools.h"
#include "unittools.h"
#include "srv_log.h"

/* server/advisors */
#include "advbuilding.h"
#include "advdata.h"
#include "autosettlers.h"
#include "infracache.h" /* adv_city */

/* ai */
#include "aitraits.h"

/* ai/default */
#include "advmilitary.h"
#include "aicity.h"
#include "aidata.h"
#include "aiplayer.h"
#include "aitech.h"
#include "aitools.h"
#include "aiunit.h"

#include "advdomestic.h"


/***************************************************************************
 * Evaluate the need for units (like caravans) that aid wonder construction.
 * If another city is building wonder and needs help but pplayer is not
 * advanced enough to build caravans, the corresponding tech will be 
 * stimulated.
 ***************************************************************************/
static void dai_choose_help_wonder(struct ai_type *ait,
                                   struct city *pcity,
                                   struct adv_choice *choice,
                                   struct adv_data *ai)
{
  struct player *pplayer = city_owner(pcity);
  Continent_id continent = tile_continent(pcity->tile);
  struct ai_city *city_data = def_ai_city_data(pcity, ait);
  /* Total count of caravans available or already being built 
   * on this continent */
  int caravans = 0;
  /* The type of the caravan */
  struct unit_type *unit_type;
  struct city *wonder_city = game_city_by_number(ai->wonder_city);

  if (num_role_units(UTYF_HELP_WONDER) == 0) {
    /* No such units available in the ruleset */
    return;
  }

  if (pcity == wonder_city 
      || wonder_city == NULL
      || city_data->distance_to_wonder_city <= 0
      || VUT_UTYPE == wonder_city->production.kind
      || !is_wonder(wonder_city->production.value.building)) {
    /* A distance of zero indicates we are very far away, possibly
     * on another continent. */
    return;
  }

  /* Count existing caravans */
  unit_list_iterate(pplayer->units, punit) {
    if (unit_has_type_flag(punit, UTYF_HELP_WONDER)
        && tile_continent(unit_tile(punit)) == continent)
      caravans++;
  } unit_list_iterate_end;

  /* Count caravans being built */
  city_list_iterate(pplayer->cities, acity) {
    if (VUT_UTYPE == acity->production.kind
        && utype_has_flag(acity->production.value.utype, UTYF_HELP_WONDER)
        && tile_continent(acity->tile) == continent) {
      caravans++;
    }
  } city_list_iterate_end;

  unit_type = best_role_unit(pcity, UTYF_HELP_WONDER);

  if (!unit_type) {
    /* We cannot build such units yet
     * but we will consider it to stimulate science */
    unit_type = get_role_unit(UTYF_HELP_WONDER, 0);
  }

  /* Check if wonder needs a little help. */
  if (build_points_left(wonder_city) 
      > utype_build_shield_cost(unit_type) * caravans) {
    struct impr_type *wonder = wonder_city->production.value.building;
    int want = wonder_city->server.adv->building_want[improvement_index(wonder)];
    int dist = city_data->distance_to_wonder_city /
               unit_type->move_rate;

    fc_assert_ret(VUT_IMPROVEMENT == wonder_city->production.kind);

    want /= MAX(dist, 1);
    CITY_LOG(LOG_DEBUG, pcity, "want %s to help wonder in %s with %d", 
             utype_rule_name(unit_type),
             city_name(wonder_city),
             want);
    if (want > choice->want) {
      /* This sets our tech want in cases where we cannot actually build
       * the unit. */
      unit_type = dai_wants_role_unit(pplayer, pcity, UTYF_HELP_WONDER, want);
      if (unit_type != NULL) {
        choice->want = want;
        choice->type = CT_CIVILIAN;
        choice->value.utype = unit_type;
      } else {
        CITY_LOG(LOG_DEBUG, pcity,
                 "would but could not build UTYF_HELP_WONDER unit, bumped reqs");
      }
    }
  }
}

/***************************************************************************
  Evaluate the need for units (like caravans) that create trade routes.
  If pplayer is not advanced enough to build caravans, the corresponding
  tech will be stimulated.
****************************************************************************/
static void dai_choose_trade_route(struct city *pcity,
                                   struct adv_choice *choice,
                                   struct adv_data *ai)
{
  struct player *pplayer = city_owner(pcity);
  struct unit_type *unit_type;
  int want;
  int income, bonus;
  int trade_routes;
  int max_routes;
  Continent_id continent = tile_continent(pcity->tile);
  bool dest_city_found = FALSE;
  bool dest_city_nat_different_cont = FALSE;
  bool dest_city_nat_same_cont = FALSE;
  bool dest_city_in_different_cont = FALSE;
  bool dest_city_in_same_cont = FALSE;
  bool prefer_different_cont;
  int pct = 0;
  int trader_trait;
  bool need_boat = FALSE;

  if (city_list_size(pplayer->cities) < 5) {
    /* Consider trade routes only if enough destination cities.
     * This is just a quick check. We make more detailed check below. */
    return;
  }

  if (num_role_units(UTYF_TRADE_ROUTE) == 0) {
    /* No such units available in the ruleset */
    return;
  }

  if (trade_route_type_trade_pct(TRT_NATIONAL_IC) >
      trade_route_type_trade_pct(TRT_NATIONAL)) {
    prefer_different_cont = TRUE;
  } else {
    prefer_different_cont = FALSE;
  }

  /* Look for proper destination city for trade. */
  if (trade_route_type_trade_pct(TRT_NATIONAL) > 0
      || trade_route_type_trade_pct(TRT_NATIONAL_IC) > 0) {
    /* National traderoutes have value */
    city_list_iterate(pplayer->cities, acity) {
      if (can_cities_trade(pcity, acity)) {
        dest_city_found = TRUE;
        if (tile_continent(acity->tile) != continent) {
          dest_city_nat_different_cont = TRUE;
          if (prefer_different_cont) {
            break;
          }
        } else {
          dest_city_nat_same_cont = TRUE;
          if (!prefer_different_cont) {
            break;
          }
        }
      }
    } city_list_iterate_end;
  }

  /* FIXME: This check should consider more about relative
   * income from different traderoute types. This works just
   * with more typical ruleset setups. */
  if (prefer_different_cont && !dest_city_nat_different_cont) {
    if (trade_route_type_trade_pct(TRT_IN_IC) >
        trade_route_type_trade_pct(TRT_IN)) {
      prefer_different_cont = TRUE;
    } else {
      prefer_different_cont = FALSE;
    }

    players_iterate(aplayer) {
      if (aplayer == pplayer || !aplayer->is_alive) {
	continue;
      }
      if (pplayers_allied(pplayer, aplayer)) {
        city_list_iterate(aplayer->cities, acity) {
          if (can_cities_trade(pcity, acity)) {
            dest_city_found = TRUE;
            if (tile_continent(acity->tile) != continent) {
              dest_city_in_different_cont = TRUE;
              if (prefer_different_cont) {
                break;
              }
            } else {
              dest_city_in_same_cont = TRUE;
              if (!prefer_different_cont) {
                break;
              }
            }
          }
        } city_list_iterate_end;
      }
      if ((dest_city_in_different_cont && prefer_different_cont)
          || (dest_city_in_same_cont && !prefer_different_cont)) {
        break;
      }
    } players_iterate_end;
  }

  if (!dest_city_found) {
    /* No proper destination city. */
    return;
  }

  unit_type = best_role_unit(pcity, UTYF_TRADE_ROUTE);

  if (!unit_type) {
    /* We cannot build such units yet
     * but we will consider it to stimulate science */
    unit_type = get_role_unit(UTYF_TRADE_ROUTE, 0);
  }

  trade_routes = city_num_trade_routes(pcity);
  /* Count also caravans enroute to establish traderoutes */
  unit_list_iterate(pcity->units_supported, punit) {
    if (unit_has_type_flag(punit, UTYF_TRADE_ROUTE)) {
      trade_routes++;
    }
  } unit_list_iterate_end;

  max_routes = max_trade_routes(pcity);

  /* We consider only initial benefit from establishing trade route.
   * We may actually get only initial benefit if both cities already
   * have four trade routes, or if there already is route between them. */

  /* We assume that we are creating trade route to city with 75% of
   * pcitys trade 10 squares away. */
  income = (10 + 10) * (1.75 * pcity->surplus[O_TRADE]) / 24 * 3;
  bonus = get_city_bonus(pcity, EFT_TRADE_REVENUE_BONUS);
  income = (float)income * pow(2.0, (double)bonus / 1000.0);

  if (dest_city_nat_same_cont) {
    pct = trade_route_type_trade_pct(TRT_NATIONAL);
  }
  if (dest_city_in_same_cont) {
    int typepct = trade_route_type_trade_pct(TRT_IN);

    pct = MAX(pct, typepct);
  }
  if (dest_city_nat_different_cont) {
    int typepct = trade_route_type_trade_pct(TRT_NATIONAL_IC);

    if (typepct > pct) {
      pct = typepct;
      need_boat = TRUE;
    }
  }
  if (dest_city_in_different_cont) {
    int typepct = trade_route_type_trade_pct(TRT_IN_IC);

    if (typepct > pct) {
      pct = typepct;
      need_boat = TRUE;
    }
  }

  income = pct * income / 100;

  want = income * ai->gold_priority + income * ai->science_priority;

  /* We get this income only once after all.
   * This value is adjusted for most interesting gameplay.
   * For optimal performance AI should build more caravans, but
   * we want it to build less valued buildings too. */
  trader_trait = ai_trait_get_value(TRAIT_TRADER, pplayer);
  want /= (130 * TRAIT_DEFAULT_VALUE / trader_trait);

  /* Increase want for trade routes if our economy is very weak.
   * We may have enough gold, but only because we have already set
   * tax rate to 100%. So don't check gold directly, but tax rate.
   * This method helps us out of deadlocks of completely stalled
   * scientific progress.
   */
  if (pplayer->economic.science < 50 && trade_routes < max_routes) {
    want *=
      (6 - pplayer->economic.science/10) * (6 - pplayer->economic.science/10);
  }

  if (trade_routes == 0 && max_routes > 0) {
    /* If we have no trade routes at all, we are certainly creating a new one. */
    want += trader_trait;
  } else if (trade_routes < max_routes) {
    /* Possibly creating a new trade route */
    want += trader_trait / 4;
  }

  want -= utype_build_shield_cost(unit_type) * SHIELD_WEIGHTING / 150;

  CITY_LOG(LOG_DEBUG, pcity,
           "want for trade route unit is %d (expected initial income %d)",
           want,
           income);

  if (want > choice->want) {
    /* This sets our tech want in cases where we cannot actually build
     * the unit. */
    unit_type = dai_wants_role_unit(pplayer, pcity, UTYF_TRADE_ROUTE, want);
    if (unit_type != NULL) {
      choice->want = want;
      choice->type = CT_CIVILIAN;
      choice->value.utype = unit_type;
      choice->need_boat = need_boat;
    } else {
      CITY_LOG(LOG_DEBUG, pcity,
               "would but could not build trade route unit, bumped reqs");
    }
  }
}

/************************************************************************** 
  This function should fill the supplied choice structure.

  If want is 0, this advisor doesn't want anything.
***************************************************************************/
void domestic_advisor_choose_build(struct ai_type *ait, struct player *pplayer,
                                   struct city *pcity, struct adv_choice *choice)
{
  struct adv_data *ai = adv_data_get(pplayer, NULL);
  /* Unit type with certain role */
  struct unit_type *settler_type;
  struct unit_type *founder_type;
  int settler_want, founder_want;
  struct ai_city *city_data = def_ai_city_data(pcity, ait);

  init_choice(choice);

  /* Find out desire for workers (terrain improvers) */
  settler_type = dai_role_utype_for_move_type(pcity, UTYF_SETTLERS, UMT_LAND);

  /* The worker want is calculated in aicity.c called from
   * dai_manage_cities.  The expand value is the % that the AI should
   * value expansion (basically to handicap easier difficulty levels)
   * and is set when the difficulty level is changed (stdinhand.c). */
  settler_want = city_data->settler_want * pplayer->ai_common.expand / 100;

  if (ai->wonder_city == pcity->id) {
    settler_want /= 5;
  }

  if (settler_type
      && (pcity->id != ai->wonder_city || settler_type->pop_cost == 0)
      && pcity->surplus[O_FOOD] > utype_upkeep_cost(settler_type,
                                                    pplayer, O_FOOD)) {
    if (settler_want > 0) {
      CITY_LOG(LOG_DEBUG, pcity, "desires terrain improvers with passion %d", 
               settler_want);
      dai_choose_role_unit(pplayer, pcity, choice, CT_CIVILIAN,
                           UTYF_SETTLERS, settler_want, FALSE);
    }
    /* Terrain improvers don't use boats (yet) */

  } else if (!settler_type && settler_want > 0) {
    /* Can't build settlers. Lets stimulate science */
    dai_wants_role_unit(pplayer, pcity, UTYF_SETTLERS, settler_want);
  }

  /* Find out desire for city founders */
  /* Basically, copied from above and adjusted. -- jjm */
  founder_type = best_role_unit(pcity, UTYF_CITIES);

  /* founder_want calculated in aisettlers.c */
  founder_want = city_data->founder_want;

  if (ai->wonder_city == pcity->id) {
    founder_want /= 5;
  }
    
  if (ai->max_num_cities <= city_list_size(pplayer->cities)) {
    founder_want /= 100;
  }

  /* Adjust founder want by traits */
  founder_want *= (double)ai_trait_get_value(TRAIT_EXPANSIONIST, pplayer)
    / TRAIT_DEFAULT_VALUE;

  if (founder_type
      && (pcity->id != ai->wonder_city
          || founder_type->pop_cost == 0)
      && pcity->surplus[O_FOOD] >= utype_upkeep_cost(founder_type,
                                                     pplayer, O_FOOD)) {

    if (founder_want > choice->want) {
      CITY_LOG(LOG_DEBUG, pcity, "desires founders with passion %d",
               founder_want);
      dai_choose_role_unit(pplayer, pcity, choice, CT_CIVILIAN,
                           UTYF_CITIES, founder_want,
                           city_data->founder_boat);

    } else if (founder_want < -choice->want) {
      /* We need boats to colonize! */
      /* We might need boats even if there are boats free,
       * if they are blockaded or in inland seas. */
      struct ai_plr *ai = dai_plr_data_get(ait, pplayer, NULL);

      CITY_LOG(LOG_DEBUG, pcity, "desires founders with passion %d and asks"
	       " for a new boat (%d of %d free)",
	       -founder_want, ai->stats.available_boats, ai->stats.boats);

      /* First fill choice with founder information */
      choice->want = 0 - founder_want;
      choice->type = CT_CIVILIAN;
      choice->value.utype = founder_type; /* default */
      choice->need_boat = TRUE;

      /* Then try to overwrite it with ferryboat information
       * If no ferryboat is found, above founder choice stays. */
      dai_choose_role_unit(pplayer, pcity, choice, CT_CIVILIAN,
                           L_FERRYBOAT, -founder_want, TRUE);
    }
  } else if (!founder_type
             && (founder_want > choice->want || founder_want < -choice->want)) {
    /* Can't build founders. Lets stimulate science */
    dai_wants_role_unit(pplayer, pcity, UTYF_CITIES, founder_want);
  }

  {
    struct adv_choice cur;

    init_choice(&cur);
    /* Consider building caravan-type units to aid wonder construction */  
    dai_choose_help_wonder(ait, pcity, &cur, ai);
    copy_if_better_choice(&cur, choice);

    init_choice(&cur);
    /* Consider city improvements */
    building_advisor_choose(pcity, &cur);
    copy_if_better_choice(&cur, choice);

    init_choice(&cur);
    /* Consider building caravan-type units for trade route */
    dai_choose_trade_route(pcity, &cur, ai);
    copy_if_better_choice(&cur, choice);
  }

  if (choice->want >= 200) {
    /* If we don't do following, we buy caravans in city X when we should be
     * saving money to buy defenses for city Y. -- Syela */
    choice->want = 199;
  }

  return;
}

/**************************************************************************
  Calculate walking distances to wonder city from nearby cities.
**************************************************************************/
void dai_wonder_city_distance(struct ai_type *ait, struct player *pplayer, 
                              struct adv_data *adv)
{
  struct pf_map *pfm;
  struct pf_parameter parameter;
  struct unit_type *punittype;
  struct unit *ghost;
  int maxrange;
  struct city *wonder_city = game_city_by_number(adv->wonder_city);

  city_list_iterate(pplayer->cities, acity) {
    /* Mark unavailable */
    def_ai_city_data(acity, ait)->distance_to_wonder_city = 0;
  } city_list_iterate_end;

  if (wonder_city == NULL) {
    return;
  }

  punittype = best_role_unit_for_player(pplayer, UTYF_HELP_WONDER);
  if (!punittype) {
    return;
  }
  ghost = unit_virtual_create(pplayer, wonder_city, punittype, 0);
  maxrange = unit_move_rate(ghost) * 7;

  pft_fill_unit_parameter(&parameter, ghost);
  pfm = pf_map_new(&parameter);

  pf_map_move_costs_iterate(pfm, ptile, move_cost, FALSE) {
    struct city *acity = tile_city(ptile);

    if (move_cost > maxrange) {
      break;
    }
    if (!acity) {
      continue;
    }
    if (city_owner(acity) == pplayer) {
      def_ai_city_data(acity, ait)->distance_to_wonder_city = move_cost;
    }
  } pf_map_move_costs_iterate_end;

  pf_map_destroy(pfm);
  unit_virtual_destroy(ghost);
}
