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

/* An trigger is provided by a source.  If the source is present, and the
 * other conditions (described below) are met, the trigger will be active.
 * Note the difference between trigger and trigger_type. */
struct trigger {
  const char * signal;
  const char * mtth;
  bool repeatable;

  const char * title;
  const char * desc;
  const int responses_num;;
  const char ** responses;

  /* An trigger can have multiple requirements.  The trigger will only be
   * active if all of these requirement are met. */
  struct requirement_vector reqs;
};

/* An trigger_list is a list of triggers. */
#define SPECLIST_TAG trigger
#define SPECLIST_TYPE struct trigger
#include "speclist.h"
#define trigger_list_iterate(trigger_list, ptrigger) \
  TYPED_LIST_ITERATE(struct trigger, trigger_list, ptrigger)
#define trigger_list_iterate_end LIST_ITERATE_END

struct trigger *trigger_new(const char * signal, const char * mtth, bool repeatable);
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

bool check_trigger(struct trigger *ptrigger,
                   const struct player *target_player,
                   const struct player *other_player,
                   const struct city *target_city,
                   const struct impr_type *target_building,
                   const struct tile *target_tile,
                   const struct unit *target_unit,
                   const struct unit_type *target_unittype,
                   const struct output_type *target_output,
                   const struct specialist *target_specialist);

void trigger_by_name(struct player *pplayer, const char * name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif  /* FC__TRIGGERS_H */
