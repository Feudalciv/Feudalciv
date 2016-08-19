/**********************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
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

/* utility */
#include "astring.h"
#include "fcintl.h"
#include "log.h"
#include "mem.h"
#include "support.h"
#include "shared.h" /* ARRAY_SIZE */
#include "string_vector.h"

/* common */
#include "city.h"
#include "game.h"
#include "government.h"
#include "improvement.h"
#include "map.h"
#include "packets.h"
#include "player.h"
#include "tech.h"

/* common/scriptcore */
#include "luascript_types.h"

/* server */
#include "script_server.h"
#include "triggerhand.h"

#include "triggers.h"

static void send_trigger(struct connection *pconn,
                         struct trigger *ptrigger, const char * desc);
void get_trigger_signal_arg_list(const struct trigger * ptrigger, int * nargs, int * args);

static bool initialized = FALSE;

static int trigger_timeout = 3;

/**************************************************************************
  The code creates a ruleset cache on ruleset load.
 **************************************************************************/

/**************************************************************************
  Ruleset cache. The cache is created during ruleset loading and the data
  is organized to enable fast queries.
**************************************************************************/
static struct {
  /* A single list containing every trigger. */
  struct trigger_list *triggers;

  /* A single list containing triggers that are awaiting a response. */
  struct trigger_response_list *responses;
} trigger_cache;


/**************************************************************************
  Get a list of triggers of this type.
**************************************************************************/
struct trigger_list *get_triggers()
{
  return trigger_cache.triggers;
}

/**************************************************************************
  Add trigger to ruleset cache.
**************************************************************************/
struct trigger *trigger_new(const char * name, const char * title, const char * desc,
        const char * mtth, bool repeatable, int num_responses, const char **responses,
        int default_response)
{
  struct trigger *ptrigger;
  int i;

  /* Create the trigger. */
  ptrigger = fc_malloc(sizeof(*ptrigger));
  ptrigger->name = fc_strdup(name);
  ptrigger->title = title == NULL ? NULL : fc_strdup(title);
  ptrigger->desc = desc == NULL ? NULL : fc_strdup(desc);
  ptrigger->mtth = mtth == NULL ? NULL : fc_strdup(mtth);
  ptrigger->repeatable = repeatable;
  ptrigger->responses_num = num_responses;
  ptrigger->responses = fc_malloc(num_responses * sizeof(const char *));
  ptrigger->default_response = default_response;
  for (i = 0; i < num_responses; i++) {
    ptrigger->responses[i] = fc_strdup(responses[i]);
  }

  requirement_vector_init(&ptrigger->reqs);

  /* Now add the trigger to the ruleset cache. */
  trigger_list_append(trigger_cache.triggers, ptrigger);

  return ptrigger;
}

/**************************************************************************
 * Remove trigger from ruleset cache.
**************************************************************************/
void trigger_delete(struct trigger *ptrigger)
{
  trigger_list_remove(trigger_cache.triggers, ptrigger);
  requirement_vector_free(&(ptrigger->reqs));
  free(ptrigger);
}

/**************************************************************************
  Return new copy of the trigger
**************************************************************************/
struct trigger *trigger_copy(struct trigger *old)
{
  struct trigger *new_trigger = trigger_new(old->name, old->title, old->desc,
          old->mtth, old->repeatable, old->responses_num, old->responses,
          old->default_response);

  requirement_vector_iterate(&old->reqs, preq) {
    trigger_req_append(new_trigger, *preq);
  } requirement_vector_iterate_end;

  return new_trigger;
}

/**************************************************************************
  Append requirement to trigger.
**************************************************************************/
void trigger_req_append(struct trigger *ptrigger, struct requirement req)
{
  requirement_vector_append(&ptrigger->reqs, req);
}

/**************************************************************************
 * Creates Signal for trigger. Should be called once all requirements
 * are added
**************************************************************************/
void trigger_signal_create(struct trigger *ptrigger)
{
  int nargs;
  int args[ptrigger->reqs.size + 1];
  get_trigger_signal_arg_list(ptrigger, &nargs, &args);
  script_server_trigger_signal_create(ptrigger->name, nargs, args);
}

/**************************************************************************
  Initialize the ruleset cache.  The ruleset cache should be empty
  before this is done (so if it's previously been initialized, it needs
  to be freed (see trigger_cache_free) before it can be reused).
**************************************************************************/
void trigger_cache_init(void)
{
  initialized = TRUE;

  trigger_cache.triggers = trigger_list_new();
  trigger_cache.responses= trigger_response_list_new();
}

/**************************************************************************
  Free the ruleset cache.  This should be called at the end of the game or
  when the client disconnects from the server.  See trigger_cache_init.
**************************************************************************/
void trigger_cache_free(void)
{
  struct trigger_list *tracker_list = trigger_cache.triggers;
  struct trigger_list *response_list = trigger_cache.responses;

  if (tracker_list) {
    trigger_list_iterate(tracker_list, ptrigger) {
      requirement_vector_free(&ptrigger->reqs);
      free(ptrigger);
    } trigger_list_iterate_end;
    trigger_list_destroy(tracker_list);
    trigger_cache.triggers = NULL;
  }

  if (response_list) {
    trigger_response_list_destroy(response_list);
    trigger_cache.responses = NULL;
  }

  initialized = FALSE;
}

/**************************************************************************
  Checks the trigger with the given target, triggers it and returns true
  if the trigger should be triggered. Otherwise returns false and nothing
  is done.
  Note that due to the mtth mechanic a trigger will not always be  triggered
  when it's requirements are met.

  (player,city,building,tile) give the exact target
  trigger_type gives the trigger type to be considered

**************************************************************************/
bool check_trigger(struct trigger *ptrigger,
                   const struct player *target_player,
                   const struct player *other_player,
                   const struct city *target_city,
                   const struct impr_type *target_building,
                   const struct tile *target_tile,
                   const struct unit *target_unit,
                   const struct unit_type *target_unittype,
                   const struct output_type *target_output,
                   const struct specialist *target_specialist)
{
  /* For each trigger, see if it is active. */
  if (are_reqs_active(target_player, target_city,
                      target_building, target_tile,
                      target_unittype,
                      target_output, target_specialist,
      &(ptrigger->reqs), RPT_CERTAIN)) {
      int nargs = ptrigger->reqs.size;
      int args[nargs];
      get_trigger_signal_arg_list(ptrigger, nargs, args);
      script_server_trigger_emit(ptrigger->name, nargs, args);
      return TRUE;
  }

  return FALSE;
}

/**************************************************************************
  Make user-friendly text for the source.  The text is put into a user
  buffer.
**************************************************************************/
void get_trigger_req_text(const struct trigger *ptrigger,
                         char *buf, size_t buf_len)
{
  buf[0] = '\0';

  /* FIXME: should we do something for present==FALSE reqs?
   * Currently we just ignore them. */
  requirement_vector_iterate(&ptrigger->reqs, preq) {
    if (buf[0] != '\0') {
      fc_strlcat(buf, Q_("?req-list-separator:+"), buf_len);
    }

    universal_name_translation(&preq->source,
			buf + strlen(buf), buf_len - strlen(buf));
  } requirement_vector_iterate_end;
}

/****************************************************************************
  Make user-friendly text for an trigger list. The text is put into a user
  astring.
****************************************************************************/
void get_trigger_list_req_text(const struct trigger_list *plist,
                              struct astring *astr)
{
  struct strvec *psv = strvec_new();
  char req_text[512];

  trigger_list_iterate(plist, ptrigger) {
    get_trigger_req_text(ptrigger, req_text, sizeof(req_text));
    strvec_append(psv, req_text);
  } trigger_list_iterate_end;

  strvec_to_and_list(psv, astr);
  strvec_destroy(psv);
}

/**************************************************************************
  Iterate through all the triggers in cache, and call callback for each.
  This is currently not very generic implementation, as we have only one user;
  ruleset sanity checking. If any callback returns FALSE, there is no
  further checking and this will return FALSE.
**************************************************************************/
bool iterate_trigger_cache(itc_cb cb, void *data)
{
  fc_assert_ret_val(cb != NULL, FALSE);

  trigger_list_iterate(trigger_cache.triggers, ptrigger) {
    if (!cb(ptrigger, data)) {
      return FALSE;
    }
  } trigger_list_iterate_end;

  return TRUE;
}

/**************************************************************************


**************************************************************************/
void get_trigger_signal_arg_list(const struct trigger * ptrigger, int * nargs, int * args)
{
  *nargs = 0;
  requirement_vector_iterate(&ptrigger->reqs, preq) {
    switch (preq->range) {
    case REQ_RANGE_WORLD:
      switch (preq->source.kind) {
      case VUT_MINYEAR:
        args[*nargs] = API_TYPE_INT;
        break;
      default:
        continue;
      }
    case REQ_RANGE_PLAYER:
      args[*nargs] = API_TYPE_PLAYER;
      break;
    case REQ_RANGE_CITY:
      args[*nargs] = API_TYPE_CITY;
      break;
    case REQ_RANGE_CONTINENT:
    case REQ_RANGE_ADJACENT:
    case REQ_RANGE_CADJACENT:
    case REQ_RANGE_LOCAL:
      switch (preq->source.kind) {
      case VUT_TERRAIN:
        args[*nargs] = API_TYPE_TERRAIN;
        break;
      case VUT_UTYPE:
      case VUT_UTFLAG:
      case VUT_UCLASS:
      case VUT_UCFLAG:
        args[*nargs] = API_TYPE_UNIT;
        break;
      default:
        continue;
      }
      break;
    }
    (*nargs)++;
  } requirement_vector_iterate_end;

  /* Response is always last argument */
  args[*nargs] = API_TYPE_INT;
  (*nargs)++;
}

/**************************************************************************
 Fire trigger for given player
**************************************************************************/
static void trigger_for_player(struct player *pdest, struct trigger *ptrigger, const char * new_desc)
{
  struct connection *dest = NULL;       /* The 'pdest' user. */

  /* Find the user of the player 'pdest'. */
  conn_list_iterate(pdest->connections, pconn) {
    if (!pconn->observer) {
      dest = pconn;
      break;
    }
  } conn_list_iterate_end;

  if (NULL != dest) {
    send_trigger(dest, ptrigger, new_desc);
  }
}


/**************************************************************************
  Send a trigger packet.
**************************************************************************/
static void send_trigger(struct connection *pconn,
                          struct trigger *ptrigger, const char * desc)
{
  struct packet_trigger packet;
  int i;

  strcpy(packet.name, ptrigger->name);
  strcpy(packet.title, ptrigger->title);
  strcpy(packet.desc, desc);
  packet.responses_num = ptrigger->responses_num;
  for (i = 0; i < ptrigger->responses_num; i++) {
    strcpy(packet.responses[i], ptrigger->responses[i]);
  }

  send_packet_trigger(pconn, &packet);
}

/**************************************************************************
  Stringifies a luascript argument for embedding in a trigger description
**************************************************************************/
static const char * trigger_arg_to_string(enum api_types type, void * arg)
{
  const char * tmp = fc_malloc(sizeof(const char *) * 256);

  switch (type) {
  case API_TYPE_INT:
    sprintf(tmp, "%d", (int)arg);
    break;
  case API_TYPE_PLAYER:
    strcpy(tmp, ((struct player *)arg)->name);
    break;
  case API_TYPE_CITY:
    strcpy(tmp, ((struct city *)arg)->name);
    break;
  case API_TYPE_UNIT:
    strcpy(tmp,  unit_name_translation((struct unit *)arg));
    break;
  default:
    strcpy(tmp, "INVALID_ARGUMENT");
  }
  return tmp;
}

/**************************************************************************
  Fire the trigger with the given name
**************************************************************************/
void trigger_by_name_array(struct player *pplayer, const char * name, int nargs, void * args[])
{
  struct trigger *ptrigger;
  struct trigger_response *presponse;
  int i, len, index, tmplen;
  const char *newdesc, *tmp, *tmpdesc;

  presponse = fc_malloc(sizeof(*presponse));
  presponse->player = pplayer;
  presponse->turn_fired = game.info.turn;

  trigger_list_iterate(trigger_cache.triggers, ptrigger) {
    if (strcmp(name, ptrigger->name) == 0) {
      len = strlen(ptrigger->desc);
      newdesc = fc_strdup(ptrigger->desc);
      for (i = 0; i < len; i++) {
        if (newdesc[i] == '$') {
          index = atoi(&newdesc[i + 1]);
          if (index == 0 || index > nargs) continue;
          tmp = trigger_arg_to_string((enum api_types)args[(index - 1) * 2], (void *)args[(index - 1) * 2 + 1]);
          tmplen = strlen(tmp);
          tmpdesc = fc_realloc(fc_strdup(newdesc), (len + tmplen) * sizeof(const char *));
          strncpy(&tmpdesc[i], tmp, tmplen);
          free(tmp);
          strcpy(&tmpdesc[i + tmplen], &newdesc[i + (int)floor(log10(abs(index))) + 2]);
          newdesc = tmpdesc;
          len += tmplen;
          i += tmplen;
        }
      }

      presponse->trigger = ptrigger;
      presponse->args = args;
      presponse->nargs = nargs;
      trigger_response_list_append(trigger_cache.responses, presponse);
      trigger_for_player(pplayer, ptrigger, newdesc);
      return;
    }
  } trigger_list_iterate_end;
  free(presponse);
}

void trigger_by_name(struct player *pplayer, const char * name, int nargs, ...)
{
  va_list args;
  int i;
  void **arg_list = fc_malloc(sizeof(void*) * (nargs * 2));

  va_start(args, nargs);
  for (i = 0; i < nargs * 2; i++) {
    arg_list[i] = (void*)va_arg(args, void*);
  }

  trigger_by_name_array(pplayer, name, nargs, arg_list);
  va_end(args);
}

struct trigger_response * remove_trigger_response_from_cache(struct player *pplayer, const char * name)
{
  struct trigger_response * matching_response = NULL;
  struct trigger * matching_trigger = NULL;
  trigger_response_list_iterate(trigger_cache.responses, presponse) {
    if (player_number(presponse->player) == player_number(pplayer) && strcmp(name, presponse->trigger->name) == 0) {
       matching_response = presponse;
       break;
    }
  } trigger_response_list_iterate_end;
  if (matching_response != NULL) {
    trigger_response_list_remove(trigger_cache.responses, matching_response);
  }
  return matching_response;
}

void send_pending_triggers(struct connection *pconn)
{
  const struct player *pplayer = conn_get_player(pconn);
  bool is_global_observer = conn_is_global_observer(pconn);
  if (!pplayer) return;

  if (!is_global_observer) {
    trigger_response_list_iterate(trigger_cache.responses, presponse) {
      if (presponse->player && player_number(presponse->player) == player_number(pplayer)) {
        trigger_by_name_array(pplayer, presponse->trigger->name, presponse->nargs, presponse->args);
      }
    } trigger_response_list_iterate_end;
  }
}

void handle_expired_triggers()
{
  trigger_response_list_iterate(trigger_cache.responses, presponse) {
    if (game.info.turn - presponse->turn_fired > trigger_timeout) {
      /* Trigger has expired, fire default response */
      handle_trigger_response_player(presponse->player, presponse->trigger->name, presponse->trigger->default_response);
    }
  } trigger_response_list_iterate_end;
}

void set_trigger_timeout(int timeout)
{
  trigger_timeout = timeout;
}
