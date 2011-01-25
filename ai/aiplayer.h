/***********************************************************************
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
#ifndef FC__AIPLAYER_H
#define FC__AIPLAYER_H

/* common */
#include "player.h"

struct player;
struct ai_plr;

struct ai_type *default_ai_get_self(void);

void default_ai_set_self(struct ai_type *ai);

void ai_player_alloc(struct player *pplayer);
void ai_player_free(struct player *pplayer);

static inline struct ai_city *def_ai_city_data(const struct city *pcity)
{
  return (struct ai_city *)city_ai_data(pcity, default_ai_get_self());
}

static inline struct unit_ai *def_ai_unit_data(const struct unit *punit)
{
  return (struct unit_ai *)unit_ai_data(punit, default_ai_get_self());
}

static inline struct ai_plr *def_ai_player_data(const struct player *pplayer)
{
  return (struct ai_plr *)player_ai_data(pplayer, default_ai_get_self());
}

#endif /* FC__AIPLAYER_H */
