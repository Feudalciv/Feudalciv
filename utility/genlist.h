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
#ifndef FC__GENLIST_H
#define FC__GENLIST_H

/********************************************************************** 
  MODULE: genlist

  A "genlist" is a generic doubly-linked list.  That is:
    generic:        stores (void*) pointers to arbitrary user data;
    doubly-linked:  can be efficiently traversed both "forwards"
                    and "backwards".
		    
  The list data structures are allocated dynamically, and list
  elements can be added or removed at arbitrary positions.
  
  Positions in the list are specified starting from 0, up to n-1
  for a list with n elements.  The position -1 can be used to
  refer to the last element (that is, the same as n-1, or n when
  adding a new element), but other negative numbers are not
  meaningful.

  There are two memory management issues:
  
  - The user-data pointed to by the genlist elements; these are
    entirely the user's responsibility, and the genlist code just
    treats these as opaque data, not doing any allocation or freeing.
    
  - The data structures used internally by the genlist to store
    data for the links etc.  These are allocated by genlist_insert(),
    and freed by genlist_unlink() and genlist_clear().  That is,
    it is the responsibility of the user to call the unlink functions
    as necessary to avoid memory leaks.

  A trap to beware of with iterators is modifying the list while the 
  iterator is active, in particular removing the next element pointed 
  to by the iterator (see further comments below).

  See also the speclist module.
***********************************************************************/

#include "support.h"    /* bool */

/* A single element of a genlist, opaque type. */
struct genlist_link;

/* A genlist, opaque type. */
struct genlist;

/* Function type definitions. */
typedef void (*genlist_free_fn_t) (void *);
typedef void * (*genlist_copy_fn_t) (const void *);
typedef bool (*genlist_comp_fn_t) (const void *, const void *);

struct genlist *genlist_new(void);
struct genlist *genlist_new_full(genlist_free_fn_t free_data_func);
void genlist_destroy(struct genlist *pgenlist);

struct genlist *genlist_copy(const struct genlist *pgenlist);
struct genlist *genlist_copy_full(const struct genlist *pgenlist,
                                  genlist_copy_fn_t copy_data_func,
                                  genlist_free_fn_t free_data_func);

void genlist_clear(struct genlist *pgenlist);
void genlist_unique(struct genlist *pgenlist);
void genlist_unique_full(struct genlist *pgenlist,
                         genlist_comp_fn_t comp_data_func);
void genlist_append(struct genlist *pgenlist, void *data);
void genlist_prepend(struct genlist *pgenlist, void *data);
void genlist_insert(struct genlist *pgenlist, void *data, int idx);
bool genlist_remove(struct genlist *pgenlist, void *data);

int genlist_size(const struct genlist *pgenlist);
void *genlist_get(const struct genlist *pgenlist, int idx);

bool genlist_search(const struct genlist *pgenlist, const void *data);

void genlist_sort(struct genlist *pgenlist,
                  int (*compar)(const void *, const void *));
void genlist_shuffle(struct genlist *pgenlist);

const struct genlist_link *genlist_head(const struct genlist *pgenlist);
const struct genlist_link *genlist_tail(const struct genlist *pgenlist);
void *genlist_link_data(const struct genlist_link *plink);
const struct genlist_link *genlist_link_prev(const struct genlist_link *plink);
const struct genlist_link *genlist_link_next(const struct genlist_link *plink);


#ifdef DEBUG
#  define TYPED_LIST_CHECK(typed_list) \
  fc_assert_action(NULL != typed_list, break)
#else
#  define TYPED_LIST_CHECK(typed_list) /* Nothing. */
#endif /* DEBUG */

/*
 * This is to iterate for a type defined like with speclist.h
 * where the pointers in the list are really pointers to "atype".
 * Eg, see speclist.h, which is what this is really for.
*/
#define TYPED_LIST_ITERATE(atype, typed_list, var)                          \
do {                                                                        \
  const struct genlist_link *myiter;                                        \
  atype *var;                                                               \
                                                                            \
  TYPED_LIST_CHECK(typed_list);                                             \
  myiter = genlist_head((const struct genlist *) typed_list);               \
  for (; genlist_link_data(myiter);) {                                      \
    var = (atype *) genlist_link_data(myiter);                              \
    myiter = genlist_link_next(myiter);

/* Balance for above: */ 
#define LIST_ITERATE_END                                                    \
  }                                                                         \
} while (FALSE);


/* Same, but iterate backwards: */
#define TYPED_LIST_ITERATE_REV(atype, typed_list, var)                      \
do {                                                                        \
  const struct genlist_link *myiter;                                        \
  atype *var;                                                               \
                                                                            \
  TYPED_LIST_CHECK(typed_list);                                             \
  myiter = genlist_tail((const struct genlist *) typed_list);               \
  for (; genlist_link_data(myiter);) {                                      \
    var = (atype *) genlist_link_data(myiter);                              \
    myiter = genlist_link_prev(myiter);

/* Balance for above: */ 
#define LIST_ITERATE_REV_END                                                \
  }                                                                         \
} while (FALSE);

#endif  /* FC__GENLIST_H */
