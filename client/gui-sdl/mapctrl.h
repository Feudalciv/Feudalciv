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
                          mapctrl.h  -  description
                             -------------------
    begin                : Thu Sep 05 2002
    copyright            : (C) 2002 by Rafa� Bursig
    email                : Rafa� Bursig <bursig@poczta.fm>
 *********************************************************************/

#ifndef FC__MAPCTRL_H
#define FC__MAPCTRL_H

#include "mapctrl_g.h"

struct unit;

void popdown_newcity_dialog(void);

void Init_MapView(void);
void Remake_MiniMap(int w, int h);
void reset_main_widget_dest_buffer(void);
void set_new_units_window_pos(void);
void set_new_mini_map_window_pos(void);
struct GUI * get_unit_info_window_widget(void);
struct GUI * get_minimap_window_widget(void);
struct GUI * get_tax_rates_widget(void);
struct GUI * get_research_widget(void);
struct GUI * get_revolution_widget(void);
void enable_and_redraw_find_city_button(void);
void enable_and_redraw_revolution_button(void);

bool map_event_handler(SDL_keysym Key);
void button_down_on_map(SDL_MouseButtonEvent * pButtonEvent);

#endif	/* FC__MAPCTRL_H */
