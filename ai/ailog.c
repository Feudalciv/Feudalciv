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

#include <stdarg.h>

#include "astring.h"
#include "city.h"
#include "log.h"
#include "shared.h"
#include "support.h"
#include "unit.h"
#include "timing.h"

#include "gotohand.h"
#include "plrhand.h"
#include "srv_main.h"

#include "aidata.h"
#include "ailog.h"

static struct timer *aitimer[AIT_LAST][2];
static int recursion[AIT_LAST];

/* General AI logging functions */

/**************************************************************************
  Log player tech messages.
**************************************************************************/
void TECH_LOG(int level, struct player *pplayer, Tech_Type_id id,
              const char *msg, ...)
{
  char buffer[500];
  char buffer2[500];
  va_list ap;
  int minlevel = MIN(LOGLEVEL_TECH, level);

  if (!tech_exists(id) || id == A_NONE) {
    return;
  }

  if (BV_ISSET(pplayer->debug, PLAYER_DEBUG_TECH)) {
    minlevel = LOG_NORMAL;
  } else if (minlevel > fc_log_level) {
    return;
  }

  my_snprintf(buffer, sizeof(buffer), "%s::%s (want %d, dist %d) ", 
              pplayer->name, get_tech_name(pplayer, id), 
              pplayer->ai.tech_want[id], 
              num_unknown_techs_for_goal(pplayer, id));

  va_start(ap, msg);
  my_vsnprintf(buffer2, sizeof(buffer2), msg, ap);
  va_end(ap);

  cat_snprintf(buffer, sizeof(buffer), buffer2);
  if (BV_ISSET(pplayer->debug, PLAYER_DEBUG_TECH)) {
    notify_conn(game.est_connections, buffer);
  }
  freelog(minlevel, buffer);
}

/**************************************************************************
  Log player messages, they will appear like this
    2: perrin [ti12 co6 lo5 e]  Increased love for a (now 9)
  where ti is timer, co countdown and lo love for target, who is e.
**************************************************************************/
void DIPLO_LOG(int level, struct player *pplayer, struct ai_data *ai, 
               const char *msg, ...)
{
  char targetbuffer[250];
  char buffer[500];
  char buffer2[500];
  va_list ap;
  int minlevel = MIN(LOGLEVEL_PLAYER, level);

  if (BV_ISSET(pplayer->debug, PLAYER_DEBUG_DIPLOMACY)) {
    minlevel = LOG_NORMAL;
  } else if (minlevel > fc_log_level) {
    return;
  }

  if (ai->diplomacy.target) {
    my_snprintf(targetbuffer, sizeof(targetbuffer), "[ti%d co%d lo%d %s] ",
                ai->diplomacy.timer, ai->diplomacy.countdown,
                pplayer->ai.love[ai->diplomacy.target->player_no],
                ai->diplomacy.target->name);
  }
  my_snprintf(buffer, sizeof(buffer), "%s %s%s%s ", pplayer->name,
              ai->diplomacy.target ? targetbuffer : "",
              ai->diplomacy.spacerace_leader &&
              ai->diplomacy.spacerace_leader->player_no == pplayer->player_no ? 
                "(spacelead) " : "",
              ai->diplomacy.alliance_leader->player_no == pplayer->player_no ?
                "(*)" : "");

  va_start(ap, msg);
  my_vsnprintf(buffer2, sizeof(buffer2), msg, ap);
  va_end(ap);

  cat_snprintf(buffer, sizeof(buffer), buffer2);
  if (BV_ISSET(pplayer->debug, PLAYER_DEBUG_DIPLOMACY)) {
    notify_conn(game.est_connections, buffer);
  }
  freelog(minlevel, buffer);
}

/**************************************************************************
  Log city messages, they will appear like this
    2: c's Romenna(5,35) [s1 d106 u11 g1] must have Archers ...
**************************************************************************/
void CITY_LOG(int level, struct city *pcity, const char *msg, ...)
{
  char buffer[500];
  char buffer2[500];
  va_list ap;
  int minlevel = MIN(LOGLEVEL_CITY, level);

  if (pcity->debug) {
    minlevel = LOG_NORMAL;
  } else if (minlevel > fc_log_level) {
    return;
  }

  my_snprintf(buffer, sizeof(buffer), "%s's %s(%d,%d) [s%d d%d u%d g%d] ",
              city_owner(pcity)->name, pcity->name,
              pcity->tile->x, pcity->tile->y, pcity->size,
              pcity->ai.danger, pcity->ai.urgency,
              pcity->ai.grave_danger);

  va_start(ap, msg);
  my_vsnprintf(buffer2, sizeof(buffer2), msg, ap);
  va_end(ap);

  cat_snprintf(buffer, sizeof(buffer), buffer2);
  if (pcity->debug) {
    notify_conn(game.est_connections, buffer);
  }
  freelog(minlevel, buffer);
}

/**************************************************************************
  Log unit messages, they will appear like this
    2: c's Archers[139] (5,35)->(0,0){0,0} stays to defend city
  where [] is unit id, ()->() are coordinates present and goto, and
  {,} contains bodyguard and ferryboat ids.
**************************************************************************/
void UNIT_LOG(int level, struct unit *punit, const char *msg, ...)
{
  char buffer[500];
  char buffer2[500];
  va_list ap;
  int minlevel = MIN(LOGLEVEL_UNIT, level);
  int gx, gy;
  bool messwin = FALSE; /* output to message window */

  if (punit->debug) {
    minlevel = LOG_NORMAL;
  } else {
    /* Are we a virtual unit evaluated in a debug city?. */
    if (punit->id == 0) {
      struct city *pcity = tile_get_city(punit->tile);

      if (pcity && pcity->debug) {
        minlevel = LOG_NORMAL;
        messwin = TRUE;
      }
    }
    if (minlevel > fc_log_level) {
      return;
    }
  }

  if (punit->goto_tile) {
    gx = punit->goto_tile->x;
    gy = punit->goto_tile->y;
  } else {
    gx = gy = -1;
  }
  
  my_snprintf(buffer, sizeof(buffer), "%s's %s[%d] (%d,%d)->(%d,%d){%d,%d} ",
              unit_owner(punit)->name, unit_type(punit)->name,
              punit->id, punit->tile->x, punit->tile->y,
	      gx, gy,
              punit->ai.bodyguard, punit->ai.ferryboat);

  va_start(ap, msg);
  my_vsnprintf(buffer2, sizeof(buffer2), msg, ap);
  va_end(ap);

  cat_snprintf(buffer, sizeof(buffer), buffer2);
  if (punit->debug || messwin) {
    notify_conn(game.est_connections, buffer);
  }
  freelog(minlevel, buffer);
}

/**************************************************************************
  Log message for bodyguards. They will appear like this
    2: ai4's bodyguard Mech. Inf.[485] (38,22){Riflemen:574@37,23} was ...
  note that these messages are likely to wrap if long.
**************************************************************************/
void BODYGUARD_LOG(int level, struct unit *punit, const char *msg)
{
  char buffer[500];
  int minlevel = MIN(LOGLEVEL_BODYGUARD, level);
  struct unit *pcharge;
  struct city *pcity;
  int id = -1;
  struct tile *ptile = NULL;
  const char *s = "none";

  if (punit->debug) {
    minlevel = LOG_NORMAL;
  } else if (minlevel > fc_log_level) {
    return;
  }

  pcity = find_city_by_id(punit->ai.charge);
  pcharge = find_unit_by_id(punit->ai.charge);
  if (pcharge) {
    ptile = pcharge->tile;
    id = pcharge->id;
    s = unit_type(pcharge)->name;
  } else if (pcity) {
    ptile = pcity->tile;
    id = pcity->id;
    s = pcity->name;
  }
  my_snprintf(buffer, sizeof(buffer),
              "%s's bodyguard %s[%d] (%d,%d){%s:%d@%d,%d} ",
              unit_owner(punit)->name, unit_type(punit)->name,
              punit->id, punit->tile->x, punit->tile->y,
	      s, id, ptile->x, ptile->y);
  cat_snprintf(buffer, sizeof(buffer), msg);
  if (punit->debug) {
    notify_conn(game.est_connections, buffer);
  }
  freelog(minlevel, buffer);
}

/**************************************************************************
  Measure the time between the calls.  Used to see where in the AI too
  much CPU is being used.
**************************************************************************/
void TIMING_LOG(enum ai_timer timer, enum ai_timer_activity activity)
{
  static int turn = -1;
  int i;

  if (turn == -1) {
    for (i = 0; i < AIT_LAST; i++) {
      aitimer[i][0] = new_timer(TIMER_CPU, TIMER_ACTIVE);
      aitimer[i][1] = new_timer(TIMER_CPU, TIMER_ACTIVE);
      recursion[i] = 0;
    }
  }

  if (game.turn != turn) {
    turn = game.turn;
    for (i = 0; i < AIT_LAST; i++) {
      clear_timer(aitimer[i][0]);
    }
    assert(activity == TIMER_START);
  }

  if (activity == TIMER_START && recursion[timer] == 0) {
    start_timer(aitimer[timer][0]);
    start_timer(aitimer[timer][1]);
    recursion[timer]++;
  } else if (activity == TIMER_STOP && recursion[timer] == 1) {
    stop_timer(aitimer[timer][0]);
    stop_timer(aitimer[timer][1]);
    recursion[timer]--;
  }
}

/**************************************************************************
  Print results
**************************************************************************/
void TIMING_RESULTS(void)
{
  char buf[200];

#define OUT(text, which)                                                 \
  my_snprintf(buf, sizeof(buf), "  %s: %g sec turn, %g sec game", text,  \
           read_timer_seconds(aitimer[which][0]),                        \
           read_timer_seconds(aitimer[which][1]));                       \
  freelog(LOG_NORMAL, buf);                                              \
  notify_conn(game.est_connections, buf);

  freelog(LOG_NORMAL, "  --- AI timing results ---");
  notify_conn(game.est_connections, "  --- AI timing results ---");
  OUT("Total AI time", AIT_ALL);
  OUT("Movemap", AIT_MOVEMAP);
  OUT("Units", AIT_UNITS);
  OUT(" - Military", AIT_MILITARY);
  OUT(" - Attack", AIT_ATTACK);
  OUT(" - Defense", AIT_DEFENDERS);
  OUT(" - Ferry", AIT_FERRY);
  OUT(" - Rampage", AIT_RAMPAGE);
  OUT(" - Bodyguard", AIT_BODYGUARD);
  OUT(" - Recover", AIT_RECOVER);
  OUT(" - Caravan", AIT_CARAVAN);
  OUT(" - Hunter", AIT_HUNTER);
  OUT(" - Airlift", AIT_AIRLIFT);
  OUT(" - Diplomat", AIT_DIPLOMAT);
  OUT(" - Air", AIT_AIRUNIT);
  OUT(" - Explore", AIT_EXPLORER);
  OUT("fstk", AIT_FSTK);
  OUT("Settlers", AIT_SETTLERS);
  OUT("Workers", AIT_WORKERS);
  OUT("Government", AIT_GOVERNMENT);
  OUT("Taxes", AIT_TAXES);
  OUT("Cities", AIT_CITIES);
  OUT(" - Buildings", AIT_BUILDINGS);
  OUT(" - Danger", AIT_DANGER);
  OUT(" - Worker want", AIT_CITY_TERRAIN);
  OUT(" - Military want", AIT_CITY_MILITARY);
  OUT(" - Settler want", AIT_CITY_SETTLERS);
  OUT("Citizen arrange", AIT_CITIZEN_ARRANGE);
  OUT("Tech", AIT_TECH);
}
