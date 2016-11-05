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
#ifndef FC__TRIGGERHAND_H
#define FC__TRIGGERHAND_H

#include "triggers.h"

#include "hand_gen.h"

void handle_trigger_response_player(const struct player *pplayer, const char *name, int response);

#endif  /* FC__TRIGGERHAND_H */
