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
  pwar = fc_malloc(sizeof(*pwar));

  pwar->aggressor = aggressor;
  pwar->defender = defender;
  pwar->casus_belli = fc_strdup(casus_belli);
  war_list_append(wars, pwar);

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
    }
    else if (player_number(pwar->defender) == player_number(leader)) {
      player_list_append(pwar->defenders, ally);
    }
  } war_list_iterate_end;
}

/**************************************************************************
  Ends a war
**************************************************************************/
void end_war(struct war *pwar)
{
  war_list_remove(wars, pwar);
  player_list_iterate(pwar->aggressors, pplayer) {
    /* TODO: Update diplomatic state of aggressors now that war is over */
    notify_player(pplayer, NULL, E_TREATY_PEACE, ftc_server,
                          _("Your ally %s has made peace with %s. "
                            "%s's war %s has ended."),
                          player_name(pwar->aggressor),
                          player_name(pwar->defender),
                          player_name(pwar->aggressor),
                          pwar->casus_belli);

  } player_list_iterate_end;

  player_list_iterate(pwar->defenders, pplayer) {
    /* TODO: Update diplomatic state of defenders now that war is over */
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
