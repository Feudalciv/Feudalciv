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
#ifndef FC__AIUNIT_H
#define FC__AIUNIT_H

#include "unittype.h"
#include "combat.h"

/*
 * To prevent integer overflows the product "power * hp * firepower"
 * is divided by POWER_DIVIDER.
 *
 * The constant may be changed since it isn't externally visible used.
 */
#define POWER_DIVIDER 	(POWER_FACTOR * 3)

/* Simple military power macros */
#define DEFENCE_POWER(punit) \
 (unit_type(punit)->defense_strength * unit_type(punit)->hp \
  * unit_type(punit)->firepower)
#define ATTACK_POWER(punit) \
 (unit_type(punit)->attack_strength * unit_type(punit)->hp \
  * unit_type(punit)->firepower)
#define IS_ATTACKER(punit) \
  (unit_type(punit)->attack_strength \
        > unit_type(punit)->transport_capacity)
#define COULD_OCCUPY(punit) \
  (is_ground_unit(punit) || is_heli_unit(punit))

struct player;
struct city;
struct unit;
struct ai_choice;

extern Unit_Type_id simple_ai_types[U_LAST];

void ai_manage_units(struct player *pplayer); 
int could_unit_move_to_tile(struct unit *punit, int dest_x, int dest_y);
int look_for_charge(struct player *pplayer, struct unit *punit,
                    struct unit **aunit, struct city **acity);

bool ai_manage_explorer(struct unit *punit);

int turns_to_enemy_city(Unit_Type_id our_type, struct city *acity,
                        int speed, bool go_by_boat, 
                        struct unit *boat, Unit_Type_id boattype);
int turns_to_enemy_unit(Unit_Type_id our_type, int speed, int x, int y, 
                        Unit_Type_id enemy_type);
int find_something_to_kill(struct player *pplayer, struct unit *punit, 
                            int *x, int *y);
int find_beachhead(struct unit *punit, int dest_x, int dest_y, int *x, int *y);

int build_cost_balanced(Unit_Type_id type);
int base_unit_belligerence_primitive(Unit_Type_id type, bool veteran,
				     int moves_left, int hp);
int unit_belligerence_basic(struct unit *punit);
int unit_vulnerability_virtual(struct unit *punit);
int unit_vulnerability_virtual2(Unit_Type_id att_type, Unit_Type_id def_type,
				int x, int y, bool fortified, bool veteran,
				bool use_alternative_hp, int alternative_hp);
int kill_desire(int benefit, int attack, int loss, int vuln, int attack_count);

bool is_on_unit_upgrade_path(Unit_Type_id test, Unit_Type_id base);

Unit_Type_id ai_wants_role_unit(struct player *pplayer, struct city *pcity,
                                int role, int want);
void ai_choose_role_unit(struct player *pplayer, struct city *pcity,
			 struct ai_choice *choice, int role, int want);
void update_simple_ai_types(void);

#define simple_ai_unit_type_iterate(m_i)                                      \
{                                                                             \
  int m_c;                                                                    \
  for (m_c = 0;; m_c++) {                                                     \
    Unit_Type_id m_i = simple_ai_types[m_c];                                  \
    if (m_i == U_LAST) {                                                      \
      break;                                                                  \
    }

#define simple_ai_unit_type_iterate_end                                       \
 }                                                                            \
}

#endif  /* FC__AIUNIT_H */
