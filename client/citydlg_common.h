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

#ifndef FC__CITYDLG_COMMON_H
#define FC__CITYDLG_COMMON_H

#include <stddef.h>		/* size_t */

#include "shared.h"		/* bool type */

struct city;

enum citizen_type {
  CITIZEN_ELVIS,
  CITIZEN_SCIENTIST,
  CITIZEN_TAXMAN,
  CITIZEN_CONTENT,
  CITIZEN_HAPPY,
  CITIZEN_UNHAPPY,
  CITIZEN_ANGRY,
  CITIZEN_LAST
};

void city_pos_to_canvas_pos(int city_x, int city_y, int *canvas_x, int *canvas_y);
void canvas_pos_to_city_pos(int canvas_x, int canvas_y, int *map_x, int *map_y);

void get_city_dialog_production(struct city *pcity,
                                char *buffer, size_t buffer_len);
void get_city_dialog_production_full(char *buffer, size_t buffer_len,
				     int id, bool is_unit,
				     struct city *pcity);
void get_city_dialog_production_row(char *buf[], size_t column_size, int id,
				    bool is_unit, struct city *pcity);

void get_city_citizen_types(struct city *pcity, int index,
			    enum citizen_type *citizens);

void activate_all_units(int map_x, int map_y);

#endif /* FC__CITYDLG_COMMON_H */
