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
                          gui_main.h  -  description
                             -------------------
    begin                : Sep 6 2002
    copyright            : (C) 2002 by Rafa� Bursig
    email                : Rafa� Bursig <bursig@poczta.fm>
 ***************************************************************************/

#ifndef FC__GUI_MAIN_H
#define FC__GUI_MAIN_H

#include "gui_main_g.h"

/* #define DEBUG_SDL */

/* SDL client Flags */
#define CF_NONE				0
#define CF_ORDERS_WIDGETS_CREATED	(1<<1)
#define CF_MAP_UNIT_W_CREATED		(1<<2)
#define CF_UNIT_INFO_SHOW		(1<<3)
#define CF_MINI_MAP_SHOW		(1<<4)
#define CF_OPTION_OPEN			(1<<5)
#define CF_OPTION_MAIN			(1<<6)
#define CF_GANE_JUST_STARTED		(1<<7)
#define CF_REVOLUTION			(1<<8)
#define CF_TOGGLED_FULLSCREEN		(1<<9)
#define CF_FOCUS_ANIMATION		(1<<10)
#define CF_CHANGED_PROD			(1<<11)
#define CF_CHANGED_CITY_NAME		(1<<12)
#define CF_CITY_STATUS_SPECIAL		(1<<13)
#define CF_CHANGE_TAXRATE_LUX_BLOCK	(1<<14)
#define CF_CHANGE_TAXRATE_SCI_BLOCK	(1<<15)
#define CF_CIV3_CITY_TEXT_STYLE	(1<<16)
#define CF_DRAW_MAP_DITHER		(1<<17)
#define CF_DRAW_CITY_GRID		(1<<18)
#define CF_DRAW_CITY_WORKER_GRID	(1<<19)

extern struct canvas_store Main;
extern struct GUI *pSellected_Widget;
extern Uint32 SDL_Client_Flags;
bool LSHIFT;
bool LCTRL;
bool LALT;

void add_autoconnect_to_timer(void);
void enable_focus_animation(void);
void disable_focus_animation(void);

#endif	/* FC__GUI_MAIN_H */
