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

#include <windows.h>
#include <windowsx.h>

#include "fcintl.h"
#include "game.h"
#include "log.h"
#include "government.h"         /* government_graphic() */
#include "map.h"
#include "mem.h"
#include "player.h"
#include "rand.h"
#include "support.h"
#include "timing.h"
#include "version.h"

#include "citydlg.h" 
#include "civclient.h"
#include "climap.h"
#include "climisc.h"
#include "colors.h"
#include "control.h" /* get_unit_in_focus() */
#include "graphics.h"
#include "gui_stuff.h"
#include "mapctrl.h"
#include "options.h"
#include "tilespec.h"      
#include "goto.h"
#include "gui_main.h"
#include "mapview.h"

static struct Sprite *indicator_sprite[3];

static HBITMAP intro_gfx;

#define single_tile_pixmap (mapview_canvas.single_tile->bitmap)

extern HBITMAP BITMAP2HBITMAP(BITMAP *bmp);

extern void do_mainwin_layout();

extern int seconds_to_turndone;   
void update_map_canvas_scrollbars_size(void);
void refresh_overview_viewrect_real(HDC hdcp);
void put_one_tile_full(HDC hdc, int x, int y,
			      int canvas_x, int canvas_y, int citymode);
static void pixmap_put_tile_iso(HDC hdc, int x, int y,
                                int canvas_x, int canvas_y,
                                int citymode,
                                int offset_x, int offset_y, int offset_y_unit,
                                int width, int height, int height_unit,
                                enum draw_type draw);
static void dither_tile(HDC hdc, struct Sprite **dither,
                        int canvas_x, int canvas_y,
                        int offset_x, int offset_y,
                        int width, int height, bool fog);
static void draw_rates(HDC hdc);


/***************************************************************************
 ...
***************************************************************************/
struct canvas *canvas_create(int width, int height)
{
  struct canvas *result = fc_malloc(sizeof(*result));
  HDC hdc;
  hdc = GetDC(root_window);
  result->bitmap = CreateCompatibleBitmap(hdc, width, height);
  result->hdc = NULL;
  ReleaseDC(root_window, hdc);
  return result;
}

/***************************************************************************
  ...
***************************************************************************/
void canvas_free(struct canvas *store)
{
  DeleteObject(store->bitmap);
  free(store);
}

static struct canvas overview_canvas;

/****************************************************************************
  Return a canvas that is the overview window.
****************************************************************************/
struct canvas *get_overview_window(void)
{
  return &overview_canvas;
}

/***************************************************************************
   ...
***************************************************************************/
void canvas_copy(struct canvas *dest, struct canvas *src,
		 int src_x, int src_y, int dest_x, int dest_y,
		 int width, int height)
{
  HDC hdcsrc = NULL;
  HDC hdcdst = NULL;
  HDC oldsrc = NULL;
  HDC olddst = NULL;
  if (src->hdc) {
    hdcsrc = src->hdc;
  } else {
    hdcsrc = CreateCompatibleDC(NULL);
    oldsrc = SelectObject(hdcsrc, src->bitmap);
  }
  if (dest->hdc) {
    hdcdst = dest->hdc;
  } else if (dest->bitmap) {
    hdcdst = CreateCompatibleDC(NULL);
    olddst = SelectObject(hdcdst, dest->bitmap);
  } else {
    hdcdst = GetDC(root_window);
  }
  BitBlt(hdcdst, dest_x, dest_y, width, height, hdcsrc, src_x, src_y, SRCCOPY);
  if (!src->hdc) {
    SelectObject(hdcsrc, oldsrc);
    DeleteDC(hdcsrc);
  }
  if (!dest->hdc) {
    if (dest->bitmap) {
      SelectObject(hdcdst, olddst);
      DeleteDC(hdcdst);
    } else {
      ReleaseDC(root_window, hdcdst);
    }
  }
}

/**************************************************************************

**************************************************************************/
void map_expose(HDC hdc)
{

  HBITMAP bmsave;
  HDC introgfxdc;
  if (!can_client_change_view()) {
    if (!intro_gfx_sprite) {
      load_intro_gfx();
    }
    if (!intro_gfx) {
      intro_gfx = BITMAP2HBITMAP(&intro_gfx_sprite->bmp);
    }
    introgfxdc = CreateCompatibleDC(hdc);
    bmsave = SelectObject(introgfxdc, intro_gfx);
    StretchBlt(hdc, 0, 0, map_win_width, map_win_height,
	       introgfxdc, 0, 0,
	       intro_gfx_sprite->width,
	       intro_gfx_sprite->height,
	       SRCCOPY);
    SelectObject(introgfxdc, bmsave);
    DeleteDC(introgfxdc);
  } else {
    HBITMAP old;
    HDC mapstoredc;
    mapstoredc = CreateCompatibleDC(NULL);
    old = SelectObject(mapstoredc, mapstorebitmap);
    BitBlt(hdc, 0, 0, map_win_width, map_win_height,
	   mapstoredc, 0, 0, SRCCOPY);
    SelectObject(mapstoredc, old);
    DeleteDC(mapstoredc);
  }
} 

/**************************************************************************
hack to ensure that mapstorebitmap is usable. 
On win95/win98 mapstorebitmap becomes somehow invalid. 
**************************************************************************/
void check_mapstore()
{
  static int n=0;
  HDC hdc;
  BITMAP bmp;
  if (GetObject(mapstorebitmap,sizeof(BITMAP),&bmp)==0) {
    DeleteObject(mapstorebitmap);
    hdc=GetDC(map_window);
    mapstorebitmap=CreateCompatibleBitmap(hdc,map_win_width,map_win_height);
    update_map_canvas_visible();
    n++;
    assert(n<5);
  }
}

/**************************************************************************

**************************************************************************/
static void draw_rates(HDC hdc)
{
  int d;
  d=0;
  for(;d<(game.player_ptr->economic.luxury)/10;d++)
    draw_sprite(get_citizen_sprite(CITIZEN_ELVIS, d, NULL), hdc,
		SMALL_TILE_WIDTH*d,taxinfoline_y);/* elvis tile */
  for(;d<(game.player_ptr->economic.science+game.player_ptr->economic.luxury)/10;d++)
    draw_sprite(get_citizen_sprite(CITIZEN_SCIENTIST, d, NULL), hdc,
		SMALL_TILE_WIDTH*d,taxinfoline_y); /* scientist tile */    
  for(;d<10;d++)
    draw_sprite(get_citizen_sprite(CITIZEN_TAXMAN, d, NULL), hdc,
		SMALL_TILE_WIDTH*d,taxinfoline_y); /* taxman tile */  
}

/**************************************************************************

**************************************************************************/
void
update_info_label(void)
{
  char buffer2[512];
  char buffer[512];
  HDC hdc;
  my_snprintf(buffer, sizeof(buffer),
	      _("Population: %s\nYear: %s\nGold: %d\nTax: %d Lux: %d Sci: %d"),
		population_to_text(civ_population(game.player_ptr)),
		textyear( game.year ),
		game.player_ptr->economic.gold,
		game.player_ptr->economic.tax,
		game.player_ptr->economic.luxury,
		game.player_ptr->economic.science );      
  my_snprintf(buffer2,sizeof(buffer2),
	      "%s\n%s",get_nation_name(game.player_ptr->nation),buffer);
  SetWindowText(infolabel_win,buffer2);
  do_mainwin_layout();
  set_indicator_icons(client_research_sprite(),
		      client_warming_sprite(),
                      client_cooling_sprite(),
                      game.player_ptr->government);
  
  hdc=GetDC(root_window);
  draw_rates(hdc);
  ReleaseDC(root_window,hdc);
  update_timeout_label();    
  
}

/**************************************************************************

**************************************************************************/
void
update_unit_info_label(struct unit *punit)
{
    if(punit) {
    char buffer[512];
    char buffer2[512];
    
    struct city *pcity;
    pcity=player_find_city_by_id(game.player_ptr, punit->homecity);
 
    my_snprintf(buffer, sizeof(buffer), "%s %s",
            unit_type(punit)->name,
            (punit->veteran) ? _("(veteran)") : "" );
    /* FIXME */         
    my_snprintf(buffer2, sizeof(buffer2), "%s\n%s\n%s\n%s",buffer,
		unit_activity_text(punit),
		map_get_tile_info_text(punit->x, punit->y),
		pcity ? pcity->name : "");     
    SetWindowText(unitinfo_win,buffer2);
    }
    else
      {
	SetWindowText(unitinfo_win,"");
	
      }
    do_mainwin_layout();
}

/**************************************************************************

**************************************************************************/
void
update_timeout_label(void)
{
  char buffer[512];
  
  if (game.timeout <= 0)
    sz_strlcpy(buffer, Q_("?timeout:off"));
  else
    format_duration(buffer, sizeof(buffer), seconds_to_turndone);
  SetWindowText(timeout_label,buffer);
}

/**************************************************************************

**************************************************************************/
void update_turn_done_button(bool do_restore)
{
  static bool flip = FALSE;

  if (!get_turn_done_button_state()) {
    return;
  }

  if (do_restore) {
    flip = FALSE;
    Button_SetState(turndone_button, 0);
  } else {
    Button_SetState(turndone_button, flip);
    flip = !flip;
  }
}

/**************************************************************************

**************************************************************************/
void
set_indicator_icons(int bulb, int sol, int flake, int gov)
{
  int i;
  HDC hdc;
  
  bulb = CLIP(0, bulb, NUM_TILES_PROGRESS-1);
  sol = CLIP(0, sol, NUM_TILES_PROGRESS-1);
  flake = CLIP(0, flake, NUM_TILES_PROGRESS-1);     
  indicator_sprite[0]=sprites.bulb[bulb];
  indicator_sprite[1]=sprites.warming[sol];
  indicator_sprite[2]=sprites.cooling[flake];
  if (game.government_count==0) {
    /* not sure what to do here */
    indicator_sprite[3] = get_citizen_sprite(CITIZEN_UNHAPPY, 0, NULL);
  } else {
    indicator_sprite[3] = get_government(gov)->sprite;    
  }
  hdc=GetDC(root_window);
  for(i=0;i<4;i++)
    draw_sprite(indicator_sprite[i],hdc,i*SMALL_TILE_WIDTH,indicator_y); 
  ReleaseDC(root_window,hdc);
}

/**************************************************************************

**************************************************************************/
void
map_size_changed(void)
{
  set_overview_win_dim(OVERVIEW_TILE_WIDTH * map.xsize,OVERVIEW_TILE_HEIGHT * map.ysize);
}

/**************************************************************************
  Flush the given part of the canvas buffer (if there is one) to the
  screen.
**************************************************************************/
void flush_mapcanvas(int canvas_x, int canvas_y,
		     int pixel_width, int pixel_height)
{
  HDC hdcwin = GetDC(map_window);
  HDC mapstoredc = CreateCompatibleDC(NULL);
  HBITMAP old = SelectObject(mapstoredc, mapstorebitmap);
  BitBlt(hdcwin, canvas_x, canvas_y,
	 pixel_width, pixel_height,
	 mapstoredc,
	 canvas_x, canvas_y,
	 SRCCOPY);
  ReleaseDC(map_window, hdcwin);
  SelectObject(mapstoredc, old);
  DeleteDC(mapstoredc);
}

#define MAX_DIRTY_RECTS 20
static int num_dirty_rects = 0;
static struct {
  int x, y, w, h;
} dirty_rects[MAX_DIRTY_RECTS];
bool is_flush_queued = FALSE;

/**************************************************************************
  A callback invoked as a result of a timer event, this function simply
  flushes the mapview canvas.
**************************************************************************/
static VOID CALLBACK unqueue_flush(HWND hwnd, UINT uMsg, UINT idEvent,
				   DWORD dwTime)
{
  flush_dirty();
  is_flush_queued = FALSE;
}

/**************************************************************************
  Called when a region is marked dirty, this function queues a flush event
  to be handled later.  The flush may end up being done by freeciv before
  then, in which case it will be a wasted call.
**************************************************************************/
static void queue_flush(void)
{
  if (!is_flush_queued) {
    SetTimer(root_window, 4, 0, unqueue_flush);
    is_flush_queued = TRUE;
  }
}

/**************************************************************************
  Mark the rectangular region as 'dirty' so that we know to flush it
  later.
**************************************************************************/
void dirty_rect(int canvas_x, int canvas_y,
		int pixel_width, int pixel_height)
{
  if (num_dirty_rects < MAX_DIRTY_RECTS) {
    dirty_rects[num_dirty_rects].x = canvas_x;
    dirty_rects[num_dirty_rects].y = canvas_y;
    dirty_rects[num_dirty_rects].w = pixel_width;
    dirty_rects[num_dirty_rects].h = pixel_height;
    num_dirty_rects++;
    queue_flush();
  }
}

/**************************************************************************
  Mark the entire screen area as "dirty" so that we can flush it later.
**************************************************************************/
void dirty_all(void)
{
  num_dirty_rects = MAX_DIRTY_RECTS;
  queue_flush();
}

/**************************************************************************
  Flush all regions that have been previously marked as dirty.  See
  dirty_rect and dirty_all.  This function is generally called after we've
  processed a batch of drawing operations.
**************************************************************************/
void flush_dirty(void)
{
  if (num_dirty_rects == MAX_DIRTY_RECTS) {
    flush_mapcanvas(0, 0, map_win_width, map_win_height);
  } else {
    int i;

    for (i = 0; i < num_dirty_rects; i++) {
      flush_mapcanvas(dirty_rects[i].x, dirty_rects[i].y,
		      dirty_rects[i].w, dirty_rects[i].h);
    }
  }
  num_dirty_rects = 0;
}

/****************************************************************************
  Do any necessary synchronization to make sure the screen is up-to-date.
  The canvas should have already been flushed to screen via flush_dirty -
  all this function does is make sure the hardware has caught up.
****************************************************************************/
void gui_flush(void)
{
  GdiFlush();
}

/**************************************************************************

**************************************************************************/
void update_map_canvas_scrollbars_size(void)
{
  int xmin, ymin, xmax, ymax, xsize, ysize;

  get_mapview_scroll_window(&xmin, &ymin, &xmax, &ymax, &xsize, &ysize);
  ScrollBar_SetRange(map_scroll_h, xmin, xmax, TRUE);
  ScrollBar_SetRange(map_scroll_v, ymin, ymax, TRUE);
}

/**************************************************************************

**************************************************************************/
void
update_map_canvas_scrollbars(void)
{
  int scroll_x, scroll_y;

  get_mapview_scroll_pos(&scroll_x, &scroll_y);
  ScrollBar_SetPos(map_scroll_h, scroll_x, TRUE);
  ScrollBar_SetPos(map_scroll_v, scroll_y, TRUE);
}

/**************************************************************************

**************************************************************************/
void
update_city_descriptions(void)
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

**************************************************************************/
void show_city_desc(struct city *pcity, int canvas_x, int canvas_y)
{
  char buffer[500];
  int y_offset;
  HDC hdc;
  HBITMAP old;

  /* TODO: hdc should be stored statically */
  hdc = CreateCompatibleDC(NULL);
  old = SelectObject(hdc, mapstorebitmap);
  SetBkMode(hdc,TRANSPARENT);

  y_offset = canvas_y + NORMAL_TILE_HEIGHT;
  if (draw_city_names && pcity->name) {
    RECT rc;

    /* FIXME: draw city growth as well, using
     * get_city_mapview_name_and_growth() */

    DrawText(hdc, pcity->name, strlen(pcity->name), &rc, DT_CALCRECT);
    rc.left = canvas_x + NORMAL_TILE_WIDTH / 2 - 10;
    rc.right = rc.left + 20;
    rc.bottom -= rc.top;
    rc.top = y_offset;
    rc.bottom += rc.top;
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawText(hdc, pcity->name, strlen(pcity->name), &rc,
	     DT_NOCLIP | DT_CENTER);
    rc.left++;
    rc.top--;
    rc.right++;
    rc.bottom--;
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, pcity->name, strlen(pcity->name), &rc,
	     DT_NOCLIP | DT_CENTER);
    y_offset = rc.bottom + 2;
  }

  if (draw_city_productions && pcity->owner == game.player_idx) {
    RECT rc;

    get_city_mapview_production(pcity, buffer, sizeof(buffer));
      
    DrawText(hdc, buffer, strlen(buffer), &rc, DT_CALCRECT);
    rc.left = canvas_x + NORMAL_TILE_WIDTH / 2 - 10;
    rc.right = rc.left + 20;
    rc.bottom -= rc.top;
    rc.top = y_offset;
    rc.bottom += rc.top; 
    SetTextColor(hdc, RGB(0, 0, 0));
    DrawText(hdc, buffer, strlen(buffer), &rc, DT_NOCLIP | DT_CENTER);
    rc.left++;
    rc.top--;
    rc.right++;
    rc.bottom--;
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawText(hdc, buffer, strlen(buffer), &rc, DT_NOCLIP | DT_CENTER);
  }

  SelectObject(hdc, old);
  DeleteDC(hdc);
}

/**************************************************************************

**************************************************************************/
void
put_cross_overlay_tile(int x,int y)
{
  HDC hdc;
  int canvas_x, canvas_y;
  get_canvas_xy(x, y, &canvas_x, &canvas_y);
  if (tile_visible_mapcanvas(x, y)) {
    hdc=GetDC(map_window);
    draw_sprite(sprites.user.attention,hdc,canvas_x,canvas_y);
    ReleaseDC(map_window,hdc);
  }
}

/**************************************************************************

**************************************************************************/
void
put_city_workers(struct city *pcity, int color)
{
	/* PORTME */
}

/**************************************************************************

**************************************************************************/
void overview_expose(HDC hdc)
{
  HDC hdctest;
  HBITMAP old;
  HBITMAP bmp;
  int i;
  if (!can_client_change_view()) {
      if (!radar_gfx_sprite) {
	load_intro_gfx();
      }
      if (radar_gfx_sprite) {
	char s[64];
	int h;
	RECT rc;
	draw_sprite(radar_gfx_sprite,hdc,overview_win_x,overview_win_y);
	SetBkMode(hdc,TRANSPARENT);
	my_snprintf(s, sizeof(s), "%d.%d.%d%s",
		    MAJOR_VERSION, MINOR_VERSION,
		    PATCH_VERSION, VERSION_LABEL);
	DrawText(hdc, word_version(), strlen(word_version()), &rc, DT_CALCRECT);
	h=rc.bottom-rc.top;
	rc.left = overview_win_x;
	rc.right = overview_win_y + overview_win_width;
	rc.bottom = overview_win_y + overview_win_height - h - 2; 
	rc.top = rc.bottom - h;
	SetTextColor(hdc, RGB(0,0,0));
	DrawText(hdc, word_version(), strlen(word_version()), &rc, DT_CENTER);
	rc.top+=h;
	rc.bottom+=h;
	DrawText(hdc, s, strlen(s), &rc, DT_CENTER);
	rc.left++;
	rc.right++;
	rc.top--;
	rc.bottom--;
	SetTextColor(hdc, RGB(255,255,255));
	DrawText(hdc, s, strlen(s), &rc, DT_CENTER);
	rc.top-=h;
	rc.bottom-=h;
	DrawText(hdc, word_version(), strlen(word_version()), &rc, DT_CENTER);
      }
    }
  else
    {
      hdctest=CreateCompatibleDC(NULL);
      old=NULL;
      bmp=NULL;
      for(i=0;i<4;i++)
	if (indicator_sprite[i]) {
	  bmp=BITMAP2HBITMAP(&indicator_sprite[i]->bmp);
	  if (!old)
	    old=SelectObject(hdctest,bmp);
	  else
	    DeleteObject(SelectObject(hdctest,bmp));
	  BitBlt(hdc,i*SMALL_TILE_WIDTH,indicator_y,
		 SMALL_TILE_WIDTH,SMALL_TILE_HEIGHT,
		 hdctest,0,0,SRCCOPY);
	}
      SelectObject(hdctest,old);
      if (bmp)
	DeleteObject(bmp);
      DeleteDC(hdctest);
      draw_rates(hdc);
      overview_canvas.hdc = hdc;
      refresh_overview_canvas(/* hdc */);
      overview_canvas.hdc = NULL;
    }
}

/**************************************************************************

**************************************************************************/
void map_handle_hscroll(int pos)
{
  int scroll_x, scroll_y;

  if (!can_client_change_view()) {
    return;
  }

  get_mapview_scroll_pos(&scroll_x, &scroll_y);
  set_mapview_scroll_pos(pos, scroll_y);
}

/**************************************************************************

**************************************************************************/
void map_handle_vscroll(int pos)
{
  int scroll_x, scroll_y;

  if (!can_client_change_view()) {
    return;
  }

  get_mapview_scroll_pos(&scroll_x, &scroll_y);
  set_mapview_scroll_pos(scroll_x, pos);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_black_tile_iso(HDC hdc,
                                      int canvas_x, int canvas_y,
                                      int offset_x, int offset_y,
                                      int width, int height)
{
  draw_sprite_part(sprites.black_tile,hdc,canvas_x+offset_x,canvas_y+offset_y,
		   width,height,
		   offset_x,offset_x);
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_overlay_tile_draw(HDC hdc,
                                         int canvas_x, int canvas_y,
                                         struct Sprite *ssprite,
                                         int offset_x, int offset_y,
                                         int width, int height,
                                         bool fog)
{
  if (!ssprite || !width || !height)
    return;
  
  draw_sprite_part(ssprite,hdc,canvas_x+offset_x,canvas_y+offset_y,
		   MIN(width, MAX(0,ssprite->width-offset_x)),
		   MIN(height, MAX(0,ssprite->height-offset_y)),
		   offset_x,offset_y);

  if (fog) {
    draw_fog_part(hdc,canvas_x+offset_x,canvas_y+offset_y,
		  MIN(width, MAX(0,ssprite->width-offset_x)),
		  MIN(height, MAX(0,ssprite->height-offset_y)),
		  offset_x,offset_y,ssprite); 
    
  }
  
}

/**************************************************************************
Only used for isometric view.
**************************************************************************/
void put_one_tile_full(HDC hdc, int x, int y,
                       int canvas_x, int canvas_y, int citymode)
{
  pixmap_put_tile_iso(hdc, x, y, canvas_x, canvas_y, citymode,
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
  HDC hdc;
  HBITMAP old = NULL; /*Remove warning*/

  /* FIXME: we don't want to have to recreate the hdc each time! */
  if (pcanvas->bitmap) {
    hdc = CreateCompatibleDC(pcanvas->hdc);
    old = SelectObject(hdc, pcanvas->bitmap);
  } else {
    hdc = pcanvas->hdc;
  }
  pixmap_put_tile_iso(hdc, map_x, map_y,
		      canvas_x, canvas_y, 0,
		      offset_x, offset_y, offset_y_unit,
		      width, height, height_unit,
		      draw);
  if (pcanvas->bitmap) {
    SelectObject(hdc, old);
    DeleteDC(hdc);
  }
}

/**************************************************************************
  Draw some or all of a sprite onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_sprite(struct canvas *pcanvas,
		       int canvas_x, int canvas_y,
		       struct Sprite *sprite,
		       int offset_x, int offset_y, int width, int height)
{
  HDC hdc;
  HBITMAP old = NULL; /*Remove warning*/

  /* FIXME: we don't want to have to recreate the hdc each time! */
  if (pcanvas->bitmap) {
    hdc = CreateCompatibleDC(pcanvas->hdc);
    old = SelectObject(hdc, pcanvas->bitmap);
  } else {
    hdc = pcanvas->hdc;
  }
  pixmap_put_overlay_tile_draw(hdc, canvas_x, canvas_y,
			       sprite, offset_x, offset_y,
			       width, height, 0);
  if (pcanvas->bitmap) {
    SelectObject(hdc, old);
    DeleteDC(hdc);
  }
}

/**************************************************************************
  Draw a full sprite onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_sprite_full(struct canvas *pcanvas,
			    int canvas_x, int canvas_y,
			    struct Sprite *sprite)
{
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
  HDC hdc;
  HBITMAP old = NULL; /*Remove warning*/
  RECT rect;

  if (pcanvas->bitmap) {
    hdc = CreateCompatibleDC(pcanvas->hdc);
    old = SelectObject(hdc, pcanvas->bitmap);
  } else {
    hdc = pcanvas->hdc;
  }

  /*"+1"s are needed because FillRect doesn't fill bottom and right edges*/
  SetRect(&rect, canvas_x, canvas_y, canvas_x + width + 1,
		 canvas_y + height + 1);

  FillRect(hdc, &rect, brush_std[color]);

  if (pcanvas->bitmap) {
    SelectObject(hdc, old);
    DeleteDC(hdc);
  }
}

/**************************************************************************
  Draw a 1-pixel-width colored line onto the mapview or citydialog canvas.
**************************************************************************/
void canvas_put_line(struct canvas *pcanvas, enum color_std color,
		     enum line_type ltype, int start_x, int start_y,
		     int dx, int dy)
{
  HDC hdc;
  HBITMAP old = NULL; /*Remove warning*/
  HPEN old_pen;

  if (pcanvas->hdc) {
    hdc = pcanvas->hdc;
  } else if (pcanvas->bitmap) {
    hdc = CreateCompatibleDC(pcanvas->hdc);
    old = SelectObject(hdc, pcanvas->bitmap);
  } else {
    hdc = GetDC(root_window);
  }

  /* FIXME: set line type (size). */
  old_pen = SelectObject(hdc, pen_std[color]);
  MoveToEx(hdc, start_x, start_y, NULL);
  LineTo(hdc, start_x + dx, start_y + dy);
  SelectObject(hdc, old_pen);

  if (!pcanvas->hdc) {
    if (pcanvas->bitmap) {
      SelectObject(hdc, old);
      DeleteDC(hdc);
    } else {
      ReleaseDC(root_window, hdc);
    }
  }

}


/**************************************************************************
  Put a drawn sprite (with given offset) onto the pixmap.
**************************************************************************/
static void pixmap_put_drawn_sprite(HDC hdc,
                                    int canvas_x, int canvas_y,
                                    struct drawn_sprite *pdsprite,
                                    int offset_x, int offset_y,
                                    int width, int height,
                                    bool fog)
{
   
  int ox = pdsprite->offset_x, oy = pdsprite->offset_y;
  
  
  pixmap_put_overlay_tile_draw(hdc, canvas_x + ox, canvas_y + oy,
                               pdsprite->sprite,
                               offset_x - ox, offset_y - oy,
                               width, height,
                               fog);
  
}



/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void put_city_pixmap_draw(struct city *pcity,HDC hdc,
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
      pixmap_put_drawn_sprite(hdc, canvas_x, canvas_y, &sprites[i],
                                   offset_x, offset_y_unit,
                                   width, height_unit,
                                   fog);
    }
  }
}


/**************************************************************************
Only used for isometric view.
**************************************************************************/
static void pixmap_put_tile_iso(HDC hdc, int x, int y,
                                int canvas_x, int canvas_y,
                                int citymode,
                                int offset_x, int offset_y, int offset_y_unit,
                                int width, int height, int height_unit,
                                enum draw_type draw)
{
  struct drawn_sprite tile_sprs[80];
  struct city *pcity;
  struct unit *punit, *pfocus;
  struct canvas canvas_store={hdc,NULL};
  enum tile_special_type special;
  int count, i;
  bool fog, solid_bg, is_real;

  if (!width || !(height || height_unit))
    return;

  count = fill_tile_sprite_array_iso(tile_sprs, x, y, citymode, &solid_bg);

  if (count == -1) { /* tile is unknown */
    pixmap_put_black_tile_iso(hdc, canvas_x, canvas_y,
                              offset_x, offset_y, width, height);
    return;
  }
  is_real = normalize_map_pos(&x, &y);
  assert(is_real);
  fog = tile_get_known(x, y) == TILE_KNOWN_FOGGED && draw_fog_of_war;
  pcity = map_get_city(x, y);
  punit = get_drawable_unit(x, y, citymode);
  pfocus = get_unit_in_focus();
  special = map_get_special(x, y);

  if (solid_bg) {
    HPEN oldpen;
    HBRUSH oldbrush;
    POINT points[4];
    points[0].x=canvas_x+NORMAL_TILE_WIDTH/2;
    points[0].y=canvas_y;
    points[1].x=canvas_x;
    points[1].y=canvas_y+NORMAL_TILE_HEIGHT/2;
    points[2].x=canvas_x+NORMAL_TILE_WIDTH/2;
    points[2].y=canvas_y+NORMAL_TILE_HEIGHT;
    points[3].x=canvas_x+NORMAL_TILE_WIDTH;
    points[3].y=canvas_y+NORMAL_TILE_HEIGHT/2;
    oldpen=SelectObject(hdc,pen_std[COLOR_STD_BACKGROUND]); 
    oldbrush=SelectObject(hdc,brush_std[COLOR_STD_BACKGROUND]);
    Polygon(hdc,points,4);
    SelectObject(hdc,oldpen);
    SelectObject(hdc,oldbrush);
  }

  /*** Draw terrain and specials ***/
  for (i = 0; i < count; i++) {
    if (tile_sprs[i].sprite)
      pixmap_put_drawn_sprite(hdc, canvas_x, canvas_y, &tile_sprs[i],
                                   offset_x, offset_y, width, height, fog);
    else
      freelog(LOG_ERROR, "sprite is NULL");
  }

  /*** Grid (map grid, borders, coastline, etc.) ***/
  tile_draw_grid_iso(&canvas_store, x, y, canvas_x, canvas_y, draw);

  if (draw_coastline && !draw_terrain) {
    enum tile_terrain_type t1 = map_get_terrain(x, y), t2;
    int x1, y1;
    HPEN old;
    old=SelectObject(hdc,pen_std[COLOR_STD_OCEAN]);
    x1=x;
    y1=y-1;
    if (normalize_map_pos(&x1,&y1)) { 
      t2=map_get_terrain(x1,y1);
      if (draw & D_M_R && (is_ocean(t1) ^ is_ocean(t2))) {
	MoveToEx(hdc,canvas_x+NORMAL_TILE_WIDTH/2,canvas_y,NULL);
	LineTo(hdc,canvas_x+NORMAL_TILE_WIDTH,
	       canvas_y+NORMAL_TILE_HEIGHT/2);
      }
    }
    x1=x-1; 
    y1=y;
    if (normalize_map_pos(&x1, &y1)) {
      t2 = map_get_terrain(x1, y1);
      if (draw & D_M_L && (is_ocean(t1) ^ is_ocean(t2))) {
	MoveToEx(hdc,canvas_x,canvas_y+NORMAL_TILE_HEIGHT/2,NULL);
	LineTo(hdc,canvas_x+NORMAL_TILE_WIDTH/2,canvas_y); 
      }
    }
  }
  
  /*** City and various terrain improvements ***/
  if (pcity && draw_cities) {
    put_city_pixmap_draw(pcity, hdc,
			 canvas_x, canvas_y - NORMAL_TILE_HEIGHT/2,
                         offset_x, offset_y_unit,
                         width, height_unit, fog);
  }
  
  if (contains_special(special, S_AIRBASE) && draw_fortress_airbase)
    pixmap_put_overlay_tile_draw(hdc,
                                 canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
                                 sprites.tx.airbase,
                                 offset_x, offset_y_unit,
                                 width, height_unit, fog);
  if (contains_special(special, S_FALLOUT) && draw_pollution)
    pixmap_put_overlay_tile_draw(hdc,
                                 canvas_x, canvas_y,
                                 sprites.tx.fallout,
                                 offset_x, offset_y,
                                 width, height, fog);
  if (contains_special(special, S_POLLUTION) && draw_pollution)
    pixmap_put_overlay_tile_draw(hdc,
                                 canvas_x, canvas_y,
                                 sprites.tx.pollution,
                                 offset_x, offset_y,
                                 width, height, fog);
  
  /*** city size ***/
  /* Not fogged as it would be unreadable */
  if (pcity && draw_cities) {
    if (pcity->size>=10)
      pixmap_put_overlay_tile_draw(hdc, 
				   canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
                                   sprites.city.size_tens[pcity->size/10],
                                   offset_x, offset_y_unit,
				   width, height_unit, 0);

    pixmap_put_overlay_tile_draw(hdc, canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
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
    pixmap_put_overlay_tile_draw(hdc,
                                 canvas_x, canvas_y-NORMAL_TILE_HEIGHT/2,
                                 sprites.tx.fortress,
                                 offset_x, offset_y_unit,
                                 width, height_unit, fog);
  
}

/**************************************************************************
 Area Selection
**************************************************************************/
void draw_selection_rectangle(int canvas_x, int canvas_y, int w, int h)
{
  /* PORTME */
}

/**************************************************************************
  This function is called when the tileset is changed.
**************************************************************************/
void tileset_changed(void)
{
  /* PORTME */
  /* Here you should do any necessary redraws (for instance, the city
   * dialogs usually need to be resized).
   */
  
  indicator_sprite[0] = NULL;
  indicator_sprite[1] = NULL;
  indicator_sprite[2] = NULL;
  citydlg_tileset_change();
}
