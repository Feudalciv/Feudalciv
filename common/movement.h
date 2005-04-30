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
#ifndef FC__MOVEMENT_H
#define FC__MOVEMENT_H

#include "fc_types.h"
#include "terrain.h"
#include "unit.h"       /* enum unit_activity */
#include "unittype.h"

int unit_move_rate(const struct unit *punit);
bool unit_can_defend_here(const struct unit *punit);

bool is_sailing_unit(const struct unit *punit);
bool is_air_unit(const struct unit *punit);
bool is_heli_unit(const struct unit *punit);
bool is_ground_unit(const struct unit *punit);
bool is_sailing_unittype(Unit_type_id id);
bool is_air_unittype(Unit_type_id id);
bool is_heli_unittype(Unit_type_id id);
bool is_ground_unittype(Unit_type_id id);

enum unit_move_type unit_move_type_from_str(const char *s);

bool can_unit_exist_at_tile(const struct unit *punit, const struct tile *ptile);
bool can_unit_survive_at_tile(const struct unit *punit,
			      const struct tile *ptile);
bool can_step_taken_wrt_to_zoc(Unit_type_id type,
			       const struct player *unit_owner,
			       const struct tile *src_tile,
			       const struct tile *dst_tile);
bool zoc_ok_move(const struct unit *punit, const struct tile *ptile);
bool can_unit_move_to_tile(const struct unit *punit, const struct tile *ptile,
			   bool igzoc);
enum unit_move_result test_unit_move_to_tile(Unit_type_id type,
					     const struct player *unit_owner,
					     enum unit_activity activity,
					     const struct tile *src_tile,
					     const struct tile *dst_tile,
					     bool igzoc);
bool can_unit_transport(const struct unit *transporter, const struct unit *transported);

#endif  /* FC__MOVEMENT_H */
