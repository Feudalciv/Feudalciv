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

#include <assert.h>
#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gtk/gtk.h>

#include "fcintl.h"
#include "game.h"
#include "government.h"		/* government_graphic() */
#include "log.h"
#include "map.h"
#include "mem.h"
#include "player.h"
#include "rand.h"
#include "support.h"
#include "timing.h"

#include "civclient.h"
#include "climap.h"
#include "climisc.h"
#include "colors.h"
#include "control.h" /* get_unit_in_focus() */
#include "goto.h"
#include "graphics.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "mapctrl.h"
#include "options.h"
#include "tilespec.h"

#include "citydlg.h" /* For reset_city_dialogs() */
#include "mapview.h"

#define map_canvas_store (mapview_canvas.store->v.pixmap)

static void pixmap_put_overlay_tile(GdkDrawable *pixmap,
				    int canvas_x, int canvas_y,
				    struct Sprite *ssprite);

static void pixmap_put_overlay_tile_draw(GdkDrawable *pixmap,
					 int canvas_x, int canvas_y,
					 struct Sprite *ssprite,
					 int offset_x, int offset_y,
					 int width, int height,
					 bool fog);
static void pixmap_put_tile_iso(GdkDrawable *pm, int x, int y,
				int canvas_x, int canvas_y,
				int citymode,
				int offset_x, int offset_y, int offset_y_unit,
				int width, int height, int height_unit,
				enum draw_type draw);
static void pixmap_put_black_tile_iso(GdkDrawable *pm,
				      int canvas_x, int canvas_y,
				      int offset_x, int offset_y,
				      int width, int height);

/* the intro picture is held in this pixmap, which is scaled to
   the screen size */
static SPRITE *scaled_intro_sprite = NULL;

static GtkObject *map_hadj, *map_vadj;


/**************************************************************************
  If do_restore is FALSE it will invert the turn done button style. If
  called regularly from a timer this will give a blinking turn done
  button. If do_restore is TRUE this will reset the turn done button
  to the default style.
**************************************************************************/
void update_turn_done_button(bool do_restore)
{
  static bool flip = FALSE;

  if (!get_turn_done_button_state()) {
    return;
  }

  if ((do_restore && flip) || !do_restore) {
    GdkGC *fore = turn_done_button->style->bg_gc[GTK_STATE_NORMAL];
    GdkGC *back = turn_done_button->style->light_gc[GTK_STATE_NORMAL];

    turn_done_button->style->bg_gc[GTK_STATE_NORMAL] = back;
    turn_done_button->style->light_gc[GTK_STATE_NORMAL] = fore;

    gtk_expose_now(turn_done_button);

    flip = !flip;
  }
}

/**************************************************************************
...
**************************************************************************/
void update_timeout_label(void)
{
  char buffer[512];

  if (game.timeout <= 0)
    sz_strlcpy(buffer, Q_("?timeout:off"));
  else
    format_duration(buffer, sizeof(buffer), seconds_to_turndone);
  gtk_label_set_text(GTK_LABEL(timeout_label), buffer);
}

/**************************************************************************
...
**************************************************************************/
void update_info_label( void )
{
  char buffer	[512];
  int  d;
  int  sol, flake;

  gtk_frame_set_label(GTK_FRAME(main_frame_civ_name),
		      get_nation_name(game.player_ptr->nation));

  my_snprintf(buffer, sizeof(buffer),
	      _("Population: %s\nYear: %s\n"
		"Gold: %d\nTax: %d Lux: %d Sci: %d"),
	      population_to_text(civ_population(game.player_ptr)),
	      textyear(game.year), game.player_ptr->economic.gold,
	      game.player_ptr->economic.tax,
	      game.player_ptr->economic.luxury,
	      game.player_ptr->economic.science);

  gtk_label_set_text(GTK_LABEL(main_label_info), buffer);

  sol = client_warming_sprite();
  flake = client_cooling_sprite();
  set_indicator_icons(client_research_sprite(),
		      sol,
		      flake,
		      game.player_ptr->government);

  d=0;
  for (; d < game.player_ptr->economic.luxury /10; d++) {
    struct Sprite *sprite = get_citizen_sprite(CITIZEN_ELVIS, d, NULL);
    gtk_image_set_from_pixmap(GTK_IMAGE(econ_label[d]),
			      sprite->pixmap, sprite->mask);
  }
 
  for (; d < (game.player_ptr->economic.science
	     + game.player_ptr->economic.luxury) / 10; d++) {
    struct Sprite *sprite = get_citizen_sprite(CITIZEN_SCIENTIST, d, NULL);
    gtk_image_set_from_pixmap(GTK_IMAGE(econ_label[d]),
			      sprite->pixmap, sprite->mask);
  }
 
  for (; d < 10; d++) {
    struct Sprite *sprite = get_citizen_sprite(CITIZEN_TAXMAN, d, NULL);
    gtk_image_set_from_pixmap(GTK_IMAGE(econ_label[d]),
			      sprite->pixmap, sprite->mask);
  }
 
  update_timeout_label();

  /* update tooltips. */
  gtk_tooltips_set_tip(main_tips, econ_ebox,
		       _("Shows your current luxury/science/tax rates;"
			 "click to toggle them."), "");

  my_snprintf(buffer, sizeof(buffer),
	      _("Shows your progress in researching "
		"the current technology.\n"
		"%s: %d/%d."),
		advances[game.player_ptr->research.researching].name,
		game.player_ptr->research.bulbs_researched,
		total_bulbs_required(game.player_ptr));
  gtk_tooltips_set_tip(main_tips, bulb_ebox, buffer, "");
  
  my_snprintf(buffer, sizeof(buffer),
	      _("Shows the progress of global warming:\n"
		"%d."),
	      sol);
  gtk_tooltips_set_tip(main_tips, sun_ebox, buffer, "");

  my_snprintf(buffer, sizeof(buffer),
	      _("Shows the progress of nuclear winter:\n"
		"%d."),
	      flake);
  gtk_tooltips_set_tip(main_tips, flake_ebox, buffer, "");

  my_snprintf(buffer, sizeof(buffer),
	      _("Shows your current government:\n"
		"%s."),
	      get_government_name(game.player_ptr->government));
  gtk_tooltips_set_tip(main_tips, government_ebox, buffer, "");
}

/**************************************************************************
  Update the information label which gives info on the current unit and the
  square under the current unit, for specified unit.  Note that in practice
  punit is always the focus unit.
  Clears label if punit is NULL.
  Also updates the cursor for the map_canvas (this is related because the
  info label includes a "select destination" prompt etc).
  Also calls update_unit_pix_label() to update the icons for units on this
  square.
**************************************************************************/
void update_unit_info_label(struct unit *punit)
{
  if(punit) {
    char buffer[512];
    struct city *pcity =
	player_find_city_by_id(game.player_ptr, punit->homecity);
    int infrastructure =
	get_tile_infrastructure_set(map_get_tile(punit->x, punit->y));
    struct unit_type *ptype = unit_type(punit);

    my_snprintf(buffer, sizeof(buffer), "%s", ptype->name);

    if (ptype->veteran[punit->veteran].name[0] != '\0') {
      sz_strlcat(buffer, " (");
      sz_strlcat(buffer, _(ptype->veteran[punit->veteran].name));
      sz_strlcat(buffer, ")");
    }

    gtk_frame_set_label(GTK_FRAME(unit_info_frame), buffer);

    my_snprintf(buffer, sizeof(buffer), "%s\n%s\n%s%s%s%s",
		(hover_unit == punit->id) ?
		_("Select destination") : unit_activity_text(punit),
		map_get_tile_info_text(punit->x, punit->y),
		infrastructure ?
		map_get_infrastructure_text(infrastructure) : "",
		infrastructure ? "\n" : "", pcity ? pcity->name : "",
		infrastructure ? "" : "\n");
    gtk_label_set_text(GTK_LABEL(unit_info_label), buffer);

    if (hover_unit != punit->id)
      set_hover_state(NULL, HOVER_NONE);

    switch (hover_state) {
    case HOVER_NONE:
      gdk_window_set_cursor (root_window, NULL);
      break;
    case HOVER_PATROL:
      gdk_window_set_cursor (root_window, patrol_cursor);
      break;
    case HOVER_GOTO:
    case HOVER_CONNECT:
      gdk_window_set_cursor (root_window, goto_cursor);
      break;
    case HOVER_NUKE:
      gdk_window_set_cursor (root_window, nuke_cursor);
      break;
    case HOVER_PARADROP:
      gdk_window_set_cursor (root_window, drop_cursor);
      break;
    }
  } else {
    gtk_frame_set_label( GTK_FRAME(unit_info_frame),"");
    gtk_label_set_text(GTK_LABEL(unit_info_label), "\n\n\n");
    gdk_window_set_cursor(root_window, NULL);
  }
  update_unit_pix_label(punit);
}


/**************************************************************************
...
**************************************************************************/
GdkPixmap *get_thumb_pixmap(int onoff)
{
  return sprites.treaty_thumb[BOOL_VAL(onoff)]->pixmap;
}

/**************************************************************************
...
**************************************************************************/
void set_indicator_icons(int bulb, int sol, int flake, int gov)
{
  struct Sprite *gov_sprite;

  bulb = CLIP(0, bulb, NUM_TILES_PROGRESS-1);
  sol = CLIP(0, sol, NUM_TILES_PROGRESS-1);
  flake = CLIP(0, flake, NUM_TILES_PROGRESS-1);

  gtk_image_set_from_pixmap(GTK_IMAGE(bulb_label),
			    sprites.bulb[bulb]->pixmap, NULL);
  gtk_image_set_from_pixmap(GTK_IMAGE(sun_label),
			    sprites.warming[sol]->pixmap, NULL);
  gtk_image_set_from_pixmap(GTK_IMAGE(flake_label),
			    sprites.cooling[flake]->pixmap, NULL);

  if (game.government_count==0) {
    /* not sure what to do here */
    gov_sprite = get_citizen_sprite(CITIZEN_UNHAPPY, 0, NULL);
  } else {
    gov_sprite = get_government(gov)->sprite;
  }
  gtk_image_set_from_pixmap(GTK_IMAGE(government_label),
			    gov_sprite->pixmap, NULL);
}

/**************************************************************************
...
**************************************************************************/
void map_size_changed(void)
{
  gtk_widget_set_size_request(overview_canvas,
			      overview.width, overview.height);
  update_map_canvas_scrollbars_size();
}

/**************************************************************************
...
**************************************************************************/
struct canvas *canvas_create(int width, int height)
{
  struct canvas *result = fc_malloc(sizeof(*result));

  result->type = CANVAS_PIXMAP;
  result->v.pixmap = gdk_pixmap_new(root_window, width, height, -1);
  return result;
}

/**************************************************************************
...
**************************************************************************/
void canvas_free(struct canvas *store)
{
  assert(store->type == CANVAS_PIXMAP);
  g_object_unref(store->v.pixmap);
  free(store);
}

/****************************************************************************
  Return a canvas that is the overview window.
****************************************************************************/
struct canvas *get_overview_window(void)
{
  static struct canvas store;

  store.type = CANVAS_PIXMAP;
  store.v.pixmap = overview_canvas->window;
  return &store;
}

/**************************************************************************
...
**************************************************************************/
gboolean overview_canvas_expose(GtkWidget *w, GdkEventExpose *ev, gpointer data)
{
  if (!can_client_change_view()) {
    if (radar_gfx_sprite) {
      gdk_draw_drawable(overview_canvas->window, civ_gc,
			radar_gfx_sprite->pixmap, ev->area.x, ev->area.y,
			ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
    }
    return TRUE;
  }
  
  refresh_overview_canvas();
  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
static bool map_center = TRUE;
static bool map_configure = FALSE;

gboolean map_canvas_configure(GtkWidget * w, GdkEventConfigure * ev,
			      gpointer data)
{
  if (map_canvas_resized(ev->width, ev->height)) {
    map_configure = TRUE;
  }

  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
gboolean map_canvas_expose(GtkWidget *w, GdkEventExpose *ev, gpointer data)
{
  static bool cleared = FALSE;

  if (!can_client_change_view()) {
    if (map_configure || !scaled_intro_sprite) {

      if (!intro_gfx_sprite) {
        load_intro_gfx();
      }

      if (scaled_intro_sprite) {
        free_sprite(scaled_intro_sprite);
      }

      scaled_intro_sprite = sprite_scale(intro_gfx_sprite,
					 w->allocation.width,
					 w->allocation.height);
    }

    if (scaled_intro_sprite) {
      gdk_draw_drawable(map_canvas->window, civ_gc,
			scaled_intro_sprite->pixmap,
			ev->area.x, ev->area.y, ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
      gtk_widget_queue_draw(overview_canvas);
      cleared = FALSE;
    } else {
      if (!cleared) {
	gtk_widget_queue_draw(w);
	cleared = TRUE;
      }
    }
    map_center = TRUE;
  }
  else
  {
    if (scaled_intro_sprite) {
      free_sprite(scaled_intro_sprite);
      scaled_intro_sprite = NULL;
    }

    if (map_exists()) { /* do we have a map at all */
      gdk_draw_drawable(map_canvas->window, civ_gc, map_canvas_store,
			ev->area.x, ev->area.y, ev->area.x, ev->area.y,
			ev->area.width, ev->area.height);
      cleared = FALSE;
    } else {
      if (!cleared) {
        gtk_widget_queue_draw(w);
	cleared = TRUE;
      }
    }

    if (!map_center) {
      center_on_something();
      map_center = FALSE;
    }
  }

  map_configure = FALSE;

  return TRUE;
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
void put_one_tile_full(GdkDrawable *pm, int x, int y,
		       int canvas_x, int canvas_y, int citymode)
{
  pixmap_put_tile_iso(pm, x, y, canvas_x, canvas_y, citymode,
		      0, 0, 0,
		      NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT, UNIT_TILE_HEIGHT,
		      D_FULL);
}

/**************************************************************************
  Draw some or all of a tile onto the canvas.
**************************************************************************/
void put_one_tile_iso(struct canvas *pcanvas,
		      int map_x, int map_y,
		      int canvas_x, int canvas_y,
		      int offset_x, int offset_y, int offset_y_unit,
		      int width, int height, int height_unit,
		      enum draw_type draw, bool citymode)
{
  pixmap_put_tile_iso(pcanvas->v.pixmap,
		      map_x, map_y, canvas_x, canvas_y,
		      citymode,
		      offset_x, offset_y, offset_y_unit,
		      width, height, height_unit, draw);
}

/**************************************************************************
  Flush the given part of the canvas buffer (if there is one) to the
  screen.
**************************************************************************/
void flush_mapcanvas(int canvas_x, int canvas_y,
		     int pixel_width, int pixel_height)
{
  gdk_draw_drawable(map_canvas->window, civ_gc, map_canvas_store,
		    canvas_x, canvas_y, canvas_x, canvas_y,
		    pixel_width, pixel_height);
}

/**************************************************************************
  Mark the rectangular region as "dirty" so that we know to flush it
  later.
**************************************************************************/
void dirty_rect(int canvas_x, int canvas_y,
		int pixel_width, int pixel_height)
{
  /* GDK gives an error if we invalidate out-of-bounds parts of the
     window. */
  GdkRectangle rect = {MAX(canvas_x, 0), MAX(canvas_y, 0),
		       MIN(pixel_width,
			   map_canvas->allocation.width - canvas_x),
		       MIN(pixel_height,
			   map_canvas->allocation.height - canvas_y)};

  gdk_window_invalidate_rect(map_canvas->window, &rect, FALSE);
}

/**************************************************************************
  Mark the entire screen area as "dirty" so that we can flush it later.
**************************************************************************/
void dirty_all(void)
{
  GdkRectangle rect = {0, 0, map_canvas->allocation.width,
		       map_canvas->allocation.height};

  gdk_window_invalidate_rect(map_canvas->window, &rect, FALSE);
}

/**************************************************************************
  Flush all regions that have been previously marked as dirty.  See
  dirty_rect and dirty_all.  This function is generally called after we've
  processed a batch of drawing operations.
**************************************************************************/
void flush_dirty(void)
{
  gdk_window_process_updates(map_canvas->window, FALSE);
}

/****************************************************************************
  Do any necessary synchronization to make sure the screen is up-to-date.
  The canvas should have already been flushed to screen via flush_dirty -
  all this function does is make sure the hardware has caught up.
****************************************************************************/
void gui_flush(void)
{
  gdk_flush();
}

/**************************************************************************
 Update display of descriptions associated with cities on the main map.
**************************************************************************/
void update_city_descriptions(void)
{
  update_map_canvas_visible();
}

/**************************************************************************
  If necessary, clear the city descriptions out of the buffer.
**************************************************************************/
void prepare_show_city_descriptions(void)
{
  /* Nothing to do */
}

/**************************************************************************
...
**************************************************************************/
void show_city_desc(struct city *pcity, int canvas_x, int canvas_y)
{
  static char buffer[512], buffer2[32];
  PangoRectangle rect, rect2;
  enum color_std color;
  int extra_width = 0;
  static PangoLayout *layout;

  if (!layout) {
    layout = pango_layout_new(gdk_pango_context_get());
  }

  canvas_x += NORMAL_TILE_WIDTH / 2;
  canvas_y += NORMAL_TILE_HEIGHT;

  if (draw_city_names) {
    get_city_mapview_name_and_growth(pcity, buffer, sizeof(buffer),
				     buffer2, sizeof(buffer2), &color);

    pango_layout_set_font_description(layout, main_font);
    if (buffer2[0] != '\0') {
      /* HACK: put a character's worth of space between the two strings. */
      pango_layout_set_text(layout, "M", -1);
      pango_layout_get_pixel_extents(layout, &rect, NULL);
      extra_width = rect.width;
    }
    pango_layout_set_text(layout, buffer, -1);
    pango_layout_get_pixel_extents(layout, &rect, NULL);
    rect.width += extra_width;

    if (draw_city_growth && pcity->owner == game.player_idx) {
      /* We need to know the size of the growth text before
	 drawing anything. */
      pango_layout_set_font_description(layout, city_productions_font);
      pango_layout_set_text(layout, buffer2, -1);
      pango_layout_get_pixel_extents(layout, &rect2, NULL);

      /* Now return the layout to its previous state. */
      pango_layout_set_font_description(layout, main_font);
      pango_layout_set_text(layout, buffer, -1);
    } else {
      rect2.width = 0;
    }

    gtk_draw_shadowed_string(map_canvas_store,
			     toplevel->style->black_gc,
			     toplevel->style->white_gc,
			     canvas_x - (rect.width + rect2.width) / 2,
			     canvas_y + PANGO_ASCENT(rect), layout);

    if (draw_city_growth && pcity->owner == game.player_idx) {
      pango_layout_set_font_description(layout, city_productions_font);
      pango_layout_set_text(layout, buffer2, -1);
      gdk_gc_set_foreground(civ_gc, colors_standard[color]);
      gtk_draw_shadowed_string(map_canvas_store,
			       toplevel->style->black_gc,
			       civ_gc,
			       canvas_x - (rect.width + rect2.width) / 2
			       + rect.width,
			       canvas_y + PANGO_ASCENT(rect)
			       + rect.height / 2 - rect2.height / 2,
			       layout);
    }

    canvas_y += rect.height + 3;
  }

  if (draw_city_productions && (pcity->owner==game.player_idx)) {
    get_city_mapview_production(pcity, buffer, sizeof(buffer));

    pango_layout_set_font_description(layout, city_productions_font);
    pango_layout_set_text(layout, buffer, -1);

    pango_layout_get_pixel_extents(layout, &rect, NULL);
    gtk_draw_shadowed_string(map_canvas_store,
			     toplevel->style->black_gc,
			     toplevel->style->white_gc,
			     canvas_x - rect.width / 2,
			     canvas_y + PANGO_ASCENT(rect), layout);
  }
}

/**************************************************************************
...
**************************************************************************/
void put_unit_gpixmap(struct unit *punit, GtkPixcomm *p)
{
  struct canvas canvas_store = {.type = CANVAS_PIXCOMM, .v.pixcomm = p};

  gtk_pixcomm_freeze(p);
  gtk_pixcomm_clear(p);

  put_unit_full(punit, &canvas_store, 0, 0);

  gtk_pixcomm_thaw(p);
}


/**************************************************************************
  FIXME:
  For now only two food, two gold one shield and two masks can be drawn per
  unit, the proper way to do this is probably something like what Civ II does.
  (One food/shield/mask drawn N times, possibly one top of itself. -- SKi 
**************************************************************************/
void put_unit_gpixmap_city_overlays(struct unit *punit, GtkPixcomm *p)
{
  struct canvas store = {.type = CANVAS_PIXCOMM, .v.pixcomm = p};
 
  gtk_pixcomm_freeze(p);

  put_unit_city_overlays(punit, &store, 0, NORMAL_TILE_HEIGHT);

  gtk_pixcomm_thaw(p);
}

/**************************************************************************
...
**************************************************************************/
static void pixmap_put_overlay_tile(GdkDrawable *pixmap,
				    int canvas_x, int canvas_y,
				    struct Sprite *ssprite)
{
  if (!ssprite)
    return;
      
  gdk_gc_set_clip_origin(civ_gc, canvas_x, canvas_y);
  gdk_gc_set_clip_mask(civ_gc, ssprite->mask);

  gdk_draw_drawable(pixmap, civ_gc, ssprite->pixmap,
		    0, 0,
		    canvas_x, canvas_y,
		    ssprite->width, ssprite->height);
  gdk_gc_set_clip_mask(civ_gc, NULL);
}

/**************************************************************************
  Place part of a (possibly masked) sprite on a pixmap.
**************************************************************************/
static void pixmap_put_sprite(GdkDrawable *pixmap,
			      int pixmap_x, int pixmap_y,
			      struct Sprite *ssprite,
			      int offset_x, int offset_y,
			      int width, int height)
{
  if (ssprite->mask) {
    gdk_gc_set_clip_origin(civ_gc, pixmap_x, pixmap_y);
    gdk_gc_set_clip_mask(civ_gc, ssprite->mask);
  }

  gdk_draw_drawable(pixmap, civ_gc, ssprite->pixmap,
		    offset_x, offset_y,
		    pixmap_x + offset_x, pixmap_y + offset_y,
		    MIN(width, MAX(0, ssprite->width - offset_x)),
		    MIN(height, MAX(0, ssprite->height - offset_y)));

  gdk_gc_set_clip_mask(civ_gc, NULL);
}

/**************************************************************************
  Draw some or all of a sprite onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_sprite(struct canvas *pcanvas,
		       int canvas_x, int canvas_y,
		       struct Sprite *sprite,
		       int offset_x, int offset_y, int width, int height)
{
  switch (pcanvas->type) {
    case CANVAS_PIXMAP:
      pixmap_put_sprite(pcanvas->v.pixmap, canvas_x, canvas_y,
	  sprite, offset_x, offset_y, width, height);
      break;
    case CANVAS_PIXCOMM:
      gtk_pixcomm_copyto(pcanvas->v.pixcomm, sprite, canvas_x, canvas_y);
      break;
    case CANVAS_PIXBUF:
      {
	GdkPixbuf *src, *dst;

	src = sprite_get_pixbuf(sprite);
	dst = pcanvas->v.pixbuf;
	gdk_pixbuf_composite(src, dst, canvas_x, canvas_y,
	    MIN(width,
	      MIN(gdk_pixbuf_get_width(dst), gdk_pixbuf_get_width(src))),
	    MIN(height,
	      MIN(gdk_pixbuf_get_height(dst), gdk_pixbuf_get_height(src))),
	    -offset_x, -offset_y, 1.0, 1.0, GDK_INTERP_NEAREST, 255);
      }
      break;
    default:
      break;
  } 
}

/**************************************************************************
  Draw a full sprite onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_sprite_full(struct canvas *pcanvas,
			    int canvas_x, int canvas_y,
			    struct Sprite *sprite)
{
  assert(sprite->pixmap);
  canvas_put_sprite(pcanvas, canvas_x, canvas_y, sprite,
		    0, 0, sprite->width, sprite->height);
}

/**************************************************************************
  Draw a filled-in colored rectangle onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_rectangle(struct canvas *pcanvas,
			  enum color_std color,
			  int canvas_x, int canvas_y, int width, int height)
{
  GdkColor *col = colors_standard[color];

  switch (pcanvas->type) {
    case CANVAS_PIXMAP:
      gdk_gc_set_foreground(fill_bg_gc, col);
      gdk_draw_rectangle(pcanvas->v.pixmap, fill_bg_gc, TRUE,
	  canvas_x, canvas_y, width, height);
      break;
    case CANVAS_PIXCOMM:
      gtk_pixcomm_fill(pcanvas->v.pixcomm, col);
      break;
    case CANVAS_PIXBUF:
      gdk_pixbuf_fill(pcanvas->v.pixbuf,
	  ((guint32)(col->red & 0xff00) << 16)
	  | ((col->green & 0xff00) << 8) | (col->blue & 0xff00) | 0xff);
      break;
    default:
      break;
  }
}

/**************************************************************************
  Draw a colored line onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_line(struct canvas *pcanvas, enum color_std color,
		     enum line_type ltype, int start_x, int start_y,
		     int dx, int dy)
{
  GdkGC *gc = NULL;

  switch (ltype) {
  case LINE_NORMAL:
    gc = civ_gc;
    break;
  case LINE_BORDER:
    gc = border_line_gc;
    break;
  case LINE_TILE_FRAME:
    gc = thick_line_gc;
    break;
  case LINE_GOTO:
    gc = thick_line_gc;
    break;
  }

  gdk_gc_set_foreground(gc, colors_standard[color]);
  gdk_draw_line(pcanvas->v.pixmap, gc,
		start_x, start_y, start_x + dx, start_y + dy);
}

/**************************************************************************
...
**************************************************************************/
void canvas_copy(struct canvas *dest, struct canvas *src,
		 int src_x, int src_y, int dest_x, int dest_y,
		 int width, int height)
{
  gdk_draw_drawable(dest->v.pixmap, fill_bg_gc, src->v.pixmap,
		    src_x, src_y, dest_x, dest_y, width, height);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_overlay_tile_draw(GdkDrawable *pixmap,
					 int canvas_x, int canvas_y,
					 struct Sprite *ssprite,
					 int offset_x, int offset_y,
					 int width, int height,
					 bool fog)
{
  if (!ssprite || !width || !height)
    return;

  pixmap_put_sprite(pixmap, canvas_x, canvas_y, ssprite,
		    offset_x, offset_y, width, height);

  /* I imagine this could be done more efficiently. Some pixels We first
     draw from the sprite, and then draw black afterwards. It would be much
     faster to just draw every second pixel black in the first place. */
  if (fog) {
    gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
    gdk_gc_set_clip_mask(fill_tile_gc, ssprite->mask);
    gdk_gc_set_foreground(fill_tile_gc, colors_standard[COLOR_STD_BLACK]);
    gdk_gc_set_stipple(fill_tile_gc, black50);

    gdk_draw_rectangle(pixmap, fill_tile_gc, TRUE,
		       canvas_x+offset_x, canvas_y+offset_y,
		       MIN(width, MAX(0, ssprite->width-offset_x)),
		       MIN(height, MAX(0, ssprite->height-offset_y)));
    gdk_gc_set_clip_mask(fill_tile_gc, NULL);
  }
}

/**************************************************************************
 Draws a cross-hair overlay on a tile
**************************************************************************/
void put_cross_overlay_tile(int x, int y)
{
  int canvas_x, canvas_y;

  if (map_to_canvas_pos(&canvas_x, &canvas_y, x, y)) {
    pixmap_put_overlay_tile(map_canvas->window,
			    canvas_x, canvas_y,
			    sprites.user.attention);
  }
}

/**************************************************************************
...
**************************************************************************/
void put_city_workers(struct city *pcity, int color)
{
  int canvas_x, canvas_y;
  static struct city *last_pcity=NULL;
  struct canvas store = {.type = CANVAS_PIXMAP, .v.pixmap = map_canvas->window};

  if (color==-1) {
    if (pcity!=last_pcity)
      city_workers_color = city_workers_color%3 + 1;
    color=city_workers_color;
  }
  gdk_gc_set_foreground(fill_tile_gc, colors_standard[color]);

  city_map_checked_iterate(pcity->x, pcity->y, i, j, x, y) {
    enum city_tile_type worked = get_worker_city(pcity, i, j);

    if (!map_to_canvas_pos(&canvas_x, &canvas_y, x, y)) {
      continue;
    }

    /* stipple the area */
    if (!is_city_center(i, j)) {
      if (worked == C_TILE_EMPTY) {
	gdk_gc_set_stipple(fill_tile_gc, gray25);
      } else if (worked == C_TILE_WORKER) {
	gdk_gc_set_stipple(fill_tile_gc, gray50);
      } else
	continue;

      if (is_isometric) {
	gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
	gdk_gc_set_clip_mask(fill_tile_gc, sprites.black_tile->mask);
	gdk_draw_drawable(map_canvas->window, fill_tile_gc, map_canvas_store,
			  canvas_x, canvas_y,
			  canvas_x, canvas_y,
			  NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	gdk_draw_rectangle(map_canvas->window, fill_tile_gc, TRUE,
			   canvas_x, canvas_y,
			   NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	gdk_gc_set_clip_mask(fill_tile_gc, NULL);
      } else {
	gdk_draw_drawable(map_canvas->window, civ_gc, map_canvas_store,
			  canvas_x, canvas_y,
			  canvas_x, canvas_y,
			  NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
	gdk_draw_rectangle(map_canvas->window, fill_tile_gc, TRUE,
			   canvas_x, canvas_y,
			   NORMAL_TILE_WIDTH, NORMAL_TILE_HEIGHT);
      }
    }

    /* draw tile output */
    if (worked == C_TILE_WORKER) {
      put_city_tile_output(pcity, i, j, &store, canvas_x, canvas_y);
    }
  } city_map_checked_iterate_end;

  last_pcity=pcity;
}

/**************************************************************************
...
**************************************************************************/
void update_map_canvas_scrollbars(void)
{
  int scroll_x, scroll_y;

  get_mapview_scroll_pos(&scroll_x, &scroll_y);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(map_hadj), scroll_x);
  gtk_adjustment_set_value(GTK_ADJUSTMENT(map_vadj), scroll_y);
}

/**************************************************************************
...
**************************************************************************/
void update_map_canvas_scrollbars_size(void)
{
  int xmin, ymin, xmax, ymax, xsize, ysize, xstep, ystep;

  get_mapview_scroll_window(&xmin, &ymin, &xmax, &ymax, &xsize, &ysize);
  get_mapview_scroll_step(&xstep, &ystep);

  map_hadj = gtk_adjustment_new(-1, xmin, xmax, xstep, xsize, xsize);
  map_vadj = gtk_adjustment_new(-1, ymin, ymax, ystep, ysize, ysize);

  gtk_range_set_adjustment(GTK_RANGE(map_horizontal_scrollbar),
	GTK_ADJUSTMENT(map_hadj));
  gtk_range_set_adjustment(GTK_RANGE(map_vertical_scrollbar),
	GTK_ADJUSTMENT(map_vadj));

  g_signal_connect(map_hadj, "value_changed",
	G_CALLBACK(scrollbar_jump_callback),
	GINT_TO_POINTER(TRUE));
  g_signal_connect(map_vadj, "value_changed",
	G_CALLBACK(scrollbar_jump_callback),
	GINT_TO_POINTER(FALSE));
}

/**************************************************************************
...
**************************************************************************/
void scrollbar_jump_callback(GtkAdjustment *adj, gpointer hscrollbar)
{
  int scroll_x, scroll_y;

  if (!can_client_change_view()) {
    return;
  }

  get_mapview_scroll_pos(&scroll_x, &scroll_y);

  if (hscrollbar) {
    scroll_x = adj->value;
  } else {
    scroll_y = adj->value;
  }

  set_mapview_scroll_pos(scroll_x, scroll_y);
}

/**************************************************************************
 Area Selection
**************************************************************************/
void draw_selection_rectangle(int canvas_x, int canvas_y, int w, int h)
{
  gdk_gc_set_foreground(civ_gc, colors_standard[COLOR_STD_YELLOW]);

  /* gdk_draw_rectangle() must start top-left.. */
  gdk_draw_line(map_canvas->window, civ_gc,
            canvas_x, canvas_y, canvas_x + w, canvas_y);
  gdk_draw_line(map_canvas->window, civ_gc,
            canvas_x, canvas_y, canvas_x, canvas_y + h);
  gdk_draw_line(map_canvas->window, civ_gc,
            canvas_x, canvas_y + h, canvas_x + w, canvas_y + h);
  gdk_draw_line(map_canvas->window, civ_gc,
            canvas_x + w, canvas_y, canvas_x + w, canvas_y + h);

  rectangle_active = TRUE;
}

/**************************************************************************
  Put a drawn sprite (with given offset) onto the pixmap.
**************************************************************************/
static void pixmap_put_drawn_sprite(GdkDrawable *pixmap,
				    int canvas_x, int canvas_y,
				    struct drawn_sprite *pdsprite,
				    int offset_x, int offset_y,
				    int width, int height,
				    bool fog)
{
  int ox = pdsprite->offset_x, oy = pdsprite->offset_y;

  pixmap_put_overlay_tile_draw(pixmap, canvas_x + ox, canvas_y + oy,
			       pdsprite->sprite,
			       offset_x - ox, offset_y - oy,
			       width, height,
			       fog);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void put_city_pixmap_draw(struct city *pcity, GdkPixmap *pm,
				 int canvas_x, int canvas_y,
				 int offset_x, int offset_y_unit,
				 int width, int height_unit,
				 bool fog)
{
  struct drawn_sprite sprites[80];
  int count = fill_city_sprite_array_iso(sprites, pcity);
  int i;

  for (i=0; i<count; i++) {
    if (sprites[i].sprite) {
      pixmap_put_drawn_sprite(pm, canvas_x, canvas_y, &sprites[i],
			      offset_x, offset_y_unit, width, height_unit,
			      fog);
    }
  }
}
/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_black_tile_iso(GdkDrawable *pm,
				      int canvas_x, int canvas_y,
				      int offset_x, int offset_y,
				      int width, int height)
{
  gdk_gc_set_clip_origin(civ_gc, canvas_x, canvas_y);
  gdk_gc_set_clip_mask(civ_gc, sprites.black_tile->mask);

  assert(width <= NORMAL_TILE_WIDTH);
  assert(height <= NORMAL_TILE_HEIGHT);
  gdk_draw_drawable(pm, civ_gc, sprites.black_tile->pixmap,
		    offset_x, offset_y,
		    canvas_x+offset_x, canvas_y+offset_y,
		    width, height);

  gdk_gc_set_clip_mask(civ_gc, NULL);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_tile_iso(GdkDrawable *pm, int x, int y,
				int canvas_x, int canvas_y,
				int citymode,
				int offset_x, int offset_y, int offset_y_unit,
				int width, int height, int height_unit,
				enum draw_type draw)
{
  struct drawn_sprite tile_sprs[80];
  struct city *pcity;
  struct unit *punit, *pfocus;
  enum tile_special_type special;
  int count, i;
  bool solid_bg, fog, tile_hilited;
  struct canvas canvas_store = {.type = CANVAS_PIXMAP, .v.pixmap = pm};

  if (!width || !(height || height_unit))
    return;

  count = fill_tile_sprite_array_iso(tile_sprs,
				     x, y, citymode, &solid_bg);

  if (count == -1) { /* tile is unknown */
    pixmap_put_black_tile_iso(pm, canvas_x, canvas_y,
			      offset_x, offset_y, width, height);
    return;
  }

  /* Replace with check for is_normal_tile later */
  assert(is_real_map_pos(x, y));
  normalize_map_pos(&x, &y);

  fog = tile_get_known(x, y) == TILE_KNOWN_FOGGED && draw_fog_of_war;
  pcity = map_get_city(x, y);
  punit = get_drawable_unit(x, y, citymode);
  pfocus = get_unit_in_focus();
  special = map_get_special(x, y);
  tile_hilited = (map_get_tile(x,y)->client.hilite != HILITE_NONE);

  if (solid_bg) {
    gdk_gc_set_clip_origin(fill_bg_gc, canvas_x, canvas_y);
    gdk_gc_set_clip_mask(fill_bg_gc, sprites.black_tile->mask);
    gdk_gc_set_foreground(fill_bg_gc, colors_standard[COLOR_STD_BACKGROUND]);

    gdk_draw_rectangle(pm, fill_bg_gc, TRUE,
		       canvas_x+offset_x, canvas_y+offset_y,
		       MIN(width, MAX(0, sprites.black_tile->width-offset_x)),
		       MIN(height, MAX(0, sprites.black_tile->height-offset_y)));
    gdk_gc_set_clip_mask(fill_bg_gc, NULL);
    if (fog) {
      gdk_gc_set_clip_origin(fill_tile_gc, canvas_x, canvas_y);
      gdk_gc_set_clip_mask(fill_tile_gc, sprites.black_tile->mask);
      gdk_gc_set_foreground(fill_tile_gc, colors_standard[COLOR_STD_BLACK]);
      gdk_gc_set_stipple(fill_tile_gc, black50);

      gdk_draw_rectangle(pm, fill_tile_gc, TRUE,
			 canvas_x+offset_x, canvas_y+offset_y,
			 MIN(width, MAX(0, sprites.black_tile->width-offset_x)),
			 MIN(height, MAX(0, sprites.black_tile->height-offset_y)));
      gdk_gc_set_clip_mask(fill_tile_gc, NULL);
    }
  }

  /*** Draw terrain and specials ***/
  for (i = 0; i < count; i++) {
    if (tile_sprs[i].sprite)
      pixmap_put_drawn_sprite(pm, canvas_x, canvas_y, &tile_sprs[i],
			      offset_x, offset_y, width, height, fog);
    else
      freelog(LOG_ERROR, "sprite is NULL");
  }

  /*** Area Selection hiliting ***/
  if (tile_hilited) {
    gdk_gc_set_foreground(thin_line_gc, colors_standard[COLOR_STD_YELLOW]);

    if (draw & D_M_R) {
      gdk_draw_line(pm, thin_line_gc,
            canvas_x + NORMAL_TILE_WIDTH / 2,
            canvas_y,
            canvas_x + NORMAL_TILE_WIDTH - 1,
            canvas_y + NORMAL_TILE_HEIGHT / 2 - 1);
    }
    if (draw & D_M_L) {
      gdk_draw_line(pm, thin_line_gc,
            canvas_x,
            canvas_y + NORMAL_TILE_HEIGHT / 2 - 1,
            canvas_x + NORMAL_TILE_WIDTH / 2 - 1,
            canvas_y);
    }
    /* The lower lines do not quite reach the tile's bottom;
     * they would be too obscured by overlapping tiles' terrain. */
    if (draw & D_B_R) {
      gdk_draw_line(pm, thin_line_gc,
            canvas_x + NORMAL_TILE_WIDTH / 2,
            canvas_y + NORMAL_TILE_HEIGHT - 3,
            canvas_x + NORMAL_TILE_WIDTH - 1,
            canvas_y + NORMAL_TILE_HEIGHT / 2 - 1);
    }
    if (draw & D_B_L) {
      gdk_draw_line(pm, thin_line_gc,
            canvas_x,
            canvas_y + NORMAL_TILE_HEIGHT / 2 - 1,
            canvas_x + NORMAL_TILE_WIDTH / 2 - 1,
            canvas_y + NORMAL_TILE_HEIGHT - 3);
    }
  }

  /*** Map grid ***/
  if (draw_map_grid && !tile_hilited) {
    /* we draw the 2 lines on top of the tile; the buttom lines will be
       drawn by the tiles underneath. */
    if (draw & D_M_R) {
      gdk_gc_set_foreground(thin_line_gc,
			    colors_standard[get_grid_color
					    (x, y, x, y - 1)]);
      gdk_draw_line(pm, thin_line_gc,
		    canvas_x + NORMAL_TILE_WIDTH / 2, canvas_y,
		    canvas_x + NORMAL_TILE_WIDTH,
		    canvas_y + NORMAL_TILE_HEIGHT / 2);
    }

    if (draw & D_M_L) {
      gdk_gc_set_foreground(thin_line_gc,
			    colors_standard[get_grid_color
					    (x, y, x - 1, y)]);
      gdk_draw_line(pm, thin_line_gc,
		    canvas_x, canvas_y + NORMAL_TILE_HEIGHT / 2,
		    canvas_x + NORMAL_TILE_WIDTH / 2, canvas_y);
    }
  }

  /* National borders */
  tile_draw_borders_iso(&canvas_store, x, y, canvas_x, canvas_y, draw);

  if (draw_coastline && !draw_terrain) {
    enum tile_terrain_type t1 = map_get_terrain(x, y), t2;
    int x1, y1;
    gdk_gc_set_foreground(thin_line_gc, colors_standard[COLOR_STD_OCEAN]);
    x1 = x; y1 = y-1;
    if (normalize_map_pos(&x1, &y1)) {
      t2 = map_get_terrain(x1, y1);
      if (draw & D_M_R && (is_ocean(t1) ^ is_ocean(t2))) {
	gdk_draw_line(pm, thin_line_gc,
		      canvas_x+NORMAL_TILE_WIDTH/2, canvas_y,
		      canvas_x+NORMAL_TILE_WIDTH, canvas_y+NORMAL_TILE_HEIGHT/2);
      }
    }
    x1 = x-1; y1 = y;
    if (normalize_map_pos(&x1, &y1)) {
      t2 = map_get_terrain(x1, y1);
      if (draw & D_M_L && (is_ocean(t1) ^ is_ocean(t2))) {
	gdk_draw_line(pm, thin_line_gc,
		      canvas_x, canvas_y + NORMAL_TILE_HEIGHT/2,
		      canvas_x+NORMAL_TILE_WIDTH/2, canvas_y);
      }
    }
  }

  /*** City and various terrain improvements ***/
  if (pcity && draw_cities) {
    put_city_pixmap_draw(pcity, pm,
			 canvas_x, canvas_y - NORMAL_TILE_HEIGHT/2,
			 offset_x, offset_y_unit,
			 width, height_unit, fog);
  }
  if (contains_special(special, S_AIRBASE) && draw_fortress_airbase)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				 sprites.tx.airbase,
				 offset_x, offset_y_unit,
				 width, height_unit, fog);
  if (contains_special(special, S_FALLOUT) && draw_pollution)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y,
				 sprites.tx.fallout,
				 offset_x, offset_y,
				 width, height, fog);
  if (contains_special(special, S_POLLUTION) && draw_pollution)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y,
				 sprites.tx.pollution,
				 offset_x, offset_y,
				 width, height, fog);

  /*** city size ***/
  /* Not fogged as it would be unreadable */
  if (pcity && draw_cities) {
    if (pcity->size>=10)
      pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				   sprites.city.size_tens[pcity->size/10],
				   offset_x, offset_y_unit,
				   width, height_unit, 0);

    pixmap_put_overlay_tile_draw(pm, canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				 sprites.city.size[pcity->size%10],
				 offset_x, offset_y_unit,
				 width, height_unit, 0);
  }

  /*** Unit ***/
  if (punit && (draw_units || (punit == pfocus && draw_focus_unit))) {
    bool stacked = (unit_list_size(&map_get_tile(x, y)->units) > 1);
    bool backdrop = !pcity;

    put_unit(punit, stacked, backdrop, &canvas_store,
	     canvas_x, canvas_y - NORMAL_TILE_HEIGHT/2,
	     offset_x, offset_y_unit,
	     width, height_unit);
  }

  if (contains_special(special, S_FORTRESS) && draw_fortress_airbase)
    pixmap_put_overlay_tile_draw(pm,
				 canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
				 sprites.tx.fortress,
				 offset_x, offset_y_unit,
				 width, height_unit, fog);
}

/**************************************************************************
  This function is called when the tileset is changed.
**************************************************************************/
void tileset_changed(void)
{
  reset_city_dialogs();
  reset_unit_table();
}
