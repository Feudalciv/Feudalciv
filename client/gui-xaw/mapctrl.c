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
#include <stdlib.h>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/SimpleMenu.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/AsciiText.h>  
#include <X11/Xaw/SmeBSB.h>
#include <X11/Xaw/SmeLine.h>

#include "fcintl.h"
#include "game.h"
#include "map.h"
#include "city.h"
#include "player.h"
#include "support.h"
#include "unit.h"

#include "chatline.h"
#include "citydlg.h"
#include "civclient.h"
#include "clinet.h"
#include "colors.h"
#include "control.h"
#include "dialogs.h"
#include "goto.h"
#include "graphics.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "inputdlg.h"
#include "mapview.h"
#include "menu.h"
#include "tilespec.h"

#include "mapctrl.h"

/* Color to use to display the workers */
int city_workers_color=COLOR_STD_WHITE;

static void popupinfo_popdown_callback(Widget w, XtPointer client_data, XtPointer call_data);

/**************************************************************************
...
**************************************************************************/
static void name_new_city_callback(Widget w, XtPointer client_data, 
				   XtPointer call_data)
{
  size_t unit_id;
  
  if((unit_id=(size_t)client_data)) {
    struct packet_unit_request req;
    req.unit_id=unit_id;
    sz_strlcpy(req.name, input_dialog_get_input(w));
    send_packet_unit_request(&aconnection, &req, PACKET_UNIT_BUILD_CITY);
  }
    
  input_dialog_destroy(w);
}

/**************************************************************************
 Popup dialog where the user choose the name of the new city
 punit = (settler) unit which builds the city
 suggestname = suggetion of the new city's name
**************************************************************************/
void popup_newcity_dialog(struct unit *punit, char *suggestname)
{
  input_dialog_create(toplevel, "shellnewcityname",
		      _("What should we call our new city?"), suggestname,
		      name_new_city_callback, INT_TO_XTPOINTER(punit->id),
		      name_new_city_callback, INT_TO_XTPOINTER(0));
}

/**************************************************************************
...
**************************************************************************/
static void popit(int xin, int yin, int xtile, int ytile)
{
  Position x, y;
  int dw, dh;
  Dimension w, h, b;
  static struct map_position cross_list[2+1];
  struct map_position *cross_head = cross_list;
  int i;
  char s[512];
  struct city *pcity;
  struct unit *punit;
  struct tile *ptile=map_get_tile(xtile, ytile);

  
  if(ptile->known>=TILE_KNOWN_FOGGED) {
    Widget p=XtCreatePopupShell("popupinfo", simpleMenuWidgetClass,
				map_canvas, NULL, 0);
    my_snprintf(s, sizeof(s), _("Terrain: %s"),
		map_get_tile_info_text(xtile, ytile));
    XtCreateManagedWidget(s, smeBSBObjectClass, p, NULL, 0);

    my_snprintf(s, sizeof(s), _("Food/Prod/Trade: %s"),
		map_get_tile_fpt_text(xtile, ytile));
    XtCreateManagedWidget(s, smeBSBObjectClass, p, NULL, 0);

    if (tile_has_special(ptile, S_HUT)) {
      XtCreateManagedWidget(_("Minor Tribe Village"), smeBSBObjectClass,
			    p, NULL, 0);
    }
    
    if((pcity=map_get_city(xtile, ytile))) {
      my_snprintf(s, sizeof(s), _("City: %s(%s) %s"), pcity->name,
		  get_nation_name(city_owner(pcity)->nation),
		  city_got_citywalls(pcity) ? _("with City Walls") : "");
      XtCreateManagedWidget(s, smeBSBObjectClass, p, NULL, 0);
    }

    if(get_tile_infrastructure_set(ptile)) {
      sz_strlcpy(s, _("Infrastructure: "));
      sz_strlcat(s, map_get_infrastructure_text(ptile->special));
      XtCreateManagedWidget(s, smeBSBObjectClass, p, NULL, 0);
    }

    if((punit=find_visible_unit(ptile)) && !pcity) {
      char cn[64];
      struct unit_type *ptype=unit_type(punit);
      cn[0]='\0';
      if(punit->owner==game.player_idx) {
	struct city *pcity;
	pcity=player_find_city_by_id(game.player_ptr, punit->homecity);
	if(pcity)
	  my_snprintf(cn, sizeof(cn), "/%s", pcity->name);
      }
      my_snprintf(s, sizeof(s), _("Unit: %s(%s%s)"), ptype->name,
		  get_nation_name(unit_owner(punit)->nation), cn);
      XtCreateManagedWidget(s, smeBSBObjectClass, p, NULL, 0);

      if(punit->owner==game.player_idx)  {
	char uc[64] = "";
	if(unit_list_size(&ptile->units)>=2) {
	  my_snprintf(uc, sizeof(uc), _("  (%d more)"),
		      unit_list_size(&ptile->units) - 1);
	}
        my_snprintf(s, sizeof(s),
		_("A:%d D:%d FP:%d HP:%d/%d%s%s"), ptype->attack_strength, 
	        ptype->defense_strength, ptype->firepower, punit->hp, 
	        ptype->hp, punit->veteran?_(" V"):"", uc);

        if(punit->activity==ACTIVITY_GOTO)  {
	  cross_head->x = goto_dest_x(punit);
	  cross_head->y = goto_dest_y(punit);
	  cross_head++;
        }
      } else {
        my_snprintf(s, sizeof(s),
		    _("A:%d D:%d FP:%d HP:%d0%%"), ptype->attack_strength, 
		    ptype->defense_strength, ptype->firepower, 
		    (punit->hp*100/ptype->hp + 9)/10 );
      }
      XtCreateManagedWidget(s, smeBSBObjectClass, p, NULL, 0);
    }

    cross_head->x = xtile;
    cross_head->y = ytile;
    cross_head++;

    xin /= NORMAL_TILE_WIDTH;
    xin *= NORMAL_TILE_WIDTH;
    yin /= NORMAL_TILE_HEIGHT;
    yin *= NORMAL_TILE_HEIGHT;
    xin += (NORMAL_TILE_WIDTH / 2);
    XtTranslateCoords(map_canvas, xin, yin, &x, &y);
    dw = XDisplayWidth (display, screen_number);
    dh = XDisplayHeight (display, screen_number);
    XtRealizeWidget(p);
    XtVaGetValues(p, XtNwidth, &w, XtNheight, &h, XtNborderWidth, &b, NULL);
    w += (2 * b);
    h += (2 * b);
    x -= (w / 2);
    y -= h;
    if ((x + w) > dw) x = dw - w;
    if (x < 0) x = 0;
    if ((y + h) > dh) y = dh - h;
    if (y < 0) y = 0;
    XtVaSetValues(p, XtNx, x, XtNy, y, NULL);

    cross_head->x = -1;
    for (i = 0; cross_list[i].x >= 0; i++) {
      put_cross_overlay_tile(cross_list[i].x,cross_list[i].y);
    }
    XtAddCallback(p,XtNpopdownCallback,popupinfo_popdown_callback,
		  (XtPointer)cross_list);

    XtPopupSpringLoaded(p);
  }
  
}

/**************************************************************************
...
**************************************************************************/
void popupinfo_popdown_callback(Widget w, XtPointer client_data,
        XtPointer call_data)
{
  struct map_position *cross_list=(struct map_position *)client_data;

  while (cross_list->x >= 0) {
    refresh_tile_mapcanvas(cross_list->x, cross_list->y, TRUE);
    cross_list++;
  }

  XtDestroyWidget(w);
}

/**************************************************************************
(RP:) wake up my own sentried units on the tile that was clicked
**************************************************************************/
void mapctrl_btn_wakeup(XEvent *event)
{
  wakeup_button_pressed(event->xbutton.x, event->xbutton.y);
}

/**************************************************************************
...
**************************************************************************/
void mapctrl_btn_mapcanvas(XEvent *event)
{
  int x, y;
  XButtonEvent *ev=&event->xbutton;

  if (!can_client_change_view()) {
    return;
  }

  if (ev->button == Button1) {
    action_button_pressed(ev->x, ev->y);
  } else if (canvas_to_map_pos(&x, &y, ev->x, ev->y)
	     && (ev->button == Button2 || ev->state & ControlMask)) {
    popit(ev->x, ev->y, x, y);
  } else if (ev->button == Button3) {
    recenter_button_pressed(ev->x, ev->y);
  }
}

/**************************************************************************
...
**************************************************************************/
void create_line_at_mouse_pos(void)
{
  Bool on_same_screen;
  Window root, child;
  int rx, ry, x, y;
  unsigned int mask;

  on_same_screen =
    XQueryPointer(display, XtWindow(map_canvas),
		  &root, &child,
		  &rx, &ry,
		  &x, &y,
		  &mask);

  if (on_same_screen) {
    update_line(x, y);
  }
}

/**************************************************************************
  Draws the on the map the tiles the given city is using
**************************************************************************/
void mapctrl_key_city_workers(XEvent *event)
{
  int x,y, is_real;
  XButtonEvent *ev=&event->xbutton;
  struct city *pcity;

  if (!can_client_change_view()) {
    return;
  }

  x = map_view_x0 + ev->x / NORMAL_TILE_WIDTH;
  y = map_view_y0 + ev->y / NORMAL_TILE_HEIGHT;
  is_real = normalize_map_pos(&x, &y);
  assert(is_real);

  pcity = find_city_near_tile(x,y);
  if(!pcity) return;

  /* Shade tiles on usage */
  city_workers_color = (city_workers_color%3)+1;
  put_city_workers(pcity, city_workers_color);
}

/**************************************************************************
...
**************************************************************************/
void mapctrl_btn_overviewcanvas(XEvent *event)
{
  int xtile, ytile;
  XButtonEvent *ev = &event->xbutton;

  if (!can_client_change_view()) {
    return;
  }

  overview_to_map_pos(&xtile, &ytile, event->xbutton.x, event->xbutton.y);

  if(ev->button==Button1)
    do_unit_goto(xtile,ytile);
  else if(ev->button==Button3)
    center_tile_mapcanvas(xtile, ytile);
}

/**************************************************************************
...
**************************************************************************/
void center_on_unit(void)
{
  request_center_focus_unit();
}

/**************************************************************************
 Enable or disable the turn done button.
 Should probably some where else.
**************************************************************************/
void set_turn_done_button_state(bool state)
{
  XtSetSensitive(turn_done_button, state);
}
