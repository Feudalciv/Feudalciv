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

#include "war.h"

/**************************************************************************
  Initialize the war cache
**************************************************************************/
void war_cache_init()
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
