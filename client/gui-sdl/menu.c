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

/***************************************************************************
                          menu.c  -  description
                             -------------------
    begin                : Wed Sep 04 2002
    copyright            : (C) 2002 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL/SDL.h>

/* utility */
#include "fcintl.h"

/* common */
#include "unitlist.h"

/* client */
#include "civclient.h" /* get_client_state */
#include "control.h"

/* gui-sdl */
#include "dialogs.h"
#include "gotodlg.h"
#include "graphics.h"
#include "gui_iconv.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "mapctrl.h"
#include "mapview.h"
#include "widget.h"

#include "menu.h"

extern struct widget *pOptions_Button;
  
static struct widget *pBeginOrderWidgetList;
static struct widget *pEndOrderWidgetList;
  
static struct widget *pOrder_Automate_Unit_Button;
static struct widget *pOrder_Build_AddTo_City_Button;
static struct widget *pOrder_Mine_Button;
static struct widget *pOrder_Irrigation_Button;
static struct widget *pOrder_Road_Button;
static struct widget *pOrder_Transform_Button;
static struct widget *pOrder_Trade_Button;
  
#define local_show(ID)                                                \
  clear_wflag(get_widget_pointer_form_ID(pBeginOrderWidgetList, ID, SCAN_FORWARD), \
	      WF_HIDDEN)

#define local_hide(ID)                                             \
  set_wflag(get_widget_pointer_form_ID(pBeginOrderWidgetList, ID, SCAN_FORWARD), \
	    WF_HIDDEN )


/**************************************************************************
  ...
**************************************************************************/
static int unit_order_callback(struct widget *pOrder_Widget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = unit_list_get(get_units_in_focus(), 0);
  
    set_wstate(pOrder_Widget, FC_WS_SELLECTED);
    pSellected_Widget = pOrder_Widget;
  
    if (!pUnit) {
      return -1;
    }
  
    switch (pOrder_Widget->ID) {
    case ID_UNIT_ORDER_BUILD_CITY:
      /* Enable the button for adding to a city in all cases, so we
         get an eventual error message from the server if we try. */
      key_unit_build_city();
      break;
    case ID_UNIT_ORDER_BUILD_WONDER:
      key_unit_build_wonder();
      break;
    case ID_UNIT_ORDER_ROAD:
      key_unit_road();
      break;
    case ID_UNIT_ORDER_TRADEROUTE:
      key_unit_traderoute();
      break;
    case ID_UNIT_ORDER_IRRIGATE:
      key_unit_irrigate();
      break;
    case ID_UNIT_ORDER_MINE:
      key_unit_mine();
      break;
    case ID_UNIT_ORDER_TRANSFORM:
      key_unit_transform();
      break;
    case ID_UNIT_ORDER_FORTRESS:
      key_unit_fortress();
      break;
    case ID_UNIT_ORDER_FORTIFY:
      key_unit_fortify();
      break;
    case ID_UNIT_ORDER_AIRBASE:
      key_unit_airbase();
      break;
    case ID_UNIT_ORDER_POLLUTION:
      key_unit_pollution();
      break;
    case ID_UNIT_ORDER_PARADROP:
      key_unit_paradrop();
      break;
    case ID_UNIT_ORDER_FALLOUT:
      key_unit_fallout();
      break;
    case ID_UNIT_ORDER_SENTRY:
      key_unit_sentry();
      break;
    case ID_UNIT_ORDER_PILLAGE:
      key_unit_pillage();
      break;
    case ID_UNIT_ORDER_HOMECITY:
      key_unit_homecity();
      break;
    case ID_UNIT_ORDER_UNLOAD:
      key_unit_unload_all();
      break;
    case ID_UNIT_ORDER_WAKEUP_OTHERS:
      key_unit_wakeup_others();
      break;
    case ID_UNIT_ORDER_AUTOMATION:
      request_unit_autosettlers(pUnit);
      break;
    case ID_UNIT_ORDER_AUTO_EXPLORE:
      key_unit_auto_explore();
      break;
    case ID_UNIT_ORDER_CONNECT:
#if 0
      /* TODO: different connect types */
      key_unit_connect();
#endif
      break;
    case ID_UNIT_ORDER_PATROL:
      key_unit_patrol();
      break;
    case ID_UNIT_ORDER_GOTO:
      key_unit_goto();
      break;
    case ID_UNIT_ORDER_GOTO_CITY:
      popup_goto_dialog();
      break;
    case ID_UNIT_ORDER_AIRLIFT:
      popup_airlift_dialog();
      break;
    case ID_UNIT_ORDER_RETURN:
      request_unit_return(pUnit);
      break;
    case ID_UNIT_ORDER_UPGRADE:
      popup_unit_upgrade_dlg(pUnit, FALSE);
      break;
    case ID_UNIT_ORDER_DISBAND:
      key_unit_disband();
      break;
    case ID_UNIT_ORDER_DIPLOMAT_DLG:
      key_unit_diplomat_actions();
      break;
    case ID_UNIT_ORDER_NUKE:
      key_unit_nuke();
      break;
    case ID_UNIT_ORDER_WAIT:
      key_unit_wait();
      flush_dirty();
      break;
    case ID_UNIT_ORDER_DONE:
      key_unit_done();
      flush_dirty();
      break;
  
    default:
      break;
    }
  }
  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
#if 0
static bool has_city_airport(struct city *pCity)
{
  return (pCity && (get_city_bonus(pCity, EFT_AIR_VETERAN) > 0));
}
#endif

static Uint16 redraw_order_widgets(void)
{
  Uint16 count = 0;
  struct widget *pTmpWidget = pBeginOrderWidgetList;

  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN)) {
      if (get_wflags(pTmpWidget) & WF_RESTORE_BACKGROUND) {
        refresh_widget_background(pTmpWidget);
      }
      widget_redraw(pTmpWidget);
      widget_mark_dirty(pTmpWidget);
      count++;
    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;
  }

  return count;
}

/**************************************************************************
  ...
**************************************************************************/
static void set_new_order_widget_start_pos(void)
{
  struct widget *pMiniMap = get_minimap_window_widget();
  struct widget *pInfoWind = get_unit_info_window_widget();
  struct widget *pTmpWidget = pBeginOrderWidgetList;
  Sint16 sx, sy, xx, yy = 0;
  int count = 0, lines = 1, w = 0, count_on_line;
  
  xx = pMiniMap->dst->dest_rect.x + pMiniMap->size.w + adj_size(10);
  w = (pInfoWind->dst->dest_rect.x - adj_size(10)) - xx;
  
  if (w < (pTmpWidget->size.w + adj_size(10)) * 2) {
    if(pMiniMap->size.h == pInfoWind->size.h) {
      xx = 0;
      w = Main.screen->w;
      yy = pInfoWind->size.h;
    } else {
      if (pMiniMap->size.h > pInfoWind->size.h) {
        w = Main.screen->w - xx - adj_size(20);
        if (w < (pTmpWidget->size.w + adj_size(10)) * 2) {
	  xx = 0;
	  w = pMiniMap->size.w;
	  yy = pMiniMap->size.h;
        } else {
          yy = pInfoWind->size.h;
        }
      } else {
	w = pInfoWind->dst->dest_rect.x - adj_size(20);
        if (w < (pTmpWidget->size.w + adj_size(10)) * 2) {
	  xx = pInfoWind->dst->dest_rect.x;
	  w = pInfoWind->size.w;
	  yy = pInfoWind->size.h;
        } else {
	  xx = adj_size(10);
          yy = pMiniMap->size.h;
        }
      }
    }
  }
    
  count_on_line = w / (pTmpWidget->size.w + adj_size(5));

  /* find how many to reposition */
  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN)) {
      count++;
    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;

  }

  pTmpWidget = pBeginOrderWidgetList;

  if (count - count_on_line > 0) {

    lines = (count + (count_on_line - 1)) / count_on_line;
  
    count = count_on_line - ((count_on_line * lines) - count);

  }

  sx = xx + (w - count * (pTmpWidget->size.w + adj_size(5))) / 2;

  sy = pTmpWidget->dst->surface->h - yy - lines * (pTmpWidget->size.h + adj_size(5));

  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN)) {

      pTmpWidget->size.x = sx;
      pTmpWidget->size.y = sy;

      count--;
      sx += (pTmpWidget->size.w + adj_size(5));
      if (!count) {
	count = count_on_line;
	lines--;

	sx = xx + (w - count * (pTmpWidget->size.w + adj_size(5))) / 2;

	sy = pTmpWidget->dst->surface->h - yy - lines * (pTmpWidget->size.h + adj_size(5));
      }

    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;

  }
}

/* ================================ PUBLIC ================================ */

/**************************************************************************
  ...
**************************************************************************/
void create_units_order_widgets(void)
{
  struct widget *pBuf = NULL;
  char cBuf[128];
  Uint16 *unibuf;  
  size_t len;
  
  /* No orders */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("No orders"), " (Space)");
  pBuf = create_themeicon(pTheme->ODone_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_SPACE;
  add_to_gui_list(ID_UNIT_ORDER_DONE, pBuf);
  /* --------- */  
  
  pEndOrderWidgetList = pBuf;

  /* Wait */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Wait"), " (W)");
  pBuf = create_themeicon(pTheme->OWait_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_w;
  add_to_gui_list(ID_UNIT_ORDER_WAIT, pBuf);
  /* --------- */  

  /* Explode Nuclear */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Explode Nuclear"), " (Shift + N)");
  pBuf = create_themeicon(pTheme->ONuke_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_n;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_NUKE, pBuf);
  /* --------- */

  /* Diplomat|Spy Actions */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Diplomat|Spy Actions"), " (D)");
  pBuf = create_themeicon(pTheme->OSpy_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_d;
  add_to_gui_list(ID_UNIT_ORDER_DIPLOMAT_DLG, pBuf);
  /* --------- */

  /* Disband */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Disband"), " (Shift + D)");
  pBuf = create_themeicon(pTheme->ODisband_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_d;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_DISBAND, pBuf);
  /* --------- */  

  /* Upgrade */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Upgrade"), " (Shift + U)");
  pBuf = create_themeicon(pTheme->Order_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_u;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_UPGRADE, pBuf);
  /* --------- */

  /* Return to nearest city */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Return to nearest city"), " (Shift + G)");
  pBuf = create_themeicon(pTheme->OReturn_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_g;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_RETURN, pBuf);
  /* --------- */
  
  /* Goto City */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Goto City"), " (L)");
  pBuf = create_themeicon(pTheme->OGotoCity_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_l;
  add_to_gui_list(ID_UNIT_ORDER_GOTO_CITY, pBuf);
  /* --------- */

  /* Airlift */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Airlift"), " (L)");
  pBuf = create_themeicon(pTheme->Order_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_l;
  add_to_gui_list(ID_UNIT_ORDER_AIRLIFT, pBuf);
  /* --------- */
  
  /* Goto location */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Goto location"), " (G)");
  pBuf = create_themeicon(pTheme->OGoto_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_g;
  add_to_gui_list(ID_UNIT_ORDER_GOTO, pBuf);
  /* --------- */

  /* Patrol */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Patrol"), " (Q)");
  pBuf = create_themeicon(pTheme->OPatrol_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_q;
  add_to_gui_list(ID_UNIT_ORDER_PATROL, pBuf);
  /* --------- */

  /* Connect */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Connect"), " (Shift + C)");
  pBuf = create_themeicon(pTheme->OAutoConnect_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_c;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_CONNECT, pBuf);
  /* --------- */

  /* Auto-Explore */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Explore"), " (X)");
  pBuf = create_themeicon(pTheme->OAutoExp_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =
      create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_x;
  add_to_gui_list(ID_UNIT_ORDER_AUTO_EXPLORE, pBuf);
  /* --------- */

  /* Auto-Attack / Auto-Settler */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Attack"), " (A)");
  len = strlen(cBuf);
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Settler"), " (A)");
  len = MAX(len, strlen(cBuf));
  
  pBuf = create_themeicon(pTheme->OAutoSett_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  len = (len + 1) * sizeof(Uint16);
  unibuf = fc_calloc(1, len);
  convertcopy_to_utf16(unibuf, len, cBuf);
  pBuf->string16 = create_string16(unibuf, len, adj_font(10));
  pBuf->key = SDLK_a;
  add_to_gui_list(ID_UNIT_ORDER_AUTOMATION, pBuf);
  
  pOrder_Automate_Unit_Button = pBuf;
  /* --------- */    
  
  /* Wake Up Others */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Wake Up Others"), " (Shift + W)");
  pBuf = create_themeicon(pTheme->OWakeUp_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_w;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_WAKEUP_OTHERS, pBuf);
  /* --------- */

  /* Unload */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Unload"), " (U)");
  pBuf = create_themeicon(pTheme->OUnload_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_u;
  add_to_gui_list(ID_UNIT_ORDER_UNLOAD, pBuf);
  /* --------- */

  /* Find Homecity */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Find Homecity"), " (H)");
  pBuf = create_themeicon(pTheme->OHomeCity_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_h;
  add_to_gui_list(ID_UNIT_ORDER_HOMECITY, pBuf);
  /* --------- */

  /* Pillage */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Pillage"), " (Shift + P)");
  pBuf = create_themeicon(pTheme->OPillage_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_p;
  pBuf->mod = KMOD_SHIFT;
  add_to_gui_list(ID_UNIT_ORDER_PILLAGE, pBuf);
  /* --------- */

  /* Sentry */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Sentry"), " (S)");
  pBuf = create_themeicon(pTheme->OSentry_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_s;
  add_to_gui_list(ID_UNIT_ORDER_SENTRY, pBuf);
  /* --------- */

  /* Clean Nuclear Fallout */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Clean Nuclear Fallout"), " (N)");
  pBuf = create_themeicon(pTheme->OFallout_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_n;
  add_to_gui_list(ID_UNIT_ORDER_FALLOUT, pBuf);
  /* --------- */

  /* Paradrop */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Paradrop"), " (P)");
  pBuf = create_themeicon(pTheme->OParaDrop_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 =  create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_p;
  add_to_gui_list(ID_UNIT_ORDER_PARADROP, pBuf);
  /* --------- */

  /* Clean Pollution */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Clean Pollution"), " (P)");
  pBuf = create_themeicon(pTheme->OPollution_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_p;
  add_to_gui_list(ID_UNIT_ORDER_POLLUTION, pBuf);
  /* --------- */

  /* Build Airbase */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Airbase"), " (E)");
  pBuf = create_themeicon(pTheme->OAirBase_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_e;
  add_to_gui_list(ID_UNIT_ORDER_AIRBASE, pBuf);
  /* --------- */

  /* Fortify */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Fortify"), " (F)");
  pBuf = create_themeicon(pTheme->OFortify_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_f;
  add_to_gui_list(ID_UNIT_ORDER_FORTIFY, pBuf);
  /* --------- */

  /* Build Fortress */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Fortress"), " (F)");
  pBuf = create_themeicon(pTheme->OFortress_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_f;
  add_to_gui_list(ID_UNIT_ORDER_FORTRESS, pBuf);
  /* --------- */

  /* Transform Tile */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Transform Tile"), " (O)");
  pBuf = create_themeicon(pTheme->OTransform_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_o;
  pOrder_Transform_Button = pBuf;
  add_to_gui_list(ID_UNIT_ORDER_TRANSFORM, pBuf);
  /* --------- */

  /* Build Mine */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Mine"), " (M)");
  pBuf = create_themeicon(pTheme->OMine_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_m;
  add_to_gui_list(ID_UNIT_ORDER_MINE, pBuf);
  
  pOrder_Mine_Button = pBuf;
  /* --------- */    

  /* Build Irrigation */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build Irrigation"), " (I)");
  pBuf = create_themeicon(pTheme->OIrrigation_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->key = SDLK_i;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  add_to_gui_list(ID_UNIT_ORDER_IRRIGATE, pBuf);
  
  pOrder_Irrigation_Button = pBuf;
  /* --------- */    

  /* Form Traderoute */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Form Traderoute"), " (R)");
  pBuf = create_themeicon(pTheme->OTrade_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_r;
  add_to_gui_list(ID_UNIT_ORDER_TRADEROUTE, pBuf);

  pOrder_Trade_Button = pBuf;
  /* --------- */    

  /* Build (Rail-)Road */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Railroad"), " (R)", 999, 
			PL_("turn", "turns", 999));
  len = strlen(cBuf);
  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Road"), " (R)", 999, 
			PL_("turn", "turns", 999));
  len = MAX(len, strlen(cBuf));
  
  pBuf = create_themeicon(pTheme->ORoad_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  len = (len + 1) * sizeof(Uint16);
  unibuf = fc_calloc(1, len);
  convertcopy_to_utf16(unibuf, len, cBuf);
  pBuf->string16 = create_string16(unibuf, len, adj_font(10));
  pBuf->key = SDLK_r;
  add_to_gui_list(ID_UNIT_ORDER_ROAD, pBuf);

  pOrder_Road_Button = pBuf;  
  /* --------- */  
  
  /* Help Build Wonder */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Help Build Wonder"), " (B)");
  pBuf = create_themeicon(pTheme->OWonder_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  pBuf->string16 = create_str16_from_char(cBuf, adj_font(10));
  pBuf->key = SDLK_b;
  add_to_gui_list(ID_UNIT_ORDER_BUILD_WONDER, pBuf);
  /* --------- */  

  /* Add to City / Build New City */
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Add to City"), " (B)");
  len = strlen(cBuf);
  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build New City"), " (B)");
  len = MAX(len, strlen(cBuf));
      
  pBuf = create_themeicon(pTheme->OBuildCity_Icon, Main.gui,
			  (WF_HIDDEN | WF_RESTORE_BACKGROUND |
			   WF_WIDGET_HAS_INFO_LABEL));
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = unit_order_callback;
  len = (len + 1) * sizeof(Uint16);
  unibuf = fc_calloc(1, len);
  convertcopy_to_utf16(unibuf, len, cBuf);
  pBuf->string16 = create_string16(unibuf, len, adj_font(10));
  pBuf->key = SDLK_b;
  add_to_gui_list(ID_UNIT_ORDER_BUILD_CITY, pBuf);
  
  pOrder_Build_AddTo_City_Button = pBuf;
  /* --------- */  

  pBeginOrderWidgetList = pBuf;

  SDL_Client_Flags |= CF_ORDERS_WIDGETS_CREATED;
}

/**************************************************************************
  ...
**************************************************************************/
void delete_units_order_widgets(void)
{
  del_group(pBeginOrderWidgetList, pEndOrderWidgetList);
}

/**************************************************************************
  ...
**************************************************************************/
void update_order_widget(void)
{
  set_new_order_widget_start_pos();
  redraw_order_widgets();
}

/**************************************************************************
  ...
**************************************************************************/
void undraw_order_widgets(void)
{
  struct widget *pTmpWidget = pBeginOrderWidgetList;

  while (TRUE) {

    if (!(get_wflags(pTmpWidget) & WF_HIDDEN) && (pTmpWidget->gfx)) {
      widget_undraw(pTmpWidget);
      widget_mark_dirty(pTmpWidget);
    }

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;
  }
}

/**************************************************************************
  ...
**************************************************************************/
void free_bcgd_order_widgets(void)
{
  struct widget *pTmpWidget = pBeginOrderWidgetList;

  while (TRUE) {

    FREESURFACE(pTmpWidget->gfx);

    if (pTmpWidget == pEndOrderWidgetList) {
      break;
    }

    pTmpWidget = pTmpWidget->next;
  }
}


/* ============================== Native =============================== */

/**************************************************************************
  Update all of the menus (sensitivity, etc.) based on the current state.
**************************************************************************/
void update_menus(void)
{
  static Uint16 counter = 0;
  struct unit *pUnit = NULL;
  static char cBuf[128];
  
  if ((get_client_state() != CLIENT_GAME_RUNNING_STATE)) {

    SDL_Client_Flags |= CF_GANE_JUST_STARTED;
	
    if (SDL_Client_Flags & CF_MAP_UNIT_W_CREATED) {
      
      set_wflag(pOptions_Button, WF_HIDDEN);      
      
      struct widget *pWidget = get_unit_info_window_widget();
	
      /* economy button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* research button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
            
      /* revolution button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* show/hide unit's window button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* ------------------------------------ */
      /* mini map window */
      pWidget = get_minimap_window_widget();
      
      /* new turn button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* players button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* find city button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* units button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* show/hide log window button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* toggle minimap mode button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
      /* show/hide minimap button */
      pWidget = pWidget->prev;
      set_wflag(pWidget, WF_HIDDEN);
      
    }

    if (SDL_Client_Flags & CF_ORDERS_WIDGETS_CREATED) {
      hide_group(pBeginOrderWidgetList, pEndOrderWidgetList);
    }

  } else if (!game.player_ptr) {
      
    if (SDL_Client_Flags & CF_MAP_UNIT_W_CREATED) {
      struct widget *pWidget = get_unit_info_window_widget();
          
      clear_wflag(pOptions_Button, WF_HIDDEN);
      widget_redraw(pOptions_Button);
      widget_mark_dirty(pOptions_Button);
	
      /* economy button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);      
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* research button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
            
      /* revolution button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* show/hide unit's window button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_NORMAL);
      
      /* ------------------------------------ */
      /* mini map window */
      pWidget = get_minimap_window_widget();
      
      /* new turn button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* players button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* find city button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* units button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* show/hide log window button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* toggle minimap mode button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_DISABLED);
      
      /* show/hide minimap button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      set_wstate(pWidget, FC_WS_NORMAL);
      
    }

    return;   
    
  } else {
      
    if (get_wstate(pEndOrderWidgetList) == FC_WS_DISABLED) {
      enable_group(pBeginOrderWidgetList, pEndOrderWidgetList);
    }
    
    if (counter) {
      undraw_order_widgets();
    }

    if (SDL_Client_Flags & CF_GANE_JUST_STARTED) {
      struct widget *pWidget = get_unit_info_window_widget();
	
      SDL_Client_Flags &= ~CF_GANE_JUST_STARTED;

      clear_wflag(pOptions_Button, WF_HIDDEN);
      widget_redraw(pOptions_Button);
      widget_mark_dirty(pOptions_Button);
      
      /* economy button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      /* research button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
            
      /* revolution button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      /* show/hide unit's window button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      /* ------------------------------------ */
      /* mini map window */
      pWidget = get_minimap_window_widget();
      
      /* new turn button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      /* players button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      /* find city button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      /* units button */
      pWidget = pWidget->prev;
      if (pWidget->size.y < pWidget->dst->surface->h - pWidget->size.h * 2) {
        clear_wflag(pWidget, WF_HIDDEN);
      }
      
      /* show/hide log window button */
      pWidget = pWidget->prev;
      if (pWidget->size.y < pWidget->dst->surface->h - pWidget->size.h * 2) {
        clear_wflag(pWidget, WF_HIDDEN);
      }
      
      /* toggle minimap mode button */
      pWidget = pWidget->prev;
      if (pWidget->size.y < pWidget->dst->surface->h - pWidget->size.h * 2) {
        clear_wflag(pWidget, WF_HIDDEN);
      }
      
      /* show/hide minimap button */
      pWidget = pWidget->prev;
      clear_wflag(pWidget, WF_HIDDEN);
      
      counter = 0;
    }

    pUnit = unit_list_get(get_units_in_focus(), 0);

    if (pUnit && !pUnit->ai.control) {
      struct city *pHomecity;
      int time;
      struct tile *pTile = pUnit->tile;
      
      if (!counter) {
	local_show(ID_UNIT_ORDER_GOTO);
	local_show(ID_UNIT_ORDER_DISBAND);

	local_show(ID_UNIT_ORDER_WAIT);
	local_show(ID_UNIT_ORDER_DONE);
      }

      /* Enable the button for adding to a city in all cases, so we
       * get an eventual error message from the server if we try. */

      if (can_unit_add_or_build_city(pUnit)) {
	if(pTile->city) {
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Add to City"), " (B)");
	} else {
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Build New City"), " (B)");
	}
	copy_chars_to_string16(pOrder_Build_AddTo_City_Button->string16, cBuf);
	clear_wflag(pOrder_Build_AddTo_City_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Build_AddTo_City_Button, WF_HIDDEN);
      }

      if (unit_can_help_build_wonder_here(pUnit)) {
	local_show(ID_UNIT_ORDER_BUILD_WONDER);
      } else {
	local_hide(ID_UNIT_ORDER_BUILD_WONDER);
      }

      time = can_unit_do_activity(pUnit, ACTIVITY_RAILROAD);
      if (can_unit_do_activity(pUnit, ACTIVITY_ROAD) || time) {
	if(time) {
	  time = tile_activity_time(ACTIVITY_RAILROAD, pUnit->tile);
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Railroad"), " (R)", time , 
			PL_("turn", "turns", time));
	  pOrder_Road_Button->theme = pTheme->ORailRoad_Icon;
	} else {
	  time = tile_activity_time(ACTIVITY_ROAD, pUnit->tile);
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Road"), " (R)", time , 
			PL_("turn", "turns", time));
	  pOrder_Road_Button->theme = pTheme->ORoad_Icon;
	}
	copy_chars_to_string16(pOrder_Road_Button->string16, cBuf);
	clear_wflag(pOrder_Road_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Road_Button, WF_HIDDEN);
      }
      
	/* unit_can_est_traderoute_here(pUnit) */
      if (pTile->city && unit_flag(pUnit, F_TRADE_ROUTE)
        && (pHomecity = find_city_by_id(pUnit->homecity))
	&& can_cities_trade(pHomecity, pTile->city)) {
	int revenue = get_caravan_enter_city_trade_bonus(pHomecity, pTile->city);
	
        if (can_establish_trade_route(pHomecity, pTile->city)) {
          my_snprintf(cBuf, sizeof(cBuf),
      		_("Form Traderoute with %s ( %d R&G + %d trade ) (R)"),
      		pHomecity->name, revenue,
      			trade_between_cities(pHomecity, pTile->city));
	} else {
          revenue = (revenue + 2) / 3;
          my_snprintf(cBuf, sizeof(cBuf),
		_("Trade with %s ( %d R&G bonus ) (R)"), pHomecity->name, revenue);
        }
	copy_chars_to_string16(pOrder_Trade_Button->string16, cBuf);
	clear_wflag(pOrder_Trade_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Trade_Button, WF_HIDDEN);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_IRRIGATE)) {
	time = tile_activity_time(ACTIVITY_IRRIGATE, pUnit->tile);

        if (!strcmp(pTile->terrain->name, "Forest") ||
          !strcmp(pTile->terrain->name, "Jungle")) {
	  /* set Crop Forest Icon */
	  my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
			_("Cut Down to"),
                pTile->terrain->irrigation_result->name
			," (I)", time , PL_("turn", "turns", time));
	  pOrder_Irrigation_Button->theme = pTheme->OCutDownForest_Icon;
        }	else if (!strcmp(pTile->terrain->name, "Swamp")) {
	  my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
			_("Irrigate to"),
                pTile->terrain->irrigation_result->name
			," (I)", time , PL_("turn", "turns", time));
	  pOrder_Irrigation_Button->theme = pTheme->OIrrigation_Icon;
        } else {
	  /* set Irrigation Icon */
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Irrigation"), " (I)", time , 
			PL_("turn", "turns", time));
	  pOrder_Irrigation_Button->theme = pTheme->OIrrigation_Icon;
	}

	copy_chars_to_string16(pOrder_Irrigation_Button->string16, cBuf);
	clear_wflag(pOrder_Irrigation_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Irrigation_Button, WF_HIDDEN);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_MINE)) {
	time = tile_activity_time(ACTIVITY_MINE, pUnit->tile);

   if (!strcmp(pTile->terrain->name, "Forest")) {  
	  /* set Irrigate Icon -> make swamp */
	  my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
			_("Irrigate to"),
			pTile->terrain->mining_result->name
			," (M)", time , PL_("turn", "turns", time));
	  pOrder_Mine_Button->theme = pTheme->OIrrigation_Icon;
   } else if (!strcmp(pTile->terrain->name, "Jungle") ||
              !strcmp(pTile->terrain->name, "Plains") ||
              !strcmp(pTile->terrain->name, "Grassland") ||
              !strcmp(pTile->terrain->name, "Swamp")) {
	  /* set Forest Icon -> plant Forrest*/
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Plant Forest"), " (M)", time , 
			PL_("turn", "turns", time));
	  pOrder_Mine_Button->theme = pTheme->OPlantForest_Icon;
   
   } else {
	  /* set Mining Icon */
	  my_snprintf(cBuf, sizeof(cBuf),"%s%s %d %s",
			_("Build Mine"), " (M)", time , 
			PL_("turn", "turns", time));
	  pOrder_Mine_Button->theme = pTheme->OMine_Icon;
	}

        copy_chars_to_string16(pOrder_Mine_Button->string16, cBuf);
	clear_wflag(pOrder_Mine_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Mine_Button, WF_HIDDEN);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_TRANSFORM)) {
	time = tile_activity_time(ACTIVITY_TRANSFORM, pUnit->tile);
	my_snprintf(cBuf, sizeof(cBuf),"%s %s%s %d %s",
	  _("Transform to"),
	  pTile->terrain->transform_result->name,
			" (M)", time , 
			PL_("turn", "turns", time));
	copy_chars_to_string16(pOrder_Transform_Button->string16, cBuf);
	clear_wflag(pOrder_Transform_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Transform_Button, WF_HIDDEN);
      }

      if (!pTile->city && can_unit_do_activity(pUnit, ACTIVITY_FORTRESS)) {
	local_show(ID_UNIT_ORDER_FORTRESS);
      } else {
	local_hide(ID_UNIT_ORDER_FORTRESS);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_FORTIFYING)) {
	local_show(ID_UNIT_ORDER_FORTIFY);
      } else {
	local_hide(ID_UNIT_ORDER_FORTIFY);
      }

      if (!pTile->city && can_unit_do_activity(pUnit, ACTIVITY_AIRBASE)) {
	local_show(ID_UNIT_ORDER_AIRBASE);
      } else {
	local_hide(ID_UNIT_ORDER_AIRBASE);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_POLLUTION)) {
	local_show(ID_UNIT_ORDER_POLLUTION);
      } else {
	local_hide(ID_UNIT_ORDER_POLLUTION);
      }

      if (can_unit_paradrop(pUnit)) {
	local_show(ID_UNIT_ORDER_PARADROP);
      } else {
	local_hide(ID_UNIT_ORDER_PARADROP);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_FALLOUT)) {
	local_show(ID_UNIT_ORDER_FALLOUT);
      } else {
	local_hide(ID_UNIT_ORDER_FALLOUT);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_SENTRY)) {
	local_show(ID_UNIT_ORDER_SENTRY);
      } else {
	local_hide(ID_UNIT_ORDER_SENTRY);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_PILLAGE)) {
	local_show(ID_UNIT_ORDER_PILLAGE);
      } else {
	local_hide(ID_UNIT_ORDER_PILLAGE);
      }

      if (pTile->city && can_unit_change_homecity(pUnit)
	&& pTile->city->id != pUnit->homecity) {
	local_show(ID_UNIT_ORDER_HOMECITY);
      } else {
	local_hide(ID_UNIT_ORDER_HOMECITY);
      }

      if (pUnit->occupy && get_transporter_occupancy(pUnit) > 0) {
	local_show(ID_UNIT_ORDER_UNLOAD);
      } else {
	local_hide(ID_UNIT_ORDER_UNLOAD);
      }

      if (is_unit_activity_on_tile(ACTIVITY_SENTRY, pUnit->tile)) {
	local_show(ID_UNIT_ORDER_WAKEUP_OTHERS);
      } else {
	local_hide(ID_UNIT_ORDER_WAKEUP_OTHERS);
      }

      if (can_unit_do_autosettlers(pUnit)) {
	if (unit_flag(pUnit, F_SETTLERS)) {
	  if(pOrder_Automate_Unit_Button->theme != pTheme->OAutoSett_Icon) {
	    my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Settler"), " (A)");
	    pOrder_Automate_Unit_Button->theme = pTheme->OAutoSett_Icon;
	    copy_chars_to_string16(pOrder_Automate_Unit_Button->string16, cBuf);
	  }
	} else {
	  if(pOrder_Automate_Unit_Button->theme != pTheme->OAutoAtt_Icon) {
	    my_snprintf(cBuf, sizeof(cBuf),"%s%s", _("Auto-Attack"), " (A)");
	    pOrder_Automate_Unit_Button->theme = pTheme->OAutoAtt_Icon;
	    copy_chars_to_string16(pOrder_Automate_Unit_Button->string16, cBuf);
	  }
	}
	clear_wflag(pOrder_Automate_Unit_Button, WF_HIDDEN);
      } else {
	set_wflag(pOrder_Automate_Unit_Button, WF_HIDDEN);
      }

      if (can_unit_do_activity(pUnit, ACTIVITY_EXPLORE)) {
	local_show(ID_UNIT_ORDER_AUTO_EXPLORE);
      } else {
	local_hide(ID_UNIT_ORDER_AUTO_EXPLORE);
      }

      if (can_unit_do_connect(pUnit, ACTIVITY_IDLE)) {
	local_show(ID_UNIT_ORDER_CONNECT);
      } else {
	local_hide(ID_UNIT_ORDER_CONNECT);
      }

      if (is_diplomat_unit(pUnit) &&
	  diplomat_can_do_action(pUnit, DIPLOMAT_ANY_ACTION, pUnit->tile)) {
	local_show(ID_UNIT_ORDER_DIPLOMAT_DLG);
      } else {
	local_hide(ID_UNIT_ORDER_DIPLOMAT_DLG);
      }

      if (unit_flag(pUnit, F_NUCLEAR)) {
	local_show(ID_UNIT_ORDER_NUKE);
      } else {
	local_hide(ID_UNIT_ORDER_NUKE);
      }

/*      if (pTile->city && has_city_airport(pTile->city) && pTile->city->airlift) {*/
      if (pTile->city && pTile->city->airlift) {      
	local_show(ID_UNIT_ORDER_AIRLIFT);
	hide(ID_UNIT_ORDER_GOTO_CITY);
      } else {
	local_show(ID_UNIT_ORDER_GOTO_CITY);
	local_hide(ID_UNIT_ORDER_AIRLIFT);
      }

      if (pTile->city && can_upgrade_unittype(game.player_ptr, pUnit->type)) {
	local_show(ID_UNIT_ORDER_UPGRADE);
      } else {
	local_hide(ID_UNIT_ORDER_UPGRADE);
      }
            
      set_new_order_widget_start_pos();
      counter = redraw_order_widgets();

    } else {
      if (counter) {
	hide_group(pBeginOrderWidgetList, pEndOrderWidgetList);
      }

      counter = 0;
    }
  }
}

void disable_order_buttons(void)
{
  undraw_order_widgets();
  disable_group(pBeginOrderWidgetList, pEndOrderWidgetList);
  redraw_order_widgets();
}

void enable_order_buttons(void)
{
  undraw_order_widgets();
  enable_group(pBeginOrderWidgetList, pEndOrderWidgetList);
  redraw_order_widgets();
}
