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
#ifndef FC_WAR_H
#define FC_WAR_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "support.h"            /* bool type */

struct war {
  /* War leader for the attacking side */
  struct player * aggressor;
  /* other participants on the attacking side */
  struct player_list * aggressors;

  /* War leader for the defending side */
  struct player * defender;
  /* other participants on the attacking side */
  struct player_list * defenders;

  const char * casus_belli;
};

#define SPECLIST_TAG war
#define SPECLIST_TYPE struct war
#include "speclist.h"

struct war_list *wars;

#define war_list_iterate(warlist, pwar) \
    TYPED_LIST_ITERATE(struct war, warlist, pwar)
#define war_list_iterate_end  LIST_ITERATE_END

void war_cache_init();

void war_cache_free();

void war_cache_load(struct section_file *file, const char *section);

void war_cache_save(struct section_file *file, const char *section);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FC__WARY_H */
