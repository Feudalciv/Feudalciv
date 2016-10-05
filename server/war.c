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

/* common/scriptcore */
#include "luascript_types.h"

/* server */
#include "diplhand.h"
#include "notify.h"
#include "triggers.h"

#include "war.h"

struct war_list *wars;

/**************************************************************************
  Starts a war between two players
**************************************************************************/
void start_war(struct player * aggressor, struct player * defender, const char * casus_belli)
{
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
bool join_war(struct player * leader, struct player *ally)
{
  war_list_iterate(wars, pwar) {
    if (player_number(pwar->aggressor) == player_number(leader)) {
      player_list_append(pwar->aggressors, ally);
      war_list_append(ally->current_wars, pwar);
    }
    else if (player_number(pwar->defender) == player_number(leader)) {
      player_list_append(pwar->defenders, ally);
      war_list_append(ally->current_wars, pwar);
    }
  } war_list_iterate_end;
}

/*************************************************************************
  Checks whether or not two players are currently in a war against each other
*************************************************************************/
static bool players_should_be_at_war(struct player *pplayer, struct player *pplayer2)
{
  struct player *defender, *aggressor;

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

  free(pwar->casus_belli);
  free(pwar);
}


/**************************************************************************
  Updates the state of the wars when a certain peace treaty is made between
    two players
**************************************************************************/
void update_wars_for_peace_treaty(struct player *pplayer1, struct player *pplayer2)
{
  int pn1 = player_number(pplayer1);
  int pn2 = player_number(pplayer2);
  int pndef, pnagg;

  war_list_iterate(wars, pwar) {
    pndef = player_number(pwar->defender);
    pnagg = player_number(pwar->aggressor);
    if ((pn1 == pndef && pn2 == pnagg) || (pn1 == pnagg && pn2 == pndef)) {
      end_war(pwar);
    } else if (pn1 == pndef || pn1 == pnagg) {
      leave_war(pplayer2, pwar);
    } else if (pn2 == pndef || pn2 == pnagg) {
      leave_war(pplayer1, pwar);
    }
  } war_list_iterate_end;
}

/**************************************************************************
  Initialize the war cache
**************************************************************************/
void initialize_wars()
{
  wars = war_list_new();
}

/**************************************************************************
  Free the war cache
**************************************************************************/
void war_cache_free()
{
  if (wars) {
    war_list_iterate(wars, pwar) {
      free(pwar->casus_belli);
      free(pwar);
    } war_list_iterate_end;
    war_list_destroy(wars);
    wars = NULL;
  }
}
