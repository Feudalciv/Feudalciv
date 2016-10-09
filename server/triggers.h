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
#ifndef FC__TRIGGERS_H
#define FC__TRIGGERS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* utility */
#include "support.h"            /* bool type */

/* common */
#include "connection.h"
#include "fc_types.h"


#include "requirements.h"

/* TODO: New description */
/* A trigger is provided by a source.  If the source is present, and the
 * other conditions (described below) are met, the trigger will active.
 * Note the difference between trigger and trigger_type. */
struct trigger {
  const char * name;
  const char * mtth;
  bool repeatable;

  const char * title;
  const char * desc;
  int responses_num;;
  const char ** responses;
  int default_response;
  int ai_response;

  /* An trigger can have multiple requirements.  The trigger will only be
   * active if all of these requirement are met. */
  struct requirement_vector reqs;
};

struct trigger_response {
  const struct trigger *trigger;
  const struct player *player;
  int turn_fired;
  int nargs;
  void **args;
};

/* A trigger_list is a list of triggers. */
#define SPECLIST_TAG trigger
#define SPECLIST_TYPE struct trigger
#include "speclist.h"
#define trigger_list_iterate(trigger_list, ptrigger) \
  TYPED_LIST_ITERATE(struct trigger, trigger_list, ptrigger)
#define trigger_list_iterate_end LIST_ITERATE_END

/* A trigger_response_list is a list of trigger responses. */
#define SPECLIST_TAG trigger_response
#define SPECLIST_TYPE struct trigger_response
#include "speclist.h"
#define trigger_response_list_iterate(trigger_response_list, ptrigger_response) \
  TYPED_LIST_ITERATE(struct trigger_response, trigger_response_list, ptrigger_response)
#define trigger_response_list_iterate_end LIST_ITERATE_END

struct trigger *trigger_new(const char * name, const char * title, const char * desc,
        const char * mtth, bool repeatable, int num_responses, const char **responses,
        int default_response, int ai_response);
struct trigger *trigger_copy(struct trigger *old);
void trigger_req_append(struct trigger *ptrigger, struct requirement req);
void trigger_signal_create(struct trigger *ptrigger);

struct astring;
void get_trigger_req_text(const struct trigger *ptrigger,
                         char *buf, size_t buf_len);
void get_trigger_list_req_text(const struct trigger_list *plist,
                              struct astring *astr);


void trigger_cache_init(void);
void trigger_cache_free(void);

typedef bool (*itc_cb)(struct trigger*, void *data);
bool iterate_trigger_cache(itc_cb cb, void *data);

void trigger_by_name(const struct player *pplayer, const char * name, int nargs, ...);
void trigger_by_name_array(const struct player *pplayer, const char * name, int nargs, void * args[]);

struct trigger_response * remove_trigger_response_from_cache(const struct player *pplayer, const char * signal);

void send_pending_triggers(struct conn_list *dest);

void set_trigger_timeout(int timout);

void trigger_cache_load(struct section_file *file, const char *section);

void trigger_cache_save(struct section_file *file, const char *section);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__TRIGGERS_H */
