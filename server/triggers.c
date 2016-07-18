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

/* server */
#include "script_server.h"

#include "triggers.h"

static void send_trigger(struct connection *pconn,
                         struct trigger *ptrigger);
void get_trigger_signal_arg_list(const struct trigger * ptrigger, int args[]);

static bool initialized = FALSE;

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
struct trigger *trigger_new(const char* signal, const char* mtth,
                          bool repeatable)
{
  struct trigger *ptrigger;

  /* Create the trigger. */
  ptrigger = fc_malloc(sizeof(*ptrigger));
  ptrigger->signal = signal;
  ptrigger->mtth = mtth;
  ptrigger->repeatable = repeatable;

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
  struct trigger *new_eff = trigger_new(old->signal, old->mtth,
                                      old->repeatable);

  requirement_vector_iterate(&old->reqs, preq) {
    trigger_req_append(new_eff, *preq);
  } requirement_vector_iterate_end;

  return new_eff;
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
  int nargs = ptrigger->reqs.size;
  int args[nargs];
  get_trigger_signal_arg_list(ptrigger, args);
  script_server_trigger_signal_create(ptrigger->signal, nargs, args);
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
}

/**************************************************************************
  Free the ruleset cache.  This should be called at the end of the game or
  when the client disconnects from the server.  See trigger_cache_init.
**************************************************************************/
void trigger_cache_free(void)
{
  struct trigger_list *tracker_list = trigger_cache.triggers;

  if (tracker_list) {
    trigger_list_iterate(tracker_list, ptrigger) {
      requirement_vector_free(&ptrigger->reqs);
      free(ptrigger);
    } trigger_list_iterate_end;
    trigger_list_destroy(tracker_list);
    trigger_cache.triggers = NULL;
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
      get_trigger_signal_arg_list(ptrigger, args);
      script_server_trigger_emit(ptrigger->signal, nargs, args);
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
void get_trigger_signal_arg_list(const struct trigger * ptrigger, int args[])
{
  requirement_vector_iterate(&ptrigger->reqs, preq) {

  } requirement_vector_iterate_end;
}

/**************************************************************************
 Fire trigger for given player
**************************************************************************/
static void trigger_for_player(struct player *pdest, struct trigger *ptrigger)
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
    send_trigger(dest, ptrigger);
  }
}


/**************************************************************************
  Send a trigger packet.
**************************************************************************/
static void send_trigger(struct connection *pconn,
                          struct trigger *ptrigger)
{
  struct packet_trigger packet;
  int i;

  strcpy(packet.name, ptrigger->signal);
  strcpy(packet.title, ptrigger->title);
  strcpy(packet.desc, ptrigger->desc);
  packet.responses_num = ptrigger->responses_num;
  for (i = 0; i < ptrigger->responses_num; i++) {
    strcpy(packet.responses[i], ptrigger->responses[i]);
  }

  send_packet_trigger(pconn, &packet);
}

/**************************************************************************
  Fire the trigger with the given name
**************************************************************************/
void trigger_by_name(struct player *pplayer, const char * name);
{
  struct trigger *ptrigger;
  /* TODO: Find trigger by name */
  trigger_for_player(pplayer, ptrigger);
}

