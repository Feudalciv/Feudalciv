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
#ifndef FC__WORKLIST_H
#define FC__WORKLIST_H

#include "registry.h"
#include "shared.h"		/* MAX_LEN_NAME */

#include "fc_types.h"

#define MAX_LEN_WORKLIST 64
#define MAX_NUM_WORKLISTS 16

/* a worklist */
struct worklist {
  bool is_valid;
  int length;
  char name[MAX_LEN_NAME];
  struct city_production entries[MAX_LEN_WORKLIST];
};

void init_worklist(struct worklist *pwl);

int worklist_length(const struct worklist *pwl);
bool worklist_is_empty(const struct worklist *pwl);
bool worklist_peek(const struct worklist *pwl, struct city_production *prod);
bool worklist_peek_ith(const struct worklist *pwl,
		       struct city_production *prod, int idx);
void worklist_advance(struct worklist *pwl);

void copy_worklist(struct worklist *dst, const struct worklist *src);
void worklist_remove(struct worklist *pwl, int idx);
bool worklist_append(struct worklist *pwl, struct city_production prod);
bool worklist_insert(struct worklist *pwl, struct city_production prod,
		     int idx);
bool are_worklists_equal(const struct worklist *wlist1,
			 const struct worklist *wlist2);

/* Functions to load and save a worklist from a registry file.  The path
 * is a printf-style string giving the registry prefix (which must be
 * the same for saving and loading). */
void worklist_load(struct section_file *file, struct worklist *pwl,
		   const char *path, ...)
  fc__attribute((format (printf, 3, 4)));
void worklist_save(struct section_file *file, struct worklist *pwl,
		   const char *path, ...)
  fc__attribute((format (printf, 3, 4)));

/* Iterate over all entries in the worklist. */
#define worklist_iterate(worklist, prod)				    \
{									    \
  struct worklist *_worklist = (worklist);				    \
  int _iter, _length = worklist_length(_worklist);			    \
  struct city_production prod;						    \
									    \
  for (_iter = 0; _iter < _length; _iter++) {				    \
    worklist_peek_ith(_worklist, &prod, _iter);

#define worklist_iterate_end						    \
  }									    \
}

#endif /* FC__WORKLIST_H */
