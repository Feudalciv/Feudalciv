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
#include <string.h>

#include <gtk/gtk.h>

#include "gtkpixcomm.h"

#include "game.h"
#include "log.h"
#include "mem.h"
#include "shared.h"
#include "support.h"
#include "unit.h"
#include "version.h"

#include "climisc.h"
#include "colors.h"
#include "gui_main.h"
#include "mapview_g.h"
#include "options.h"
#include "tilespec.h"

#include "graphics.h"

#include "goto_cursor.xbm"
#include "goto_cursor_mask.xbm"
#include "drop_cursor.xbm"
#include "drop_cursor_mask.xbm"
#include "nuke_cursor.xbm"
#include "nuke_cursor_mask.xbm"
#include "patrol_cursor.xbm"
#include "patrol_cursor_mask.xbm"

SPRITE *		intro_gfx_sprite;
SPRITE *		radar_gfx_sprite;

GdkCursor *		goto_cursor;
GdkCursor *		drop_cursor;
GdkCursor *		nuke_cursor;
GdkCursor *		patrol_cursor;

/***************************************************************************
...
***************************************************************************/
bool isometric_view_supported(void)
{
  return TRUE;
}

/***************************************************************************
...
***************************************************************************/
bool overhead_view_supported(void)
{
  return TRUE;
}

/***************************************************************************
...
***************************************************************************/
#define COLOR_MOTTO_FACE_R    0x2D
#define COLOR_MOTTO_FACE_G    0x71
#define COLOR_MOTTO_FACE_B    0xE3

/**************************************************************************
...
**************************************************************************/
void gtk_draw_shadowed_string(GdkDrawable *drawable,
			      GdkGC *black_gc,
			      GdkGC *white_gc,
			      gint x, gint y, PangoLayout *layout)
{
  gdk_draw_layout(drawable, black_gc, x + 1, y + 1, layout);
  gdk_draw_layout(drawable, white_gc, x, y, layout);
}

/**************************************************************************
...
**************************************************************************/
void load_intro_gfx(void)
{
  int tot, y;
  char s[64];
  GdkColor face;
  GdkGC *face_gc;
  PangoLayout *layout;
  PangoRectangle rect;

  layout = pango_layout_new(gdk_pango_context_get());
  pango_layout_set_font_description(layout, main_font);

  /* get colors */
  face.red  = COLOR_MOTTO_FACE_R<<8;
  face.green= COLOR_MOTTO_FACE_G<<8;
  face.blue = COLOR_MOTTO_FACE_B<<8;
  face_gc = gdk_gc_new(root_window);

  /* Main graphic */
  intro_gfx_sprite = load_gfxfile(main_intro_filename);
  tot=intro_gfx_sprite->width;

  pango_layout_set_text(layout, freeciv_motto(), -1);
  pango_layout_get_pixel_extents(layout, &rect, NULL);

  y = intro_gfx_sprite->height-45;

  gdk_gc_set_rgb_fg_color(face_gc, &face);
  gdk_draw_layout(intro_gfx_sprite->pixmap, face_gc,
		  (tot-rect.width) / 2, y, layout);
  gdk_gc_destroy (face_gc);

  /* Minimap graphic */
  radar_gfx_sprite = load_gfxfile(minimap_intro_filename);
  tot=radar_gfx_sprite->width;

  my_snprintf(s, sizeof(s), "%d.%d.%d%s",
	      MAJOR_VERSION, MINOR_VERSION,
	      PATCH_VERSION, VERSION_LABEL);
  pango_layout_set_text(layout, s, -1);
  pango_layout_get_pixel_extents(layout, &rect, NULL);

  y = radar_gfx_sprite->height - (rect.height + 6);

  gtk_draw_shadowed_string(radar_gfx_sprite->pixmap,
			toplevel->style->black_gc,
			toplevel->style->white_gc,
			(tot - rect.width) / 2, y,
			layout);

  pango_layout_set_text(layout, word_version(), -1);
  pango_layout_get_pixel_extents(layout, &rect, NULL);
  y-=rect.height+3;

  gtk_draw_shadowed_string(radar_gfx_sprite->pixmap,
			toplevel->style->black_gc,
			toplevel->style->white_gc,
			(tot - rect.width) / 2, y,
			layout);

  /* done */
  g_object_unref(layout);
  return;
}

/***************************************************************************
return newly allocated sprite cropped from source
***************************************************************************/
struct Sprite *crop_sprite(struct Sprite *source,
			   int x, int y,
			   int width, int height)
{
  GdkPixmap *mypixmap, *mask = NULL;

  mypixmap = gdk_pixmap_new(root_window, width, height, -1);

  gdk_draw_pixmap(mypixmap, civ_gc, source->pixmap, x, y, 0, 0,
		  width, height);

  if (source->has_mask) {
    mask = gdk_pixmap_new(NULL, width, height, 1);
    gdk_draw_rectangle(mask, mask_bg_gc, TRUE, 0, 0, -1, -1);

    gdk_draw_pixmap(mask, mask_fg_gc, source->mask,
		    x, y, 0, 0, width, height);
  }

  return ctor_sprite_mask(mypixmap, mask, width, height);
}


/***************************************************************************
...
***************************************************************************/
void load_cursors(void)
{
  GdkBitmap *pixmap, *mask;
  GdkColor *white, *black;

  white = colors_standard[COLOR_STD_WHITE];
  black = colors_standard[COLOR_STD_BLACK];

  /* goto */
  pixmap = gdk_bitmap_create_from_data(root_window, goto_cursor_bits,
				      goto_cursor_width,
				      goto_cursor_height);
  mask   = gdk_bitmap_create_from_data(root_window, goto_cursor_mask_bits,
				      goto_cursor_mask_width,
				      goto_cursor_mask_height);
  goto_cursor = gdk_cursor_new_from_pixmap(pixmap, mask,
					  white, black,
					  goto_cursor_x_hot, goto_cursor_y_hot);
  gdk_bitmap_unref(pixmap);
  gdk_bitmap_unref(mask);

  /* drop */
  pixmap = gdk_bitmap_create_from_data(root_window, drop_cursor_bits,
				      drop_cursor_width,
				      drop_cursor_height);
  mask   = gdk_bitmap_create_from_data(root_window, drop_cursor_mask_bits,
				      drop_cursor_mask_width,
				      drop_cursor_mask_height);
  drop_cursor = gdk_cursor_new_from_pixmap(pixmap, mask,
					  white, black,
					  drop_cursor_x_hot, drop_cursor_y_hot);
  gdk_bitmap_unref(pixmap);
  gdk_bitmap_unref(mask);

  /* nuke */
  pixmap = gdk_bitmap_create_from_data(root_window, nuke_cursor_bits,
				      nuke_cursor_width,
				      nuke_cursor_height);
  mask   = gdk_bitmap_create_from_data(root_window, nuke_cursor_mask_bits,
				      nuke_cursor_mask_width,
				      nuke_cursor_mask_height);
  nuke_cursor = gdk_cursor_new_from_pixmap(pixmap, mask,
					  white, black,
					  nuke_cursor_x_hot, nuke_cursor_y_hot);
  gdk_bitmap_unref(pixmap);
  gdk_bitmap_unref(mask);

  /* patrol */
  pixmap = gdk_bitmap_create_from_data(root_window, patrol_cursor_bits,
				      patrol_cursor_width,
				      patrol_cursor_height);
  mask   = gdk_bitmap_create_from_data(root_window, patrol_cursor_mask_bits,
				      patrol_cursor_mask_width,
				      patrol_cursor_mask_height);
  patrol_cursor = gdk_cursor_new_from_pixmap(pixmap, mask,
					     white, black,
					     patrol_cursor_x_hot, patrol_cursor_y_hot);
  gdk_bitmap_unref(pixmap);
  gdk_bitmap_unref(mask);
}

/***************************************************************************
 Create a new sprite with the given pixmap, dimensions, and
 (optional) mask.
***************************************************************************/
SPRITE *ctor_sprite_mask( GdkPixmap *mypixmap, GdkPixmap *mask, 
			  int width, int height )
{
    SPRITE *mysprite = fc_malloc(sizeof(SPRITE));

    mysprite->pixmap	= mypixmap;

    mysprite->has_mask = (mask != NULL);
    mysprite->mask = mask;

    mysprite->width	= width;
    mysprite->height	= height;

    return mysprite;
}


#ifdef UNUSED
/***************************************************************************
...
***************************************************************************/
void dtor_sprite( SPRITE *mysprite )
{
    free_sprite( mysprite );
    return;
}
#endif

/***************************************************************************
 Returns the filename extensions the client supports
 Order is important.
***************************************************************************/
const char **gfx_fileextensions(void)
{
  static const char *ext[] =
  {
    "png",
    "xpm",
    NULL
  };

  return ext;
}

/***************************************************************************
...
***************************************************************************/
struct Sprite *load_gfxfile(const char *filename)
{
  GdkPixbuf *im;
  SPRITE    *mysprite;
  int	     w, h;

  if (!(im = gdk_pixbuf_new_from_file(filename, NULL))) {
    freelog(LOG_FATAL, "Failed reading XPM file: %s", filename);
    exit(EXIT_FAILURE);
  }

  mysprite=fc_malloc(sizeof(struct Sprite));

  w = gdk_pixbuf_get_width(im); h = gdk_pixbuf_get_height(im);
  gdk_pixbuf_render_pixmap_and_mask(im, &mysprite->pixmap, &mysprite->mask, 1);

  mysprite->has_mask  = (mysprite->mask != NULL);
  mysprite->width     = w;
  mysprite->height    = h;

  g_object_unref(im);

  return mysprite;
}

/***************************************************************************
   Deletes a sprite.  These things can use a lot of memory.
***************************************************************************/
void free_sprite(SPRITE *s)
{
  if (s->pixmap)
    g_object_unref(s->pixmap);
  if (s->mask)
    g_object_unref(s->mask);
  free(s);
  return;
}

/***************************************************************************
 ...
***************************************************************************/
void create_overlay_unit(GtkWidget *pixcomm, int i)
{
  enum color_std bg_color;
  
  /* Give tile a background color, based on the type of unit */
  switch (get_unit_type(i)->move_type) {
    case LAND_MOVING: bg_color = COLOR_STD_GROUND; break;
    case SEA_MOVING:  bg_color = COLOR_STD_OCEAN;  break;
    case HELI_MOVING: bg_color = COLOR_STD_YELLOW; break;
    case AIR_MOVING:  bg_color = COLOR_STD_CYAN;   break;
    default:	      bg_color = COLOR_STD_BLACK;  break;
  }
  gtk_pixcomm_freeze(GTK_PIXCOMM(pixcomm));
  gtk_pixcomm_fill(GTK_PIXCOMM(pixcomm), colors_standard[bg_color]);

  /* If we're using flags, put one on the tile */
  if(!solid_color_behind_units)  {
    struct Sprite *flag=get_nation_by_plr(game.player_ptr)->flag_sprite;

    gtk_pixcomm_copyto(GTK_PIXCOMM(pixcomm), flag, 0, 0);
  }

  /* Finally, put a picture of the unit in the tile */
  if(i<game.num_unit_types) {
    struct Sprite *s=get_unit_type(i)->sprite;

    gtk_pixcomm_copyto(GTK_PIXCOMM(pixcomm), s, 0, 0);
  }
  gtk_pixcomm_thaw(GTK_PIXCOMM(pixcomm));
}

/***************************************************************************
  This function is so that packhand.c can be gui-independent, and
  not have to deal with Sprites itself.
***************************************************************************/
void free_intro_radar_sprites(void)
{
  if (intro_gfx_sprite) {
    free_sprite(intro_gfx_sprite);
    intro_gfx_sprite=NULL;
  }
  if (radar_gfx_sprite) {
    free_sprite(radar_gfx_sprite);
    radar_gfx_sprite=NULL;
  }
}

/***************************************************************************
 Draws a black border around the sprite. This is done by parsing the
 mask and replacing the first and last pixel in every column and row
 by a black one.
***************************************************************************/
void sprite_draw_black_border(SPRITE * sprite)
{
  GdkImage *mask_image, *pixmap_image;
  int x, y;

  pixmap_image =
      gdk_image_get(sprite->pixmap, 0, 0, sprite->width, sprite->height);

  mask_image =
      gdk_image_get(sprite->mask, 0, 0, sprite->width, sprite->height);

  /* parsing columns */
  for (x = 0; x < sprite->width; x++) {
    for (y = 0; y < sprite->height; y++) {
      if (gdk_image_get_pixel(mask_image, x, y) != 0) {
	gdk_image_put_pixel(pixmap_image, x, y, 0);
	break;
      }
    }

    for (y = sprite->height - 1; y > 0; y--) {
      if (gdk_image_get_pixel(mask_image, x, y) != 0) {
	gdk_image_put_pixel(pixmap_image, x, y, 0);
	break;
      }
    }
  }

  /* parsing rows */
  for (y = 0; y < sprite->height; y++) {
    for (x = 0; x < sprite->width; x++) {
      if (gdk_image_get_pixel(mask_image, x, y) != 0) {
	gdk_image_put_pixel(pixmap_image, x, y, 0);
	break;
      }
    }

    for (x = sprite->width - 1; x > 0; x--) {
      if (gdk_image_get_pixel(mask_image, x, y) != 0) {
	gdk_image_put_pixel(pixmap_image, x, y, 0);
	break;
      }
    }
  }

  gdk_draw_image(sprite->pixmap, civ_gc, pixmap_image, 0, 0, 0, 0,
		 sprite->width, sprite->height);

  gdk_image_destroy(mask_image);
  gdk_image_destroy(pixmap_image);
}

/***************************************************************************
  Scales a sprite. If the sprite contains a mask, the mask is scaled
  as as well. The algorithm produces rather fast but beautiful
  results.
***************************************************************************/
SPRITE* sprite_scale(SPRITE *src, int new_w, int new_h)
{
  SPRITE *dst = ctor_sprite_mask(NULL, NULL, new_w, new_h);
  GdkImage *xi_src, *xi_dst, *xb_src;
  guint32 pixel;
  int xoffset_table[4096];
  int x, xoffset, xadd, xremsum, xremadd;
  int y, yoffset, yadd, yremsum, yremadd;

  xi_src = gdk_image_get(src->pixmap, 0, 0, src->width, src->height);

  xi_dst = gdk_image_new(GDK_IMAGE_FASTEST,
			 gdk_window_get_visual(root_window), new_w, new_h);

  /* for each pixel in dst, calculate pixel offset in src */
  xadd = src->width / new_w;
  xremadd = src->width % new_w;
  xoffset = 0;
  xremsum = new_w / 2;

  for (x = 0; x < new_w; x++) {
    xoffset_table[x] = xoffset;
    xoffset += xadd;
    xremsum += xremadd;
    if (xremsum >= new_w) {
      xremsum -= new_w;
      xoffset++;
    }
  }

  yadd = src->height / new_h;
  yremadd = src->height % new_h;
  yoffset = 0;
  yremsum = new_h / 2;

  if (src->has_mask) {
    xb_src = gdk_image_get(src->mask, 0, 0, src->width, src->height);

    dst->mask = gdk_pixmap_new(root_window, new_w, new_h, 1);
    gdk_draw_rectangle(dst->mask, mask_bg_gc, TRUE, 0, 0, -1, -1);

    for (y = 0; y < new_h; y++) {
      for (x = 0; x < new_w; x++) {
	pixel = gdk_image_get_pixel(xi_src, xoffset_table[x], yoffset);
	gdk_image_put_pixel(xi_dst, x, y, pixel);

	if (gdk_image_get_pixel(xb_src, xoffset_table[x], yoffset) != 0) {
	  gdk_draw_point(dst->mask, mask_fg_gc, x, y);
	}
      }

      yoffset += yadd;
      yremsum += yremadd;
      if (yremsum >= new_h) {
	yremsum -= new_h;
	yoffset++;
      }
    }

    gdk_image_destroy(xb_src);
  } else {
    for (y = 0; y < new_h; y++) {
      for (x = 0; x < new_w; x++) {
	pixel = gdk_image_get_pixel(xi_src, xoffset_table[x], yoffset);
	gdk_image_put_pixel(xi_dst, x, y, pixel);
      }

      yoffset += yadd;
      yremsum += yremadd;
      if (yremsum >= new_h) {
	yremsum -= new_h;
	yoffset++;
      }
    }
  }

  dst->pixmap = gdk_pixmap_new(root_window, new_w, new_h, -1);
  gdk_draw_image(dst->pixmap, civ_gc, xi_dst, 0, 0, 0, 0, new_w, new_h);
  gdk_image_destroy(xi_src);
  gdk_image_destroy(xi_dst);

  dst->has_mask = src->has_mask;
  return dst;
}

/***************************************************************************
 Method returns the bounding box of a sprite. It assumes a rectangular
 object/mask. The bounding box contains the border (pixel which have
 unset pixel as neighbours) pixel.
***************************************************************************/
void sprite_get_bounding_box(SPRITE * sprite, int *start_x,
			     int *start_y, int *end_x, int *end_y)
{
  GdkImage *mask_image;
  int i, j;

  if (!sprite->has_mask || !sprite->mask) {
    *start_x = 0;
    *start_y = 0;
    *end_x = sprite->width - 1;
    *end_y = sprite->height - 1;
    return;
  }

  mask_image =
      gdk_image_get(sprite->mask, 0, 0, sprite->width, sprite->height);


  /* parses mask image for the first column that contains a visible pixel */
  *start_x = -1;
  for (i = 0; i < sprite->width && *start_x == -1; i++) {
    for (j = 0; j < sprite->height; j++) {
      if (gdk_image_get_pixel(mask_image, i, j) != 0) {
	*start_x = i;
	break;
      }
    }
  }

  /* parses mask image for the last column that contains a visible pixel */
  *end_x = -1;
  for (i = sprite->width - 1; i >= *start_x && *end_x == -1; i--) {
    for (j = 0; j < sprite->height; j++) {
      if (gdk_image_get_pixel(mask_image, i, j) != 0) {
	*end_x = i;
	break;
      }
    }
  }

  /* parses mask image for the first row that contains a visible pixel */
  *start_y = -1;
  for (i = 0; i < sprite->height && *start_y == -1; i++) {
    for (j = *start_x; j <= *end_x; j++) {
      if (gdk_image_get_pixel(mask_image, j, i) != 0) {
	*start_y = i;
	break;
      }
    }
  }

  /* parses mask image for the last row that contains a visible pixel */
  *end_y = -1;
  for (i = sprite->height - 1; i >= *end_y && *end_y == -1; i--) {
    for (j = *start_x; j <= *end_x; j++) {
      if (gdk_image_get_pixel(mask_image, j, i) != 0) {
	*end_y = i;
	break;
      }
    }
  }

  gdk_image_destroy(mask_image);
}

/*********************************************************************
 Crops all blankspace from a sprite (insofar as is possible as a rectangle)
*********************************************************************/
SPRITE *crop_blankspace(SPRITE *s)
{
  int x1, y1, x2, y2;

  sprite_get_bounding_box(s, &x1, &y1, &x2, &y2);

  return crop_sprite(s, x1, y1, x2 - x1 + 1, y2 - y1 + 1);
}
