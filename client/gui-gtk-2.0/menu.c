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

#include "astring.h"
#include "fcintl.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "support.h"
#include "unit.h"

#include "chatline.h"
#include "cityrep.h"
#include "civclient.h"
#include "clinet.h"
#include "control.h"
#include "dialogs.h"
#include "finddlg.h"
#include "gotodlg.h"
#include "gui_stuff.h"
#include "helpdlg.h"
#include "mapctrl.h"   /* center_on_unit */
#include "messagedlg.h"
#include "messagewin.h"
#include "optiondlg.h"
#include "options.h"
#include "plrdlg.h"
#include "ratesdlg.h"
#include "repodlgs.h"
#include "spaceshipdlg.h"
#include "wldlg.h"

#include "menu.h"

static GtkItemFactory *item_factory = NULL;
GtkAccelGroup *toplevel_accel = NULL;
static enum unit_activity road_activity;

static void menus_rename(const char *path, char *s);

/****************************************************************
...
*****************************************************************/
enum MenuID {
  MENU_END_OF_LIST=0,

  MENU_GAME_OPTIONS,
  MENU_GAME_MSG_OPTIONS,
  MENU_GAME_SAVE_SETTINGS,
  MENU_GAME_SERVER_OPTIONS1,
  MENU_GAME_SERVER_OPTIONS2,
  MENU_GAME_OUTPUT_LOG,
  MENU_GAME_CLEAR_OUTPUT,
  MENU_GAME_DISCONNECT,
  MENU_GAME_QUIT,
  
  MENU_KINGDOM_TAX_RATE,
  MENU_KINGDOM_FIND_CITY,
  MENU_KINGDOM_WORKLISTS,
  MENU_KINGDOM_REVOLUTION,

  MENU_VIEW_SHOW_MAP_GRID,
  MENU_VIEW_SHOW_CITY_NAMES,
  MENU_VIEW_SHOW_CITY_PRODUCTIONS,
  MENU_VIEW_SHOW_TERRAIN,
  MENU_VIEW_SHOW_COASTLINE,
  MENU_VIEW_SHOW_ROADS_RAILS,
  MENU_VIEW_SHOW_IRRIGATION,
  MENU_VIEW_SHOW_MINES,
  MENU_VIEW_SHOW_FORTRESS_AIRBASE,
  MENU_VIEW_SHOW_SPECIALS,
  MENU_VIEW_SHOW_POLLUTION,
  MENU_VIEW_SHOW_CITIES,
  MENU_VIEW_SHOW_UNITS,
  MENU_VIEW_SHOW_FOCUS_UNIT,
  MENU_VIEW_SHOW_FOG_OF_WAR,
  MENU_VIEW_CENTER_VIEW,

  MENU_ORDER_BUILD_CITY,     /* shared with BUILD_WONDER */
  MENU_ORDER_ROAD,           /* shared with TRADEROUTE */
  MENU_ORDER_IRRIGATE,
  MENU_ORDER_MINE,
  MENU_ORDER_TRANSFORM,
  MENU_ORDER_FORTRESS,       /* shared with FORTIFY */
  MENU_ORDER_AIRBASE,
  MENU_ORDER_POLLUTION,      /* shared with PARADROP */
  MENU_ORDER_FALLOUT,
  MENU_ORDER_SENTRY,
  MENU_ORDER_PILLAGE,
  MENU_ORDER_HOMECITY,
  MENU_ORDER_UNLOAD,
  MENU_ORDER_WAKEUP_OTHERS,
  MENU_ORDER_AUTO_SETTLER,   /* shared with AUTO_ATTACK */
  MENU_ORDER_AUTO_EXPLORE,
  MENU_ORDER_CONNECT,
  MENU_ORDER_PATROL,
  MENU_ORDER_GOTO,
  MENU_ORDER_GOTO_CITY,
  MENU_ORDER_DISBAND,
  MENU_ORDER_DIPLOMAT_DLG,
  MENU_ORDER_NUKE,
  MENU_ORDER_WAIT,
  MENU_ORDER_DONE,

  MENU_REPORT_CITIES,
  MENU_REPORT_UNITS,
  MENU_REPORT_PLAYERS,
  MENU_REPORT_ECONOMY,
  MENU_REPORT_SCIENCE,
  MENU_REPORT_WOW,
  MENU_REPORT_TOP_CITIES,
  MENU_REPORT_MESSAGES,
  MENU_REPORT_DEMOGRAPHIC,
  MENU_REPORT_SPACESHIP,

  MENU_HELP_LANGUAGES,
  MENU_HELP_CONNECTING,
  MENU_HELP_CONTROLS,
  MENU_HELP_CHATLINE,
  MENU_HELP_WORKLIST_EDITOR,
  MENU_HELP_CMA,
  MENU_HELP_PLAYING,
  MENU_HELP_IMPROVEMENTS,
  MENU_HELP_UNITS,
  MENU_HELP_COMBAT,
  MENU_HELP_ZOC,
  MENU_HELP_TECH,
  MENU_HELP_TERRAIN,
  MENU_HELP_WONDERS,
  MENU_HELP_GOVERNMENT,
  MENU_HELP_HAPPINESS,
  MENU_HELP_SPACE_RACE,
  MENU_HELP_COPYING,
  MENU_HELP_ABOUT
};


/****************************************************************
...
*****************************************************************/
static void game_menu_callback(gpointer callback_data,
			       guint callback_action, GtkWidget *widget)
{
  switch(callback_action) {
  case MENU_GAME_OPTIONS:
    popup_option_dialog();
    break;
  case MENU_GAME_MSG_OPTIONS:
    popup_messageopt_dialog();
    break;
  case MENU_GAME_SAVE_SETTINGS:
    save_options();
    break;
  case MENU_GAME_SERVER_OPTIONS1:
    send_report_request(REPORT_SERVER_OPTIONS1);
    break;
  case MENU_GAME_SERVER_OPTIONS2:
    send_report_request(REPORT_SERVER_OPTIONS2);
    break;
  case MENU_GAME_OUTPUT_LOG:
    log_output_window();
    break;
  case MENU_GAME_CLEAR_OUTPUT:
    clear_output_window();
    break;
  case MENU_GAME_DISCONNECT:
    disconnect_from_server();
    break;
  case MENU_GAME_QUIT:
    exit(EXIT_SUCCESS);
  }
}


/****************************************************************
...
*****************************************************************/
static void kingdom_menu_callback(gpointer callback_data,
				  guint callback_action, GtkWidget *widget)
{
  switch(callback_action) {
  case MENU_KINGDOM_TAX_RATE:
    popup_rates_dialog();
    break;
  case MENU_KINGDOM_FIND_CITY:
    popup_find_dialog();
    break;
  case MENU_KINGDOM_WORKLISTS:
    popup_worklists_report(game.player_ptr);
    break;
  case MENU_KINGDOM_REVOLUTION:
    popup_revolution_dialog();
    break;
  }
}


static void menus_set_sensitive(const char *, int);
/****************************************************************
...
*****************************************************************/
static void view_menu_callback(gpointer callback_data, guint callback_action,
			       GtkWidget *widget)
{
  switch(callback_action) {
  case MENU_VIEW_SHOW_MAP_GRID:
    if (draw_map_grid ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_map_grid_toggle();
    break;
  case MENU_VIEW_SHOW_CITY_NAMES:
    if (draw_city_names ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_city_names_toggle();
    break;
  case MENU_VIEW_SHOW_CITY_PRODUCTIONS:
    if (draw_city_productions ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_city_productions_toggle();
    break;
  case MENU_VIEW_SHOW_TERRAIN:
    if (draw_terrain ^ GTK_CHECK_MENU_ITEM(widget)->active) {
      key_terrain_toggle();
      menus_set_sensitive("<main>/View/Coastline", !draw_terrain);
    }
    break;
  case MENU_VIEW_SHOW_COASTLINE:
    if (draw_coastline ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_coastline_toggle();
    break;
  case MENU_VIEW_SHOW_ROADS_RAILS:
    if (draw_roads_rails ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_roads_rails_toggle();
    break;
  case MENU_VIEW_SHOW_IRRIGATION:
    if (draw_irrigation ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_irrigation_toggle();
    break;
  case MENU_VIEW_SHOW_MINES:
    if (draw_mines ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_mines_toggle();
    break;
  case MENU_VIEW_SHOW_FORTRESS_AIRBASE:
    if (draw_fortress_airbase ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_fortress_airbase_toggle();
    break;
  case MENU_VIEW_SHOW_SPECIALS:
    if (draw_specials ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_specials_toggle();
    break;
  case MENU_VIEW_SHOW_POLLUTION:
    if (draw_pollution ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_pollution_toggle();
    break;
  case MENU_VIEW_SHOW_CITIES:
    if (draw_cities ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_cities_toggle();
    break;
  case MENU_VIEW_SHOW_UNITS:
    if (draw_units ^ GTK_CHECK_MENU_ITEM(widget)->active) {
      key_units_toggle();
      menus_set_sensitive("<main>/View/Focus Unit", !draw_units);
    }
    break;
  case MENU_VIEW_SHOW_FOCUS_UNIT:
    if (draw_focus_unit ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_focus_unit_toggle();
    break;
  case MENU_VIEW_SHOW_FOG_OF_WAR:
    if (draw_fog_of_war ^ GTK_CHECK_MENU_ITEM(widget)->active)
      key_fog_of_war_toggle();
    break;
  case MENU_VIEW_CENTER_VIEW:
    center_on_unit();
    break;
  }
}


/****************************************************************
...
*****************************************************************/
static void orders_menu_callback(gpointer callback_data,
				 guint callback_action, GtkWidget *widget)
{
  switch(callback_action) {
   case MENU_ORDER_BUILD_CITY:
    if (get_unit_in_focus()) {
      struct unit *punit = get_unit_in_focus();
      /* Enable the button for adding to a city in all cases, so we
	 get an eventual error message from the server if we try. */
      if (can_unit_add_or_build_city(punit)) {
	key_unit_build_city();
      } else {
	key_unit_build_wonder();
      }
    }
    break;
   case MENU_ORDER_ROAD:
    if (get_unit_in_focus()) {
      if (unit_can_est_traderoute_here(get_unit_in_focus()))
	key_unit_traderoute();
      else
	key_unit_road();
    }
    break;
   case MENU_ORDER_IRRIGATE:
    key_unit_irrigate();
    break;
   case MENU_ORDER_MINE:
    key_unit_mine();
    break;
   case MENU_ORDER_TRANSFORM:
    key_unit_transform();
    break;
   case MENU_ORDER_FORTRESS:
    if (get_unit_in_focus()) {
      if (can_unit_do_activity(get_unit_in_focus(), ACTIVITY_FORTRESS))
	key_unit_fortress();
      else
	key_unit_fortify();
    }
    break;
   case MENU_ORDER_AIRBASE:
    key_unit_airbase(); 
    break;
   case MENU_ORDER_POLLUTION:
    if (get_unit_in_focus()) {
      if (can_unit_paradrop(get_unit_in_focus()))
	key_unit_paradrop();
      else
	key_unit_pollution();
    }
    break;
   case MENU_ORDER_FALLOUT:
    key_unit_fallout();
    break;
   case MENU_ORDER_SENTRY:
    key_unit_sentry();
    break;
   case MENU_ORDER_PILLAGE:
    key_unit_pillage();
    break;
   case MENU_ORDER_HOMECITY:
    key_unit_homecity();
    break;
   case MENU_ORDER_UNLOAD:
    key_unit_unload();
    break;
   case MENU_ORDER_WAKEUP_OTHERS:
    key_unit_wakeup_others();
    break;
   case MENU_ORDER_AUTO_SETTLER:
    if(get_unit_in_focus())
      request_unit_auto(get_unit_in_focus());
    break;
   case MENU_ORDER_AUTO_EXPLORE:
    key_unit_auto_explore();
    break;
   case MENU_ORDER_CONNECT:
    key_unit_connect();
    break;
   case MENU_ORDER_PATROL:
    key_unit_patrol();
    break;
   case MENU_ORDER_GOTO:
    key_unit_goto();
    break;
   case MENU_ORDER_GOTO_CITY:
    if(get_unit_in_focus())
      popup_goto_dialog();
    break;
   case MENU_ORDER_DISBAND:
    key_unit_disband();
    break;
   case MENU_ORDER_DIPLOMAT_DLG:
    key_unit_diplomat_actions();
    break;
   case MENU_ORDER_NUKE:
    key_unit_nuke();
    break;
   case MENU_ORDER_WAIT:
    key_unit_wait();
    break;
   case MENU_ORDER_DONE:
    key_unit_done();
    break;
  }
}


/****************************************************************
...
*****************************************************************/
static void reports_menu_callback(gpointer callback_data,
				  guint callback_action, GtkWidget *widget)
{
  switch(callback_action) {
   case MENU_REPORT_CITIES:
    popup_city_report_dialog(0);
    break;
   case MENU_REPORT_UNITS:
    popup_activeunits_report_dialog(0);
    break;
  case MENU_REPORT_PLAYERS:
    popup_players_dialog();
    break;
   case MENU_REPORT_ECONOMY:
    popup_economy_report_dialog(0);
    break;
   case MENU_REPORT_SCIENCE:
    popup_science_dialog(0);
    break;
   case MENU_REPORT_WOW:
    send_report_request(REPORT_WONDERS_OF_THE_WORLD);
    break;
   case MENU_REPORT_TOP_CITIES:
    send_report_request(REPORT_TOP_5_CITIES);
    break;
  case MENU_REPORT_MESSAGES:
    popup_meswin_dialog();
    break;
   case MENU_REPORT_DEMOGRAPHIC:
    send_report_request(REPORT_DEMOGRAPHIC);
    break;
   case MENU_REPORT_SPACESHIP:
    popup_spaceship_dialog(game.player_ptr);
    break;
  }
}


/****************************************************************
...
*****************************************************************/
static void help_menu_callback(gpointer callback_data,
			       guint callback_action, GtkWidget *widget)
{
  switch(callback_action) {
  case MENU_HELP_LANGUAGES:
    popup_help_dialog_string(HELP_LANGUAGES_ITEM);
    break;
  case MENU_HELP_CONNECTING:
    popup_help_dialog_string(HELP_CONNECTING_ITEM);
    break;
  case MENU_HELP_CONTROLS:
    popup_help_dialog_string(HELP_CONTROLS_ITEM);
    break;
  case MENU_HELP_CHATLINE:
    popup_help_dialog_string(HELP_CHATLINE_ITEM);
    break;
  case MENU_HELP_WORKLIST_EDITOR:
    popup_help_dialog_string(HELP_WORKLIST_EDITOR_ITEM);
    break;
  case MENU_HELP_CMA:
    popup_help_dialog_string(HELP_CMA_ITEM);
    break;
  case MENU_HELP_PLAYING:
    popup_help_dialog_string(HELP_PLAYING_ITEM);
    break;
  case MENU_HELP_IMPROVEMENTS:
    popup_help_dialog_string(HELP_IMPROVEMENTS_ITEM);
    break;
  case MENU_HELP_UNITS:
    popup_help_dialog_string(HELP_UNITS_ITEM);
    break;
  case MENU_HELP_COMBAT:
    popup_help_dialog_string(HELP_COMBAT_ITEM);
    break;
  case MENU_HELP_ZOC:
    popup_help_dialog_string(HELP_ZOC_ITEM);
    break;
  case MENU_HELP_TECH:
    popup_help_dialog_string(HELP_TECHS_ITEM);
    break;
  case MENU_HELP_TERRAIN:
    popup_help_dialog_string(HELP_TERRAIN_ITEM);
    break;
  case MENU_HELP_WONDERS:
    popup_help_dialog_string(HELP_WONDERS_ITEM);
    break;
  case MENU_HELP_GOVERNMENT:
    popup_help_dialog_string(HELP_GOVERNMENT_ITEM);
    break;
  case MENU_HELP_HAPPINESS:
    popup_help_dialog_string(HELP_HAPPINESS_ITEM);
    break;
  case MENU_HELP_SPACE_RACE:
    popup_help_dialog_string(HELP_SPACE_RACE_ITEM);
    break;
  case MENU_HELP_COPYING:
    popup_help_dialog_string(HELP_COPYING_ITEM);
    break;
  case MENU_HELP_ABOUT:
    popup_help_dialog_string(HELP_ABOUT_ITEM);
    break;
  }
}

/* This is the GtkItemFactoryEntry structure used to generate new menus.
          Item 1: The menu path. The letter after the underscore indicates an
                  accelerator key once the menu is open.
          Item 2: The accelerator key for the entry
          Item 3: The callback function.
          Item 4: The callback action.  This changes the parameters with
                  which the function is called.  The default is 0.
          Item 5: The item type, used to define what kind of an item it is.
                  Here are the possible values:

                  NULL               -> "<Item>"
                  ""                 -> "<Item>"
                  "<Title>"          -> create a title item
                  "<Item>"           -> create a simple item
                  "<CheckItem>"      -> create a check item
                  "<ToggleItem>"     -> create a toggle item
                  "<RadioItem>"      -> create a radio item
                  <path>             -> path of a radio item to link against
                  "<Separator>"      -> create a separator
                  "<Branch>"         -> create an item to hold sub items
                  "<LastBranch>"     -> create a right justified branch 

Important: The underscore is NOT just for show (see Item 1 above)!
           At the top level, use with "Alt" key to open the menu.
	   Some less often used commands in the Order menu are not underscored
	   due to possible conflicts.
*/
static GtkItemFactoryEntry menu_items[]	=
{
  /* Game menu ... */
  { "/" N_("_Game"),					NULL,
	NULL,			0,					"<Branch>"	},
  { "/" N_("Game") "/tearoff1",				NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("Game") "/sep1",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Game") "/" N_("_Local Options"),		NULL,
	game_menu_callback,	MENU_GAME_OPTIONS					},
  { "/" N_("Game") "/" N_("Messa_ge Options"),		NULL,
	game_menu_callback,	MENU_GAME_MSG_OPTIONS					},
  { "/" N_("Game") "/" N_("_Save Settings"),		NULL,
	game_menu_callback,	MENU_GAME_SAVE_SETTINGS					},
  { "/" N_("Game") "/sep2",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Game") "/" N_("Server Opt _initial"),	NULL,
	game_menu_callback,	MENU_GAME_SERVER_OPTIONS1				},
  { "/" N_("Game") "/" N_("Server Opt _ongoing"),	NULL,
	game_menu_callback,	MENU_GAME_SERVER_OPTIONS2				},
  { "/" N_("Game") "/sep3",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Game") "/" N_("_Export Log"),		NULL,
	game_menu_callback,	MENU_GAME_OUTPUT_LOG					},
  { "/" N_("Game") "/" N_("_Clear Log"),		NULL,
	game_menu_callback,	MENU_GAME_CLEAR_OUTPUT					},
  { "/" N_("Game") "/sep4",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Game") "/" N_("_Disconnect"),		NULL,
	game_menu_callback,	MENU_GAME_DISCONNECT					},
  { "/" N_("Game") "/" N_("_Quit"),			"<control>q",
	gtk_main_quit,		0							},
  /* Kingdom menu ... */
  { "/" N_("_Kingdom"),					NULL,
	NULL,			0,					"<Branch>"	},
  { "/" N_("Kingdom") "/tearoff1",			NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("Kingdom") "/" N_("_Tax Rates"),		"<shift>t",
	kingdom_menu_callback,	MENU_KINGDOM_TAX_RATE					},
  { "/" N_("Kingdom") "/sep1",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Kingdom") "/" N_("_Find City"),		"<shift>f",
	kingdom_menu_callback,	MENU_KINGDOM_FIND_CITY					},
  { "/" N_("Kingdom") "/" N_("Work_lists"),		"<shift>l",
	kingdom_menu_callback,	MENU_KINGDOM_WORKLISTS					},
  { "/" N_("Kingdom") "/sep2",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Kingdom") "/" N_("_Revolution"),	        "<shift>r",
	kingdom_menu_callback,	MENU_KINGDOM_REVOLUTION					},
  /* View menu ... */
  { "/" N_("_View"),					NULL,
	NULL,			0,					"<Branch>"	},
  { "/" N_("View") "/tearoff1",				NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("View") "/" N_("Map _Grid"),			"<control>g",
	view_menu_callback,	MENU_VIEW_SHOW_MAP_GRID,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("City _Names"),		"<control>n",
	view_menu_callback,	MENU_VIEW_SHOW_CITY_NAMES,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("City _Productions"),		"<control>p",
	view_menu_callback,	MENU_VIEW_SHOW_CITY_PRODUCTIONS,	"<CheckItem>"	},
  { "/" N_("View") "/sep1",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("View") "/" N_("Terrain"),                   NULL,
        view_menu_callback,     MENU_VIEW_SHOW_TERRAIN,	                "<CheckItem>"   },
  { "/" N_("View") "/" N_("Coastline"),	                NULL,
        view_menu_callback,     MENU_VIEW_SHOW_COASTLINE,       	"<CheckItem>"   },
  { "/" N_("View") "/" N_("Improvements"),		NULL,
	NULL,			0,					"<Branch>"	},
  { "/" N_("View") "/" N_("Improvements") "/tearoff1",	NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("View") "/" N_("Improvements") "/" N_("Roads & Rails"), NULL,
	view_menu_callback,	MENU_VIEW_SHOW_ROADS_RAILS,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("Improvements") "/" N_("Irrigation"), NULL,
	view_menu_callback,	MENU_VIEW_SHOW_IRRIGATION,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("Improvements") "/" N_("Mines"),	NULL,
	view_menu_callback,	MENU_VIEW_SHOW_MINES,			"<CheckItem>"	},
  { "/" N_("View") "/" N_("Improvements") "/" N_("Fortress & Airbase"), NULL,
	view_menu_callback,	MENU_VIEW_SHOW_FORTRESS_AIRBASE,	"<CheckItem>"	},
  { "/" N_("View") "/" N_("Specials"),			NULL,
	view_menu_callback,	MENU_VIEW_SHOW_SPECIALS,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("Pollution & Fallout"),	NULL,
	view_menu_callback,	MENU_VIEW_SHOW_POLLUTION,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("Cities"),			NULL,
	view_menu_callback,	MENU_VIEW_SHOW_CITIES,			"<CheckItem>"	},
  { "/" N_("View") "/" N_("Units"),			NULL,
	view_menu_callback,	MENU_VIEW_SHOW_UNITS,			"<CheckItem>"	},
  { "/" N_("View") "/" N_("Focus Unit"),		NULL,
	view_menu_callback,	MENU_VIEW_SHOW_FOCUS_UNIT,		"<CheckItem>"	},
  { "/" N_("View") "/" N_("Fog of War"),		NULL,
	view_menu_callback,	MENU_VIEW_SHOW_FOG_OF_WAR,		"<CheckItem>"	},
  { "/" N_("View") "/sep2",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("View") "/" N_("_Center View"),		"c",
	view_menu_callback,	MENU_VIEW_CENTER_VIEW					},
  /* Orders menu ... */
  { "/" N_("_Orders"),					NULL,
	NULL,			0,					"<Branch>"	},
  { "/" N_("Orders") "/tearoff1",			NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("Orders") "/" N_("_Build City"),		"b",
	orders_menu_callback,	MENU_ORDER_BUILD_CITY					},
  { "/" N_("Orders") "/" N_("Build _Road"),		"r",
	orders_menu_callback,	MENU_ORDER_ROAD						},
  { "/" N_("Orders") "/" N_("Build _Irrigation"),	"i",
	orders_menu_callback,	MENU_ORDER_IRRIGATE					},
  { "/" N_("Orders") "/" N_("Build _Mine"),		"m",
	orders_menu_callback,	MENU_ORDER_MINE						},
  { "/" N_("Orders") "/" N_("Transf_orm Terrain"),	"o",
	orders_menu_callback,	MENU_ORDER_TRANSFORM					},
  { "/" N_("Orders") "/" N_("Build _Fortress"),		"f",
	orders_menu_callback,	MENU_ORDER_FORTRESS					},
  { "/" N_("Orders") "/" N_("Build Airbas_e"),		"e",
	orders_menu_callback,	MENU_ORDER_AIRBASE					},
  { "/" N_("Orders") "/" N_("Clean _Pollution"),	"p",
	orders_menu_callback,	MENU_ORDER_POLLUTION					},
  { "/" N_("Orders") "/" N_("Clean _Nuclear Fallout"),	"n",
	orders_menu_callback,	MENU_ORDER_FALLOUT					},
  { "/" N_("Orders") "/sep1",			NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Orders") "/" N_("_Sentry"),			"s",
	orders_menu_callback,	MENU_ORDER_SENTRY					},
  { "/" N_("Orders") "/" N_("Pillage"),		        "<shift>p",
	orders_menu_callback,	MENU_ORDER_PILLAGE					},
  { "/" N_("Orders") "/sep2",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Orders") "/" N_("Make _Homecity"),		"h",
	orders_menu_callback,	MENU_ORDER_HOMECITY					},
  { "/" N_("Orders") "/" N_("_Unload"),			"u",
	orders_menu_callback,	MENU_ORDER_UNLOAD					},
  { "/" N_("Orders") "/" N_("Wake up o_thers"),		"<shift>w",
	orders_menu_callback,	MENU_ORDER_WAKEUP_OTHERS				},
  { "/" N_("Orders") "/sep3",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Orders") "/" N_("_Auto Settler"),		"a",
	orders_menu_callback,	MENU_ORDER_AUTO_SETTLER					},
  { "/" N_("Orders") "/" N_("Auto E_xplore"),		"x",
	orders_menu_callback,	MENU_ORDER_AUTO_EXPLORE					},
  { "/" N_("Orders") "/" N_("_Connect"),		"<shift>c",
	orders_menu_callback,	MENU_ORDER_CONNECT					},
  { "/" N_("Orders") "/" N_("Patrol (_Q)"),		"q",
	orders_menu_callback,	MENU_ORDER_PATROL					},
  { "/" N_("Orders") "/" N_("_Go to"),			"g",
	orders_menu_callback,	MENU_ORDER_GOTO						},
  { "/" N_("Orders") "/" N_("Go\\/Airlift to City"),	"l",
	orders_menu_callback,	MENU_ORDER_GOTO_CITY					},
  { "/" N_("Orders") "/sep4",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Orders") "/" N_("Disband Unit"),		"<shift>d",
	orders_menu_callback,	MENU_ORDER_DISBAND					},
  { "/" N_("Orders") "/" N_("Diplomat\\/Spy Actions"),	"d",
	orders_menu_callback,	MENU_ORDER_DIPLOMAT_DLG					},
  { "/" N_("Orders") "/" N_("Explode Nuclear"),        "<shift>n",
	orders_menu_callback,	MENU_ORDER_NUKE						},
  { "/" N_("Orders") "/sep5",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Orders") "/" N_("_Wait"),			"w",
	orders_menu_callback,	MENU_ORDER_WAIT						},
  { "/" N_("Orders") "/" N_("Done"),			"space",
	orders_menu_callback,	MENU_ORDER_DONE						},
  /* Reports menu ... */
  { "/" N_("_Reports"),					NULL,
	NULL,			0,					"<Branch>"	},
  { "/" N_("Reports") "/tearoff1",			NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("Reports") "/" N_("_Cities"),		"F1",
	reports_menu_callback,	MENU_REPORT_CITIES					},
  { "/" N_("Reports") "/" N_("_Units"),			"F2",
	reports_menu_callback,	MENU_REPORT_UNITS					},
  { "/" N_("Reports") "/" N_("_Players"),		"F3",
	reports_menu_callback,	MENU_REPORT_PLAYERS					},
  { "/" N_("Reports") "/" N_("_Economy"),		"F5",
	reports_menu_callback,	MENU_REPORT_ECONOMY					},
  { "/" N_("Reports") "/" N_("_Science"),		"F6",
	reports_menu_callback,	MENU_REPORT_SCIENCE					},
  { "/" N_("Reports") "/sep1",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Reports") "/" N_("_Wonders of the World"),	"F7",
	reports_menu_callback,	MENU_REPORT_WOW						},
  { "/" N_("Reports") "/" N_("_Top Five Cities"),	"F8",
	reports_menu_callback,	MENU_REPORT_TOP_CITIES					},
  { "/" N_("Reports") "/" N_("_Messages"),		"F9",
	reports_menu_callback,	MENU_REPORT_MESSAGES					},
  { "/" N_("Reports") "/" N_("_Demographics"),		"F11",
	reports_menu_callback,	MENU_REPORT_DEMOGRAPHIC					},
  { "/" N_("Reports") "/" N_("S_paceship"),		"F12",
	reports_menu_callback,	MENU_REPORT_SPACESHIP					},
  /* Help menu ... */
  { "/" N_("_Help"),					NULL,
	NULL,			0,					"<LastBranch>"	},
  { "/" N_("Help") "/tearoff1",				NULL,
	NULL,			0,					"<Tearoff>"	},
  { "/" N_("Help") "/" N_("Language_s"),		NULL,
	help_menu_callback,	MENU_HELP_LANGUAGES					},
  { "/" N_("Help") "/" N_("Co_nnecting"),		NULL,
	help_menu_callback,	MENU_HELP_CONNECTING					},
  { "/" N_("Help") "/" N_("C_ontrols"),			NULL,
	help_menu_callback,	MENU_HELP_CONTROLS					},
  { "/" N_("Help") "/" N_("C_hatline"),			NULL,
	help_menu_callback,	MENU_HELP_CHATLINE					},
  { "/" N_("Help") "/" N_("_Worklist Editor"),			NULL,
	help_menu_callback,	MENU_HELP_WORKLIST_EDITOR				},
  { "/" N_("Help") "/" N_("Citizen _Management"),			NULL,
	help_menu_callback,	MENU_HELP_CMA						},
  { "/" N_("Help") "/" N_("_Playing"),			NULL,
	help_menu_callback,	MENU_HELP_PLAYING					},
  { "/" N_("Help") "/sep1",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Help") "/" N_("City _Improvements"),        NULL,
	help_menu_callback,	MENU_HELP_IMPROVEMENTS					},
  { "/" N_("Help") "/" N_("_Units"),			NULL,
	help_menu_callback,	MENU_HELP_UNITS						},
  { "/" N_("Help") "/" N_("Com_bat"),			NULL,
	help_menu_callback,	MENU_HELP_COMBAT					},
  { "/" N_("Help") "/" N_("_ZOC"),			NULL,
	help_menu_callback,	MENU_HELP_ZOC						},
  { "/" N_("Help") "/" N_("Techno_logy"),		NULL,
	help_menu_callback,	MENU_HELP_TECH						},
  { "/" N_("Help") "/" N_("_Terrain"),			NULL,
	help_menu_callback,	MENU_HELP_TERRAIN					},
  { "/" N_("Help") "/" N_("Won_ders"),			NULL,
	help_menu_callback,	MENU_HELP_WONDERS					},
  { "/" N_("Help") "/" N_("_Government"),		NULL,
	help_menu_callback,	MENU_HELP_GOVERNMENT					},
  { "/" N_("Help") "/" N_("Happin_ess"),		NULL,
	help_menu_callback,	MENU_HELP_HAPPINESS					},
  { "/" N_("Help") "/" N_("Space _Race"),		NULL,
	help_menu_callback,	MENU_HELP_SPACE_RACE					},
  { "/" N_("Help") "/sep2",				NULL,
	NULL,			0,					"<Separator>"	},
  { "/" N_("Help") "/" N_("_Copying"),			NULL,
	help_menu_callback,	MENU_HELP_COPYING					},
  { "/" N_("Help") "/" N_("_About"),			NULL,
	help_menu_callback,	MENU_HELP_ABOUT						}
};


/****************************************************************
  gettext-translates each "/" delimited component of menu path,
  puts them back together, and returns as a static string.
  Any component which is of form "<foo>" is _not_ translated.

  Path should include underscores like in the menu itself.
*****************************************************************/
static const char *translate_menu_path(const char *path, int remove_uline)
{
#ifndef ENABLE_NLS
  static char res[100];
  strcpy(res, path);
#else
  static struct astring in, out, tmp;   /* these are never free'd */
  char *tok, *s, *trn, *t;
  int len;
  char *res;

  /* copy to in so can modify with strtok: */
  astr_minsize(&in, strlen(path)+1);
  strcpy(in.str, path);
  astr_minsize(&out, 1);
  out.str[0] = '\0';
  freelog(LOG_DEBUG, "trans: %s", in.str);

  s = in.str;
  while ((tok=strtok(s, "/"))) {
    len = strlen(tok);
    freelog(LOG_DEBUG, "tok \"%s\", len %d", tok, len);
    if (len && tok[0] == '<' && tok[len-1] == '>') {
      t = tok;
    } else {
      trn = _(tok);
      len = strlen(trn) + 1;	/* string plus leading '/' */
      astr_minsize(&tmp, len+1);
      sprintf(tmp.str, "/%s", trn);
      t = tmp.str;
      len = strlen(t);
    }
    astr_minsize(&out, out.n + len);
    strcat(out.str, t);
    freelog(LOG_DEBUG, "t \"%s\", len %d, out \"%s\"", t, len, out.str);
    s = NULL;
  }
  res = out.str;
#endif

  if (remove_uline) {
    char *from, *to;
    from = to = res;
    do {
      if (*from != '_') {
	*(to++) = *from;
      }
    } while (*(from++));
  }

  return res;
}

/****************************************************************
...
*****************************************************************/
void setup_menus(GtkWidget *window, GtkWidget **menubar)
{
  const int nmenu_items = ARRAY_SIZE(menu_items);
  int i;

  toplevel_accel = gtk_accel_group_new();
  item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>",toplevel_accel);
  gtk_accel_group_lock(toplevel_accel);
   
  for(i=0; i<nmenu_items; i++) {
    menu_items[i].path = mystrdup(translate_menu_path(menu_items[i].path, 0));
  }
  
  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);

  gtk_window_add_accel_group(GTK_WINDOW(window), toplevel_accel);

  if(menubar)
    *menubar=gtk_item_factory_get_widget(item_factory, "<main>");
}

/****************************************************************
...
*****************************************************************/
static void menus_set_sensitive(const char *path, int sensitive)
{
  GtkWidget *item;

  path = translate_menu_path(path, 1);
  
  if(!(item=gtk_item_factory_get_widget(item_factory, path))) {
    freelog(LOG_ERROR,
	    "Can't set sensitivity for non-existent menu %s.", path);
    return;
  }

  if(GTK_IS_MENU(item))
    item=gtk_menu_get_attach_widget(GTK_MENU(item));
  gtk_widget_set_sensitive(item, sensitive);
}

/****************************************************************
...
*****************************************************************/
static void menus_set_active(const char *path, int active)
{
  GtkWidget *item;

  path = translate_menu_path(path, 1);

  if (!(item = gtk_item_factory_get_widget(item_factory, path))) {
    freelog(LOG_ERROR,
	    "Can't set active for non-existent menu %s.", path);
    return;
  }

  if (GTK_IS_MENU(item))
    item = gtk_menu_get_attach_widget(GTK_MENU(item));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), active);
}

#ifdef UNUSED 
/****************************************************************
...
*****************************************************************/
static void menus_set_shown(const char *path, int shown)
{
  GtkWidget *item;
  
  path = translate_menu_path(path, 1);
  
  if(!(item=gtk_item_factory_get_widget(item_factory, path))) {
    freelog(LOG_ERROR, "Can't show non-existent menu %s.", path);
    return;
  }

  if(GTK_IS_MENU(item))
    item=gtk_menu_get_attach_widget(GTK_MENU(item));

  if(shown)
    gtk_widget_show(item);
  else
    gtk_widget_hide(item);
}
#endif /* UNUSED */

/****************************************************************
...
*****************************************************************/
static void menus_rename(const char *path, char *s)
{
  GtkWidget *item;
  
  path = translate_menu_path(path, 1);
  
  if(!(item=gtk_item_factory_get_widget(item_factory, path))) {
    freelog(LOG_ERROR, "Can't rename non-existent menu %s.", path);
    return;
  }

  if (GTK_IS_MENU(item))
    item=gtk_menu_get_attach_widget(GTK_MENU(item));

  gtk_label_set_text_with_mnemonic(GTK_LABEL(GTK_BIN(item)->child), s);
}


/****************************************************************
Note: the menu strings should contain underscores as in the
menu_items struct. The underscores will be removed elsewhere if
the string is used for a lookup via gtk_item_factory_get_widget()
*****************************************************************/
void update_menus(void)
{
  if(get_client_state()!=CLIENT_GAME_RUNNING_STATE) {
    menus_set_sensitive("<main>/_Reports", FALSE);
    menus_set_sensitive("<main>/_Kingdom", FALSE);
    menus_set_sensitive("<main>/_View", FALSE);
    menus_set_sensitive("<main>/_Orders", FALSE);
  } else {
    struct unit *punit;
    menus_set_sensitive("<main>/_Reports", TRUE);
    menus_set_sensitive("<main>/_Kingdom", TRUE);
    menus_set_sensitive("<main>/_View", TRUE);
    menus_set_sensitive("<main>/_Orders", !client_is_observer());

    menus_set_sensitive("<main>/_Kingdom/_Tax Rates", !client_is_observer());
    menus_set_sensitive("<main>/_Kingdom/Work_lists", !client_is_observer());
    menus_set_sensitive("<main>/_Kingdom/_Revolution",
			!client_is_observer());

    menus_set_sensitive("<main>/_Reports/S_paceship",
			(game.player_ptr->spaceship.state!=SSHIP_NONE));

    menus_set_active("<main>/_View/Map _Grid", draw_map_grid);
    menus_set_active("<main>/_View/City _Names", draw_city_names);
    menus_set_active("<main>/_View/City _Productions", draw_city_productions);
    menus_set_active("<main>/_View/Terrain", draw_terrain);
    menus_set_active("<main>/_View/Coastline", draw_coastline);
    menus_set_sensitive("<main>/_View/Coastline", !draw_terrain);
    menus_set_active("<main>/_View/Improvements/Roads & Rails", draw_roads_rails);
    menus_set_active("<main>/_View/Improvements/Irrigation", draw_irrigation);
    menus_set_active("<main>/_View/Improvements/Mines", draw_mines);
    menus_set_active("<main>/_View/Improvements/Fortress & Airbase", draw_fortress_airbase);
    menus_set_active("<main>/_View/Specials", draw_specials);
    menus_set_active("<main>/_View/Pollution & Fallout", draw_pollution);
    menus_set_active("<main>/_View/Cities", draw_cities);
    menus_set_active("<main>/_View/Units", draw_units);
    menus_set_active("<main>/_View/Focus Unit", draw_focus_unit);
    menus_set_sensitive("<main>/_View/Focus Unit", !draw_units);
    menus_set_active("<main>/_View/Fog of War", draw_fog_of_war);

    /* Remaining part of this function: Update Orders menu */

    if (client_is_observer()) {
      return;
    }

    if((punit=get_unit_in_focus())) {
      char *irrfmt = _("Change to %s (_I)");
      char *minfmt = _("Change to %s (_M)");
      char *transfmt = _("Transf_orm to %s");
      char irrtext[128], mintext[128], transtext[128];
      char *roadtext;
      enum tile_terrain_type  ttype;
      struct tile_type *      tinfo;

      sz_strlcpy(irrtext, _("Build _Irrigation"));
      sz_strlcpy(mintext, _("Build _Mine"));
      sz_strlcpy(transtext, _("Transf_orm Terrain"));
      
      /* Enable the button for adding to a city in all cases, so we
	 get an eventual error message from the server if we try. */
      menus_set_sensitive("<main>/_Orders/_Build City",
			  can_unit_add_or_build_city(punit) ||
			  unit_can_help_build_wonder_here(punit));
      menus_set_sensitive("<main>/_Orders/Build _Road",
                          (can_unit_do_activity(punit, ACTIVITY_ROAD) ||
                           can_unit_do_activity(punit, ACTIVITY_RAILROAD) ||
                           unit_can_est_traderoute_here(punit)));
      menus_set_sensitive("<main>/_Orders/Build _Irrigation",
                          can_unit_do_activity(punit, ACTIVITY_IRRIGATE));
      menus_set_sensitive("<main>/_Orders/Build _Mine",
                          can_unit_do_activity(punit, ACTIVITY_MINE));
      menus_set_sensitive("<main>/_Orders/Transf_orm Terrain",
			  can_unit_do_activity(punit, ACTIVITY_TRANSFORM));
      menus_set_sensitive("<main>/_Orders/Build _Fortress",
                          (can_unit_do_activity(punit, ACTIVITY_FORTRESS) ||
                           can_unit_do_activity(punit, ACTIVITY_FORTIFYING)));
      menus_set_sensitive("<main>/_Orders/Build Airbas_e",
			  can_unit_do_activity(punit, ACTIVITY_AIRBASE));
      menus_set_sensitive("<main>/_Orders/Clean _Pollution",
                          (can_unit_do_activity(punit, ACTIVITY_POLLUTION) ||
                           can_unit_paradrop(punit)));
      menus_set_sensitive("<main>/_Orders/Clean _Nuclear Fallout",
			  can_unit_do_activity(punit, ACTIVITY_FALLOUT));
      menus_set_sensitive("<main>/_Orders/_Sentry",
			  can_unit_do_activity(punit, ACTIVITY_SENTRY));
      menus_set_sensitive("<main>/_Orders/Pillage",
			  can_unit_do_activity(punit, ACTIVITY_PILLAGE));
      menus_set_sensitive("<main>/_Orders/Make _Homecity",
			  can_unit_change_homecity(punit));
      menus_set_sensitive("<main>/_Orders/_Unload",
			  get_transporter_capacity(punit)>0);
      menus_set_sensitive("<main>/_Orders/Wake up o_thers", 
			  is_unit_activity_on_tile(ACTIVITY_SENTRY,
                                                   punit->x, punit->y));
      menus_set_sensitive("<main>/_Orders/_Auto Settler",
                          can_unit_do_auto(punit));
      menus_set_sensitive("<main>/_Orders/Auto E_xplore",
                          can_unit_do_activity(punit, ACTIVITY_EXPLORE));
      menus_set_sensitive("<main>/_Orders/_Connect",
                          can_unit_do_connect(punit, ACTIVITY_IDLE));
      menus_set_sensitive("<main>/_Orders/Patrol (_Q)",
                          can_unit_do_activity(punit, ACTIVITY_PATROL));
      menus_set_sensitive("<main>/_Orders/Diplomat\\/Spy Actions",
                          (is_diplomat_unit(punit)
                           && diplomat_can_do_action(punit, DIPLOMAT_ANY_ACTION,
						     punit->x, punit->y)));
      menus_set_sensitive("<main>/_Orders/Explode Nuclear",
			  unit_flag(punit, F_NUCLEAR));
      if (unit_flag(punit, F_HELP_WONDER))
	menus_rename("<main>/_Orders/_Build City", _("Help _Build Wonder"));
      else if (unit_flag(punit, F_CITIES)) {
	if (map_get_city(punit->x, punit->y))
	  menus_rename("<main>/_Orders/_Build City", _("Add to City (_B)"));
	else
	  menus_rename("<main>/_Orders/_Build City", _("_Build City"));
      }
      else 
	menus_rename("<main>/_Orders/_Build City", _("_Build City"));
 
      if (unit_flag(punit, F_TRADE_ROUTE))
	menus_rename("<main>/_Orders/Build _Road", _("Make Trade _Route"));
      else if (unit_flag(punit, F_SETTLERS)) {
	if (map_has_special(punit->x, punit->y, S_ROAD)) {
	  roadtext = _("Build _Railroad");
	  road_activity=ACTIVITY_RAILROAD;  
	} 
	else {
	  roadtext = _("Build _Road");
	  road_activity=ACTIVITY_ROAD;  
	}
	menus_rename("<main>/_Orders/Build _Road", roadtext);
      }
      else
	menus_rename("<main>/_Orders/Build _Road", _("Build _Road"));

      ttype = map_get_tile(punit->x, punit->y)->terrain;
      tinfo = get_tile_type(ttype);
      if ((tinfo->irrigation_result != T_LAST) && (tinfo->irrigation_result != ttype))
	{
	  my_snprintf (irrtext, sizeof(irrtext), irrfmt,
		   (get_tile_type(tinfo->irrigation_result))->terrain_name);
	}
      else if (map_has_special(punit->x, punit->y, S_IRRIGATION) &&
	       player_knows_techs_with_flag(game.player_ptr, TF_FARMLAND))
	{
	  sz_strlcpy (irrtext, _("Bu_ild Farmland"));
	}
      if ((tinfo->mining_result != T_LAST) && (tinfo->mining_result != ttype))
	{
	  my_snprintf (mintext, sizeof(mintext), minfmt,
		   (get_tile_type(tinfo->mining_result))->terrain_name);
	}
      if ((tinfo->transform_result != T_LAST) && (tinfo->transform_result != ttype))
	{
	  my_snprintf (transtext, sizeof(transtext), transfmt,
		   (get_tile_type(tinfo->transform_result))->terrain_name);
	}

      menus_rename("<main>/_Orders/Build _Irrigation", irrtext);
      menus_rename("<main>/_Orders/Build _Mine", mintext);
      menus_rename("<main>/_Orders/Transf_orm Terrain", transtext);

      if (can_unit_do_activity(punit, ACTIVITY_FORTIFYING))
	menus_rename("<main>/_Orders/Build _Fortress", _("_Fortify"));
      else
	menus_rename("<main>/_Orders/Build _Fortress", _("Build _Fortress"));

      if (unit_flag(punit, F_PARATROOPERS))
	menus_rename("<main>/_Orders/Clean _Pollution", _("_Paradrop"));
      else
	menus_rename("<main>/_Orders/Clean _Pollution", _("Clean _Pollution"));

      if (!unit_flag(punit, F_SETTLERS))
	menus_rename("<main>/_Orders/_Auto Settler", _("_Auto Attack"));
      else
	menus_rename("<main>/_Orders/_Auto Settler", _("_Auto Settler"));

      menus_set_sensitive("<main>/_Orders", TRUE);
    }
    else
      menus_set_sensitive("<main>/_Orders", FALSE);
  }
}
