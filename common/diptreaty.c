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
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "genlist.h"
#include "log.h"
#include "mem.h"
#include "player.h"

#include "diptreaty.h"

#define SPECLIST_TAG clause
#define SPECLIST_TYPE struct Clause
#include "speclist_c.h"

/****************************************************************
...
*****************************************************************/
void init_treaty(struct Treaty *ptreaty, 
		 struct player *plr0, struct player *plr1)
{
  ptreaty->plr0=plr0;
  ptreaty->plr1=plr1;
  ptreaty->accept0 = FALSE;
  ptreaty->accept1 = FALSE;
  clause_list_init(&ptreaty->clauses);
}


/****************************************************************
...
*****************************************************************/
bool remove_clause(struct Treaty *ptreaty, struct player *pfrom, 
		  enum clause_type type, int val)
{
  clause_list_iterate(ptreaty->clauses, pclause) {
    if(pclause->type==type && pclause->from==pfrom &&
       pclause->value==val) {
      clause_list_unlink(&ptreaty->clauses, pclause);
      free(pclause);

      ptreaty->accept0 = FALSE;
      ptreaty->accept1 = FALSE;

      return TRUE;
    }
  } clause_list_iterate_end;

  return FALSE;
}


/****************************************************************
...
*****************************************************************/
bool add_clause(struct Treaty *ptreaty, struct player *pfrom, 
		enum clause_type type, int val)
{
  struct Clause *pclause;

  if (type == CLAUSE_ADVANCE && !tech_exists(val)) {
    freelog(LOG_ERROR, "Illegal tech value %i in clause.", val);
    return FALSE;
  }
  
  clause_list_iterate(ptreaty->clauses, pclause) {
    if(pclause->type==type
       && pclause->from==pfrom
       && pclause->value==val) {
      /* same clause already there */
      return FALSE;
    }
    if(is_pact_clause(type) &&
       is_pact_clause(pclause->type)) {
      /* pact clause already there */
      ptreaty->accept0 = FALSE;
      ptreaty->accept1 = FALSE;
      pclause->type=type;
      return TRUE;
    }
    if (type == CLAUSE_GOLD && pclause->type==CLAUSE_GOLD &&
        pclause->from==pfrom) {
      /* gold clause there, different value */
      ptreaty->accept0 = FALSE;
      ptreaty->accept1 = FALSE;
      pclause->value=val;
      return TRUE;
    }
  } clause_list_iterate_end;
   
  pclause=(struct Clause *)fc_malloc(sizeof(struct Clause));

  pclause->type=type;
  pclause->from=pfrom;
  pclause->value=val;
  
  clause_list_insert_back(&ptreaty->clauses, pclause);

  ptreaty->accept0 = FALSE;
  ptreaty->accept1 = FALSE;

  return TRUE;
}
