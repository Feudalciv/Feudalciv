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
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "packets.h"
#include "shared.h"
#include "support.h"

#include "cityrep.h"
#include "civclient.h"
#include "clinet.h"
#include "dialogs.h"
#include "gui_main.h"
#include "gui_stuff.h"
#include "helpdlg.h"
#include "optiondlg.h"

#include "repodlgs_common.h"
#include "repodlgs.h"

/******************************************************************/

static void create_science_dialog(bool make_modal);
static void science_help_callback(GtkTreeSelection *ts, GtkTreeModel *model);
static void science_change_callback(GtkWidget * widget, gpointer data);
static void science_goal_callback(GtkWidget * widget, gpointer data);

/******************************************************************/
static GtkWidget *science_dialog_shell = NULL;
static GtkWidget *science_label;
static GtkWidget *science_current_label, *science_goal_label;
static GtkWidget *science_change_menu_button, *science_goal_menu_button;
static GtkWidget *science_help_toggle;
static GtkListStore *science_model[4];
static int science_dialog_shell_is_modal;
static GtkWidget *popupmenu, *goalmenu;

/******************************************************************/
static void create_economy_report_dialog(bool make_modal);
static void economy_command_callback(GtkWidget *w, gint response_id);
static void economy_selection_callback(GtkTreeSelection *selection,
				       gpointer data);
static int economy_improvement_type[B_LAST];

static GtkWidget *economy_dialog_shell = NULL;
static GtkWidget *economy_label2;
static GtkListStore *economy_store;
static GtkTreeSelection *economy_selection;
static GtkWidget *sellall_command, *sellobsolete_command;
static int economy_dialog_shell_is_modal;

/******************************************************************/
static void create_activeunits_report_dialog(bool make_modal);
static void activeunits_command_callback(GtkWidget *w, gint response_id);
static void activeunits_selection_callback(GtkTreeSelection *selection,
					   gpointer data);
static int activeunits_type[U_LAST];
static GtkWidget *activeunits_dialog_shell = NULL;
static GtkListStore *activeunits_store;
static GtkTreeSelection *activeunits_selection;

static GtkWidget *upgrade_command;

static int activeunits_dialog_shell_is_modal;
/******************************************************************/

/******************************************************************
...
*******************************************************************/
void update_report_dialogs(void)
{
  if(is_report_dialogs_frozen()) return;
  activeunits_report_dialog_update();
  economy_report_dialog_update();
  city_report_dialog_update(); 
  science_dialog_update();
}


/****************************************************************
...
*****************************************************************/
void popup_science_dialog(bool make_modal)
{
  if(!science_dialog_shell) {
    science_dialog_shell_is_modal = make_modal;
    
    create_science_dialog(make_modal);
    gtk_set_relative_position(toplevel, science_dialog_shell, 10, 10);
  }

  gtk_window_present(GTK_WINDOW(science_dialog_shell));
}


/****************************************************************
 Closes the science dialog.
*****************************************************************/
void popdown_science_dialog(void)
{
  if (science_dialog_shell) {
    gtk_widget_destroy(science_dialog_shell);
  }
}
 

/****************************************************************
...
*****************************************************************/
void create_science_dialog(bool make_modal)
{
  GtkWidget *frame, *hbox, *w;
  int i;

  science_dialog_shell = gtk_dialog_new_with_buttons(_("Science"),
  	NULL,
	0,
	GTK_STOCK_CLOSE,
	GTK_RESPONSE_CLOSE,
	NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(science_dialog_shell),
	GTK_RESPONSE_CLOSE);

  if (make_modal) {
    gtk_window_set_transient_for(GTK_WINDOW(science_dialog_shell),
				 GTK_WINDOW(toplevel));
    gtk_window_set_modal(GTK_WINDOW(science_dialog_shell), TRUE);
  }

  g_signal_connect(science_dialog_shell, "response",
		   G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(science_dialog_shell, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &science_dialog_shell);

  science_label = gtk_label_new("no text set yet");

  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(science_dialog_shell)->vbox ),
        science_label, FALSE, FALSE, 0 );

  frame = gtk_frame_new(_("Researching"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(science_dialog_shell)->vbox ),
        frame, FALSE, FALSE, 0 );

  hbox = gtk_hbox_new( TRUE, 5 );
  gtk_container_add(GTK_CONTAINER(frame), hbox);

  science_change_menu_button = gtk_option_menu_new();
  gtk_box_pack_start( GTK_BOX( hbox ), science_change_menu_button,TRUE, TRUE, 0 );

  popupmenu = gtk_menu_new();
  gtk_widget_show_all(popupmenu);

  science_current_label=gtk_progress_bar_new();
  gtk_box_pack_start( GTK_BOX( hbox ), science_current_label,TRUE, FALSE, 0 );
  gtk_widget_set_usize(science_current_label, 0, 25);
  
  science_help_toggle = gtk_check_button_new_with_label (_("Help"));
  gtk_box_pack_start( GTK_BOX( hbox ), science_help_toggle, TRUE, FALSE, 0 );

  frame = gtk_frame_new( _("Goal"));
  gtk_box_pack_start( GTK_BOX( GTK_DIALOG(science_dialog_shell)->vbox ),
        frame, FALSE, FALSE, 0 );

  hbox = gtk_hbox_new( TRUE, 5 );
  gtk_container_add(GTK_CONTAINER(frame),hbox);

  science_goal_menu_button = gtk_option_menu_new();
  gtk_box_pack_start( GTK_BOX( hbox ), science_goal_menu_button,TRUE, TRUE, 0 );

  goalmenu = gtk_menu_new();
  gtk_widget_show_all(goalmenu);

  science_goal_label = gtk_label_new("");
  gtk_box_pack_start( GTK_BOX( hbox ), science_goal_label, TRUE, FALSE, 0 );
  gtk_widget_set_usize(science_goal_label, 0,25);

  w = gtk_label_new("");
  gtk_box_pack_start( GTK_BOX( hbox ), w,TRUE, FALSE, 0 );

  hbox = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(science_dialog_shell)->vbox),
		     hbox, TRUE, TRUE, 5);



  for (i=0; i<4; i++) {
    GtkWidget *view;
    GtkTreeSelection *selection;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    science_model[i] = gtk_list_store_new(1, G_TYPE_STRING);
    view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(science_model[i]));
    gtk_box_pack_start(GTK_BOX(hbox), view, TRUE, TRUE, 0);
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    g_object_unref(science_model[i]);
    gtk_tree_view_columns_autosize(GTK_TREE_VIEW(view));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(NULL, renderer,
	"text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    g_signal_connect(selection, "changed",
		     G_CALLBACK(science_help_callback), science_model[i]);
  }

  gtk_widget_show_all(GTK_DIALOG(science_dialog_shell)->vbox);

  science_dialog_update();
  gtk_window_set_focus(GTK_WINDOW(science_dialog_shell),
	science_change_menu_button);
}

/****************************************************************
...
*****************************************************************/
void science_change_callback(GtkWidget *widget, gpointer data)
{
  char text[512];
  struct packet_player_request packet;
  size_t to;

  to=(size_t)data;

  if (GTK_TOGGLE_BUTTON(science_help_toggle)->active) {
    popup_help_dialog_typed(advances[to].name, HELP_TECH);
    /* Following is to make the menu go back to the current research;
     * there may be a better way to do this?  --dwp */
    science_dialog_update();
  } else {
    gdouble pct;

    gtk_widget_set_sensitive(science_change_menu_button,
			     can_client_issue_orders());
    my_snprintf(text, sizeof(text), "%d/%d",
		game.player_ptr->research.bulbs_researched,
		total_bulbs_required(game.player_ptr));
    pct=CLAMP((gdouble) game.player_ptr->research.bulbs_researched /
		total_bulbs_required(game.player_ptr), 0.0, 1.0);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(science_current_label), pct);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(science_current_label), text);
    
    packet.tech=to;
    send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_RESEARCH);
  }
}

/****************************************************************
...
*****************************************************************/
void science_goal_callback(GtkWidget *widget, gpointer data)
{
  char text[512];
  struct packet_player_request packet;
  size_t to;

  to=(size_t)data;

  if (GTK_TOGGLE_BUTTON(science_help_toggle)->active) {
    popup_help_dialog_typed(advances[to].name, HELP_TECH);
    /* Following is to make the menu go back to the current goal;
     * there may be a better way to do this?  --dwp */
    science_dialog_update();
  }
  else {  
    int steps = num_unknown_techs_for_goal(game.player_ptr, to);
    my_snprintf(text, sizeof(text),
		PL_("(%d step)", "(%d steps)", steps), steps);
    gtk_set_label(science_goal_label,text);

    packet.tech=to;
    send_packet_player_request(&aconnection, &packet, PACKET_PLAYER_TECH_GOAL);
  }
}

/****************************************************************
...
*****************************************************************/
static void science_help_callback(GtkTreeSelection *ts, GtkTreeModel *model)
{
  GtkTreeIter it;

  if (!gtk_tree_selection_get_selected(ts, NULL, &it))
    return;

  gtk_tree_selection_unselect_all(ts);

  if (GTK_TOGGLE_BUTTON(science_help_toggle)->active)
  {
    char *s;

    gtk_tree_model_get(model, &it, 0, &s, -1);
    if (*s != '\0')
      popup_help_dialog_typed(s, HELP_TECH);
    else
      popup_help_dialog_string(HELP_TECHS_ITEM);
  }
}

/****************************************************************
...
*****************************************************************/
static gint cmp_func(gconstpointer a_p, gconstpointer b_p)
{
  gchar *a_str, *b_str;
  gchar text_a[512], text_b[512];
  gint a = GPOINTER_TO_INT(a_p), b = GPOINTER_TO_INT(b_p);

  if (!is_future_tech(a)) {
    a_str=advances[a].name;
  } else {
    my_snprintf(text_a,sizeof(text_a), _("Future Tech. %d"),
		a - game.num_tech_types);
    a_str=text_a;
  }

  if(!is_future_tech(b)) {
    b_str=advances[b].name;
  } else {
    my_snprintf(text_b,sizeof(text_b), _("Future Tech. %d"),
		b - game.num_tech_types);
    b_str=text_b;
  }

  return strcmp(a_str,b_str);
}

/****************************************************************
...
*****************************************************************/
void science_dialog_update(void)
{
  if(science_dialog_shell) {
  char text[512];
  int i, j, hist;
  GtkWidget *item;
  GList *sorting_list = NULL;
  gdouble pct;
  int turns_to_advance;
  int steps;

  if(is_report_dialogs_frozen()) return;

  turns_to_advance = tech_turns_to_advance(game.player_ptr);
  if (turns_to_advance == FC_INFINITY) {
    my_snprintf(text, sizeof(text), _("Research speed: no research"));
  } else {
    my_snprintf(text, sizeof(text),
		PL_("Research speed: %d turn/advance",
		    "Research speed: %d turns/advance", turns_to_advance),
		turns_to_advance);
  }

  gtk_set_label(science_label, text);

  for (i=0; i<4; i++) {
    gtk_list_store_clear(science_model[i]);
  }

  /* collect all researched techs in sorting_list */
  for(i=A_FIRST; i<game.num_tech_types; i++) {
    if ((get_invention(game.player_ptr, i)==TECH_KNOWN)) {
      sorting_list = g_list_append(sorting_list, GINT_TO_POINTER(i));
    }
  }

  /* sort them, and install them in the list */
  sorting_list = g_list_sort(sorting_list, cmp_func);
  for(i=0; i<g_list_length(sorting_list); i++) {
    GtkTreeIter it;
    GValue value = { 0, };

    j = GPOINTER_TO_INT(g_list_nth_data(sorting_list, i));
    gtk_list_store_append(science_model[i%4], &it);

    g_value_init(&value, G_TYPE_STRING);
    g_value_set_static_string(&value, advances[j].name);
    gtk_list_store_set_value(science_model[i%4], &it, 0, &value);
    g_value_unset(&value);
  }
  g_list_free(sorting_list);
  sorting_list = NULL;

  gtk_widget_destroy(popupmenu);
  popupmenu = gtk_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(science_change_menu_button),
	popupmenu);
  gtk_widget_set_sensitive(science_change_menu_button,
			   can_client_issue_orders());

  my_snprintf(text, sizeof(text), "%d/%d",
	      game.player_ptr->research.bulbs_researched,
	      total_bulbs_required(game.player_ptr));

  pct=CLAMP((gdouble) game.player_ptr->research.bulbs_researched /
	    total_bulbs_required(game.player_ptr), 0.0, 1.0);

  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(science_current_label), pct);
  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(science_current_label), text);

  /* collect all techs which are reachable in the next step
   * hist will hold afterwards the techid of the current choice
   */
  hist=0;
  if (!is_future_tech(game.player_ptr->research.researching)) {
    for(i=A_FIRST; i<game.num_tech_types; i++) {
      if(get_invention(game.player_ptr, i)!=TECH_REACHABLE)
	continue;

      if (i==game.player_ptr->research.researching)
	hist=i;
      sorting_list = g_list_append(sorting_list, GINT_TO_POINTER(i));
    }
  } else {
    sorting_list = g_list_append(sorting_list,
				 GINT_TO_POINTER(game.num_tech_types + 1 +
						 game.player_ptr->
						 future_tech));
  }

  /* sort the list and build from it the menu */
  sorting_list = g_list_sort(sorting_list, cmp_func);
  for (i = 0; i < g_list_length(sorting_list); i++) {
    gchar *data;

    if (GPOINTER_TO_INT(g_list_nth_data(sorting_list, i)) <
	game.num_tech_types) {
      data=advances[GPOINTER_TO_INT(g_list_nth_data(sorting_list, i))].name;
    } else {
      my_snprintf(text, sizeof(text), _("Future Tech. %d"),
		  GPOINTER_TO_INT(g_list_nth_data(sorting_list, i))
		  - game.num_tech_types);
      data=text;
    }

    item = gtk_menu_item_new_with_label(data);
    gtk_menu_shell_append(GTK_MENU_SHELL(popupmenu), item);
    if (strlen(data) > 0)
      gtk_signal_connect(GTK_OBJECT(item), "activate",
			 GTK_SIGNAL_FUNC(science_change_callback),
			 g_list_nth_data(sorting_list, i));
  }

  gtk_widget_show_all(popupmenu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(science_change_menu_button),
	g_list_index(sorting_list, GINT_TO_POINTER(hist)));
  g_list_free(sorting_list);
  sorting_list = NULL;

  gtk_widget_destroy(goalmenu);
  goalmenu = gtk_menu_new();
  gtk_option_menu_set_menu(GTK_OPTION_MENU(science_goal_menu_button),
	goalmenu);
  gtk_widget_set_sensitive(science_goal_menu_button,
			   can_client_issue_orders());
  
  steps = num_unknown_techs_for_goal(game.player_ptr,
				     game.player_ptr->ai.tech_goal);
  my_snprintf(text, sizeof(text), PL_("(%d step)", "(%d steps)", steps),
	      steps);
  gtk_set_label(science_goal_label,text);

  if (game.player_ptr->ai.tech_goal == A_UNSET) {
    item = gtk_menu_item_new_with_label(advances[A_NONE].name);
    gtk_menu_shell_append(GTK_MENU_SHELL(goalmenu), item);
  }

  /* collect all techs which are reachable in under 11 steps
   * hist will hold afterwards the techid of the current choice
   */
  hist=0;
  for(i=A_FIRST; i<game.num_tech_types; i++) {
    if(get_invention(game.player_ptr, i) != TECH_KNOWN &&
       advances[i].req[0] != A_LAST && advances[i].req[1] != A_LAST &&
       num_unknown_techs_for_goal(game.player_ptr, i) < 11) {
      if (i==game.player_ptr->ai.tech_goal)
	hist=i;
      sorting_list = g_list_append(sorting_list, GINT_TO_POINTER(i));
    }
  }

  /* sort the list and build from it the menu */
  sorting_list = g_list_sort(sorting_list, cmp_func);
  for (i = 0; i < g_list_length(sorting_list); i++) {
    gchar *data =
	advances[GPOINTER_TO_INT(g_list_nth_data(sorting_list, i))].name;

    item = gtk_menu_item_new_with_label(data);
    gtk_menu_shell_append(GTK_MENU_SHELL(goalmenu), item);
    gtk_signal_connect(GTK_OBJECT(item), "activate",
		       GTK_SIGNAL_FUNC(science_goal_callback),
		       g_list_nth_data(sorting_list, i));
  }

  gtk_widget_show_all(goalmenu);
  gtk_option_menu_set_history(GTK_OPTION_MENU(science_goal_menu_button),
	g_list_index(sorting_list, GINT_TO_POINTER(hist)));
  g_list_free(sorting_list);
  sorting_list = NULL;
  }
}


/****************************************************************

                      ECONOMY REPORT DIALOG
 
****************************************************************/

/****************************************************************
...
****************************************************************/
void popup_economy_report_dialog(bool make_modal)
{
  if(!economy_dialog_shell) {
    economy_dialog_shell_is_modal = make_modal;

    create_economy_report_dialog(make_modal);
    gtk_set_relative_position(toplevel, economy_dialog_shell, 10, 10);
  }

  gtk_window_present(GTK_WINDOW(economy_dialog_shell));
}


/****************************************************************
 Close the economy report dialog.
****************************************************************/
void popdown_economy_report_dialog(void)
{
  if (economy_dialog_shell) {
    gtk_widget_destroy(economy_dialog_shell);
  }
}
 

/****************************************************************
...
*****************************************************************/
void create_economy_report_dialog(bool make_modal)
{
  static char *titles[4] = {
    N_("Building Name"),
    N_("Count"),
    N_("Cost"),
    N_("U Total")
  };
  static bool titles_done;
  int i;

  static GType model_types[4] = {
    G_TYPE_STRING,
    G_TYPE_INT,
    G_TYPE_INT,
    G_TYPE_INT
  };
  GtkWidget *view, *sw;

  intl_slist(ARRAY_SIZE(titles), titles, &titles_done);
  
  economy_dialog_shell = gtk_dialog_new_with_buttons(_("Economy"),
  	NULL,
	0,
	GTK_STOCK_CLOSE,
	GTK_RESPONSE_CLOSE,
	NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(economy_dialog_shell),
	GTK_RESPONSE_CLOSE);

  if (make_modal) {
    gtk_window_set_transient_for(GTK_WINDOW(economy_dialog_shell),
				 GTK_WINDOW(toplevel));
    gtk_window_set_modal(GTK_WINDOW(economy_dialog_shell), TRUE);
  }

  economy_store = gtk_list_store_newv(ARRAY_SIZE(model_types), model_types);

  sw = gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(economy_dialog_shell)->vbox),
	sw, TRUE, TRUE, 0);

  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(economy_store));
  g_object_unref(economy_store);
  economy_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  g_signal_connect(economy_selection, "changed",
		   G_CALLBACK(economy_selection_callback), NULL);

  for (i=0; i<4; i++) {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    renderer = gtk_cell_renderer_text_new();
      
    col = gtk_tree_view_column_new_with_attributes(titles[i], renderer,
	"text", i, NULL);

    if (i > 0) {
      GValue value = { 0, };

      g_value_init(&value, G_TYPE_FLOAT);
      g_value_set_float(&value, 1.0);
      g_object_set_property(G_OBJECT(renderer), "xalign", &value);
      g_value_unset(&value);

      gtk_tree_view_column_set_alignment(col, 1.0);
    }

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  }
  gtk_container_add(GTK_CONTAINER(sw), view);

  economy_label2 = gtk_label_new(_("Total Cost:"));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(economy_dialog_shell)->vbox),
	economy_label2, FALSE, FALSE, 0);
  gtk_misc_set_padding(GTK_MISC(economy_label2), 5, 5);

  sellobsolete_command = gtk_button_new_with_mnemonic(_("Sell Obsolete"));
  gtk_dialog_add_action_widget(GTK_DIALOG(economy_dialog_shell),
			       sellobsolete_command, 1);
  gtk_widget_set_sensitive(sellobsolete_command, FALSE);

  sellall_command = gtk_button_new_with_mnemonic(_("Sell All"));
  gtk_dialog_add_action_widget(GTK_DIALOG(economy_dialog_shell),
			       sellall_command, 2);
  gtk_widget_set_sensitive(sellall_command, FALSE);

  g_signal_connect(economy_dialog_shell, "response",
		   G_CALLBACK(economy_command_callback), NULL);
  g_signal_connect(economy_dialog_shell, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &economy_dialog_shell);

  economy_report_dialog_update();
  gtk_window_set_default_size(GTK_WINDOW(economy_dialog_shell), -1, 350);

  gtk_widget_show_all(GTK_DIALOG(economy_dialog_shell)->vbox);
  gtk_widget_show_all(GTK_DIALOG(economy_dialog_shell)->action_area);

  gtk_tree_view_focus(GTK_TREE_VIEW(view));
}


/****************************************************************
  Called when a building type is selected in the economy list.
*****************************************************************/
static void economy_selection_callback(GtkTreeSelection *selection,
				       gpointer data)
{
  gint row = gtk_tree_selection_get_row(selection);

  if (row >= 0) {
    /* The user has selected an improvement type. */
    int i = economy_improvement_type[row];
    bool is_sellable = (i >= 0 && i < game.num_impr_types && !is_wonder(i));

    gtk_widget_set_sensitive(sellobsolete_command, is_sellable
			     && can_client_issue_orders()
			     && improvement_obsolete(game.player_ptr, i));
    gtk_widget_set_sensitive(sellall_command, is_sellable
			     && can_client_issue_orders());
  } else {
    /* No selection has been made. */
    gtk_widget_set_sensitive(sellobsolete_command, FALSE);
    gtk_widget_set_sensitive(sellall_command, FALSE);
  }
}

/****************************************************************
...
*****************************************************************/
static void economy_command_callback(GtkWidget *w, gint response_id)
{
  int i, count = 0, gold = 0;
  struct packet_city_request packet;
  gint row;
  GtkWidget *shell;

  switch (response_id) {
    case 1:
    case 2:     break;
    default:	gtk_widget_destroy(economy_dialog_shell);	return;
  }

  /* sell obsolete and sell all. */
  row = gtk_tree_selection_get_row(economy_selection);
  i = economy_improvement_type[row];

  city_list_iterate(game.player_ptr->cities, pcity) {
    if(!pcity->did_sell && city_got_building(pcity, i) && 
       (response_id == 2 ||
	improvement_obsolete(game.player_ptr,i) ||
        wonder_replacement(pcity, i) ))  {
	count++; gold+=improvement_value(i);
        packet.city_id=pcity->id;
        packet.build_id=i;
        send_packet_city_request(&aconnection, &packet, PACKET_CITY_SELL);
    }
  } city_list_iterate_end;

  if (count > 0) {
    shell = gtk_message_dialog_new(GTK_WINDOW(economy_dialog_shell),
	GTK_DIALOG_DESTROY_WITH_PARENT,
	GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
	_("Sold %d %s for %d gold"), count, get_improvement_name(i), gold);
  } else {
    shell = gtk_message_dialog_new(GTK_WINDOW(economy_dialog_shell),
	GTK_DIALOG_DESTROY_WITH_PARENT,
	GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE,
	_("No %s could be sold"), get_improvement_name(i));
  }
  g_signal_connect(shell, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_window_set_title(GTK_WINDOW(shell), _("Sell-Off: Results"));
  gtk_window_present(GTK_WINDOW(shell));
}

/****************************************************************
...
*****************************************************************/
void economy_report_dialog_update(void)
{
  if(!is_report_dialogs_frozen() && economy_dialog_shell) {
    int tax, total, i, entries_used;
    char economy_total[48];
    struct improvement_entry entries[B_LAST];
    GtkTreeIter it;
    GValue value = { 0, };

    gtk_list_store_clear(economy_store);

    get_economy_report_data(entries, &entries_used, &total, &tax);

    for (i = 0; i < entries_used; i++) {
      struct improvement_entry *p = &entries[i];

      gtk_list_store_append(economy_store, &it);
      gtk_list_store_set(economy_store, &it,
	1, p->count,
	2, p->cost,
	3, p->total_cost, -1);
      g_value_init(&value, G_TYPE_STRING);
      g_value_set_static_string(&value, get_improvement_name(p->type));
      gtk_list_store_set_value(economy_store, &it, 0, &value);
      g_value_unset(&value);

      economy_improvement_type[i] = p->type;
    }

    my_snprintf(economy_total, sizeof(economy_total),
		_("Income: %d    Total Costs: %d"), tax, total); 
    gtk_label_set_text(GTK_LABEL(economy_label2), economy_total);
  }  
}

/****************************************************************

                      ACTIVE UNITS REPORT DIALOG
 
****************************************************************/

#define AU_COL 6

/****************************************************************
...
****************************************************************/
void popup_activeunits_report_dialog(bool make_modal)
{
  if(!activeunits_dialog_shell) {
    activeunits_dialog_shell_is_modal = make_modal;
    
    create_activeunits_report_dialog(make_modal);
    gtk_set_relative_position(toplevel, activeunits_dialog_shell, 10, 10);
  }

  gtk_window_present(GTK_WINDOW(activeunits_dialog_shell));
}


/****************************************************************
 Closes the units report dialog.
****************************************************************/
void popdown_activeunits_report_dialog(void)
{
  if (activeunits_dialog_shell) {
    gtk_widget_destroy(activeunits_dialog_shell);
  }
}

 
/****************************************************************
...
*****************************************************************/
static void activeunits_cell_data_func(GtkTreeViewColumn *col,
				       GtkCellRenderer *cell,
				       GtkTreeModel *model, GtkTreeIter *it,
				       gpointer data)
{
  gboolean  b;
  gchar    *s;
  GValue    value = { 0, };

  if (!it)
    return;

  gtk_tree_model_get(model, it, 0, &s, 6, &b, -1);

  g_value_init(&value, G_TYPE_BOOLEAN);

  if (!b && (*s == '\0' || GPOINTER_TO_INT(data) == 1)) {
    g_value_set_boolean(&value, FALSE);
  } else {
    g_value_set_boolean(&value, TRUE);
  }

  g_object_set_property(G_OBJECT(cell), "visible", &value);
  g_value_unset(&value);
}
					     
/****************************************************************
...
*****************************************************************/
void create_activeunits_report_dialog(bool make_modal)
{
  static char *titles[AU_COL] = {
    N_("Unit Type"),
    N_("U"),
    N_("In-Prog"),
    N_("Active"),
    N_("Shield"),
    N_("Food")
  };
  static bool titles_done;
  int i;

  static GType model_types[AU_COL+1] = {
    G_TYPE_STRING,
    G_TYPE_BOOLEAN,
    G_TYPE_INT,
    G_TYPE_INT,
    G_TYPE_INT,
    G_TYPE_INT,
    G_TYPE_BOOLEAN
  };
  GtkWidget *view, *sw;
  GtkWidget *refresh_command;

  intl_slist(ARRAY_SIZE(titles), titles, &titles_done);

  activeunits_dialog_shell = gtk_dialog_new_with_buttons(_("Units"),
  	NULL,
	0,
	GTK_STOCK_CLOSE,
	GTK_RESPONSE_CLOSE,
	NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(activeunits_dialog_shell),
	GTK_RESPONSE_CLOSE);

  if (make_modal) {
    gtk_window_set_transient_for(GTK_WINDOW(activeunits_dialog_shell),
				 GTK_WINDOW(toplevel));
    gtk_window_set_modal(GTK_WINDOW(activeunits_dialog_shell), TRUE);
  }

  activeunits_store = gtk_list_store_newv(ARRAY_SIZE(model_types), model_types);

  sw = gtk_scrolled_window_new(NULL,NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw),
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(activeunits_dialog_shell)->vbox),
	sw, TRUE, TRUE, 0);

  view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(activeunits_store));
  g_object_unref(activeunits_store);
  activeunits_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
  g_signal_connect(activeunits_selection, "changed",
	G_CALLBACK(activeunits_selection_callback), NULL);

  for (i=0; i<AU_COL; i++) {
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;

    if (model_types[i] == G_TYPE_BOOLEAN) {
      renderer = gtk_cell_renderer_toggle_new();
      
      col = gtk_tree_view_column_new_with_attributes(titles[i], renderer,
	"active", i, NULL);
    } else {
      renderer = gtk_cell_renderer_text_new();
      
      col = gtk_tree_view_column_new_with_attributes(titles[i], renderer,
	"text", i, NULL);
    }

    if (i > 0) {
      GValue value = { 0, };

      g_value_init(&value, G_TYPE_FLOAT);
      g_value_set_float(&value, 1.0);
      g_object_set_property(G_OBJECT(renderer), "xalign", &value);
      g_value_unset(&value);

      gtk_tree_view_column_set_alignment(col, 1.0);

      gtk_tree_view_column_set_cell_data_func(col, renderer,
	activeunits_cell_data_func, GINT_TO_POINTER(i), NULL);
    }

    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  }
  gtk_container_add(GTK_CONTAINER(sw), view);

  upgrade_command = gtk_button_new_with_mnemonic(_("_Upgrade"));
  gtk_dialog_add_action_widget(GTK_DIALOG(activeunits_dialog_shell),
			       upgrade_command, 1);
  gtk_widget_set_sensitive(upgrade_command, FALSE);

  refresh_command = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
  gtk_dialog_add_action_widget(GTK_DIALOG(activeunits_dialog_shell),
			       refresh_command, 2);

  g_signal_connect(activeunits_dialog_shell, "response",
		   G_CALLBACK(activeunits_command_callback), NULL);
  g_signal_connect(activeunits_dialog_shell, "destroy",
		   G_CALLBACK(gtk_widget_destroyed), &activeunits_dialog_shell);

  activeunits_report_dialog_update();
  gtk_window_set_default_size(GTK_WINDOW(activeunits_dialog_shell), -1, 350);

  gtk_widget_show_all(GTK_DIALOG(activeunits_dialog_shell)->vbox);
  gtk_widget_show_all(GTK_DIALOG(activeunits_dialog_shell)->action_area);

  gtk_tree_view_focus(GTK_TREE_VIEW(view));
}

/****************************************************************
...
*****************************************************************/
static void activeunits_selection_callback(GtkTreeSelection *selection,
					   gpointer data)
{
  gint row, n;

  if ((row = gtk_tree_selection_get_row(selection)) == -1) {
    gtk_widget_set_sensitive(upgrade_command, FALSE);
    return;
  }

  n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(activeunits_store), NULL);

  if (row < n-2 &&
      unit_type_exists(activeunits_type[row]) &&
      can_upgrade_unittype(game.player_ptr, activeunits_type[row]) != -1) {
    gtk_widget_set_sensitive(upgrade_command, can_client_issue_orders());
  } else {
    gtk_widget_set_sensitive(upgrade_command, FALSE);
  }
}

/****************************************************************
...
*****************************************************************/
static void activeunits_command_callback(GtkWidget *w, gint response_id)
{
  int        ut1, ut2;
  gint       row;
  GtkWidget *shell;

  switch (response_id) {
    case 1:     break;
    case 2:	activeunits_report_dialog_update();		return;
    default:	gtk_widget_destroy(activeunits_dialog_shell);	return;
  }

  /* upgrade command. */
  row = gtk_tree_selection_get_row(activeunits_selection);
  ut1 = activeunits_type[row];

  if (!unit_type_exists(ut1))
    return;

  ut2 = can_upgrade_unittype(game.player_ptr, activeunits_type[row]);

  shell = gtk_message_dialog_new(
	GTK_WINDOW(activeunits_dialog_shell),
	GTK_DIALOG_MODAL|GTK_DIALOG_DESTROY_WITH_PARENT,
	GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
	_("Upgrade as many %s to %s as possible for %d gold each?\n"
	  "Treasury contains %d gold."),
	unit_types[ut1].name, unit_types[ut2].name,
	unit_upgrade_price(game.player_ptr, ut1, ut2),
	game.player_ptr->economic.gold);
  gtk_window_set_title(GTK_WINDOW(shell), _("Upgrade Obsolete Units"));

  if (gtk_dialog_run(GTK_DIALOG(shell)) == GTK_RESPONSE_YES) {
    send_packet_unittype_info(&aconnection, ut1, PACKET_UNITTYPE_UPGRADE);
  }

  gtk_widget_destroy(shell);
}

/****************************************************************
...
*****************************************************************/
void activeunits_report_dialog_update(void)
{
  struct repoinfo {
    int active_count;
    int upkeep_shield;
    int upkeep_food;
    /* int upkeep_gold;   FIXME: add gold when gold is implemented --jjm */
    int building_count;
  };

  if (is_report_dialogs_frozen())
    return;

  if (activeunits_dialog_shell) {
    int    k, can;
    struct repoinfo unitarray[U_LAST];
    struct repoinfo unittotals;
    GtkTreeIter it;
    GValue value = { 0, };

    gtk_list_store_clear(activeunits_store);

    memset(unitarray, '\0', sizeof(unitarray));
    unit_list_iterate(game.player_ptr->units, punit) {
      (unitarray[punit->type].active_count)++;
      if (punit->homecity) {
	unitarray[punit->type].upkeep_shield += punit->upkeep;
	unitarray[punit->type].upkeep_food += punit->upkeep_food;
      }
    }
    unit_list_iterate_end;
    city_list_iterate(game.player_ptr->cities,pcity) {
      if (pcity->is_building_unit &&
	  (unit_type_exists (pcity->currently_building)))
	(unitarray[pcity->currently_building].building_count)++;
    }
    city_list_iterate_end;

    k = 0;
    memset(&unittotals, '\0', sizeof(unittotals));
    unit_type_iterate(i) {
    
      if ((unitarray[i].active_count > 0) || (unitarray[i].building_count > 0)) {
	can = (can_upgrade_unittype(game.player_ptr, i) != -1);
	
        gtk_list_store_append(activeunits_store, &it);
	gtk_list_store_set(activeunits_store, &it,
		1, can,
		2, unitarray[i].building_count,
		3, unitarray[i].active_count,
		4, unitarray[i].upkeep_shield,
		5, unitarray[i].upkeep_food,
		6, TRUE, -1);
	g_value_init(&value, G_TYPE_STRING);
	g_value_set_static_string(&value, unit_name(i));
	gtk_list_store_set_value(activeunits_store, &it, 0, &value);
	g_value_unset(&value);

	activeunits_type[k]=(unitarray[i].active_count > 0) ? i : U_LAST;
	k++;
	unittotals.active_count += unitarray[i].active_count;
	unittotals.upkeep_shield += unitarray[i].upkeep_shield;
	unittotals.upkeep_food += unitarray[i].upkeep_food;
	unittotals.building_count += unitarray[i].building_count;
      }
    } unit_type_iterate_end;

    gtk_list_store_append(activeunits_store, &it);
    gtk_list_store_set(activeunits_store, &it,
	    1, FALSE,
	    2, 0,
	    3, 0,
	    4, 0,
	    5, 0,
	    6, FALSE, -1);
    g_value_init(&value, G_TYPE_STRING);
    g_value_set_static_string(&value, "");
    gtk_list_store_set_value(activeunits_store, &it, 0, &value);
    g_value_unset(&value);

    gtk_list_store_append(activeunits_store, &it);
    gtk_list_store_set(activeunits_store, &it,
	    1, FALSE,
    	    2, unittotals.building_count,
    	    3, unittotals.active_count,
    	    4, unittotals.upkeep_shield,
    	    5, unittotals.upkeep_food,
	    6, FALSE, -1);
    g_value_init(&value, G_TYPE_STRING);
    g_value_set_static_string(&value, _("Totals:"));
    gtk_list_store_set_value(activeunits_store, &it, 0, &value);
    g_value_unset(&value);
  }
}
