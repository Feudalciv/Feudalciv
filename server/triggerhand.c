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

#include <ctype.h>
#include <string.h>

/* common */
#include "player.h"

/* server */
#include "triggers.h"
#include "script_server.h"

#include "triggerhand.h"

void handle_trigger_response(struct connection *pc, const char *name, int response)
{
  handle_trigger_response_player(pc->playing, name, response);
}


void handle_trigger_response_player(const struct player *pplayer, const char *name, int response)
{
  struct trigger_response * presponse = remove_trigger_response_from_cache(pplayer, name);
  presponse->nargs++;
  presponse->args = fc_realloc(presponse->args, presponse->nargs * sizeof(void *) * 2);
  void ** args = presponse->args;

  /* Response is always last */
  args[presponse->nargs * 2 - 2] = (void*)(long)API_TYPE_INT;
  args[presponse->nargs * 2 - 1] = (void*)(long)response;
  script_server_trigger_emit(presponse->trigger->name, presponse->nargs, presponse->args);
  free(presponse->args);
  free(presponse);
}
