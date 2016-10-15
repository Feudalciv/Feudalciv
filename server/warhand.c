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
#include "log.h"
#include "mem.h"

/* common */
#include "game.h"
#include "player.h"
#include "war.h"

/* common/scriptcore */
#include "luascript_types.h"

/* server */
#include "diplhand.h"
#include "notify.h"
#include "triggers.h"

#include "warhand.h"

/**************************************************************************
  Starts a war between two players
**************************************************************************/
void start_war(struct player * aggressor, struct player * defender, const char * casus_belli)
{
  fc_assert(NULL != aggressor);
  fc_assert(NULL != defender);

  struct war *pwar;
  struct player *overlord;

  pwar = fc_malloc(sizeof(*pwar));

  overlord = get_player_overlord(defender);
  if (overlord != NULL && player_number(overlord) != player_number(get_player_overlord(aggressor))
          && player_number(aggressor) != player_number(overlord)) {
    /* bring overlord into the war if attacked by a foreign power*/
    notify_player(overlord, NULL, E_TREATY_BROKEN, ftc_server,
                  _("%s declared war on our subject %s. "
                    "We are obligated to defend our subject %s and have taken control of the conflict."),
                  player_name(aggressor),
                  player_name(defender),
                  player_name(defender));
    handle_diplomacy_cancel_pact(aggressor, player_number(overlord), CLAUSE_LAST);
    /* Overlord becomes the defender */
    defender = overlord;
  }

  pwar->aggressor = aggressor;
  pwar->defender = defender;
  pwar->casus_belli = fc_strdup(casus_belli);
  pwar->aggressors = player_list_new();
  pwar->defenders = player_list_new();
  player_list_append(pwar->aggressors, aggressor);
  player_list_append(pwar->defenders, defender);
  war_list_append(wars, pwar);
  war_list_append(aggressor->current_wars, pwar);
  war_list_append(defender->current_wars, pwar);

  players_iterate_alive(otherplayer) {
    if (pplayers_allied(otherplayer, aggressor) &&
            player_number(otherplayer) != player_number(aggressor) &&
            !pplayers_at_war(otherplayer, defender)) {
      trigger_by_name(otherplayer, "trigger_call_to_arms", 3, API_TYPE_PLAYER, otherplayer,
                                                              API_TYPE_PLAYER, aggressor,
                                                              API_TYPE_PLAYER, defender);
    }
    else if (pplayers_allied(otherplayer, defender) &&
            player_number(otherplayer) != player_number(defender) &&
            !pplayers_at_war(otherplayer, aggressor)) {
      trigger_by_name(otherplayer, "trigger_call_to_arms", 3, API_TYPE_PLAYER, otherplayer,
                                                              API_TYPE_PLAYER, defender,
                                                              API_TYPE_PLAYER, aggressor);
    }
  } players_iterate_alive_end;

}

/**************************************************************************
  Makes a player join another player's existing wars
**************************************************************************/
bool join_war(struct player * pplayer, struct player *ally, struct player *enemy)
{
  fc_assert_ret_val(NULL != pplayer, FALSE);
  fc_assert_ret_val(NULL != ally, FALSE);
  fc_assert_ret_val(NULL != enemy, FALSE);

  war_list_iterate(wars, pwar) {
    if (player_number(pwar->aggressor) == player_number(ally) && player_number(pwar->defender == player_number(enemy))) {
      player_list_iterate(pwar->defenders, defender) {
        handle_diplomacy_cancel_pact(pplayer, player_number(defender), CLAUSE_LAST);
      } player_list_iterate_end;
      player_list_append(pwar->aggressors, pplayer);
      war_list_append(pplayer->current_wars, pwar);
      player_subjects_iterate(pplayer, subject) {
        trigger_by_name(subject, "trigger_call_to_arms", 3, API_TYPE_PLAYER, subject,
                                                            API_TYPE_PLAYER, pwar->aggressor,
                                                            API_TYPE_PLAYER, pwar->defender);
      } player_subjects_iterate_end;
      return TRUE;
    }
    else if (player_number(pwar->defender) == player_number(ally) && player_number(pwar->aggressor) == player_number(enemy)) {
      player_list_iterate(pwar->aggressors, aggressor) {
        handle_diplomacy_cancel_pact(aggressor, player_number(pplayer), CLAUSE_LAST);
      } player_list_iterate_end;
      player_list_append(pwar->defenders, pplayer);
      war_list_append(pplayer->current_wars, pwar);
      player_subjects_iterate(pplayer, subject) {
        trigger_by_name(subject, "trigger_call_to_arms", 3, API_TYPE_PLAYER, subject,
                                                            API_TYPE_PLAYER, pwar->defender,
                                                            API_TYPE_PLAYER, pwar->aggressor);
      } player_subjects_iterate_end;
      return TRUE;
    }
  } war_list_iterate_end;

  return FALSE;
}

/*************************************************************************
  Checks whether or not two players are currently in a war against each other
*************************************************************************/
static bool players_should_be_at_war(struct player *pplayer, struct player *pplayer2)
{
  struct player *defender, *aggressor;

  if (pplayer == NULL || pplayer2 == NULL) return FALSE;

  war_list_iterate(pplayer->current_wars, pwar) {
    aggressor = defender = NULL;

    player_list_iterate(pwar->aggressors, aggressor) {
      if (player_number(aggressor) == player_number(pplayer2)) {
        aggressor = pplayer2;
        break;
      } else if (player_number(aggressor) == player_number(pplayer)) {
        aggressor = pplayer;
        break;
      }
    } player_list_iterate_end;
    player_list_iterate(pwar->defenders, defender) {
      if (player_number(defender) == player_number(pplayer2)) {
        defender = pplayer2;
        break;
      } else if (player_number(defender) == player_number(pplayer)) {
        defender = pplayer;
        break;
      }
    } player_list_iterate_end;

    if (aggressor != NULL && defender != NULL) return TRUE;
  } war_list_iterate_end;
  return FALSE;
}

/**************************************************************************
  Makes peace between a player and the players on the other side in a war
**************************************************************************/
static void make_peace(struct player *pplayer, struct war *pwar)
{
  fc_assert(NULL != pplayer);
  fc_assert(NULL != pwar);

  war_list_remove(pplayer->current_wars, pwar);
  if (player_list_remove(pwar->defenders, pplayer)) {
    player_list_iterate(pwar->aggressors, aggressor) {
      if (!players_should_be_at_war(pplayer, aggressor)) {
        set_peace(pplayer, aggressor);
      }
    } player_list_iterate_end;
  } else if (player_list_remove(pwar->aggressors, pplayer)) {
    player_list_iterate(pwar->defenders, defender) {
      if (!players_should_be_at_war(pplayer, defender)) {
        set_peace(pplayer, defender);
      }
    } player_list_iterate_end;
  }
}

/**************************************************************************
  Makes a player make peace with an enemy and leave all wars with that enemy
**************************************************************************/
bool leave_war(struct player * pplayer, struct war *pwar)
{
  fc_assert(NULL != pplayer);
  fc_assert(NULL != pwar);

  if (player_list_remove(pwar->defenders, pplayer)) {
    war_list_remove(pplayer->current_wars, pwar);
    make_peace(pplayer, pwar);
  } else if (player_list_remove(pwar->aggressors, pplayer)) {
    war_list_remove(pplayer->current_wars, pwar);
    make_peace(pplayer, pwar);
  }
}

/**************************************************************************
  Ends a war
**************************************************************************/
void end_war(struct war *pwar)
{
  fc_assert(NULL != pwar);

  war_list_remove(wars, pwar);

  /* Update state for primary participants first,
     note that other wars may cause them to still be at war despite the peace treaty */
  make_peace(pwar->defender, pwar);
  make_peace(pwar->aggressor, pwar);

  /* Update state of aggressors */
  player_list_iterate(pwar->aggressors, pplayer) {
    make_peace(pplayer, pwar);
    notify_player(pplayer, NULL, E_TREATY_PEACE, ftc_server,
                          _("Your ally %s has made peace with %s. "
                            "%s's war %s has ended."),
                          player_name(pwar->aggressor),
                          player_name(pwar->defender),
                          player_name(pwar->aggressor),
                          pwar->casus_belli);

  } player_list_iterate_end;

  /* Update state of defenders*/
  player_list_iterate(pwar->defenders, pplayer) {
    make_peace(pplayer, pwar);
    notify_player(pplayer, NULL, E_TREATY_PEACE, ftc_server,
                          _("Your ally %s has made peace with %s. "
                            "%s's war %s has ended."),
                          player_name(pwar->defender),
                          player_name(pwar->aggressor),
                          player_name(pwar->aggressor),
                          pwar->casus_belli);
  } player_list_iterate_end;

  player_list_destroy(pwar->defenders);
  player_list_destroy(pwar->aggressors);
  free(pwar->casus_belli);
  free(pwar);
}


/**************************************************************************
  Updates the state of the wars when a certain peace treaty is made between
    two players
**************************************************************************/
void update_wars_for_peace_treaty(struct player *pplayer1, struct player *pplayer2)
{
  fc_assert(NULL != pplayer1);
  fc_assert(NULL != pplayer2);

  int pn1 = player_number(pplayer1);
  int pn2 = player_number(pplayer2);
  int pndef, pnagg;
  bool pn1def, pn1agg, pn2def, pn2agg;

  war_list_iterate(wars, pwar) {
    pndef = player_number(pwar->defender);
    pnagg = player_number(pwar->aggressor);
    pn1def = player_list_search(pwar->defenders, pplayer1);
    pn1agg = player_list_search(pwar->aggressors, pplayer1);
    pn2def = player_list_search(pwar->defenders, pplayer2);
    pn2agg = player_list_search(pwar->aggressors, pplayer2);
    if (!((pn1def || pn1agg) && (pn2def || pn2agg))) continue;

    if ((pn1 == pndef && pn2 == pnagg) || (pn1 == pnagg && pn2 == pndef)) {
      end_war(pwar);
    } else if ((pn1 == pndef && pn2agg) || (pn1 == pnagg && pn2def)) {
      leave_war(pplayer2, pwar);
    } else if ((pn2 == pndef && pn1agg) || (pn2 == pnagg && pn1def)) {
      leave_war(pplayer1, pwar);
    }
  } war_list_iterate_end;
}


/**************************************************************************
  Makes a ceasefire between a player and the players on the other side in a war
**************************************************************************/
static void make_ceasefire(struct player *pplayer, struct war *pwar)
{
  fc_assert(NULL != pplayer);
  fc_assert(NULL != pwar);

  if (player_list_search(pwar->defenders, pplayer)) {
    player_list_iterate(pwar->aggressors, aggressor) {
      set_ceasefire(pplayer, aggressor);
    } player_list_iterate_end;
  } else if (player_list_search(pwar->aggressors, pplayer)) {
    player_list_iterate(pwar->defenders, defender) {
      set_ceasefire(pplayer, defender);
    } player_list_iterate_end;
  }
}

/**************************************************************************
  Makes a ceasefire between a player and the players on the other side in a war
**************************************************************************/
static void break_ceasefire(struct player *pplayer, struct war *pwar)
{
  fc_assert(NULL != pplayer);
  fc_assert(NULL != pwar);

  if (player_list_search(pwar->defenders, pplayer)) {
    player_list_iterate(pwar->aggressors, aggressor) {
      handle_diplomacy_cancel_pact(pplayer, player_number(aggressor), CLAUSE_LAST);
    } player_list_iterate_end;
  } else if (player_list_search(pwar->aggressors, pplayer)) {
    player_list_iterate(pwar->defenders, defender) {
      handle_diplomacy_cancel_pact(pplayer, player_number(defender), CLAUSE_LAST);
    } player_list_iterate_end;
  }
}

/**************************************************************************
  Updates the state of the wars when a certain ceasefire is made between
    two players
**************************************************************************/
void update_wars_for_ceasefire(struct player *pplayer1, struct player *pplayer2)
{
  fc_assert(NULL != pplayer1);
  fc_assert(NULL != pplayer2);

  int pn1 = player_number(pplayer1);
  int pn2 = player_number(pplayer2);
  int pndef, pnagg;
  bool pn1def, pn1agg, pn2def, pn2agg;

  war_list_iterate(wars, pwar) {
    pndef = player_number(pwar->defender);
    pnagg = player_number(pwar->aggressor);
    pn1def = player_list_search(pwar->defenders, pplayer1);
    pn1agg = player_list_search(pwar->aggressors, pplayer1);
    pn2def = player_list_search(pwar->defenders, pplayer2);
    pn2agg = player_list_search(pwar->aggressors, pplayer2);
    if (!((pn1def || pn1agg) && (pn2def || pn2agg))) continue;

    if ((pn1 == pndef && pn2 == pnagg) || (pn1 == pnagg && pn2 == pndef)) {
      /* War leaders called for ceasefire, make ceasefire between all sides */
      player_list_iterate(pwar->defenders, pplayer) {
        make_ceasefire(pplayer, pwar);
      } player_list_iterate_end;
    } else if ((pn1 == pndef && pn2agg) || (pn1 == pnagg && pn2def)) {
      /* War leader player 1 called for a ceasefire with player 2, make ceasefire with entire side */
      make_ceasefire(pplayer2, pwar);
    } else if ((pn2 == pndef && pn1agg) || (pn2 == pnagg && pn1def)) {
      /* Player 2 called for a ceasefire with war leader, make ceasefire with entire side */
      make_ceasefire(pplayer2, pwar);
    }
  } war_list_iterate_end;
}

/**************************************************************************
  Updates the state of the wars when a certain ceasefire is made between
    two players
**************************************************************************/
void update_wars_for_broken_ceasefire(struct player *pplayer1, struct player *pplayer2)
{
  fc_assert(NULL != pplayer1);
  fc_assert(NULL != pplayer2);

  int pn1 = player_number(pplayer1);
  int pn2 = player_number(pplayer2);
  int pndef, pnagg;
  bool pn1def, pn1agg, pn2def, pn2agg;

  war_list_iterate(wars, pwar) {
    pndef = player_number(pwar->defender);
    pnagg = player_number(pwar->aggressor);
    pn1def = player_list_search(pwar->defenders, pplayer1);
    pn1agg = player_list_search(pwar->aggressors, pplayer1);
    pn2def = player_list_search(pwar->defenders, pplayer2);
    pn2agg = player_list_search(pwar->aggressors, pplayer2);
    if (!((pn1def || pn1agg) && (pn2def || pn2agg))) continue;

    if (pn1 == pndef && pn2 == pnagg) {
      /* War leaders broke ceasefire, ceasefire is cancelled between all sides */
      player_list_iterate(pwar->defenders, pplayer) {
        break_ceasefire(pplayer, pwar);
      } player_list_iterate_end;
    } else if (pn1 == pnagg && pn2 == pndef) {
      /* War leaders broke ceasefire, ceasefire is cancelled between all sides */
      player_list_iterate(pwar->aggressors, pplayer) {
        break_ceasefire(pplayer, pwar);
      } player_list_iterate_end;
    } else if (pn1 == pndef && pn2agg) {
      /* War leader broke ceasefire with player 2, break ceasefire for all sides*/
      player_list_iterate(pwar->defenders, pplayer) {
        break_ceasefire(pplayer, pwar);
      } player_list_iterate_end;
    } else if (pn1 == pnagg && pn2def) {
      /* War leader broke ceasefire with player 2, break ceasefire for all sides*/
      player_list_iterate(pwar->aggressors, pplayer) {
        break_ceasefire(pplayer, pwar);
      } player_list_iterate_end;
    } else if ((pn2 == pndef && pn1agg) || (pn2 == pnagg && pn1def)) {
      /* Player 1 broke ceasefire with war leader, break ceasefire with entire side */
      break_ceasefire(pplayer1, pwar);
    }
    /* Otherwise ceasefire is only broken between the two players and nothing needs to be done */
  } war_list_iterate_end;
}
