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

#ifndef FC__MAPVIEW_COMMON_H
#define FC__MAPVIEW_COMMON_H

#include "shared.h"		/* bool type */

#include "colors_g.h"

/*
The bottom row of the map was sometimes hidden.

As of now the top left corner is always aligned with the tiles. This
is what causes the problem in the first place. The ideal solution
would be to align the window with the bottom left tiles if you tried
to center the window on a tile closer than (screen_tiles_height/2-1)
to the south pole.

But, for now, I just grepped for occurences where the ysize (or the
values derived from it) were used, and those places that had relevance
to drawing the map, and I added 1 (using the EXTRA_BOTTOM_ROW
constant).

-Thue
*/

/* If we have isometric view we need to be able to scroll a little
 *  extra down.  The places that needs to be adjusted are the same as
 *  above. 
*/
#define EXTRA_BOTTOM_ROW (is_isometric ? 6 : 1)

void refresh_tile_mapcanvas(int x, int y, bool write_to_screen);
enum color_std get_grid_color(int x1, int y1, int x2, int y2);

bool get_canvas_xy(int map_x, int map_y, int *canvas_x, int *canvas_y);
void get_map_xy(int canvas_x, int canvas_y, int *map_x, int *map_y);

void get_center_tile_mapcanvas(int *map_x, int *map_y);
void base_center_tile_mapcanvas(int map_x, int map_y,
				int *map_view_topleft_map_x,
				int *map_view_topleft_map_y,
				int map_view_map_width,
				int map_view_map_height);

void update_map_canvas_visible(void);
				
struct city *find_city_near_tile(int x, int y);

void get_city_mapview_production(struct city *pcity,
                                 char *buf, size_t buf_len);
void get_city_mapview_name_and_growth(struct city *pcity,
				      char *name_buffer,
				      size_t name_buffer_len,
				      char *growth_buffer,
				      size_t growth_buffer_len,
				      enum color_std *grwoth_color);

void queue_mapview_update(void);
void unqueue_mapview_update(void);

#endif /* FC__MAPVIEW_COMMON_H */
