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

/**********************************************************************
                          citydlg.c  -  description
                             -------------------
    begin                : Wed Sep 04 2002
    copyright            : (C) 2002 by Rafał Bursig
    email                : Rafał Bursig <bursig@poczta.fm>
 **********************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <SDL/SDL.h>

/* utility */
#include "fcintl.h"
#include "log.h"

/* common */
#include "game.h"
#include "unitlist.h"

/* client */
#include "civclient.h"
#include "climisc.h"
#include "clinet.h"
#include "control.h"
#include "text.h"

/* gui-sdl */
#include "cityrep.h"
#include "cma_fe.h"
#include "colors.h"
#include "dialogs.h"
#include "graphics.h"
#include "gui_iconv.h"
#include "gui_id.h"
#include "gui_main.h"
#include "gui_tilespec.h"
#include "gui_zoom.h"
#include "mapview.h"
#include "menu.h"
#include "sprite.h"
#include "themespec.h"
#include "widget.h"
#include "wldlg.h"

#include "citydlg.h"

/* ============================================================= */
#define SCALLED_TILE_WIDTH	48
#define SCALLED_TILE_HEIGHT	24

#define NUM_UNITS_SHOWN		 3

static struct city_dialog {
  struct city *pCity;

  enum {
    INFO_PAGE = 0,
    HAPPINESS_PAGE = 1,
    ARMY_PAGE,
    SUPPORTED_UNITS_PAGE,
    MISC_PAGE
  } page;

  /* main window group list */
  struct widget *pBeginCityWidgetList;
  struct widget *pEndCityWidgetList;

  /* Imprvm. vscrollbar */
  struct ADVANCED_DLG *pImprv;
  
  /* Penel group list */
  struct ADVANCED_DLG *pPanel;
    
  /* Menu imprv. dlg. */
  struct widget *pBeginCityMenuWidgetList;
  struct widget *pEndCityMenuWidgetList;

  /* shortcuts */
  struct widget *pAdd_Point;
  struct widget *pBuy_Button;
  struct widget *pResource_Map;
  struct widget *pCity_Name_Edit;

  SDL_Rect specs_area[3];	/* active area of specialist
				   0 - elvis
				   1 - taxman
				   2 - scientists
				   change when pressed on this area */
  bool specs[3];
  
  bool lock;
} *pCityDlg = NULL;

enum specialist_type {
  SP_ELVIS, SP_SCIENTIST, SP_TAXMAN, SP_LAST   
};

static float city_map_zoom = 1;

static struct SMALL_DLG *pHurry_Prod_Dlg = NULL;

static void popdown_hurry_production_dialog(void);
static void disable_city_dlg_widgets(void);
static void redraw_city_dialog(struct city *pCity);
static void rebuild_imprm_list(struct city *pCity);
static void rebuild_citydlg_title_str(struct widget *pWindow, struct city *pCity);

/* ======================================================================= */

Impr_type_id get_building_for_effect(enum effect_type effect_type) {
 
  impr_type_iterate(imp) {
    if (building_has_effect(imp, effect_type))
      return imp;        
  } impr_type_iterate_end;
  
  return B_LAST;  
}

/**************************************************************************
  Destroy City Menu Dlg but not undraw.
**************************************************************************/
static void del_city_menu_dlg(bool enable)
{
  if (pCityDlg->pEndCityMenuWidgetList) {
    del_group_of_widgets_from_gui_list(pCityDlg->pBeginCityMenuWidgetList,
				       pCityDlg->pEndCityMenuWidgetList);
    pCityDlg->pEndCityMenuWidgetList = NULL;
  }
  if (enable) {
    /* enable city dlg */
    enable_city_dlg_widgets();
  }
}

/**************************************************************************
  Destroy City Dlg
**************************************************************************/
static void del_city_dialog(void)
{
  if (pCityDlg) {

    if (pCityDlg->pImprv->pEndWidgetList) {
      del_group_of_widgets_from_gui_list(pCityDlg->pImprv->pBeginWidgetList,
					 pCityDlg->pImprv->pEndWidgetList);
    }
    FC_FREE(pCityDlg->pImprv->pScroll);
    FC_FREE(pCityDlg->pImprv);

    if (pCityDlg->pPanel) {
      del_group_of_widgets_from_gui_list(pCityDlg->pPanel->pBeginWidgetList,
					 pCityDlg->pPanel->pEndWidgetList);
      FC_FREE(pCityDlg->pPanel->pScroll);
      FC_FREE(pCityDlg->pPanel);
    }
        
    if (pHurry_Prod_Dlg)
    {
      del_group_of_widgets_from_gui_list(pHurry_Prod_Dlg->pBeginWidgetList,
			      		 pHurry_Prod_Dlg->pEndWidgetList);

      FC_FREE(pHurry_Prod_Dlg);
    }
    
    free_city_units_lists();
    del_city_menu_dlg(FALSE);
    
    popdown_window_group_dialog(pCityDlg->pBeginCityWidgetList,
				       pCityDlg->pEndCityWidgetList);
    FC_FREE(pCityDlg);
  }
}

/**************************************************************************
  Main Citu Dlg. window callback.
  Here was implemented change specialist ( Elvis, Taxman, Scientist ) code. 
**************************************************************************/
static int city_dlg_callback(struct widget *pWindow)
{  
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (!cma_is_city_under_agent(pCityDlg->pCity, NULL)
       && city_owner(pCityDlg->pCity) == game.player_ptr) {
         
      /* check elvis area */
      if (pCityDlg->specs[0]
         && is_in_rect_area(Main.event.motion.x, Main.event.motion.y,
                                          pCityDlg->specs_area[0])) {
        city_change_specialist(pCityDlg->pCity, SP_ELVIS, SP_TAXMAN);
        return -1;
      }
  
      /* check TAXMANs area */
      if (pCityDlg->specs[1]
         && is_in_rect_area(Main.event.motion.x, Main.event.motion.y,
                                          pCityDlg->specs_area[1])) {
        city_change_specialist(pCityDlg->pCity, SP_TAXMAN, SP_SCIENTIST);
        return -1;
      }
  
      /* check SCIENTISTs area */
      if (pCityDlg->specs[2]
         && is_in_rect_area(Main.event.motion.x, Main.event.motion.y,
                                          pCityDlg->specs_area[2])) {
        city_change_specialist(pCityDlg->pCity, SP_SCIENTIST, SP_ELVIS);
        return -1;
      }
      
    }
    
    if(!pCityDlg->lock &&
          sellect_window_group_dialog(pCityDlg->pBeginCityWidgetList, pWindow)) {
      widget_flush(pWindow);
    }      
  }
  return -1;
}

/* ===================================================================== */
/* ========================== Units Orders Menu ======================== */
/* ===================================================================== */

/**************************************************************************
  Popdown unit city orders menu.
**************************************************************************/
static int cancel_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_window_group_dialog(pCityDlg->pBeginCityMenuWidgetList,
                                pCityDlg->pEndCityMenuWidgetList);
    pCityDlg->pEndCityMenuWidgetList = NULL;
    
    /* enable city dlg */
    enable_city_dlg_widgets();
    flush_dirty();
  }
  return -1;
}

/**************************************************************************
  activate unit and del unit order dlg. widget group.
  update screen is unused becouse common code call here 
  redraw_city_dialog(pCityDlg->pCity);
**************************************************************************/
static int activate_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
  
    del_city_menu_dlg(TRUE);
    if(pUnit) {
      set_unit_focus(pUnit);
    }
  }
  return -1;
}

/**************************************************************************
  activate unit and popdow city dlg. + center on unit.
**************************************************************************/
static int activate_and_exit_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
  
    if(pUnit) {
      
      popdown_window_group_dialog(pCityDlg->pBeginCityMenuWidgetList,
                                  pCityDlg->pEndCityMenuWidgetList);
      pCityDlg->pEndCityMenuWidgetList = NULL;
      
      popdown_city_dialog(pCityDlg->pCity);
      
      center_tile_mapcanvas(pUnit->tile);
      set_unit_focus(pUnit);
    }
  }
  return -1;
}

/**************************************************************************
  sentry unit and del unit order dlg. widget group.
  update screen is unused becouse common code call here 
  redraw_city_dialog(pCityDlg->pCity);
**************************************************************************/
static int sentry_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
  
    del_city_menu_dlg(TRUE);
    if(pUnit) {
      request_unit_sentry(pUnit);
    }
  }
  return -1;
}

/**************************************************************************
  fortify unit and del unit order dlg. widget group.
  update screen is unused becouse common code call here 
  redraw_city_dialog(pCityDlg->pCity);
**************************************************************************/
static int fortify_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
  
    del_city_menu_dlg(TRUE);
    if(pUnit) {
      request_unit_fortify(pUnit);
    }
  }
  return -1;
}

/**************************************************************************
  disband unit and del unit order dlg. widget group.
  update screen is unused becouse common code call here 
  redraw_city_dialog(pCityDlg->pCity);
**************************************************************************/
static int disband_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
  
    free_city_units_lists();
    del_city_menu_dlg(TRUE);
  
    /* ugly hack becouse this free unit widget list*/
    /* FIX ME: add remove from list support */
    pCityDlg->page = INFO_PAGE;
  
    if(pUnit) {
      request_unit_disband(pUnit);
    }
  }
  return -1;
}

/**************************************************************************
  homecity unit and del unit order dlg. widget group.
  update screen is unused becouse common code call here 
  redraw_city_dialog(pCityDlg->pCity);
**************************************************************************/
static int homecity_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
  
    del_city_menu_dlg(TRUE);
    if(pUnit) {
      request_unit_change_homecity(pUnit);
    }
  }
  return -1;
}

/**************************************************************************
  upgrade unit and del unit order dlg. widget group.
  update screen is unused becouse common code call here 
  redraw_city_dialog(pCityDlg->pCity);
**************************************************************************/
static int upgrade_units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct unit *pUnit = pButton->data.unit;
      
    popdown_window_group_dialog(pCityDlg->pBeginCityMenuWidgetList,
                                pCityDlg->pEndCityMenuWidgetList);
    pCityDlg->pEndCityMenuWidgetList = NULL;
    popup_unit_upgrade_dlg(pUnit, TRUE);
  }
  return -1;
}

/**************************************************************************
  Main unit order dlg. callback.
**************************************************************************/
static int units_orders_dlg_callback(struct widget *pButton)
{
  return -1;
}

/**************************************************************************
  popup units orders menu.
**************************************************************************/
static int units_orders_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    SDL_String16 *pStr;
    char cBuf[80];
    struct widget *pBuf, *pWindow = pCityDlg->pEndCityWidgetList;
    struct unit *pUnit;
    struct unit_type *pUType;
    Uint16 ww, i, hh;
  
    pUnit = player_find_unit_by_id(game.player_ptr, MAX_ID - pButton->ID);
    
    if(!pUnit || !can_client_issue_orders()) {
      return -1;
    }
    
    if(Main.event.button.button == SDL_BUTTON_RIGHT) {
      popdown_city_dialog(pCityDlg->pCity);
      center_tile_mapcanvas(pUnit->tile);
      set_unit_focus(pUnit);
      return -1;
    }
      
    /* Disable city dlg */
    unsellect_widget_action();
    disable_city_dlg_widgets();
  
    ww = 0;
    hh = 0;
    i = 0;
    pUType = pUnit->type;
  
    /* window */
    my_snprintf(cBuf, sizeof(cBuf), "%s:", _("Unit Commands"));
    pStr = create_str16_from_char(cBuf, adj_font(12));
    pStr->style |= TTF_STYLE_BOLD;
    pWindow = create_window(NULL, pStr, 1, 1, 0);
    
    widget_set_position(pWindow,
                        pButton->size.x + adj_size(2),
                        pButton->size.y + WINDOW_TITLE_HEIGHT + adj_size(2));
    
    ww = MAX(ww, pWindow->size.w);
    pWindow->action = units_orders_dlg_callback;
    set_wstate(pWindow, FC_WS_NORMAL);
    add_to_gui_list(ID_REVOLUTION_DLG_WINDOW, pWindow);
    pCityDlg->pEndCityMenuWidgetList = pWindow;
    
    /* unit description */
    my_snprintf(cBuf, sizeof(cBuf), "%s", unit_description(pUnit));
    pStr = create_str16_from_char(cBuf, adj_font(12));
    pStr->style |= (TTF_STYLE_BOLD|SF_CENTER);
    pBuf = create_iconlabel(adj_surf(get_unittype_surface(pUnit->type)),
                            pWindow->dst, pStr, WF_FREE_THEME);
    ww = MAX(ww, pBuf->size.w);
    add_to_gui_list(ID_LABEL, pBuf);
    
    /* Activate unit */
    pBuf =
        create_icon_button_from_chars(NULL, pWindow->dst,
                                                _("Activate unit"), adj_font(12), 0);
    i++;
    ww = MAX(ww, pBuf->size.w);
    hh = MAX(hh, pBuf->size.h);
    pBuf->action = activate_units_orders_city_dlg_callback;
    pBuf->data = pButton->data;
    set_wstate(pBuf, FC_WS_NORMAL);
    add_to_gui_list(pButton->ID, pBuf);
    
    /* Activate unit, close dlg. */
    pBuf = create_icon_button_from_chars(NULL, pWindow->dst,
                    _("Activate unit, close dialog"),  adj_font(12), 0);
    i++;
    ww = MAX(ww, pBuf->size.w);
    hh = MAX(hh, pBuf->size.h);
    pBuf->action = activate_and_exit_units_orders_city_dlg_callback;
    pBuf->data = pButton->data;
    set_wstate(pBuf, FC_WS_NORMAL);
    add_to_gui_list(pButton->ID, pBuf);
    /* ----- */
    
    if (pCityDlg->page == ARMY_PAGE) {
      /* Sentry unit */
      pBuf = create_icon_button_from_chars(NULL, pWindow->dst,
                                          _("Sentry unit"), adj_font(12), 0);
      i++;
      ww = MAX(ww, pBuf->size.w);
      hh = MAX(hh, pBuf->size.h);
      pBuf->data = pButton->data;
      pBuf->action = sentry_units_orders_city_dlg_callback;
      if (pUnit->activity != ACTIVITY_SENTRY
          && can_unit_do_activity(pUnit, ACTIVITY_SENTRY)) {
        set_wstate(pBuf, FC_WS_NORMAL);
      }
      add_to_gui_list(pButton->ID, pBuf);
      /* ----- */
      
      /* Fortify unit */
      pBuf = create_icon_button_from_chars(NULL, pWindow->dst,
                                              _("Fortify unit"), adj_font(12), 0);
      i++;
      ww = MAX(ww, pBuf->size.w);
      hh = MAX(hh, pBuf->size.h);
      pBuf->data = pButton->data;
      pBuf->action = fortify_units_orders_city_dlg_callback;
      if (pUnit->activity != ACTIVITY_FORTIFYING
          && can_unit_do_activity(pUnit, ACTIVITY_FORTIFYING)) {
        set_wstate(pBuf, FC_WS_NORMAL);
      }
      add_to_gui_list(pButton->ID, pBuf);
    }
    /* ----- */
    
    /* Disband unit */
    pBuf = create_icon_button_from_chars(NULL, pWindow->dst,
                                    _("Disband unit"), adj_font(12), 0);
    i++;
    ww = MAX(ww, pBuf->size.w);
    hh = MAX(hh, pBuf->size.h);
    pBuf->data = pButton->data;
    pBuf->action = disband_units_orders_city_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    add_to_gui_list(pButton->ID, pBuf);
    /* ----- */
  
    if (pCityDlg->page == ARMY_PAGE) {
      if (pUnit->homecity != pCityDlg->pCity->id) {
        /* Make new Homecity */
        pBuf = create_icon_button_from_chars(NULL, pWindow->dst, 
                                          _("Make new homecity"), adj_font(12), 0);
        i++;
        ww = MAX(ww, pBuf->size.w);
        hh = MAX(hh, pBuf->size.h);
        pBuf->data = pButton->data;
        pBuf->action = homecity_units_orders_city_dlg_callback;
        set_wstate(pBuf, FC_WS_NORMAL);
        add_to_gui_list(pButton->ID, pBuf);
      }
      /* ----- */
      
      if (can_upgrade_unittype(game.player_ptr, pUType)) {
        /* Upgrade unit */
        pBuf = create_icon_button_from_chars(NULL, pWindow->dst,
                                              _("Upgrade unit"), adj_font(12), 0);
        i++;
        ww = MAX(ww, pBuf->size.w);
        hh = MAX(hh, pBuf->size.h);
        pBuf->data = pButton->data;
        pBuf->action = upgrade_units_orders_city_dlg_callback;
        set_wstate(pBuf, FC_WS_NORMAL);
        add_to_gui_list(pButton->ID, pBuf);
      }
    }
  
    /* ----- */
    /* Cancel */
    pBuf = create_icon_button_from_chars(NULL, pWindow->dst,
                                                  _("Cancel"), adj_font(12), 0);
    i++;
    ww = MAX(ww, pBuf->size.w);
    hh = MAX(hh, pBuf->size.h);
    pBuf->key = SDLK_ESCAPE;
    pBuf->action = cancel_units_orders_city_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    add_to_gui_list(pButton->ID, pBuf);
    pCityDlg->pBeginCityMenuWidgetList = pBuf;
  
    /* ================================================== */
    unsellect_widget_action();
    /* ================================================== */
  
    ww += adj_size(10) + pTheme->FR_Left->w + pTheme->FR_Right->w;
    hh += adj_size(4);
  
    /* create window background */
    resize_window(pWindow, NULL,
                  get_game_colorRGB(COLOR_THEME_BACKGROUND), ww,
                  pTheme->FR_Bottom->h + WINDOW_TITLE_HEIGHT + pWindow->prev->size.h +
                  (i * hh) + pTheme->FR_Bottom->h + adj_size(5)); 
    
    /* label */
    pBuf = pWindow->prev;
    pBuf->size.w = ww - pTheme->FR_Left->w - pTheme->FR_Right->w;
    pBuf->size.x = pWindow->size.x + pTheme->FR_Left->w;
    pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(2);
    pBuf = pBuf->prev;
  
    /* first button */
    pBuf->size.w = ww - pTheme->FR_Left->w - pTheme->FR_Right->w;
    pBuf->size.h = hh;
    pBuf->size.x = pWindow->size.x + pTheme->FR_Left->w;
    pBuf->size.y = pBuf->next->size.y + pBuf->next->size.h + adj_size(5);
    pBuf = pBuf->prev;
  
    while (pBuf) {
      pBuf->size.w = ww - pTheme->FR_Left->w - pTheme->FR_Right->w;
      pBuf->size.h = hh;
      pBuf->size.x = pBuf->next->size.x;
      pBuf->size.y = pBuf->next->size.y + pBuf->next->size.h;
      if (pBuf == pCityDlg->pBeginCityMenuWidgetList) {
        break;
      }
      pBuf = pBuf->prev;
    }
  
    /* ================================================== */
    /* redraw */
    redraw_group(pCityDlg->pBeginCityMenuWidgetList, pWindow, 0);
    widget_flush(pWindow);
  }
  return -1;
}

/* ======================================================================= */
/* ======================= City Dlg. Panels ============================== */
/* ======================================================================= */

/**************************************************************************
  create unit icon with support icons.
**************************************************************************/
static SDL_Surface *create_unit_surface(struct unit *pUnit, bool support)
{
  int i, step;
  SDL_Rect dest;
/*  SDL_Surface *pSurf =
    SDL_DisplayFormatAlpha(get_unittype_surface(pUnit->type)); */

  SDL_Surface *pSurf = create_surf_alpha(tileset_full_tile_width(tileset),
                        tileset_full_tile_height(tileset), SDL_SWSURFACE);
  
  struct canvas *destcanvas = canvas_create(tileset_full_tile_width(tileset),
                                             tileset_full_tile_height(tileset));  
  SDL_SetColorKey(destcanvas->surf, SDL_SRCCOLORKEY, 0);
  
  put_unit(pUnit, destcanvas, 0, 0);
  
  alphablit(destcanvas->surf, NULL, pSurf, NULL);

  canvas_free(destcanvas);

  if (pSurf->w > 59) {
    float zoom = 59.0 / pSurf->w;
    SDL_Surface *pZoomed = ZoomSurface(pSurf, zoom, zoom, 1);
    FREESURFACE(pSurf);
    pSurf = pZoomed;
  }
  
  if (support) {
    i = pUnit->upkeep[O_SHIELD] + pUnit->upkeep[O_FOOD] +
	pUnit->upkeep[O_GOLD] + pUnit->unhappiness;

    if (i * pIcons->pFood->w > pSurf->w / 2) {
      step = (pSurf->w / 2 - pIcons->pFood->w) / (i - 1);
    } else {
      step = pIcons->pFood->w;
    }

    dest.y = pSurf->h - pIcons->pFood->h - 2;
    dest.x = pSurf->w / 8;

    for (i = 0; i < pUnit->upkeep[O_SHIELD]; i++) {
      alphablit(pIcons->pShield, NULL, pSurf, &dest);
      dest.x += step;
    }

    for (i = 0; i < pUnit->upkeep[O_FOOD]; i++) {
      alphablit(pIcons->pFood, NULL, pSurf, &dest);
      dest.x += step;
    }

    for (i = 0; i < pUnit->upkeep[O_GOLD]; i++) {
      alphablit(pIcons->pCoin, NULL, pSurf, &dest);
      dest.x += step;
    }

    for (i = 0; i < pUnit->unhappiness; i++) {
      alphablit(pIcons->pFace, NULL, pSurf, &dest);
      dest.x += step;
    }

  }

  return pSurf;
}

/**************************************************************************
  create present/supported units widget list
  207 pixels is panel width in city dlg.
  220 - max y position pixel position belong to panel area.
**************************************************************************/
static void create_present_supported_units_widget_list(struct unit_list *pList)
{
  int i;
  struct widget *pBuf = NULL;
  struct widget *pEnd = NULL;
  struct widget *pWindow = pCityDlg->pEndCityWidgetList;
  struct city *pHome_City;
  struct unit_type *pUType;
  SDL_Surface *pSurf;
  SDL_String16 *pStr;
  char cBuf[256];
  
  i = 0;

  unit_list_iterate(pList, pUnit) {
        
    pUType = pUnit->type;
    pHome_City = find_city_by_id(pUnit->homecity);
    my_snprintf(cBuf, sizeof(cBuf), "%s (%d,%d,%d)%s\n%s\n(%d/%d)\n%s",
		pUType->name, pUType->attack_strength,
		pUType->defense_strength, pUType->move_rate / SINGLE_MOVE,
                (pUnit->veteran ? _("\nveteran") : ""),
                unit_activity_text(pUnit),
		pUnit->hp, pUType->hp,
		pHome_City ? pHome_City->name : _("None"));
    
    if (pCityDlg->page == SUPPORTED_UNITS_PAGE) {
      int pCity_near_dist;
      struct city *pNear_City = get_nearest_city(pUnit, &pCity_near_dist);

      sz_strlcat(cBuf, "\n");
      sz_strlcat(cBuf, get_nearest_city_text(pNear_City, pCity_near_dist));
      pSurf = adj_surf(create_unit_surface(pUnit, 1));
    } else {
      pSurf = adj_surf(create_unit_surface(pUnit, 0));
    }
        
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= SF_CENTER;
    
    pBuf = create_icon2(pSurf, pWindow->dst,
	(WF_FREE_THEME | WF_RESTORE_BACKGROUND | WF_WIDGET_HAS_INFO_LABEL));
	
    pBuf->string16 = pStr;
    pBuf->data.unit = pUnit;
    add_to_gui_list(MAX_ID - pUnit->id, pBuf);

    if (!pEnd) {
      pEnd = pBuf;
    }
    
    if (++i > NUM_UNITS_SHOWN * NUM_UNITS_SHOWN) {
      set_wflag(pBuf, WF_HIDDEN);
    }
  
    if (pCityDlg->pCity->owner == game.player_ptr) {    
      set_wstate(pBuf, FC_WS_NORMAL);
    }
    
    pBuf->action = units_orders_city_dlg_callback;

  } unit_list_iterate_end;
  
  pCityDlg->pPanel = fc_calloc(1, sizeof(struct ADVANCED_DLG));
  pCityDlg->pPanel->pEndWidgetList = pEnd;
  pCityDlg->pPanel->pEndActiveWidgetList = pEnd;
  pCityDlg->pPanel->pBeginWidgetList = pBuf;
  pCityDlg->pPanel->pBeginActiveWidgetList = pBuf;
  pCityDlg->pPanel->pActiveWidgetList = pEnd;
  
  setup_vertical_widgets_position(NUM_UNITS_SHOWN,
	pWindow->size.x + adj_size(7),
	pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(48),
	  0, 0, pCityDlg->pPanel->pBeginActiveWidgetList,
			  pCityDlg->pPanel->pEndActiveWidgetList);
  
  if (i > NUM_UNITS_SHOWN * NUM_UNITS_SHOWN) {
    
/* FIXME: this can probably be removed */
#if 0
    pCityDlg->pPanel->pScroll = fc_calloc(1, sizeof(struct ScrollBar));
    pCityDlg->pPanel->pScroll->active = NUM_UNITS_SHOWN;
    pCityDlg->pPanel->pScroll->step = NUM_UNITS_SHOWN;
    pCityDlg->pPanel->pScroll->count = i;
#endif
    
    create_vertical_scrollbar(pCityDlg->pPanel,
	NUM_UNITS_SHOWN, NUM_UNITS_SHOWN, TRUE, TRUE);
    
    setup_vertical_scrollbar_area(pCityDlg->pPanel->pScroll,
		pWindow->size.x + adj_size(212),
                pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(50),
                150, TRUE);
  }
    
}

/**************************************************************************
  free city present/supported units panel list.
**************************************************************************/
void free_city_units_lists(void)
{
  if (pCityDlg && pCityDlg->pPanel) {
    del_group_of_widgets_from_gui_list(pCityDlg->pPanel->pBeginWidgetList,
					 pCityDlg->pPanel->pEndWidgetList);
    FC_FREE(pCityDlg->pPanel->pScroll);
    FC_FREE(pCityDlg->pPanel);
  }
}

/**************************************************************************
  change to present units panel.
**************************************************************************/
static int army_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (pCityDlg->page != ARMY_PAGE) {
      free_city_units_lists();
      pCityDlg->page = ARMY_PAGE;
      redraw_city_dialog(pCityDlg->pCity);
      flush_dirty();
    } else {
      widget_redraw(pButton);
      widget_flush(pButton);
    }
  }
  return -1;
}

/**************************************************************************
  change to supported units panel.
**************************************************************************/
static int supported_unit_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (pCityDlg->page != SUPPORTED_UNITS_PAGE) {
      free_city_units_lists();
      pCityDlg->page = SUPPORTED_UNITS_PAGE;
      redraw_city_dialog(pCityDlg->pCity);
      flush_dirty();
    } else {
      widget_redraw(pButton);
      widget_flush(pButton);
    }
  }
  return -1;
}

/* ---------------------- */

/**************************************************************************
  change to info panel.
**************************************************************************/
static int info_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (pCityDlg->page != INFO_PAGE) {
      free_city_units_lists();
      pCityDlg->page = INFO_PAGE;
      redraw_city_dialog(pCityDlg->pCity);
      flush_dirty();
    } else {
      widget_redraw(pButton);
      widget_flush(pButton);
    }
  }
  return -1;
}

/* ---------------------- */
/**************************************************************************
  change to happines panel.
**************************************************************************/
static int happy_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (pCityDlg->page != HAPPINESS_PAGE) {
      free_city_units_lists();
      pCityDlg->page = HAPPINESS_PAGE;
      redraw_city_dialog(pCityDlg->pCity);
      flush_dirty();
    } else {
      widget_redraw(pButton);
      widget_flush(pButton);
    }
  }
  return -1;
}

/**************************************************************************
  city option callback
**************************************************************************/
static int misc_panel_city_dlg_callback(struct widget *pWidget)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
/*  int new = pCityDlg->pCity->city_options & 0xff; */
    bv_city_options new_options = pCityDlg->pCity->city_options;
  
    switch (MAX_ID - pWidget->ID) {
    case 0x10:
      if (BV_ISSET(new_options, CITYO_DISBAND))
        BV_CLR(new_options, CITYO_DISBAND);
      else
        BV_SET(new_options, CITYO_DISBAND);
      break;
    case 0x20:
      if (BV_ISSET(new_options, CITYO_NEW_EINSTEIN))
        BV_CLR(new_options, CITYO_NEW_EINSTEIN);
      else
        BV_SET(new_options, CITYO_NEW_EINSTEIN);
   
      if (BV_ISSET(new_options, CITYO_NEW_TAXMAN))
        BV_CLR(new_options, CITYO_NEW_TAXMAN);
      else
        BV_SET(new_options, CITYO_NEW_TAXMAN);
    
      pWidget->gfx = get_tax_surface(O_GOLD);
      pWidget->ID = MAX_ID - 0x40;
      widget_redraw(pWidget);
      widget_flush(pWidget);
      break;
    case 0x40:
      BV_CLR(new_options, CITYO_NEW_EINSTEIN);
      BV_CLR(new_options, CITYO_NEW_TAXMAN);
      pWidget->gfx = get_tax_surface(O_LUXURY);
      pWidget->ID = MAX_ID - 0x60;
      widget_redraw(pWidget);
      widget_flush(pWidget);
      break;
    case 0x60:
      if (BV_ISSET(new_options, CITYO_NEW_EINSTEIN))
        BV_CLR(new_options, CITYO_NEW_EINSTEIN);
      else
        BV_SET(new_options, CITYO_NEW_EINSTEIN);
      pWidget->gfx = get_tax_surface(O_SCIENCE);
      pWidget->ID = MAX_ID - 0x20;
      widget_redraw(pWidget);
      widget_flush(pWidget);
      break;
    }
  
    dsend_packet_city_options_req(&aconnection, pCityDlg->pCity->id, new_options);
  }
  return -1;
}

/**************************************************************************
  ...
**************************************************************************/
static void create_city_options_widget_list(struct city *pCity)
{
  struct widget *pBuf, *pWindow = pCityDlg->pEndCityWidgetList;
  SDL_Surface *pSurf;
  SDL_String16 *pStr;
  char cBuf[80];

  my_snprintf(cBuf, sizeof(cBuf),
	      _("Disband if build\nsettler at size 1"));
  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->style |= TTF_STYLE_BOLD;
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CHECKBOX_LABEL_TEXT);
  
  pBuf =
      create_textcheckbox(pWindow->dst, BV_ISSET(pCity->city_options, CITYO_DISBAND), pStr,
			  WF_RESTORE_BACKGROUND);
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->action = misc_panel_city_dlg_callback;
  add_to_gui_list(MAX_ID - 0x10, pBuf);
  pBuf->size.x = pWindow->size.x + adj_size(10);
  pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(48 + 1);

  /* ----- */
  
  pCityDlg->pPanel = fc_calloc(1, sizeof(struct ADVANCED_DLG));
  pCityDlg->pPanel->pEndWidgetList = pBuf;

  /* ----- */
  
  my_snprintf(cBuf, sizeof(cBuf), "%s:", _("New citizens are"));
  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->style |= SF_CENTER;
  change_ptsize16(pStr, adj_font(13));

  if (BV_ISSET(pCity->city_options, CITYO_NEW_EINSTEIN)) {
    pSurf = get_tax_surface(O_SCIENCE);
    pBuf = create_icon_button(pSurf, pWindow->dst, pStr, WF_ICON_CENTER_RIGHT | WF_FREE_THEME);
    add_to_gui_list(MAX_ID - 0x20, pBuf);
  } else {
    if (BV_ISSET(pCity->city_options, CITYO_NEW_TAXMAN)) {
      pSurf = get_tax_surface(O_GOLD);
      pBuf = create_icon_button(pSurf, pWindow->dst,
				      pStr, WF_ICON_CENTER_RIGHT | WF_FREE_THEME);
      add_to_gui_list(MAX_ID - 0x40, pBuf);
    } else {
      pSurf = get_tax_surface(O_LUXURY);
      pBuf = create_icon_button(pSurf, pWindow->dst,
				pStr, WF_ICON_CENTER_RIGHT | WF_FREE_THEME);
      add_to_gui_list(MAX_ID - 0x60, pBuf);
    }
  }

  pBuf->size.w = adj_size(199);
  pBuf->action = misc_panel_city_dlg_callback;
  set_wstate(pBuf, FC_WS_NORMAL);

  pBuf->size.x = pBuf->next->size.x;
  pBuf->size.y = pBuf->next->size.y + pBuf->next->size.h + adj_size(5);
  pCityDlg->pPanel->pBeginWidgetList = pBuf;
}

/**************************************************************************
  change to city options panel.
**************************************************************************/
static int options_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    if (pCityDlg->page != MISC_PAGE) {
      free_city_units_lists();
      pCityDlg->page = MISC_PAGE;
      redraw_city_dialog(pCityDlg->pCity);
      flush_dirty();
    } else {
      widget_redraw(pButton);
      widget_flush(pButton);
    }
  }
  return -1;
}

/* ======================================================================= */

/**************************************************************************
  ...
**************************************************************************/
static int cma_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    disable_city_dlg_widgets();
    popup_city_cma_dialog(pCityDlg->pCity);
  }
  return -1;
}

/**************************************************************************
  Exit city dialog.
**************************************************************************/
static int exit_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_city_dialog(pCityDlg->pCity);
  }
  return -1;
}

/* ======================================================================= */
/* ======================== Buy Production Dlg. ========================== */
/* ======================================================================= */

/**************************************************************************
  popdown buy productions dlg.
**************************************************************************/
static int cancel_buy_prod_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_hurry_production_dialog();
    
    if (pCityDlg)
    {
      /* enable city dlg */
      enable_city_dlg_widgets();
    }
  }  
  return -1;
}

/**************************************************************************
  buy productions.
**************************************************************************/
static int ok_buy_prod_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    city_buy_production(pButton->data.city);
    
    if (pCityDlg)
    {
      popdown_window_group_dialog(pHurry_Prod_Dlg->pBeginWidgetList,
                                  pHurry_Prod_Dlg->pEndWidgetList);
      FC_FREE(pHurry_Prod_Dlg);
      /* enable city dlg */
      enable_city_dlg_widgets();
      set_wstate(pCityDlg->pBuy_Button, FC_WS_DISABLED);
    } else {
      popdown_hurry_production_dialog();
    }
  }    
  return -1;
}

/**************************************************************************
  popup buy productions dlg.
**************************************************************************/
static int buy_prod_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    widget_redraw(pButton);
    widget_flush(pButton);
    disable_city_dlg_widgets();
    popup_hurry_production_dialog(pCityDlg->pCity, pButton->dst->surface);
  }
  return -1;
}

/**************************************************************************
  popup buy productions dlg.
**************************************************************************/
static void popdown_hurry_production_dialog(void)
{
  if (pHurry_Prod_Dlg) {
    popdown_window_group_dialog(pHurry_Prod_Dlg->pBeginWidgetList,
			      pHurry_Prod_Dlg->pEndWidgetList);
    FC_FREE(pHurry_Prod_Dlg);
    flush_dirty();
  }
}

/**************************************************************************
  main hurry productions dlg. callback
**************************************************************************/
static int hurry_production_window_callback(struct widget *pWindow)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    move_window_group(pHurry_Prod_Dlg->pBeginWidgetList, pWindow);
  }
  return -1;
}

/**************************************************************************
  popup buy productions dlg.
**************************************************************************/
void popup_hurry_production_dialog(struct city *pCity, SDL_Surface *pDest)
{

  int value, hh, ww = 0;
  const char *name;
  char cBuf[512];
  struct widget *pBuf = NULL, *pWindow;
  SDL_String16 *pStr;
  SDL_Surface *pText;
  SDL_Rect dst;
  int window_x = 0, window_y = 0;
  
  if (pHurry_Prod_Dlg) {
    return;
  }
  
  pHurry_Prod_Dlg = fc_calloc(1, sizeof(struct SMALL_DLG));
  
  if (pCity->production.is_unit) {
    name = get_unit_type(pCity->production.value)->name;
  } else {
    name = get_impr_name_ex(pCity, pCity->production.value);
  }

  value = city_buy_cost(pCity);
  if(!pCity->did_buy) {
    if (game.player_ptr->economic.gold >= value) {
      my_snprintf(cBuf, sizeof(cBuf),
		_("Buy %s for %d gold?\n"
		  "Treasury contains %d gold."),
		name, value, game.player_ptr->economic.gold);
    } else {
      my_snprintf(cBuf, sizeof(cBuf),
		_("%s costs %d gold.\n"
		  "Treasury contains %d gold."),
		name, value, game.player_ptr->economic.gold);
    }
  } else {
    my_snprintf(cBuf, sizeof(cBuf),
		_("Sorry, You have already bought here in this turn"));
  }

  ww = pTheme->FR_Left->w;
  hh = WINDOW_TITLE_HEIGHT + 2;
  
  pStr = create_str16_from_char(_("Buy It?"), adj_font(12));
  pStr->style |= TTF_STYLE_BOLD;
  pWindow = create_window(NULL, pStr, 1, 1, 0);
  pWindow->action = hurry_production_window_callback;
  set_wstate(pWindow, FC_WS_NORMAL);
  ww += pWindow->size.w;
  pHurry_Prod_Dlg->pEndWidgetList = pWindow;
  add_to_gui_list(ID_WINDOW, pWindow);
  /* ============================================================= */
  
  /* label */
  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->style |= (TTF_STYLE_BOLD|SF_CENTER);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_BUY);
  
  pText = create_text_surf_from_str16(pStr);
  FREESTRING16(pStr);
  ww = MAX(ww , pText->w);
  hh += pText->h + 5;

  pBuf = create_themeicon_button_from_chars(pTheme->CANCEL_Icon,
			    pWindow->dst, _("No"), adj_font(12), 0);

  pBuf->action = cancel_buy_prod_city_dlg_callback;
  set_wstate(pBuf, FC_WS_NORMAL);
  pBuf->key = SDLK_ESCAPE;
  hh += pBuf->size.h;

  add_to_gui_list(ID_BUTTON, pBuf);

  if (!pCity->did_buy && game.player_ptr->economic.gold >= value) {
    pBuf = create_themeicon_button_from_chars(pTheme->OK_Icon, pWindow->dst,
					      _("Yes"), adj_font(12), 0);

    pBuf->action = ok_buy_prod_city_dlg_callback;
    set_wstate(pBuf, FC_WS_NORMAL);
    pBuf->data.city = pCity;
    pBuf->key = SDLK_RETURN;
    add_to_gui_list(ID_BUTTON, pBuf);
    pBuf->size.w = MAX(pBuf->next->size.w, pBuf->size.w);
    pBuf->next->size.w = pBuf->size.w;
    ww = MAX(ww , 2 * pBuf->size.w + adj_size(20));
  }
  
  pHurry_Prod_Dlg->pBeginWidgetList = pBuf;
  
  /* setup window size and start position */
  ww += adj_size(10) + pTheme->FR_Right->w;
  hh += adj_size(5) + pTheme->FR_Bottom->h;

  pBuf = pWindow->prev;
  
  if (city_dialog_is_open(pCity))
  {
    window_x = pCityDlg->pBuy_Button->size.x;
    window_y = pCityDlg->pBuy_Button->size.y - hh;
  } else {
    if(is_city_report_open()) {
      assert(pSellected_Widget != NULL);
      if (pSellected_Widget->size.x + tileset_tile_width(tileset) + ww > Main.screen->w)
      {
        window_x = pSellected_Widget->size.x - ww;
      } else {
        window_x = pSellected_Widget->size.x + tileset_tile_width(tileset);
      }
    
      window_y = pSellected_Widget->size.y + (pSellected_Widget->size.h - hh) / 2;
      if (window_y + hh > Main.screen->h)
      {
	window_y = Main.screen->h - hh - 1;
      } else {
        if (window_y < 0) {
	  window_y = 0;
	}
      }
    } else {
      put_window_near_map_tile(pWindow, ww, hh, pCity->tile);
    }
    
  }

  widget_set_position(pWindow, window_x, window_y);
  
  resize_window(pWindow, NULL, get_game_colorRGB(COLOR_THEME_BACKGROUND), ww, hh);

  /* setup rest of widgets */
  /* label */
  dst.x = pTheme->FR_Left->w + (pWindow->size.w - pTheme->FR_Left->w - pTheme->FR_Right->w - pText->w) / 2;
  dst.y = WINDOW_TITLE_HEIGHT + 2;
  alphablit(pText, NULL, pWindow->theme, &dst);
  dst.y += pText->h + 5;
  FREESURFACE(pText);
  
  /* no */
  pBuf = pWindow->prev;
  pBuf->size.y = pWindow->size.y + dst.y;
  
  if (!pCity->did_buy && game.player_ptr->economic.gold >= value) {
    /* yes */
    pBuf = pBuf->prev;
    pBuf->size.x = pWindow->size.x +
      (pWindow->size.w - pTheme->FR_Left->w - pTheme->FR_Right->w - (2 * pBuf->size.w + adj_size(20))) / 2;
    pBuf->size.y = pWindow->size.y + dst.y;
    
    /* no */
    pBuf->next->size.x = pBuf->size.x + pBuf->size.w + adj_size(20);
  } else {
    /* no */
    pBuf->size.x = pWindow->size.x +
      pWindow->size.w - pTheme->FR_Right->w - pBuf->size.w - adj_size(10);
  }
  /* ================================================== */
  /* redraw */
  redraw_group(pHurry_Prod_Dlg->pBeginWidgetList, pWindow, 0);
  widget_mark_dirty(pWindow);
  flush_dirty();
}

/* =======================================================================*/
/* ========================== CHANGE PRODUCTION ==========================*/
/* =======================================================================*/

/**************************************************************************
  Popup the change production dialog.
**************************************************************************/
static int change_prod_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    widget_redraw(pButton);
    widget_flush(pButton);
  
    disable_city_dlg_widgets();
    popup_worklist_editor(pCityDlg->pCity, &pCityDlg->pCity->worklist);
  }
  return -1;
}

/* =======================================================================*/
/* =========================== SELL IMPROVMENTS ==========================*/
/* =======================================================================*/

/**************************************************************************
  Popdown Sell Imprv. Dlg. and exit without sell.
**************************************************************************/
static int sell_imprvm_dlg_cancel_callback(struct widget *pCancel_Button)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    popdown_window_group_dialog(pCityDlg->pBeginCityMenuWidgetList,
                                pCityDlg->pEndCityMenuWidgetList);
    pCityDlg->pEndCityMenuWidgetList = NULL;
    enable_city_dlg_widgets();
    redraw_city_dialog(pCityDlg->pCity);
    flush_dirty();
  }
  return -1;
}

/**************************************************************************
  Popdown Sell Imprv. Dlg. and exit with sell.
**************************************************************************/
static int sell_imprvm_dlg_ok_callback(struct widget *pOK_Button)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct widget *pTmp = (struct widget *)pOK_Button->data.ptr;
  
    city_sell_improvement(pCityDlg->pCity, MAX_ID - 3000 - pTmp->ID);
    
    /* popdown, we don't redraw and flush becouse this is make by redraw city dlg.
       when response from server come */
    del_group_of_widgets_from_gui_list(pCityDlg->pBeginCityMenuWidgetList,
                                       pCityDlg->pEndCityMenuWidgetList);
  
    pCityDlg->pEndCityMenuWidgetList = NULL;
  
    /* del imprv from widget list */
    del_widget_from_vertical_scroll_widget_list(pCityDlg->pImprv, pTmp);
    
    enable_city_dlg_widgets();
  
    if (pCityDlg->pImprv->pEndWidgetList) {
      set_group_state(pCityDlg->pImprv->pBeginActiveWidgetList,
                      pCityDlg->pImprv->pEndActiveWidgetList, FC_WS_DISABLED);
    }
  
    redraw_city_dialog(pCityDlg->pCity);
    flush_dirty();
  }  
  return -1;
}

/**************************************************************************
  Popup Sell Imprvm. Dlg.
**************************************************************************/
static int sell_imprvm_dlg_callback(struct widget *pImpr)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    struct SDL_String16 *pStr = NULL;
    struct widget *pLabel = pImpr;
    struct widget *pWindow = NULL;
    struct widget *pCancel_Button = NULL;
    struct widget *pOK_Button = NULL;
    char cBuf[80];
    int ww;
    int id;
  
    unsellect_widget_action();
    disable_city_dlg_widgets();
  
    pStr = create_str16_from_char(_("Sell It?"), adj_font(12));
    pStr->style |= TTF_STYLE_BOLD;
    pWindow = create_window(NULL, pStr, 1, 1, 0);
    /*pWindow->action = move_sell_imprvm_dlg_callback; */
    /*set_wstate( pWindow, FC_WS_NORMAL ); */
    add_to_gui_list(ID_WINDOW, pWindow);
    pCityDlg->pEndCityMenuWidgetList = pWindow;  
  
    /* create text label */
    id = MAX_ID - 3000 - pImpr->ID;
  
    my_snprintf(cBuf, sizeof(cBuf), _("Sell %s for %d gold?"),
                get_impr_name_ex(pCityDlg->pCity, id),
                impr_sell_gold(id));
    pStr = create_str16_from_char(cBuf, adj_font(10));
    pStr->style |= (TTF_STYLE_BOLD|SF_CENTER);
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_SELL);
    pLabel = create_iconlabel(NULL, pWindow->dst, pStr, 0);
    add_to_gui_list(ID_LABEL, pLabel);
  
    /* create cancel button */
    pCancel_Button =
        create_themeicon_button_from_chars(pTheme->Small_CANCEL_Icon,
                          pWindow->dst, _("Cancel"), adj_font(10), 0);
    pCancel_Button->action = sell_imprvm_dlg_cancel_callback;
    pCancel_Button->key = SDLK_ESCAPE;  
    set_wstate(pCancel_Button, FC_WS_NORMAL);
    add_to_gui_list(ID_BUTTON, pCancel_Button);
    
    /* create ok button */
    pOK_Button = create_themeicon_button_from_chars(
                  pTheme->Small_OK_Icon, pImpr->dst, _("Sell"), adj_font(10),  0);
    pOK_Button->data.ptr = (void *)pLabel;
    pOK_Button->size.w = pCancel_Button->size.w;
    pOK_Button->action = sell_imprvm_dlg_ok_callback;
    pOK_Button->key = SDLK_RETURN;
    set_wstate(pOK_Button, FC_WS_NORMAL);    
    add_to_gui_list(ID_BUTTON, pOK_Button);
  
    pCityDlg->pBeginCityMenuWidgetList = pOK_Button;
    
    /* correct sizes */
    if ((pOK_Button->size.w + pCancel_Button->size.w + 30) >
        pLabel->size.w + 20) {
      ww = pOK_Button->size.w + pCancel_Button->size.w + 30;
    } else {
      ww = pLabel->size.w + 20;
    }
  
    pWindow->size.w = ww;
    pWindow->size.h = pOK_Button->size.h + pLabel->size.h + WINDOW_TITLE_HEIGHT + 25;
  
    /* set start positions */
    widget_set_position(pWindow,
                        (Main.screen->w - pWindow->size.w) / 2,
                        (Main.screen->h - pWindow->size.h) / 2 + 10);
  
    pOK_Button->size.x = pWindow->size.x + 10;
    pOK_Button->size.y = pWindow->size.y + pWindow->size.h -
        pOK_Button->size.h - 10;
  
    pCancel_Button->size.y = pOK_Button->size.y;
    pCancel_Button->size.x = pWindow->size.x + pWindow->size.w -
        pCancel_Button->size.w - 10;
  
    pLabel->size.x = pWindow->size.x;
    pLabel->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + 5;
    pLabel->size.w = pWindow->size.w;
    
    /* create window background */
    resize_window(pWindow, NULL,
                  get_game_colorRGB(COLOR_THEME_BACKGROUND),
                  pWindow->size.w, pWindow->size.h);
  
  #if 0
    /* redraw */
    redraw_group(pCityDlg->pBeginCityMenuWidgetList,
                 pCityDlg->pEndCityMenuWidgetList, 0);
  
    widget_flush(pWindow);
  #endif
  
    redraw_city_dialog(pCityDlg->pCity);
    flush_dirty();
  }
  return -1;
}
/* ====================================================================== */

/**************************************************************************
  ...
**************************************************************************/
void enable_city_dlg_widgets(void)
{
  if (pCityDlg) {
    set_group_state(pCityDlg->pBeginCityWidgetList,
		  pCityDlg->pEndCityWidgetList->prev, FC_WS_NORMAL);
  
    if (pCityDlg->pImprv->pEndWidgetList) {
      if (pCityDlg->pImprv->pScroll) {
        set_wstate(pCityDlg->pImprv->pScroll->pScrollBar, FC_WS_NORMAL);	/* vscroll */
        set_wstate(pCityDlg->pImprv->pScroll->pUp_Left_Button, FC_WS_NORMAL); /* up */
        set_wstate(pCityDlg->pImprv->pScroll->pDown_Right_Button, FC_WS_NORMAL); /* down */
      }

      if (pCityDlg->pCity->did_sell) {
        set_group_state(pCityDlg->pImprv->pBeginActiveWidgetList,
		      pCityDlg->pImprv->pEndActiveWidgetList, FC_WS_DISABLED);
      } else {
        struct widget *pTmpWidget = pCityDlg->pImprv->pEndActiveWidgetList;

        while (TRUE) {
	  if (is_wonder(MAX_ID - 3000 - pTmpWidget->ID)) {
	    set_wstate(pTmpWidget, FC_WS_DISABLED);
	  } else {
	    set_wstate(pTmpWidget, FC_WS_NORMAL);
	  }

	  if (pTmpWidget == pCityDlg->pImprv->pBeginActiveWidgetList) {
	    break;
	  }

	  pTmpWidget = pTmpWidget->prev;

        }				/* while */
      }
    }
  
    if (pCityDlg->pCity->did_buy && pCityDlg->pBuy_Button) {
      set_wstate(pCityDlg->pBuy_Button, FC_WS_DISABLED);
    }

    if (pCityDlg->pPanel) {
      set_group_state(pCityDlg->pPanel->pBeginWidgetList,
		    pCityDlg->pPanel->pEndWidgetList, FC_WS_NORMAL);
    }

    if (cma_is_city_under_agent(pCityDlg->pCity, NULL)) {
      set_wstate(pCityDlg->pResource_Map, FC_WS_DISABLED);
    }
  
    pCityDlg->lock = FALSE;
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void disable_city_dlg_widgets(void)
{
  if (pCityDlg->pPanel) {
    set_group_state(pCityDlg->pPanel->pBeginWidgetList,
		    pCityDlg->pPanel->pEndWidgetList, FC_WS_DISABLED);
  }


  if (pCityDlg->pImprv->pEndWidgetList) {
    set_group_state(pCityDlg->pImprv->pBeginWidgetList,
		    pCityDlg->pImprv->pEndWidgetList, FC_WS_DISABLED);
  }

  set_group_state(pCityDlg->pBeginCityWidgetList,
		  pCityDlg->pEndCityWidgetList->prev, FC_WS_DISABLED);
  pCityDlg->lock = TRUE;
}
/* ======================================================================== */

/**************************************************************************
  ...
**************************************************************************/
SDL_Surface * get_scaled_city_map(struct city *pCity)
{
  SDL_Surface *pBuf = create_city_map(pCity);
  
  if ((pBuf->w > adj_size(192)) || (pBuf->h > adj_size(134))) {
    city_map_zoom = ((pBuf->w > pBuf->h) ?
                       (float)adj_size(192) / pBuf->w
                     : (float)adj_size(134) / pBuf->h);
    
    SDL_Surface *pRet = ZoomSurface(pBuf, city_map_zoom, city_map_zoom, 1);
    return pRet;
  } 
   
  return pBuf;
}

/**************************************************************************
  city resource map: event callback
**************************************************************************/
static int resource_map_city_dlg_callback(struct widget *pMap)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    int col, row;
  
    if (canvas_to_city_pos(&col, &row,
                     1/city_map_zoom * (Main.event.motion.x - pMap->size.x),
                     1/city_map_zoom * (Main.event.motion.y - pMap->size.y))) {
  
      city_toggle_worker(pCityDlg->pCity, col, row);
    }
  }
  return -1;
}

/* ====================================================================== */

/************************************************************************
  Helper for switch_city_callback.
*************************************************************************/
static int city_comp_by_turn_founded(const void *a, const void *b)
{
  struct city *pCity1 = *((struct city **) a);
  struct city *pCity2 = *((struct city **) b);

  return pCity1->turn_founded - pCity2->turn_founded;
}

/**************************************************************************
  Callback for next/prev city button
**************************************************************************/
static int next_prev_city_dlg_callback(struct widget *pButton)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    int i, dir, non_open_size, size =
        city_list_size(game.player_ptr->cities);
    struct city **array;
  
    assert(size >= 1);
    assert(pCityDlg->pCity->owner == game.player_ptr);
  
    if (size == 1) {
      return -1;
    }
  
    /* dir = 1 will advance to the city, dir = -1 will get previous */
    if (pButton->ID == ID_CITY_DLG_NEXT_BUTTON) {
      dir = 1;
    } else {
      if (pButton->ID == ID_CITY_DLG_PREV_BUTTON) {
        dir = -1;
      } else {
        assert(0);
        dir = 1;
      }
    }
  
    array = fc_calloc(1, size * sizeof(struct city *));
  
    non_open_size = 0;
    for (i = 0; i < size; i++) {
      array[non_open_size++] = city_list_get(game.player_ptr->cities, i);
    }
  
    assert(non_open_size > 0);
  
    if (non_open_size == 1) {
      FC_FREE(array);
      return -1;
    }
  
    qsort(array, non_open_size, sizeof(struct city *),
                                                  city_comp_by_turn_founded);
  
    for (i = 0; i < non_open_size; i++) {
      if (pCityDlg->pCity == array[i]) {
        break;
      }
    }
  
    assert(i < non_open_size);
    pCityDlg->pCity = array[(i + dir + non_open_size) % non_open_size];
    FC_FREE(array);
  
    /* free panel widgets */
    free_city_units_lists();
    /* refresh resource map */
    FREESURFACE(pCityDlg->pResource_Map->theme);
    pCityDlg->pResource_Map->theme = get_scaled_city_map(pCityDlg->pCity);
    rebuild_imprm_list(pCityDlg->pCity);
  
    /* redraw */
    redraw_city_dialog(pCityDlg->pCity);
    flush_dirty();
  }
  return -1;
}

/**************************************************************************
  Rename city name: 
**************************************************************************/
static int new_name_city_dlg_callback(struct widget *pEdit)
{
  if (Main.event.button.button == SDL_BUTTON_LEFT) {
    char *tmp = convert_to_chars(pEdit->string16->text);
  
    if(tmp) {
      if(strcmp(tmp, pCityDlg->pCity->name)) {
        SDL_Client_Flags |= CF_CHANGED_CITY_NAME;
        city_rename(pCityDlg->pCity, tmp);
      }
      
      FC_FREE(tmp);
    } else {
      /* empty input -> restore previous content */
      copy_chars_to_string16(pEdit->string16, pCityDlg->pCity->name);
      widget_redraw(pEdit);
      widget_mark_dirty(pEdit);
      flush_dirty();
    }  
  } 
  return -1;
}

/* ======================================================================= */
/* ======================== Redrawing City Dlg. ========================== */
/* ======================================================================= */

/**************************************************************************
  Refresh (update) the city names for the dialog
**************************************************************************/
static void refresh_city_names(struct city *pCity)
{
  if (pCityDlg->pCity_Name_Edit) {
    char name[MAX_LEN_NAME];
    
    convertcopy_to_chars(name, MAX_LEN_NAME,
			    pCityDlg->pCity_Name_Edit->string16->text);
    if ((strcmp(pCity->name, name) != 0)
      || (SDL_Client_Flags & CF_CHANGED_CITY_NAME)) {
      copy_chars_to_string16(pCityDlg->pCity_Name_Edit->string16, pCity->name);
      rebuild_citydlg_title_str(pCityDlg->pEndCityWidgetList, pCity);
      SDL_Client_Flags &= ~CF_CHANGED_CITY_NAME;
    }
  }
}

/**************************************************************************
  Redraw city option panel
  207 = max panel width
**************************************************************************/
static void redraw_misc_city_dialog(struct widget *pCityWindow,
				    struct city *pCity)
{
  char cBuf[60];
  SDL_String16 *pStr;
  SDL_Surface *pSurf;
  SDL_Rect dest;

  my_snprintf(cBuf, sizeof(cBuf), _("Options panel"));

  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PANEL);
  pStr->style |= TTF_STYLE_BOLD;

  pSurf = create_text_surf_from_str16(pStr);

  dest.x = pCityWindow->size.x + adj_size(5) + (adj_size(207) - pSurf->w) / 2;
  dest.y = pCityWindow->size.y + WINDOW_TITLE_HEIGHT + pTheme->INFO_Icon->h + adj_size(10);

  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

  FREESURFACE(pSurf);
  FREESTRING16(pStr);

  if (!pCityDlg->pPanel) {
    create_city_options_widget_list(pCity);
  }
  redraw_group(pCityDlg->pPanel->pBeginWidgetList,
		 pCityDlg->pPanel->pEndWidgetList, 0);
}

/**************************************************************************
  Redraw supported unit panel
  207 = max panel width
**************************************************************************/
static void redraw_supported_units_city_dialog(struct widget *pCityWindow,
					       struct city *pCity)
{
  char cBuf[60];
  SDL_String16 *pStr;
  SDL_Surface *pSurf;
  SDL_Rect dest;
  struct unit_list *pList;
  int size;

  if (pCityDlg->pCity->owner != game.player_ptr) {
    pList = (pCityDlg->pCity->info_units_supported);
  } else {
    pList = (pCityDlg->pCity->units_supported);
  }

  size = unit_list_size(pList);

  my_snprintf(cBuf, sizeof(cBuf), _("Unit maintenance panel (%d %s)"),
	      size, PL_("unit", "units", size));

  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PANEL);
  pStr->style |= TTF_STYLE_BOLD;

  pSurf = create_text_surf_from_str16(pStr);

  dest.x = pCityWindow->size.x + adj_size(5) + (adj_size(207) - pSurf->w) / 2;
  dest.y = pCityWindow->size.y + WINDOW_TITLE_HEIGHT + pTheme->INFO_Icon->h + adj_size(10);
  
  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

  FREESURFACE(pSurf);
  FREESTRING16(pStr);

  if (pCityDlg->pPanel) {
    if (size) {
      redraw_group(pCityDlg->pPanel->pBeginWidgetList,
		   pCityDlg->pPanel->pEndWidgetList, 0);
    } else {
      del_group_of_widgets_from_gui_list(pCityDlg->pPanel->pBeginWidgetList,
					 pCityDlg->pPanel->pEndWidgetList);
      FC_FREE(pCityDlg->pPanel->pScroll);
      FC_FREE(pCityDlg->pPanel);
    }
  } else {
    if (size) {
      create_present_supported_units_widget_list(pList);
      redraw_group(pCityDlg->pPanel->pBeginWidgetList,
		   pCityDlg->pPanel->pEndWidgetList, 0);
    }
  }
}

/**************************************************************************
  Redraw garrison panel
  207 = max panel width
**************************************************************************/
static void redraw_army_city_dialog(struct widget *pCityWindow,
				    struct city *pCity)
{
  char cBuf[60];
  SDL_String16 *pStr;
  SDL_Surface *pSurf;
  SDL_Rect dest;
  struct unit_list *pList;

  int size;

  if (pCityDlg->pCity->owner != game.player_ptr) {
    pList = pCityDlg->pCity->info_units_present;
  } else {
    pList = pCityDlg->pCity->tile->units;
  }

  size = unit_list_size(pList);

  my_snprintf(cBuf, sizeof(cBuf), _("Garrison Panel (%d %s)"),
	      size, PL_("unit", "units", size));

  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PANEL);
  pStr->style |= TTF_STYLE_BOLD;

  pSurf = create_text_surf_from_str16(pStr);

  dest.x = pCityWindow->size.x + adj_size(5) + (adj_size(207) - pSurf->w) / 2;
  dest.y = pCityWindow->size.y + WINDOW_TITLE_HEIGHT + pTheme->INFO_Icon->h + adj_size(10);
  
  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

  FREESURFACE(pSurf);
  FREESTRING16(pStr);

  if (pCityDlg->pPanel) {
    if (size) {
      redraw_group(pCityDlg->pPanel->pBeginWidgetList,
		   pCityDlg->pPanel->pEndWidgetList, 0);
    } else {
      del_group_of_widgets_from_gui_list(pCityDlg->pPanel->pBeginWidgetList,
					 pCityDlg->pPanel->pEndWidgetList);
      FC_FREE(pCityDlg->pPanel->pScroll);
      FC_FREE(pCityDlg->pPanel);
    }
  } else {
    if (size) {
      create_present_supported_units_widget_list(pList);
      redraw_group(pCityDlg->pPanel->pBeginWidgetList,
		   pCityDlg->pPanel->pEndWidgetList, 0);
    }
  }
}

/**************************************************************************
  Redraw Info panel
  207 = max panel width
**************************************************************************/
static void redraw_info_city_dialog(struct widget *pCityWindow,
				    struct city *pCity)
{
  char cBuf[30];
  struct city *pTradeCity = NULL;
  int step, i, xx;
  SDL_String16 *pStr = NULL;
  SDL_Surface *pSurf = NULL;
  SDL_Rect dest;

  my_snprintf(cBuf, sizeof(cBuf), _("Info Panel"));
  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PANEL);
  pStr->style |= TTF_STYLE_BOLD;

  pSurf = create_text_surf_from_str16(pStr);
  
  dest.x = pCityWindow->size.x + adj_size(5) + (adj_size(207) - pSurf->w) / 2;
  dest.y = pCityWindow->size.y + WINDOW_TITLE_HEIGHT + pTheme->INFO_Icon->h + adj_size(10);
  
  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

  dest.x = pCityWindow->size.x + adj_size(10);
  dest.y += pSurf->h + 1;

  FREESURFACE(pSurf);

  change_ptsize16(pStr, adj_font(11));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_INFOPANEL);
  
  if (pCity->pollution) {
    my_snprintf(cBuf, sizeof(cBuf), _("Pollution: %d"),
		pCity->pollution);

    copy_chars_to_string16(pStr, cBuf);
    
    pSurf = create_text_surf_from_str16(pStr);

    alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

    dest.y += pSurf->h + adj_size(3);

    FREESURFACE(pSurf);

    if (((pIcons->pPollution->w + 1) * pCity->pollution) > adj_size(187)) {
      step = (adj_size(187) - pIcons->pPollution->w) / (pCity->pollution - 1);
    } else {
      step = pIcons->pPollution->w + 1;
    }

    for (i = 0; i < pCity->pollution; i++) {
      alphablit(pIcons->pPollution, NULL, pCityWindow->dst->surface, &dest);
      dest.x += step;
    }

    dest.x = pCityWindow->size.x + adj_size(10);
    dest.y += pIcons->pPollution->h + adj_size(30);

  } else {
    my_snprintf(cBuf, sizeof(cBuf), _("Pollution: none"));

    copy_chars_to_string16(pStr, cBuf);

    pSurf = create_text_surf_from_str16(pStr);

    alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

    dest.y += pSurf->h + adj_size(3);

    FREESURFACE(pSurf);
  }

  my_snprintf(cBuf, sizeof(cBuf), _("Trade routes: "));

  copy_chars_to_string16(pStr, cBuf);

  pSurf = create_text_surf_from_str16(pStr);

  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

  xx = dest.x + pSurf->w;
  dest.y += pSurf->h + adj_size(3);

  FREESURFACE(pSurf);

  step = 0;
  dest.x = pCityWindow->size.x + adj_size(10);

  for (i = 0; i < NUM_TRADEROUTES; i++) {
    if (pCity->trade[i]) {
      step += pCity->trade_value[i];

      if ((pTradeCity = find_city_by_id(pCity->trade[i]))) {
	my_snprintf(cBuf, sizeof(cBuf), "%s: +%d", pTradeCity->name,
		    pCity->trade_value[i]);
      } else {
	my_snprintf(cBuf, sizeof(cBuf), "%s: +%d", _("Unknown"),
		    pCity->trade_value[i]);
      }


      copy_chars_to_string16(pStr, cBuf);

      pSurf = create_text_surf_from_str16(pStr);

      alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

      /* blit trade icon */
      dest.x += pSurf->w + adj_size(3);
      dest.y += adj_size(4);
      alphablit(pIcons->pTrade, NULL, pCityWindow->dst->surface, &dest);
      dest.x = pCityWindow->size.x + adj_size(10);
      dest.y -= adj_size(4);

      dest.y += pSurf->h;

      FREESURFACE(pSurf);
    }
  }

  if (step) {
    my_snprintf(cBuf, sizeof(cBuf), _("Trade: +%d"), step);

    copy_chars_to_string16(pStr, cBuf);
    pSurf = create_text_surf_from_str16(pStr);
    alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

    dest.x += pSurf->w + adj_size(3);
    dest.y += adj_size(4);
    alphablit(pIcons->pTrade, NULL, pCityWindow->dst->surface, &dest);

    FREESURFACE(pSurf);
  } else {
    my_snprintf(cBuf, sizeof(cBuf), _("none"));

    copy_chars_to_string16(pStr, cBuf);

    pSurf = create_text_surf_from_str16(pStr);

    dest.x = xx;
    dest.y -= pSurf->h + adj_size(3);
    alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);

    FREESURFACE(pSurf);
  }


  FREESTRING16(pStr);
}

/**************************************************************************
  Redraw (refresh/update) the happiness info for the dialog
  207 - max panel width
  180 - max citizens icons area width
**************************************************************************/
static void redraw_happyness_city_dialog(const struct widget *pCityWindow,
					 struct city *pCity)
{
  char cBuf[30];
  int step, i, j, count;
  SDL_Surface *pTmp;
  SDL_String16 *pStr = NULL;
  SDL_Surface *pSurf = NULL;
  SDL_Rect dest = {0, 0, 0, 0};
  struct effect_list *sources = effect_list_new();   

  my_snprintf(cBuf, sizeof(cBuf), _("Happiness panel"));

  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PANEL);
  pStr->style |= TTF_STYLE_BOLD;

  pSurf = create_text_surf_from_str16(pStr);

  dest.x = pCityWindow->size.x + adj_size(5) + (adj_size(207) - pSurf->w) / 2;
  dest.y = pCityWindow->size.y + WINDOW_TITLE_HEIGHT + pTheme->INFO_Icon->h + adj_size(10);
  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);
  
  dest.x = pCityWindow->size.x + adj_size(10);
  dest.y += pSurf->h + 1;

  FREESURFACE(pSurf);
  FREESTRING16(pStr);

  count = (pCity->ppl_happy[4] + pCity->ppl_content[4]
	   + pCity->ppl_unhappy[4] + pCity->ppl_angry[4]
	   + pCity->specialists[SP_ELVIS] + pCity->specialists[SP_SCIENTIST]
	   + pCity->specialists[SP_TAXMAN]);

  if (count * pIcons->pMale_Happy->w > adj_size(180)) {
    step = (adj_size(180) - pIcons->pMale_Happy->w) / (count - 1);
  } else {
    step = pIcons->pMale_Happy->w;
  }

  for (j = 0; j < 5; j++) {
    if (j == 0 || pCity->ppl_happy[j - 1] != pCity->ppl_happy[j]
	|| pCity->ppl_content[j - 1] != pCity->ppl_content[j]
	|| pCity->ppl_unhappy[j - 1] != pCity->ppl_unhappy[j]
	|| pCity->ppl_angry[j - 1] != pCity->ppl_angry[j]) {

      if (j != 0) {
	putline(pCityWindow->dst->surface, dest.x, dest.y, dest.x + adj_size(195), dest.y,
          map_rgba(pCityWindow->dst->surface->format, *get_game_colorRGB(COLOR_THEME_CITYDLG_FRAME)));
	dest.y += adj_size(5);
      }

      if (pCity->ppl_happy[j]) {
	pSurf = pIcons->pMale_Happy;
	for (i = 0; i < pCity->ppl_happy[j]; i++) {
	  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	  if (pSurf == pIcons->pMale_Happy) {
	    pSurf = pIcons->pFemale_Happy;
	  } else {
	    pSurf = pIcons->pMale_Happy;
	  }
	}
      }

      if (pCity->ppl_content[j]) {
	pSurf = pIcons->pMale_Content;
	for (i = 0; i < pCity->ppl_content[j]; i++) {
	  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	  if (pSurf == pIcons->pMale_Content) {
	    pSurf = pIcons->pFemale_Content;
	  } else {
	    pSurf = pIcons->pMale_Content;
	  }
	}
      }

      if (pCity->ppl_unhappy[j]) {
	pSurf = pIcons->pMale_Unhappy;
	for (i = 0; i < pCity->ppl_unhappy[j]; i++) {
	  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	  if (pSurf == pIcons->pMale_Unhappy) {
	    pSurf = pIcons->pFemale_Unhappy;
	  } else {
	    pSurf = pIcons->pMale_Unhappy;
	  }
	}
      }

      if (pCity->ppl_angry[j]) {
	pSurf = pIcons->pMale_Angry;
	for (i = 0; i < pCity->ppl_angry[j]; i++) {
	  alphablit(pSurf, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	  if (pSurf == pIcons->pMale_Angry) {
	    pSurf = pIcons->pFemale_Angry;
	  } else {
	    pSurf = pIcons->pMale_Angry;
	  }
	}
      }

      if (pCity->specialists[SP_ELVIS]) {
	for (i = 0; i < pCity->specialists[SP_ELVIS]; i++) {
	  alphablit(pIcons->pSpec_Lux, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	}
      }

      if (pCity->specialists[SP_TAXMAN]) {
	for (i = 0; i < pCity->specialists[SP_TAXMAN]; i++) {
	  alphablit(pIcons->pSpec_Tax, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	}
      }

      if (pCity->specialists[SP_SCIENTIST]) {
	for (i = 0; i < pCity->specialists[SP_SCIENTIST]; i++) {
	  alphablit(pIcons->pSpec_Sci, NULL, pCityWindow->dst->surface, &dest);
	  dest.x += step;
	}
      }

      if (j == 1) { /* luxury effect */
	dest.x =
	    pCityWindow->size.x + adj_size(212) - pIcons->pBIG_Luxury->w - adj_size(2);
	count = dest.y;
	dest.y += (pIcons->pMale_Happy->h -
		   pIcons->pBIG_Luxury->h) / 2;
	alphablit(pIcons->pBIG_Luxury, NULL, pCityWindow->dst->surface, &dest);
	dest.y = count;
      }

      if (j == 2) { /* improvments effects */
	pSurf = NULL;
	count = 0;

        get_city_bonus_effects(sources, pCity, NULL, EFT_MAKE_CONTENT);
        
        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);
          
	  count += (pTmp->h + 1);

          if (!pSurf) {
	    pSurf = pTmp;
	  } else {
            FREESURFACE(pTmp);
          }
             
        } effect_list_iterate_end;

	dest.x = pCityWindow->size.x + adj_size(212) - pSurf->w - adj_size(2);
	i = dest.y;
	dest.y += (pIcons->pMale_Happy->h - count) / 2;
        
        FREESURFACE(pSurf);
        
        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);

	  alphablit(pTmp, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp->h + 1);
 
          FREESURFACE(pTmp);            
        } effect_list_iterate_end;
        
        effect_list_unlink_all(sources);

	dest.y = i;        
        
        /* TODO: check if code replacement above is correct */
#if 0          
	if (city_got_building(pCity, B_TEMPLE)) {
	  pTmp1 =
	    ZoomSurface(GET_SURF(get_improvement_type(B_TEMPLE)->sprite),
			0.5, 0.5, 1);
	  count += (pTmp1->h + 1);
	  pSurf = pTmp1;
	} else {
	  pTmp1 = NULL;
	}

	if (city_got_building(pCity, B_COLOSSEUM)) {
	  pTmp2 =
	    ZoomSurface(GET_SURF(get_improvement_type(B_COLOSSEUM)->sprite),
			0.5, 0.5, 1);
	  count += (pTmp2->h + 1);
	  if (!pSurf) {
	    pSurf = pTmp2;
	  }
	} else {
	  pTmp2 = NULL;
	}

	if (city_got_building(pCity, B_CATHEDRAL) ||
	    city_affected_by_wonder(pCity, B_MICHELANGELO)) {
	  pTmp3 =
	    ZoomSurface(GET_SURF(get_improvement_type(B_CATHEDRAL)->sprite),
			0.5, 0.5, 1);
	  count += (pTmp3->h + 1);
	  if (!pSurf) {
	    pSurf = pTmp3;
	  }
	} else {
	  pTmp3 = NULL;
	}


	dest.x = pCityWindow->size.x + adj_size(212) - pSurf->w - adj_size(2);
	i = dest.y;
	dest.y += (pIcons->pMale_Happy->h - count) / 2;


	if (pTmp1) { /* Temple */
	  alphablit(pTmp1, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp1->h + 1);
	}

	if (pTmp2) { /* Colosseum */
	  alphablit(pTmp2, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp2->h + 1);
	}

	if (pTmp3) { /* Cathedral */
	  alphablit(pTmp3, NULL, pCityWindow->dst->surface, &dest);
	  /*dest.y += (pTmp3->h + 1); */
	}


	FREESURFACE(pTmp1);
	FREESURFACE(pTmp2);
	FREESURFACE(pTmp3);
	dest.y = i;
#endif        
      }

      if (j == 3) { /* police effect */
	dest.x = pCityWindow->size.x + adj_size(212) - pIcons->pPolice->w - adj_size(5);
	i = dest.y;
	dest.y +=
	    (pIcons->pMale_Happy->h - pIcons->pPolice->h) / 2;
	alphablit(pIcons->pPolice, NULL, pCityWindow->dst->surface, &dest);
	dest.y = i;
      }

      if (j == 4) { /* wonders effect */
	count = 0;

        get_city_bonus_effects(sources, pCity, NULL, EFT_MAKE_HAPPY);          
        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect( psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);
          
	  count += (pTmp->h + 1);

          if (!pSurf) {
	    pSurf = pTmp;
	  } else {
            FREESURFACE(pTmp);           
          }
             
        } effect_list_iterate_end;

        effect_list_unlink_all(sources);

        get_city_bonus_effects(sources, pCity, NULL, EFT_FORCE_CONTENT);
        
        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);
	  count += (pTmp->h + 1);

          if (!pSurf) {
	    pSurf = pTmp;
	  } else {
            FREESURFACE(pTmp);            
          }
             
        } effect_list_iterate_end;

        effect_list_unlink_all(sources);

        get_city_bonus_effects(sources, pCity, NULL, EFT_NO_UNHAPPY);

        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);
            
	  count += (pTmp->h + 1);

          FREESURFACE(pTmp);

        } effect_list_iterate_end;

        effect_list_unlink_all(sources);

        
	dest.x = pCityWindow->size.x + adj_size(212) - pSurf->w - adj_size(2);
	i = dest.y;
	dest.y += (pIcons->pMale_Happy->h - count) / 2;
        
        FREESURFACE(pSurf);

        get_city_bonus_effects(sources, pCity, NULL, EFT_MAKE_HAPPY);        

        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);

          alphablit(pTmp, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp->h + 1);
 
          FREESURFACE(pTmp);            
          
        } effect_list_iterate_end;
        effect_list_unlink_all(sources);        

        get_city_bonus_effects(sources, pCity, NULL, EFT_FORCE_CONTENT);        

        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);

	  alphablit(pTmp, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp->h + 1);
 
          FREESURFACE(pTmp);            
          
        } effect_list_iterate_end;
        effect_list_unlink_all(sources);        

        get_city_bonus_effects(sources, pCity, NULL, EFT_NO_UNHAPPY);        

        effect_list_iterate(sources, psource) {

          pTmp = get_building_surface(get_building_for_effect(psource->type));
          pTmp = ZoomSurface(pTmp, DEFAULT_ZOOM * ((float)18 / pTmp->w), DEFAULT_ZOOM * ((float)18 / pTmp->w), 1);

	  alphablit(pTmp, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp->h + 1);
 
          FREESURFACE(pTmp);            
          
        } effect_list_iterate_end;
        effect_list_unlink_all(sources);
        
	dest.y = i;        

        /* TODO: check if code replacement above is correct */        
#if 0	  
	if (city_affected_by_wonder(pCity, B_CURE)) {
	  pTmp1 =
	    ZoomSurface(GET_SURF(get_improvement_type(B_CURE)->sprite),
			0.5, 0.5, 1);
	  count += (pTmp1->h + 1);
	  pSurf = pTmp1;
	} else {
	  pTmp1 = NULL;
	}

	if (city_affected_by_wonder(pCity, B_SHAKESPEARE)) {
	  pTmp2 = ZoomSurface(
	  	GET_SURF(get_improvement_type(B_SHAKESPEARE)->sprite),
			      0.5, 0.5, 1);
	  count += (pTmp2->h + 1);
	  if (!pSurf) {
	    pSurf = pTmp2;
	  }
	} else {
	  pTmp2 = NULL;
	}

	if (city_affected_by_wonder(pCity, B_BACH)) {
	  pTmp3 =
	    ZoomSurface(GET_SURF(get_improvement_type(B_BACH)->sprite),
			0.5, 0.5, 1);
	  count += (pTmp3->h + 1);
	  if (!pSurf) {
	    pSurf = pTmp3;
	  }
	} else {
	  pTmp3 = NULL;
	}

	if (city_affected_by_wonder(pCity, B_HANGING)) {
	  pTmp4 =
	    ZoomSurface(GET_SURF(get_improvement_type(B_HANGING)->sprite),
			0.5, 0.5, 1);
	  count += (pTmp4->h + 1);
	  if (!pSurf) {
	    pSurf = pTmp4;
	  }
	} else {
	  pTmp4 = NULL;
	}

	dest.x = pCityWindow->size.x + adj_size(212) - pSurf->w - adj_size(2);
	i = dest.y;
	dest.y += (pIcons->pMale_Happy->h - count) / 2;


	if (pTmp1) { /* Cure of Cancer */
	  alphablit(pTmp1, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp1->h + 1);
	}

	if (pTmp2) { /* Shakespeare Theater */
	  alphablit(pTmp2, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp2->h + 1);
	}

	if (pTmp3) { /* J. S. Bach ... */
	  alphablit(pTmp3, NULL, pCityWindow->dst->surface, &dest);
	  dest.y += (pTmp3->h + 1);
	}

	if (pTmp4) { /* Hanging Gardens */
	  alphablit(pTmp4, NULL, pCityWindow->dst->surface, &dest);
	  /*dest.y += (pTmp4->h + 1); */
	}

	FREESURFACE(pTmp1);
	FREESURFACE(pTmp2);
	FREESURFACE(pTmp3);
	FREESURFACE(pTmp4);
	dest.y = i;
#endif        
      }

      dest.x = pCityWindow->size.x + adj_size(10);
      dest.y += pIcons->pMale_Happy->h + adj_size(5);

    }
  }
  
  effect_list_free(sources);
}

/**************************************************************************
  Redraw the dialog.
**************************************************************************/
static void redraw_city_dialog(struct city *pCity)
{
  char cBuf[40];
  int i, step, count, limit;
  int cost = 0;
  SDL_Rect dest;
  struct widget *pWindow = pCityDlg->pEndCityWidgetList;
  SDL_Surface *pBuf = NULL, *pBuf2 = NULL;
  SDL_String16 *pStr = NULL;

  refresh_city_names(pCity);

  if ((city_unhappy(pCity) || city_celebrating(pCity) || city_happy(pCity) ||
      cma_is_city_under_agent(pCity, NULL))
      ^ ((SDL_Client_Flags & CF_CITY_STATUS_SPECIAL) == CF_CITY_STATUS_SPECIAL)) {
    /* city status was changed : NORMAL <-> DISORDER, HAPPY, CELEBR. */

    SDL_Client_Flags ^= CF_CITY_STATUS_SPECIAL;

#if 0
    /* upd. resource map */
    FREESURFACE(pCityDlg->pResource_Map->theme);
    pCityDlg->pResource_Map->theme = get_scaled_city_map(pCity);
#endif	

    /* upd. window title */
    rebuild_citydlg_title_str(pCityDlg->pEndCityWidgetList, pCity);
  }

  /* update resource map */
  FREESURFACE(pCityDlg->pResource_Map->theme);
  pCityDlg->pResource_Map->theme = get_scaled_city_map(pCity);

  /* redraw city dlg */
  redraw_group(pCityDlg->pBeginCityWidgetList,
	       			pCityDlg->pEndCityWidgetList, 0);
  
  /* ================================================================= */
  my_snprintf(cBuf, sizeof(cBuf), _("City map"));

  pStr = create_str16_from_char(cBuf, adj_font(10));
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_GOLD);
  pStr->style |= TTF_STYLE_BOLD;

  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(222) + (adj_size(115) - pBuf->w) / 2;
  dest.y = pWindow->size.y + adj_size(69) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  my_snprintf(cBuf, sizeof(cBuf), _("Citizens"));

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_LUX);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(354) + (adj_size(147) - pBuf->w) / 2;
  dest.y = pWindow->size.y + adj_size(67) + (adj_size(13) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  my_snprintf(cBuf, sizeof(cBuf), _("City Improvements"));

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_GOLD);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(517) + (adj_size(115) - pBuf->w) / 2;
  dest.y = pWindow->size.y + adj_size(69) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);
  /* ================================================================= */
  /* food label */
  my_snprintf(cBuf, sizeof(cBuf), _("Food: %d per turn"),
	      pCity->prod[O_FOOD]);

  copy_chars_to_string16(pStr, cBuf);

  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_FOODPERTURN);

  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(200);
  dest.y = pWindow->size.y + adj_size(228) + (adj_size(16) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw food income */
  dest.y = pWindow->size.y + adj_size(246) + (adj_size(16) - pIcons->pBIG_Food->h) / 2;
  dest.x = pWindow->size.x + adj_size(203);

  if (pCity->surplus[O_FOOD] >= 0) {
    count = pCity->prod[O_FOOD] - pCity->surplus[O_FOOD];
  } else {
    count = pCity->prod[O_FOOD];
  }

  if (((pIcons->pBIG_Food->w + 1) * count) > adj_size(200)) {
    step = (adj_size(200) - pIcons->pBIG_Food->w) / (count - 1);
  } else {
    step = pIcons->pBIG_Food->w + 1;
  }

  for (i = 0; i < count; i++) {
    alphablit(pIcons->pBIG_Food, NULL, pWindow->dst->surface, &dest);
    dest.x += step;
  }

  my_snprintf(cBuf, sizeof(cBuf), Q_("?food:Surplus: %d"),
					      pCity->surplus[O_FOOD]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_FOOD_SURPLUS);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(440) - pBuf->w;
  dest.y = pWindow->size.y + adj_size(228) + (adj_size(16) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw surplus of food */
  if (pCity->surplus[O_FOOD]) {

    if (pCity->surplus[O_FOOD] > 0) {
      count = pCity->surplus[O_FOOD];
      pBuf = pIcons->pBIG_Food;
    } else {
      count = -1 * pCity->surplus[O_FOOD];
      pBuf = pIcons->pBIG_Food_Corr;
    }

    dest.x = pWindow->size.x + adj_size(423);
    dest.y = pWindow->size.y + adj_size(246) + (adj_size(16) - pBuf->h) / 2;

    /*if ( ((pBuf->w + 1) * count ) > 30 ) */
    if (count > 2) {
      if (count < 18) {
	step = (adj_size(30) - pBuf->w) / (count - 1);
      } else {
	step = 1;
	count = 17;
      }
    } else {
      step = pBuf->w + 1;
    }

    for (i = 0; i < count; i++) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x -= step;
    }
  }
  /* ================================================================= */
  /* productions label */
  my_snprintf(cBuf, sizeof(cBuf), _("Production: %d (%d) per turn"),
	      pCity->surplus[O_SHIELD] ,
		  pCity->prod[O_SHIELD] + pCity->waste[O_SHIELD]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_PROD);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(200);
  dest.y = pWindow->size.y + adj_size(263) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw productions schields */
  if (pCity->surplus[O_SHIELD]) {

    if (pCity->surplus[O_SHIELD] > 0) {
      count = pCity->surplus[O_SHIELD] + pCity->waste[O_SHIELD];
      pBuf = pIcons->pBIG_Shield;
    } else {
      count = -1 * pCity->surplus[O_SHIELD];
      pBuf = pIcons->pBIG_Shield_Corr;
    }

    dest.y = pWindow->size.y + adj_size(281) + (adj_size(16) - pBuf->h) / 2;
    dest.x = pWindow->size.x + adj_size(203);
    
    if ((pBuf->w * count) > adj_size(200)) {
      step = (adj_size(200) - pBuf->w) / (count - 1);
    } else {
      step = pBuf->w;
    }

    for (i = 0; i < count; i++) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      if(i > pCity->surplus[O_SHIELD]) {
	pBuf = pIcons->pBIG_Shield_Corr;
      }
    }
  }

  /* support shields label */
  my_snprintf(cBuf, sizeof(cBuf), Q_("?production:Support: %d"),
	  pCity->prod[O_SHIELD] + pCity->waste[O_SHIELD] - pCity->surplus[O_SHIELD]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_SUPPORT);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(440) - pBuf->w;
  dest.y = pWindow->size.y + adj_size(263) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw support shields */
  if (pCity->prod[O_SHIELD] - pCity->surplus[O_SHIELD]) {
    dest.x = pWindow->size.x + adj_size(423);
    dest.y =
	pWindow->size.y + adj_size(281) + (adj_size(16) - pIcons->pBIG_Shield->h) / 2;

    if ((pIcons->pBIG_Shield->w + 1) * (pCity->prod[O_SHIELD] -
					    pCity->surplus[O_SHIELD]) > adj_size(30)) {
      step =
	  (adj_size(30) - pIcons->pBIG_Food->w) / (pCity->prod[O_SHIELD] -
					     pCity->surplus[O_SHIELD] - 1);
    } else {
      step = pIcons->pBIG_Shield->w + 1;
    }

    for (i = 0; i < (pCity->prod[O_SHIELD] - pCity->surplus[O_SHIELD]); i++) {
      alphablit(pIcons->pBIG_Shield, NULL, pWindow->dst->surface, &dest);
      dest.x -= step;
    }
  }
  /* ================================================================= */

  /* trade label */
  my_snprintf(cBuf, sizeof(cBuf), _("Trade: %d per turn"),
	      pCity->surplus[O_TRADE]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_TRADE);
  
  pBuf = create_text_surf_from_str16(pStr);
  
  dest.x = pWindow->size.x + adj_size(200);
  dest.y = pWindow->size.y + adj_size(298) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw total (trade - corruption) */
  if (pCity->surplus[O_TRADE]) {
    dest.y =
	pWindow->size.y + adj_size(316) + (adj_size(16) - pIcons->pBIG_Trade->h) / 2;
    dest.x = pWindow->size.x + adj_size(203);
    
    if (((pIcons->pBIG_Trade->w + 1) * pCity->surplus[O_TRADE]) > adj_size(200)) {
      step = (adj_size(200) - pIcons->pBIG_Trade->w) / (pCity->surplus[O_TRADE] - 1);
    } else {
      step = pIcons->pBIG_Trade->w + 1;
    }

    for (i = 0; i < pCity->surplus[O_TRADE]; i++) {
      alphablit(pIcons->pBIG_Trade, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
    }
  }

  /* corruption label */
  my_snprintf(cBuf, sizeof(cBuf), _("Corruption: %d"),
	      pCity->waste[O_TRADE]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_CORRUPTION);

  pBuf = create_text_surf_from_str16(pStr);
  
  dest.x = pWindow->size.x + adj_size(440) - pBuf->w;
  dest.y = pWindow->size.y + adj_size(298) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw corruption */
  if (pCity->waste[O_TRADE] > 0) {
    dest.x = pWindow->size.x + adj_size(423);
    dest.y =
	pWindow->size.y + adj_size(316) + (adj_size(16) - pIcons->pBIG_Trade->h) / 2;
    
    if (((pIcons->pBIG_Trade_Corr->w + 1) * pCity->waste[O_TRADE]) > adj_size(30)) {
      step =
	  (adj_size(30) - pIcons->pBIG_Trade_Corr->w) / (pCity->waste[O_TRADE] - 1);
    } else {
      step = pIcons->pBIG_Trade_Corr->w + 1;
    }

    for (i = 0; i < pCity->waste[O_TRADE]; i++) {
      alphablit(pIcons->pBIG_Trade_Corr, NULL, pWindow->dst->surface,
		      &dest);
      dest.x -= step;
    }

  }
  /* ================================================================= */
  /* gold label */
  my_snprintf(cBuf, sizeof(cBuf), _("Gold: %d (%d) per turn"),
	      pCity->surplus[O_GOLD], pCity->prod[O_GOLD]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_GOLD);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(200);
  dest.y = pWindow->size.y + adj_size(342) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw coins */
  count = pCity->surplus[O_GOLD];
  if (count) {

    if (count > 0) {
      pBuf = pIcons->pBIG_Coin;
    } else {
      count *= -1;
      pBuf = pIcons->pBIG_Coin_Corr;
    }

    dest.y = pWindow->size.y + adj_size(359) + (adj_size(16) - pBuf->h) / 2;
    dest.x = pWindow->size.x + adj_size(203);
    
    if ((pBuf->w * count) > adj_size(110)) {
      step = (adj_size(110) - pBuf->w) / (count - 1);
      if (!step) {
	step = 1;
	count = 97;
      }
    } else {
      step = pBuf->w;
    }

    for (i = 0; i < count; i++) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
    }

  }

  /* upkeep label */
  my_snprintf(cBuf, sizeof(cBuf), _("Upkeep: %d"),
	      pCity->prod[O_GOLD] - pCity->surplus[O_GOLD]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_UPKEEP);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(440) - pBuf->w;
  dest.y = pWindow->size.y + adj_size(342) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw upkeep */
  count = pCity->surplus[O_GOLD];
  if (pCity->prod[O_GOLD] - count) {

    dest.x = pWindow->size.x + adj_size(423);
    dest.y = pWindow->size.y + adj_size(359)
      + (adj_size(16) - pIcons->pBIG_Coin_UpKeep->h) / 2;
    
    if (((pIcons->pBIG_Coin_UpKeep->w + 1) *
	 (pCity->prod[O_GOLD] - count)) > adj_size(110)) {
      step = (adj_size(110) - pIcons->pBIG_Coin_UpKeep->w) /
	  (pCity->prod[O_GOLD] - count - 1);
    } else {
      step = pIcons->pBIG_Coin_UpKeep->w + 1;
    }

    for (i = 0; i < (pCity->prod[O_GOLD] - count); i++) {
      alphablit(pIcons->pBIG_Coin_UpKeep, NULL, pWindow->dst->surface,
		      &dest);
      dest.x -= step;
    }
  }
  /* ================================================================= */
  /* science label */
  my_snprintf(cBuf, sizeof(cBuf), _("Science: %d per turn"),
	      pCity->prod[O_SCIENCE]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_SCIENCE);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(200);
  dest.y = pWindow->size.y + adj_size(376) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw colb */
  count = pCity->prod[O_SCIENCE];
  if (count) {

    dest.y =
	pWindow->size.y + adj_size(394) + (adj_size(16) - pIcons->pBIG_Colb->h) / 2;
    dest.x = pWindow->size.x + adj_size(203);
    
    if ((pIcons->pBIG_Colb->w * count) > adj_size(235)) {
      step = (adj_size(235) - pIcons->pBIG_Colb->w) / (count - 1);
      if (!step) {
	step = 1;
	count = 222;
      }
    } else {
      step = pIcons->pBIG_Colb->w;
    }

    for (i = 0; i < count; i++) {
      alphablit(pIcons->pBIG_Colb, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
    }
  }
  /* ================================================================= */
  /* luxury label */
  my_snprintf(cBuf, sizeof(cBuf), _("Luxury: %d per turn"),
	      pCity->prod[O_LUXURY]);

  copy_chars_to_string16(pStr, cBuf);
  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_LUX);
  
  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(200);
  dest.y = pWindow->size.y + adj_size(412) + (adj_size(15) - pBuf->h) / 2;

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);

  /* draw luxury */
  if (pCity->prod[O_LUXURY]) {

    dest.y =
	pWindow->size.y + adj_size(429) + (adj_size(16) - pIcons->pBIG_Luxury->h) / 2;
    dest.x = pWindow->size.x + adj_size(203);
    
    if ((pIcons->pBIG_Luxury->w * pCity->prod[O_LUXURY]) > adj_size(235)) {
      step =
	  (adj_size(235) - pIcons->pBIG_Luxury->w) / (pCity->prod[O_LUXURY] - 1);
    } else {
      step = pIcons->pBIG_Luxury->w;
    }

    for (i = 0; i < pCity->prod[O_LUXURY]; i++) {
      alphablit(pIcons->pBIG_Luxury, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
    }
  }
  /* ================================================================= */
  /* turns to grow label */
  count = city_turns_to_grow(pCity);
  if (count == 0) {
    my_snprintf(cBuf, sizeof(cBuf), _("City growth: blocked"));
  } else if (count == FC_INFINITY) {
    my_snprintf(cBuf, sizeof(cBuf), _("City growth: never"));
  } else if (count < 0) {
    /* turns until famine */
    my_snprintf(cBuf, sizeof(cBuf),
		_("City shrinks: %d %s"), abs(count),
		PL_("turn", "turns", abs(count)));
  } else {
    my_snprintf(cBuf, sizeof(cBuf),
		_("City growth: %d %s"), count,
		PL_("turn", "turns", count));
  }

  copy_chars_to_string16(pStr, cBuf);

  pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_GROWTH);  

  pBuf = create_text_surf_from_str16(pStr);

  dest.x = pWindow->size.x + adj_size(445) + (adj_size(192) - pBuf->w) / 2;
  dest.y = pWindow->size.y + adj_size(227);

  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);


  count = (city_granary_size(pCity->size)) / 10;

  if (count > 12) {
    step = (adj_size(168) - pIcons->pBIG_Food->h) / adj_size((11 + count - 12));
    i = (count - 1) * step + 14;
    count = 12;
  } else {
    step = pIcons->pBIG_Food->h;
    i = count * step;
  }

  /* food stock */
  
    /* FIXME: check if this code replacement is correct */
    /*  if (city_got_building(pCity, B_GRANARY)              */
    /*      || city_affected_by_wonder(pCity, B_PYRAMIDS)) { */
          
    if (get_city_bonus(pCity, EFT_GROWTH_FOOD) > 0) {

    /* with granary */
    /* stocks label */
    copy_chars_to_string16(pStr, _("Stock"));
    pBuf = create_text_surf_from_str16(pStr);

    dest.x = pWindow->size.x + adj_size(461) + (adj_size(76) - pBuf->w) / 2;
    dest.y = pWindow->size.y + adj_size(258) - pBuf->h - 1;

    alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

    FREESURFACE(pBuf);

    /* granary label */
    copy_chars_to_string16(pStr, _("Granary"));
    pBuf = create_text_surf_from_str16(pStr);

    dest.x = pWindow->size.x + adj_size(549) + (adj_size(76) - pBuf->w) / 2;
    dest.y = pWindow->size.y + adj_size(258) - pBuf->h - 1;

    alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

    FREESURFACE(pBuf);

    /* draw bcgd granary */
    dest.x = pWindow->size.x + adj_size(462);
    dest.y = pWindow->size.y + adj_size(260);
    dest.w = 70 + 4;
    dest.h = i + 4;

    SDL_FillRectAlpha(pWindow->dst->surface, &dest, get_game_colorRGB(COLOR_THEME_CITYDLG_GRANARY));

    putframe(pWindow->dst->surface, dest.x - 1, dest.y - 1, dest.x + dest.w, dest.y + dest.h,
      map_rgba(pWindow->dst->surface->format, *get_game_colorRGB(COLOR_THEME_CITYDLG_FRAME)));
		
    /* draw bcgd stocks*/
    dest.x = pWindow->size.x + adj_size(550);
    dest.y = pWindow->size.y + adj_size(260);

    SDL_FillRectAlpha(pWindow->dst->surface, &dest, get_game_colorRGB(COLOR_THEME_CITYDLG_STOCKS));

    putframe(pWindow->dst->surface, dest.x - 1, dest.y - 1, dest.x + dest.w, dest.y + dest.h,
      map_rgba(pWindow->dst->surface->format, *get_game_colorRGB(COLOR_THEME_CITYDLG_FRAME)));

    /* draw stocks icons */
    cost = city_granary_size(pCity->size);
    if (pCity->food_stock + pCity->surplus[O_FOOD] > cost) {
      count = cost;
    } else {
      if(pCity->surplus[O_FOOD] < 0) {
        count = pCity->food_stock;
      } else {
	count = pCity->food_stock + pCity->surplus[O_FOOD];
      }
    }
    cost /= 2;
    
    if(pCity->surplus[O_FOOD] < 0) {
      limit = pCity->food_stock + pCity->surplus[O_FOOD];
      if(limit < 0) {
	limit = 0;
      }
    } else {
      limit = 0xffff;
    }
    
    dest.x += 2;
    dest.y += 2;
    i = 0;
    pBuf = pIcons->pBIG_Food;
    while (count && cost) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += pBuf->w;
      count--;
      cost--;
      i++;
      if (dest.x > pWindow->size.x + adj_size(620)) {
	dest.x = pWindow->size.x + adj_size(552);
	dest.y += step;
      }
      if(i > limit - 1) {
	pBuf = pIcons->pBIG_Food_Corr;
      } else {
        if(i > pCity->food_stock - 1)
        {
	  pBuf = pIcons->pBIG_Food_Surplus;
        }
      }
    }
    /* draw granary icons */
    dest.x = pWindow->size.x + adj_size(462) + adj_size(2);
    dest.y = pWindow->size.y + adj_size(260) + adj_size(2);
        
    while (count) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += pBuf->w;
      count--;
      i++;
      if (dest.x > pWindow->size.x + adj_size(532)) {
	dest.x = pWindow->size.x + adj_size(464);
	dest.y += step;
      }
      if(i > limit - 1) {
	pBuf = pIcons->pBIG_Food_Corr;
      } else {
        if(i > pCity->food_stock - 1)
        {
	  pBuf = pIcons->pBIG_Food_Surplus;
        }
      }
    }
    
  } else {
    /* without granary */
    /* stocks label */
    copy_chars_to_string16(pStr, _("Stock"));
    pBuf = create_text_surf_from_str16(pStr);

    dest.x = pWindow->size.x + adj_size(461) + (adj_size(144) - pBuf->w) / 2;
    dest.y = pWindow->size.y + adj_size(258) - pBuf->h - 1;

    alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
    FREESURFACE(pBuf);
    
    /* food stock */

    /* draw bcgd */
    dest.x = pWindow->size.x + adj_size(462);
    dest.y = pWindow->size.y + adj_size(260);
    dest.w = adj_size(144);
    dest.h = i + adj_size(4);

    SDL_FillRectAlpha(pWindow->dst->surface, &dest, get_game_colorRGB(COLOR_THEME_CITYDLG_FOODSTOCK));

    putframe(pWindow->dst->surface, dest.x - 1, dest.y - 1, dest.x + dest.w, dest.y + dest.h,
      map_rgba(pWindow->dst->surface->format, *get_game_colorRGB(COLOR_THEME_CITYDLG_FRAME)));

    /* draw icons */
    cost = city_granary_size(pCity->size);
    if (pCity->food_stock + pCity->surplus[O_FOOD] > cost) {
      count = cost;
    } else {
      if(pCity->surplus[O_FOOD] < 0) {
        count = pCity->food_stock;
      } else {
	count = pCity->food_stock + pCity->surplus[O_FOOD];
      }
    }
        
    if(pCity->surplus[O_FOOD] < 0) {
      limit = pCity->food_stock + pCity->surplus[O_FOOD];
      if(limit < 0) {
	limit = 0;
      }
    } else {
      limit = 0xffff;
    }
        
    dest.x += adj_size(2);
    dest.y += adj_size(2);
    i = 0;
    pBuf = pIcons->pBIG_Food;
    while (count) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += pBuf->w;
      count--;
      i++;
      if (dest.x > pWindow->size.x + adj_size(602)) {
	dest.x = pWindow->size.x + adj_size(464);
	dest.y += step;
      }
      if(i > limit - 1) {
	pBuf = pIcons->pBIG_Food_Corr;
      } else {
        if(i > pCity->food_stock - 1)
        {
	  pBuf = pIcons->pBIG_Food_Surplus;
        }
      }
    }
  }
  /* ================================================================= */

  /* draw productions shields progress */
  if (pCity->production.is_unit) {
    struct unit_type *pUnit = get_unit_type(pCity->production.value);
    cost = unit_build_shield_cost(get_unit_type(pCity->production.value));
    count = cost / 10;
        
    copy_chars_to_string16(pStr, pUnit->name);
    pBuf = create_text_surf_from_str16(pStr);
    
    pBuf2 = get_unittype_surface(get_unit_type(pCity->production.value));
    pBuf2 = ZoomSurface(pBuf2, DEFAULT_ZOOM * ((float)32 / pBuf2->h), DEFAULT_ZOOM * ((float)32 / pBuf2->h), 1);

    /* blit unit icon */
    dest.x = pWindow->size.x + adj_size(6) + (adj_size(185) - (pBuf->w + pBuf2->w + adj_size(5))) / 2;
    dest.y = pWindow->size.y + adj_size(233);
    
    alphablit(pBuf2, NULL, pWindow->dst->surface, &dest);

    dest.y += (pBuf2->h - pBuf->h) / 2;
    dest.x += pBuf2->w + adj_size(5);

  } else {
    struct impr_type *pImpr =
	get_improvement_type(pCity->production.value);

    if (impr_flag(pCity->production.value, IF_GOLD)) {

      if (pCityDlg->pBuy_Button
	 && get_wstate(pCityDlg->pBuy_Button) != FC_WS_DISABLED) {
	set_wstate(pCityDlg->pBuy_Button, FC_WS_DISABLED);
	widget_redraw(pCityDlg->pBuy_Button);
      }

      /* You can't see capitalization progres */
      count = 0;

    } else {

      if (!pCity->did_buy && pCityDlg->pBuy_Button
	 && (get_wstate(pCityDlg->pBuy_Button) == FC_WS_DISABLED)) {
	set_wstate(pCityDlg->pBuy_Button, FC_WS_NORMAL);
	widget_redraw(pCityDlg->pBuy_Button);
      }

      cost = impr_build_shield_cost(pCity->production.value);
      count = cost / 10;
      
    }

    copy_chars_to_string16(pStr, pImpr->name);
    pBuf = create_text_surf_from_str16(pStr);
    
    pBuf2 = get_building_surface(pCity->production.value);
    pBuf2 = ZoomSurface(pBuf2, DEFAULT_ZOOM * ((float)32 / pBuf2->h), DEFAULT_ZOOM * ((float)32 / pBuf2->h), 1);

    /* blit impr icon */
    dest.x = pWindow->size.x + adj_size(6) + (adj_size(185) - (pBuf->w + pBuf2->w + adj_size(5))) / 2;
    dest.y = pWindow->size.y + adj_size(233);

    alphablit(pBuf2, NULL, pWindow->dst->surface, &dest);
    
    dest.y += (pBuf2->h - pBuf->h) / 2;
    dest.x += pBuf2->w + adj_size(5);
  }

  /* blit unit/impr name */
  alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

  FREESURFACE(pBuf);
  FREESURFACE(pBuf2);
  
  if (count) {
    if (count > 11) {
      step = (adj_size(154) - pIcons->pBIG_Shield->h) / adj_size((10 + count - 11));
      
      if(!step) step = 1;
      
      i = (step * (count - 1)) + pIcons->pBIG_Shield->h;
    } else {
      step = pIcons->pBIG_Shield->h;
      i = count * step;
    }
    
    /* draw sheild stock background */
    dest.x = pWindow->size.x + adj_size(28);
    dest.y = pWindow->size.y + adj_size(270);
    dest.w = adj_size(144);
    dest.h = i + adj_size(4);

    SDL_FillRectAlpha(pWindow->dst->surface, &dest, get_game_colorRGB(COLOR_THEME_CITYDLG_SHIELDSTOCK));
    putframe(pWindow->dst->surface, dest.x - 1, dest.y - 1, dest.x + dest.w, dest.y + dest.h,
      map_rgba(pWindow->dst->surface->format, *get_game_colorRGB(COLOR_THEME_CITYDLG_FRAME)));
    
    /* draw production progres text */
    dest.y = pWindow->size.y + adj_size(270) + dest.h + 1;
    
    if (pCity->shield_stock < cost) {
      count = city_turns_to_build(pCity,
    	pCity->production, TRUE);
      if (count == 999) {
        my_snprintf(cBuf, sizeof(cBuf), "(%d/%d) %s!",
		  		pCity->shield_stock, cost,  _("blocked"));
      } else {
        my_snprintf(cBuf, sizeof(cBuf), "(%d/%d) %d %s",
	    pCity->shield_stock, cost, count, PL_("turn", "turns", count));
     }
   } else {
     my_snprintf(cBuf, sizeof(cBuf), "(%d/%d) %s!",
		    		pCity->shield_stock, cost, _("finished"));
   }

    copy_chars_to_string16(pStr, cBuf);
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_LUX);
    
    pBuf = create_text_surf_from_str16(pStr);

    dest.x = pWindow->size.x + adj_size(6) + (adj_size(185) - pBuf->w) / 2;

    alphablit(pBuf, NULL, pWindow->dst->surface, &dest);

    FREESTRING16(pStr);
    FREESURFACE(pBuf);
    
    /* draw sheild stock */
    if (pCity->shield_stock + pCity->surplus[O_SHIELD] <= cost) {
      count = pCity->shield_stock + pCity->surplus[O_SHIELD];
    } else {
      count = cost;
    }
    dest.x = pWindow->size.x + adj_size(29) + adj_size(2);
    dest.y = pWindow->size.y + adj_size(270) + adj_size(2);
    i = 0;
    
    pBuf = pIcons->pBIG_Shield;
    while (count > 0) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += pBuf->w;
      count--;
      if (dest.x > pWindow->size.x + adj_size(170)) {
	dest.x = pWindow->size.x + adj_size(31);
	dest.y += step;
      }
      i++;
      if(i > pCity->shield_stock - 1) {
	pBuf = pIcons->pBIG_Shield_Surplus;
      }
    }   
  }

  /* count != 0 */
  /* ==================================================== */
  /* Draw Citizens */
  count = (pCity->ppl_happy[4] + pCity->ppl_content[4]
	   + pCity->ppl_unhappy[4] + pCity->ppl_angry[4]
	   + pCity->specialists[SP_ELVIS] + pCity->specialists[SP_SCIENTIST]
	   + pCity->specialists[SP_TAXMAN]);

  pBuf = get_tax_surface(O_LUXURY);
  
  if (count > 13) {
    step = (adj_size(400) - pBuf->w) / (adj_size(12 + count - 13));
  } else {
    step = pBuf->w;
  }

  dest.y = pWindow->size.y + adj_size(26) + (adj_size(42) - pBuf->h) / 2;
  dest.x = pWindow->size.x + adj_size(227);

  FREESURFACE(pBuf);
  
  if (pCity->ppl_happy[4]) {
    for (i = 0; i < pCity->ppl_happy[4]; i++) {
      pBuf = adj_surf(get_citizen_surface(CITIZEN_HAPPY, i));
      
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      FREESURFACE(pBuf);
    }
  }

  if (pCity->ppl_content[4]) {
    for (i = 0; i < pCity->ppl_content[4]; i++) {
      pBuf = adj_surf(get_citizen_surface(CITIZEN_CONTENT, i));
      
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      FREESURFACE(pBuf);
    }
  }

  if (pCity->ppl_unhappy[4]) {
    for (i = 0; i < pCity->ppl_unhappy[4]; i++) {
      pBuf = adj_surf(get_citizen_surface(CITIZEN_UNHAPPY, i));
      
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      FREESURFACE(pBuf);
    }
  }

  if (pCity->ppl_angry[4]) {
    for (i = 0; i < pCity->ppl_angry[4]; i++) {
      pBuf = adj_surf(get_citizen_surface(CITIZEN_ANGRY, i));
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      FREESURFACE(pBuf);
    }
  }
    
  pCityDlg->specs[0] = FALSE;
  pCityDlg->specs[1] = FALSE;
  pCityDlg->specs[2] = FALSE;
  
  if (pCity->specialists[SP_ELVIS]) {
    pBuf = get_tax_surface(O_LUXURY);
    
    pCityDlg->specs_area[0].x = dest.x;
    pCityDlg->specs_area[0].y = dest.y;
    pCityDlg->specs_area[0].w = pBuf->w;
    pCityDlg->specs_area[0].h = pBuf->h;
    for (i = 0; i < pCity->specialists[SP_ELVIS]; i++) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      pCityDlg->specs_area[0].w += step;
    }
    FREESURFACE(pBuf);
    pCityDlg->specs_area[0].w -= step;
    pCityDlg->specs[0] = TRUE;
  }

  if (pCity->specialists[SP_TAXMAN]) {
    pBuf = get_tax_surface(O_GOLD);
    
    pCityDlg->specs_area[1].x = dest.x;
    pCityDlg->specs_area[1].y = dest.y;
    pCityDlg->specs_area[1].w = pBuf->w;
    pCityDlg->specs_area[1].h = pBuf->h;
    for (i = 0; i < pCity->specialists[SP_TAXMAN]; i++) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      pCityDlg->specs_area[1].w += step;
    }
    FREESURFACE(pBuf);
    pCityDlg->specs_area[1].w -= step;
    pCityDlg->specs[1] = TRUE;
  }

  if (pCity->specialists[SP_SCIENTIST]) {
    pBuf = get_tax_surface(O_SCIENCE);
    
    pCityDlg->specs_area[2].x = dest.x;
    pCityDlg->specs_area[2].y = dest.y;
    pCityDlg->specs_area[2].w = pBuf->w;
    pCityDlg->specs_area[2].h = pBuf->h;
    for (i = 0; i < pCity->specialists[SP_SCIENTIST]; i++) {
      alphablit(pBuf, NULL, pWindow->dst->surface, &dest);
      dest.x += step;
      pCityDlg->specs_area[2].w += step;
    }
    FREESURFACE(pBuf);
    pCityDlg->specs_area[2].w -= step;
    pCityDlg->specs[2] = TRUE;
  }

  /* ==================================================== */


  switch (pCityDlg->page) {
  case INFO_PAGE:
    redraw_info_city_dialog(pWindow, pCity);
    break;

  case HAPPINESS_PAGE:
    redraw_happyness_city_dialog(pWindow, pCity);
    break;

  case ARMY_PAGE:
    redraw_army_city_dialog(pWindow, pCity);
    break;

  case SUPPORTED_UNITS_PAGE:
    redraw_supported_units_city_dialog(pWindow, pCity);
    break;

  case MISC_PAGE:
    redraw_misc_city_dialog(pWindow, pCity);
    break;

  default:
    break;

  }
  
  /* redraw "sell improvement" dialog */
  redraw_group(pCityDlg->pBeginCityMenuWidgetList,
	       pCityDlg->pEndCityMenuWidgetList, 0);
 
  widget_mark_dirty(pWindow);
}

/* ============================================================== */

/**************************************************************************
  ...
**************************************************************************/
static void rebuild_imprm_list(struct city *pCity)
{
  int count = 0;
  struct widget *pWindow = pCityDlg->pEndCityWidgetList;
  struct widget *pAdd_Dock, *pBuf, *pLast;
  SDL_Surface *pLogo = NULL;
  SDL_String16 *pStr = NULL;
  struct impr_type *pImpr = NULL;
  struct player *pOwner = city_owner(pCity);
  int prev_y = 0;
    
  if(!pCityDlg->pImprv) {
    pCityDlg->pImprv = fc_calloc(1, sizeof(struct ADVANCED_DLG));
  }
  
  /* free old list */
  if (pCityDlg->pImprv->pEndWidgetList) {
    del_group_of_widgets_from_gui_list(pCityDlg->pImprv->pBeginWidgetList,
				       pCityDlg->pImprv->pEndWidgetList);
    pCityDlg->pImprv->pEndWidgetList = NULL;
    pCityDlg->pImprv->pBeginWidgetList = NULL;
    pCityDlg->pImprv->pActiveWidgetList = NULL;
    FC_FREE(pCityDlg->pImprv->pScroll);
  } 
    
  pAdd_Dock = pCityDlg->pAdd_Point;
  pBuf = pLast = pAdd_Dock;
  
  /* allock new */
  built_impr_iterate(pCity, imp) {

    pImpr = get_improvement_type(imp);

    pStr = create_str16_from_char(get_impr_name_ex(pCity, imp), adj_font(12));
    pStr->fgcol = *get_game_colorRGB(COLOR_THEME_CITYDLG_IMPR);

    pStr->style |= TTF_STYLE_BOLD;

    pLogo = get_building_surface(imp);
    pLogo = ZoomSurface(pLogo, DEFAULT_ZOOM * ((float)22 / pLogo->w), DEFAULT_ZOOM * ((float)22 / pLogo->w), 1);
    
    pBuf = create_iconlabel(pLogo, pWindow->dst, pStr,
			 (WF_FREE_THEME | WF_RESTORE_BACKGROUND));

    pBuf->size.x = pWindow->size.x + adj_size(428);
    pBuf->size.y = pWindow->size.y + adj_size(91) + prev_y;
    
    prev_y += pBuf->size.h;
    
    pBuf->size.w = adj_size(182);
    pBuf->action = sell_imprvm_dlg_callback;

    if (!pCityDlg->pCity->did_sell
        && !is_wonder(imp) && (pOwner == game.player_ptr)) {
      set_wstate(pBuf, FC_WS_NORMAL);
    }

    pBuf->ID = MAX_ID - imp - 3000;
    DownAdd(pBuf, pAdd_Dock);
    pAdd_Dock = pBuf;
        
    count++;

    if (count > 8) {
      set_wflag(pBuf, WF_HIDDEN);
    }

  } built_impr_iterate_end;

  if (count) {
    pCityDlg->pImprv->pEndWidgetList = pLast->prev;
    pCityDlg->pImprv->pEndActiveWidgetList = pLast->prev;
    pCityDlg->pImprv->pBeginWidgetList = pBuf;
    pCityDlg->pImprv->pBeginActiveWidgetList = pBuf;

    if (count > 8) {
      pCityDlg->pImprv->pActiveWidgetList =
		    pCityDlg->pImprv->pEndActiveWidgetList;
      
/* FIXME: this can probably be removed */
#if 0
      pCityDlg->pImprv->pScroll = fc_calloc(1, sizeof(struct ScrollBar));
      pCityDlg->pImprv->pScroll->step = 1;  
      pCityDlg->pImprv->pScroll->active = 8;
      pCityDlg->pImprv->pScroll->count = count;
#endif
    
      create_vertical_scrollbar(pCityDlg->pImprv, 1, 8, TRUE, TRUE);
    
      setup_vertical_scrollbar_area(pCityDlg->pImprv->pScroll,
	pWindow->size.x + adj_size(629), pWindow->size.y + adj_size(90), adj_size(130), TRUE);
    }
  }
}

/**************************************************************************
  ...
**************************************************************************/
static void rebuild_citydlg_title_str(struct widget *pWindow,
				      struct city *pCity)
{
  char cBuf[512];

  my_snprintf(cBuf, sizeof(cBuf),
	      _("City of %s (Population %s citizens)"), pCity->name,
	      population_to_text(city_population(pCity)));

  if (city_unhappy(pCity)) {
    mystrlcat(cBuf, _(" - DISORDER"), sizeof(cBuf));
  } else {
    if (city_celebrating(pCity)) {
      mystrlcat(cBuf, _(" - celebrating"), sizeof(cBuf));
    } else {
      if (city_happy(pCity)) {
	mystrlcat(cBuf, _(" - happy"), sizeof(cBuf));
      }
    }
  }

  if (cma_is_city_under_agent(pCity, NULL)) {
    mystrlcat(cBuf, _(" - under Citizen Governor control."), sizeof(cBuf));
  }
  
  copy_chars_to_string16(pWindow->string16, cBuf);
}


/* ========================= Public ================================== */

/**************************************************************************
  Pop up (or bring to the front) a dialog for the given city.  It may or
  may not be modal.
**************************************************************************/
void popup_city_dialog(struct city *pCity)
{
  struct widget *pWindow = NULL, *pBuf = NULL;
  SDL_Surface *pLogo = NULL;
  SDL_String16 *pStr = NULL;
  int cs;
  struct player *pOwner = city_owner(pCity);
    
  if (pCityDlg) {
    return;
  }

  update_menus();

  pCityDlg = fc_calloc(1, sizeof(struct city_dialog));
  pCityDlg->pCity = pCity;
  pCityDlg->page = ARMY_PAGE;
  
  pStr = create_string16(NULL, 0, adj_font(12));
  pStr->style |= TTF_STYLE_BOLD;
  pWindow = create_window(NULL, pStr, adj_size(640), adj_size(480), 0);
  
  rebuild_citydlg_title_str(pWindow, pCity);

  widget_set_position(pWindow,
                      (Main.screen->w - adj_size(640)) / 2,
                      (Main.screen->h - adj_size(480)) / 2);
  
  pWindow->size.w = adj_size(640);
  pWindow->size.h = adj_size(480);
  pWindow->action = city_dlg_callback;
  set_wstate(pWindow, FC_WS_NORMAL);

  /* create window background */
  pLogo = theme_get_background(theme, BACKGROUND_CITYDLG);
  if (resize_window(pWindow, pLogo, NULL, pWindow->size.w, pWindow->size.h)) {
    FREESURFACE(pLogo);
  }
    
  pLogo = get_city_gfx();
  alphablit(pLogo, NULL, pWindow->theme, NULL);
  
  pCityDlg->pEndCityWidgetList = pWindow;
  add_to_gui_list(ID_CITY_DLG_WINDOW, pWindow);

  /* ============================================================= */

#if 0  
  /* in title bar */
  pBuf = create_themeicon(pTheme->CANCEL_Icon, pWindow->dst->surface,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Cancel"), adj_font(12));
  pBuf->action = exit_city_dlg_callback;
  pBuf->size.x = pWindow->size.x + pWindow->size.w - pBuf->size.w;
  pBuf->size.y = pWindow->size.y;
  pBuf->key = SDLK_ESCAPE;
  set_wstate(pBuf, FC_WS_NORMAL);
  add_to_gui_list(ID_CITY_DLG_EXIT_BUTTON, pBuf);
#endif

  /* Buttons */
  pBuf = create_themeicon(pTheme->CANCEL_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Cancel"), adj_font(12));
  pBuf->action = exit_city_dlg_callback;
  pBuf->key = SDLK_ESCAPE;
  pBuf->size.x = pWindow->size.x + pWindow->size.w - pBuf->size.w - adj_size(10);
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(9);
  set_wstate(pBuf, FC_WS_NORMAL);
  add_to_gui_list(ID_CITY_DLG_EXIT_BUTTON, pBuf);
  /* -------- */
  
  pBuf = create_themeicon(pTheme->Support_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Maintenance panel"), adj_font(12));
  pBuf->action = supported_unit_city_dlg_callback;
  pBuf->size.x =
      pWindow->size.x + adj_size(5) + ((adj_size(207) - 5 * pBuf->size.w) / 6);
  pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(7);
  set_wstate(pBuf, FC_WS_NORMAL);
  add_to_gui_list(ID_CITY_DLG_SUPPORT_BUTTON, pBuf);
  /* -------- */
  
  pBuf = create_themeicon(pTheme->Army_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Garrison panel"), adj_font(12));
  pBuf->action = army_city_dlg_callback;
  pBuf->size.x =
      pWindow->size.x + adj_size(5) + 2 * ((adj_size(207) - 5 * pBuf->size.w) / 6) + pBuf->size.w;
  pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(7);
  set_wstate(pBuf, FC_WS_NORMAL);
  add_to_gui_list(ID_CITY_DLG_ARMY_BUTTON, pBuf);
  /* -------- */
  
  pBuf = create_themeicon(pTheme->Happy_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Happiness panel"), adj_font(12));
  pBuf->action = happy_city_dlg_callback;
  pBuf->size.x =
      pWindow->size.x + adj_size(5) + 3 * ((adj_size(207) - 5 * pBuf->size.w) / 6) + 2 * pBuf->size.w;
  pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(7);
  set_wstate(pBuf, FC_WS_NORMAL);
  add_to_gui_list(ID_CITY_DLG_HAPPY_BUTTON, pBuf);
  /* -------- */
  
  pBuf = create_themeicon(pTheme->INFO_Icon, pWindow->dst,
			  (WF_WIDGET_HAS_INFO_LABEL |
			   WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Information panel"), adj_font(12));
  pBuf->action = info_city_dlg_callback;
  pBuf->size.x =
      pWindow->size.x + adj_size(5) + 4 * ((adj_size(207) - 5 * pBuf->size.w) / 6) + 3 * pBuf->size.w;
  pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(7);
  set_wstate(pBuf, FC_WS_NORMAL);
  add_to_gui_list(ID_CITY_DLG_INFO_BUTTON, pBuf);

  pCityDlg->pAdd_Point = pBuf;
  pCityDlg->pBeginCityWidgetList = pBuf;
  /* ===================================================== */
  rebuild_imprm_list(pCity);
  /* ===================================================== */
  
  pLogo = get_scaled_city_map(pCity);
  pBuf = create_themelabel(pLogo, pWindow->dst, NULL, pLogo->w, pLogo->h, 0);

  pCityDlg->pResource_Map = pBuf;

  pBuf->action = resource_map_city_dlg_callback;
  if (!cma_is_city_under_agent(pCity, NULL) && (pOwner == game.player_ptr)) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  pBuf->size.x =
      pWindow->size.x + (pWindow->size.w - pBuf->size.w) / 2 - 1;
  pBuf->size.y = pWindow->size.y + adj_size(87) + (adj_size(134) - pBuf->size.h) / 2;
  add_to_gui_list(ID_CITY_DLG_RESOURCE_MAP, pBuf);  
  /* -------- */
  
  pBuf = create_themeicon(pTheme->Options_Icon, pWindow->dst,
                        (WF_WIDGET_HAS_INFO_LABEL |
                         WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Options panel"), adj_font(12));
  pBuf->action = options_city_dlg_callback;
  pBuf->size.x =
    pWindow->size.x + adj_size(5) + 5 * ((adj_size(207) - 5 * pBuf->size.w) / 6) + 4 * pBuf->size.w;
  pBuf->size.y = pWindow->size.y + WINDOW_TITLE_HEIGHT + adj_size(7);
  if (pOwner == game.player_ptr) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  add_to_gui_list(ID_CITY_DLG_OPTIONS_BUTTON, pBuf);
  /* -------- */

  pBuf = create_themeicon(pTheme->PROD_Icon, pWindow->dst,
                        (WF_WIDGET_HAS_INFO_LABEL |
                         WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Change Production"), adj_font(12));
  pBuf->action = change_prod_dlg_callback;
  pBuf->size.x = pWindow->size.x + adj_size(10);
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(9);
  if (pOwner == game.player_ptr) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  pBuf->key = SDLK_c;
  add_to_gui_list(ID_CITY_DLG_CHANGE_PROD_BUTTON, pBuf);
  /* -------- */

  pBuf = create_themeicon(pTheme->Buy_PROD_Icon, pWindow->dst,
                        (WF_WIDGET_HAS_INFO_LABEL |
                         WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Hurry production"), adj_font(12));
  pBuf->action = buy_prod_city_dlg_callback;
  pBuf->size.x = pWindow->size.x + adj_size(10) + (pBuf->size.w + 2);
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(9);
  pCityDlg->pBuy_Button = pBuf;
  pBuf->key = SDLK_h;
  if ((pOwner == game.player_ptr) && (!pCity->did_buy)) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  add_to_gui_list(ID_CITY_DLG_PROD_BUY_BUTTON, pBuf);
  /* -------- */

  pBuf = create_themeicon(pTheme->CMA_Icon, pWindow->dst,
                        (WF_WIDGET_HAS_INFO_LABEL |
                         WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Citizen Governor"), adj_font(12));
  pBuf->action = cma_city_dlg_callback;
  pBuf->key = SDLK_a;
  pBuf->size.x = pWindow->size.x + adj_size(10) + (pBuf->size.w + adj_size(2)) * 2;
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(9);
  if (pOwner == game.player_ptr) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  add_to_gui_list(ID_CITY_DLG_CMA_BUTTON, pBuf);


  /* -------- */
  pBuf = create_themeicon(pTheme->L_ARROW_Icon, pWindow->dst,
                        (WF_WIDGET_HAS_INFO_LABEL |
                         WF_RESTORE_BACKGROUND));

  pBuf->string16 = create_str16_from_char(_("Prev city"), adj_font(12));
  pBuf->action = next_prev_city_dlg_callback;
  pBuf->size.x = pWindow->size.x + adj_size(220) - pBuf->size.w - adj_size(5);
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(2);
  if (pOwner == game.player_ptr) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  pBuf->key = SDLK_LEFT;
  pBuf->mod = KMOD_LSHIFT;
  add_to_gui_list(ID_CITY_DLG_PREV_BUTTON, pBuf);
  /* -------- */
  
  pBuf = create_themeicon(pTheme->R_ARROW_Icon, pWindow->dst,
                        (WF_WIDGET_HAS_INFO_LABEL |
                         WF_RESTORE_BACKGROUND));
  pBuf->string16 = create_str16_from_char(_("Next city"), adj_font(12));
  pBuf->action = next_prev_city_dlg_callback;
  pBuf->size.x = pWindow->size.x + adj_size(420) + adj_size(5);
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(2);
  if (pOwner == game.player_ptr) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }
  pBuf->key = SDLK_RIGHT;
  pBuf->mod = KMOD_LSHIFT;
  add_to_gui_list(ID_CITY_DLG_NEXT_BUTTON, pBuf);
  /* -------- */
  
  pBuf = create_edit_from_chars(NULL, pWindow->dst, pCity->name,
                              adj_font(10), adj_size(200), WF_RESTORE_BACKGROUND);
  pBuf->action = new_name_city_dlg_callback;
  pBuf->size.x = pWindow->size.x + (pWindow->size.w - pBuf->size.w) / 2;
  pBuf->size.y = pWindow->size.y + pWindow->size.h - pBuf->size.h - adj_size(5);
  if (pOwner == game.player_ptr) {
    set_wstate(pBuf, FC_WS_NORMAL);
  }

  pCityDlg->pCity_Name_Edit = pBuf;
  add_to_gui_list(ID_CITY_DLG_NAME_EDIT, pBuf);
  
  pCityDlg->pBeginCityWidgetList = pBuf;
  
  /* check if Citizen Icons style was loaded */
  cs = get_city_style(pCity);

  if (cs != pIcons->style) {
    reload_citizens_icons(cs);
  }

  /* ===================================================== */
  if ((city_unhappy(pCity) || city_celebrating(pCity)
       || city_happy(pCity))) {
    SDL_Client_Flags |= CF_CITY_STATUS_SPECIAL;
  }
  /* ===================================================== */

  redraw_city_dialog(pCity);
  flush_dirty();
}

/**************************************************************************
  Close the dialog for the given city.
**************************************************************************/
void popdown_city_dialog(struct city *pCity)
{
  if (city_dialog_is_open(pCity)) {
    del_city_dialog();
    
    flush_dirty();
	  
    SDL_Client_Flags &= ~CF_CITY_STATUS_SPECIAL;
    update_menus();
  }
}

/**************************************************************************
  Close all cities dialogs.
**************************************************************************/
void popdown_all_city_dialogs(void)
{
  if (pCityDlg) {
    popdown_city_dialog(pCityDlg->pCity);
  }
}

/**************************************************************************
  Refresh (update) all data for the given city's dialog.
**************************************************************************/
void refresh_city_dialog(struct city *pCity)
{
  if (city_dialog_is_open(pCity)) {
    redraw_city_dialog(pCityDlg->pCity);
    flush_dirty();
  }
}

/**************************************************************************
  Update city dialogs when the given unit's status changes.  This
  typically means updating both the unit's home city (if any) and the
  city in which it is present (if any).
**************************************************************************/
void refresh_unit_city_dialogs(struct unit *pUnit)
{

  struct city *pCity_sup = find_city_by_id(pUnit->homecity);
  struct city *pCity_pre = tile_get_city(pUnit->tile);

  if (pCityDlg && ((pCityDlg->pCity == pCity_sup)
		   || (pCityDlg->pCity == pCity_pre))) {
    free_city_units_lists();
    redraw_city_dialog(pCityDlg->pCity);
    flush_dirty();
  }

}

/**************************************************************************
  Return whether the dialog for the given city is open.
**************************************************************************/
bool city_dialog_is_open(struct city *pCity)
{
  return (pCityDlg && (pCityDlg->pCity == pCity));
}
