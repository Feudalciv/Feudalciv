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

#include <gtk/gtk.h>

#include "gtkpixcomm.h"

#include "citydlg_common.h"
#include "mapview_g.h"
#include "mapview_common.h"

#include "graphics.h"

struct unit;
struct city;

GdkPixmap *get_thumb_pixmap(int onoff);

gboolean overview_canvas_expose(GtkWidget *w, GdkEventExpose *ev, gpointer data);
gboolean map_canvas_expose(GtkWidget *w, GdkEventExpose *ev, gpointer data);
gboolean map_canvas_configure(GtkWidget *w, GdkEventConfigure *ev,
			      gpointer data);

void put_city_tile_output(GdkDrawable *pm, int canvas_x, int canvas_y, 
			  int food, int shield, int trade);
void put_unit_gpixmap(struct unit *punit, GtkPixcomm *p);

void put_unit_gpixmap_city_overlays(struct unit *punit, GtkPixcomm *p);
void put_one_tile_full(GdkDrawable *pm, int x, int y,
		       int canvas_x, int canvas_y, int citymode);
void pixmap_frame_tile_red(GdkDrawable *pm,
			   int canvas_x, int canvas_y);
void pixmap_put_tile(GdkDrawable *pm, int x, int y,
		     int canvas_x, int canvas_y, int citymode);
void pixmap_put_black_tile(GdkDrawable *pm,
			   int canvas_x, int canvas_y);

void pixmap_put_sprite_full(GdkDrawable *pixmap,
			    int pixmap_x, int pixmap_y,
			    struct Sprite *ssprite);

void scrollbar_jump_callback(GtkAdjustment *adj, gpointer hscrollbar);
void update_map_canvas_scrollbars_size(void);

/* These values are stored in the mapview_canvas struct now. */
#define map_view_x0 mapview_canvas.map_x0
#define map_view_y0 mapview_canvas.map_y0
#define map_canvas_store_twidth mapview_canvas.tile_width
#define map_canvas_store_theight mapview_canvas.tile_height

#endif  /* FC__MAPVIEW_H */
