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
#ifndef FC__MAPVIEW_H
#define FC__MAPVIEW_H

#include <X11/Intrinsic.h>

#include "mapview_g.h"

#include "citydlg_common.h"

#include "graphics.h"

struct unit;
struct city;

Pixmap get_thumb_pixmap(int onoff);
Pixmap get_citizen_pixmap(enum citizen_type type, int cnum,
			  struct city *pcity);

void put_unit_pixmap(struct unit *punit, Pixmap pm,
		     int canvas_x, int canvas_y);

void put_unit_pixmap_city_overlays(struct unit *punit, Pixmap pm);

void put_city_tile_output(Pixmap pm, int canvas_x, int canvas_y, 
			  int food, int shield, int trade);

void overview_canvas_expose(Widget w, XEvent *event, Region exposed, 
			    void *client_data);
void map_canvas_expose(Widget w, XEvent *event, Region exposed, 
		       void *client_data);
void map_canvas_resize(void);

void pixmap_put_tile(Pixmap pm, int x, int y, int canvas_x, int canvas_y, 
		     int citymode);
void pixmap_put_black_tile(Pixmap pm, int canvas_x, int canvas_y);
void pixmap_frame_tile_red(Pixmap pm, int canvas_x, int canvas_y);

void scrollbar_jump_callback(Widget scrollbar, XtPointer client_data,
			     XtPointer percent_ptr);
void scrollbar_scroll_callback(Widget w, XtPointer client_data,
			     XtPointer position_ptr);

/* These values are stored in the mapview_canvas struct now. */
#define map_view_x0 mapview_canvas.map_x0
#define map_view_y0 mapview_canvas.map_y0
#define map_canvas_store_twidth mapview_canvas.tile_width
#define map_canvas_store_theight mapview_canvas.tile_height

#endif  /* FC__MAPVIEW_H */
