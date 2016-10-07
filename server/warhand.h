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
#ifndef FC_WARHAND_H
#define FC_WARHAND_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "support.h"            /* bool type */

/* common */
#include "war.h"

void start_war(struct player * aggressor, struct player * defender, const char * casus_belli);

void update_wars_for_peace_treaty(struct player *pplayer1, struct player *pplayer2);

bool join_war(struct player *pplayer, struct player *ally, struct player *enemy);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC__WARHAND_H */
