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
#ifndef FC__AITOOLS_H
#define FC__AITOOLS_H

#include "shared.h"		/* bool type */

struct ai_choice;
struct city;
struct government;
struct player;
struct unit;

enum bodyguard_enum {
  BODYGUARD_WANTED=-1,
  BODYGUARD_NONE
};

struct unit *create_unit_virtual(struct player *pplayer, int x, int y,
				 Unit_Type_id type, bool make_veteran);
void destroy_unit_virtual(struct unit *punit);
int is_stack_vulnerable(int x, int y);

void ai_unit_new_role(struct unit *punit, enum ai_unit_task utask);
bool ai_unit_make_homecity(struct unit *punit, struct city *pcity);
void ai_unit_attack(struct unit *punit, int x, int y);
bool ai_unit_move(struct unit *punit, int x, int y);

struct city *dist_nearest_city(struct player *pplayer, int x, int y,
                               bool everywhere, bool enemy);

void ai_government_change(struct player *pplayer, int gov);
int ai_gold_reserve(struct player *pplayer);

void init_choice(struct ai_choice *choice);
void adjust_choice(int value, struct ai_choice *choice);
void copy_if_better_choice(struct ai_choice *cur, struct ai_choice *best);
void ai_advisor_choose_building(struct city *pcity, struct ai_choice *choice);
bool ai_assess_military_unhappiness(struct city *pcity, struct government *g);

int ai_evaluate_government(struct player *pplayer, struct government *g);

#endif  /* FC__AITOOLS_H */
