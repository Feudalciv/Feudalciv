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
#include <time.h>
#include <assert.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_WINSOCK
#include <winsock.h>
#endif

#include "capstr.h"
#include "fcintl.h"
#include "game.h"
#include "idex.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "packets.h"
#include "rand.h"
#include "support.h"
#include "version.h"

#include "chatline_g.h"
#include "citydlg_g.h"
#include "cityrepdata.h"
#include "climisc.h"
#include "clinet.h"
#include "connectdlg_g.h"
#include "control.h" 
#include "dialogs_g.h"
#include "diplodlg_g.h"
#include "gui_main_g.h"
#include "goto.h"
#include "helpdata.h"		/* boot_help_texts() */
#include "mapctrl_g.h"
#include "mapview_g.h"
#include "menu_g.h"
#include "messagewin_g.h"
#include "netintf.h"
#include "options.h"
#include "packhand.h"
#include "plrdlg_g.h"
#include "repodlgs_g.h"
#include "tilespec.h"
#include "attribute.h"
#include "agents.h"
#include "audio.h"

#include "cma_core.h" /* kludge */

#include "civclient.h"

/* this is used in strange places, and is 'extern'd where
   needed (hence, it is not 'extern'd in civclient.h) */
bool is_server = FALSE;

char tile_set_name[512] = "\0";
char sound_plugin_name[512] = "\0";
char sound_set_name[512] = "\0";
char server_host[512] = "\0";
char player_name[512] = "\0";
char metaserver[512] = "\0";
int  server_port = -1;
bool auto_connect = FALSE; /* TRUE = skip "Connect to Freeciv Server" dialog */

static enum client_states client_state = CLIENT_BOOT_STATE;

int seconds_to_turndone;

int last_turn_gold_amount;
int turn_gold_difference;

/* TRUE if an end turn request is blocked by busy agents */
bool waiting_for_end_turn = FALSE;

/* 
 * TRUE for the time between sending PACKET_TURN_DONE and receiving
 * PACKET_NEW_YEAR. 
 */
bool turn_done_sent = FALSE;

static void client_remove_all_cli_conn(void);

/**************************************************************************
...
**************************************************************************/
int main(int argc, char *argv[])
{
  int i;
  int loglevel;
  char *logfile=NULL;
  char *option=NULL;

  init_nls();
  dont_run_as_root(argv[0], "freeciv_client");
  audio_init();

  /* default argument values are set in options.c */
  loglevel=LOG_NORMAL;

  i = 1;

  while (i < argc) {
   if (is_option("--help", argv[i])) {
    fprintf(stderr, _("Usage: %s [option ...]\n"
		      "Valid options are:\n"), argv[0]);
    fprintf(stderr, _("  -a, --autoconnect\tSkip connect dialog\n"));
#ifdef DEBUG
    fprintf(stderr, _("  -d, --debug NUM\tSet debug log level (0 to 4,"
                                  " or 4:file1,min,max:...)\n"));
#else
    fprintf(stderr, _("  -d, --debug NUM\tSet debug log level (0 to 3)\n"));
#endif
    fprintf(stderr, _("  -h, --help\t\tPrint a summary of the options\n"));
    fprintf(stderr, _("  -l, --log FILE\tUse FILE as logfile\n"));
    fprintf(stderr, _("  -m, --meta HOST\t"
		      "Connect to the metaserver at HOST\n"));
    fprintf(stderr, _("  -n, --name NAME\tUse NAME as name\n"));
    fprintf(stderr, _("  -p, --port PORT\tConnect to server port PORT\n"));
    fprintf(stderr, _("  -P, --Plugin PLUGIN\tUse PLUGIN for sound output %s\n"),
	    audio_get_all_plugin_names());
    fprintf(stderr, _("  -s, --server HOST\tConnect to the server at HOST\n"));
    fprintf(stderr, _("  -S, --Sound FILE\tRead sound tags from FILE\n"));
    fprintf(stderr, _("  -t, --tiles FILE\t"
		      "Use data file FILE.tilespec for tiles\n"));
    fprintf(stderr, _("  -v, --version\t\tPrint the version number\n"));
    exit(EXIT_SUCCESS);
   } else if (is_option("--version",argv[i])) {
    fprintf(stderr, "%s %s\n", freeciv_name_version(), client_string);
    exit(EXIT_SUCCESS);
   } else if ((option = get_option("--log",argv,&i,argc)))
      logfile = mystrdup(option); /* never free()d */
   else if ((option = get_option("--name",argv,&i,argc)))
      sz_strlcpy(player_name, option);
   else if ((option = get_option("--meta",argv,&i,argc)))
      sz_strlcpy(metaserver, option);
   else if ((option = get_option("--Sound", argv, &i, argc)))
      sz_strlcpy(sound_set_name, option);
   else if ((option = get_option("--Plugin", argv, &i, argc)))
      sz_strlcpy(sound_plugin_name, option);
   else if ((option = get_option("--port",argv,&i,argc))) {
     if(sscanf(option, "%d", &server_port) != 1) {
        exit(EXIT_FAILURE);
     }
   } else if ((option = get_option("--server",argv,&i,argc)))
      sz_strlcpy(server_host, option);
   else if (is_option("--autoconnect",argv[i]))
      auto_connect = TRUE;
   else if ((option = get_option("--debug",argv,&i,argc))) {
      loglevel=log_parse_level_str(option);
      if (loglevel==-1) {
        exit(EXIT_FAILURE);
      }
   } else if ((option = get_option("--tiles", argv, &i, argc)))
      sz_strlcpy(tile_set_name, option);
   else { 
      fprintf(stderr, _("Unrecognized option: \"%s\"\n"), argv[i]);
      exit(EXIT_FAILURE);
   }
   i++;
  } /* of while */

  log_init(logfile, loglevel, NULL);

  /* after log_init: */

  sz_strlcpy(default_player_name, user_username());

  /* initialization */

  ui_init();
  my_init_network();
  init_messages_where();
  init_city_report_data();

  load_general_options();

  if (tile_set_name[0] == '\0') 
    sz_strlcpy(tile_set_name, default_tile_set_name); 
  if (sound_set_name[0] == '\0') 
    sz_strlcpy(sound_set_name, default_sound_set_name); 
  if (sound_plugin_name[0] == '\0')
    sz_strlcpy(sound_plugin_name, default_sound_plugin_name); 
  if (server_host[0] == '\0')
    sz_strlcpy(server_host, default_server_host); 
  if (player_name[0] == '\0')
    sz_strlcpy(player_name, default_player_name); 
  if (metaserver[0] == '\0')
    sz_strlcpy(metaserver, default_metaserver); 
  if (server_port == -1) server_port = default_server_port;


  /* This seed is not saved anywhere; randoms in the client should
     have cosmetic effects only (eg city name suggestions).  --dwp */
  mysrand(time(NULL));

  boot_help_texts();
  tilespec_read_toplevel(tile_set_name); /* get tile sizes etc */

  audio_real_init(sound_set_name, sound_plugin_name);
  audio_play_music("music_start", NULL);

  /* run gui-specific client */

  ui_main(argc, argv);

  /* termination */
  attribute_flush();
  my_shutdown_network();

  client_game_free();

  exit(EXIT_SUCCESS);
}


/**************************************************************************
...
**************************************************************************/
void handle_packet_input(void *packet, int type)
{
  switch(type) {
  case PACKET_JOIN_GAME_REPLY:
    handle_join_game_reply((struct packet_join_game_reply *)packet);
    break;

  case PACKET_SERVER_SHUTDOWN:
    freelog(LOG_VERBOSE, "server shutdown");
    break;

  case PACKET_BEFORE_NEW_YEAR:
    handle_before_new_year();
    break;

  case PACKET_NEW_YEAR:
    handle_new_year((struct packet_new_year *)packet);
    break;

  case PACKET_UNIT_INFO:
    handle_unit_info((struct packet_unit_info *)packet);
    break;

   case PACKET_MOVE_UNIT:
    handle_move_unit();
    break;
    
  case PACKET_TILE_INFO:
    handle_tile_info((struct packet_tile_info *)packet);
    break;

  case PACKET_SELECT_NATION:
    handle_select_nation((struct packet_nations_used *)packet);
    break;

  case PACKET_PLAYER_INFO:
    handle_player_info((struct packet_player_info *)packet);
    break;
    
  case PACKET_GAME_INFO:
    handle_game_info((struct packet_game_info *)packet);
    break;

  case PACKET_MAP_INFO:
    handle_map_info((struct packet_map_info *)packet);
    break;
    
  case PACKET_CHAT_MSG:
    handle_chat_msg((struct packet_generic_message *)packet);
    break;

  case PACKET_PAGE_MSG:
    handle_page_msg((struct packet_generic_message *)packet);
    break;
    
  case PACKET_CITY_INFO:
    handle_city_info((struct packet_city_info *)packet);
    break;

  case PACKET_SHORT_CITY:
    handle_short_city((struct packet_short_city *)packet);
    break;

  case PACKET_REMOVE_UNIT:
    handle_remove_unit((struct packet_generic_integer *)packet);
    break;

  case PACKET_REMOVE_CITY:
    handle_remove_city((struct packet_generic_integer *)packet);
    break;
    
  case PACKET_UNIT_COMBAT:
    handle_unit_combat((struct packet_unit_combat *)packet);
    break;

  case PACKET_GAME_STATE:
    handle_game_state(((struct packet_generic_integer *)packet));
    break;

  case PACKET_NUKE_TILE:
    handle_nuke_tile(((struct packet_nuke_tile *)packet));
    break;

  case PACKET_DIPLOMACY_INIT_MEETING:
    handle_diplomacy_init_meeting((struct packet_diplomacy_info *)packet);  
    break;

  case PACKET_DIPLOMACY_CANCEL_MEETING:
    handle_diplomacy_cancel_meeting((struct packet_diplomacy_info *)packet);  
    break;

  case PACKET_DIPLOMACY_CREATE_CLAUSE:
    handle_diplomacy_create_clause((struct packet_diplomacy_info *)packet);  
    break;

  case PACKET_DIPLOMACY_REMOVE_CLAUSE:
    handle_diplomacy_remove_clause((struct packet_diplomacy_info *)packet);  
    break;

  case PACKET_DIPLOMACY_ACCEPT_TREATY:
    handle_diplomacy_accept_treaty((struct packet_diplomacy_info *)packet);  
    break;

  case PACKET_REMOVE_PLAYER:
    handle_remove_player((struct packet_generic_integer *)packet);
    break;

  case PACKET_RULESET_CONTROL:
    handle_ruleset_control((struct packet_ruleset_control *)packet);
    break;

  case PACKET_RULESET_UNIT:
    handle_ruleset_unit((struct packet_ruleset_unit *)packet);
    break;
    
  case PACKET_RULESET_TECH:
    handle_ruleset_tech((struct packet_ruleset_tech *)packet);
    break;
    
  case PACKET_RULESET_BUILDING:
    handle_ruleset_building((struct packet_ruleset_building *)packet);
    break;

  case PACKET_RULESET_TERRAIN:
    handle_ruleset_terrain((struct packet_ruleset_terrain *)packet);
    break;

  case PACKET_RULESET_TERRAIN_CONTROL:
    handle_ruleset_terrain_control((struct terrain_misc *)packet);
    break;

  case PACKET_RULESET_GOVERNMENT:
    handle_ruleset_government((struct packet_ruleset_government *)packet);
    break;
  
  case PACKET_RULESET_GOVERNMENT_RULER_TITLE:
    handle_ruleset_government_ruler_title((struct packet_ruleset_government_ruler_title *)packet);
    break;
  
  case PACKET_RULESET_NATION:
    handle_ruleset_nation((struct packet_ruleset_nation *)packet);
    break;

  case PACKET_RULESET_CITY:
    handle_ruleset_city((struct packet_ruleset_city *)packet);
    break;

  case PACKET_RULESET_GAME:
    handle_ruleset_game((struct packet_ruleset_game *)packet);
    break;

  case PACKET_INCITE_COST:
    handle_incite_cost((struct packet_generic_values *)packet);
    break;

  case PACKET_CITY_OPTIONS:
    handle_city_options((struct packet_generic_values *)packet);
    break;
    
  case PACKET_SPACESHIP_INFO:
    handle_spaceship_info((struct packet_spaceship_info *)packet);
    break;
    
  case PACKET_CITY_NAME_SUGGESTION:
    handle_city_name_suggestion((struct packet_city_name_suggestion *)packet);
    break;

  case PACKET_SABOTAGE_LIST:
    handle_sabotage_list((struct packet_sabotage_list *)packet);
    break;

  case PACKET_DIPLOMAT_ACTION:
    handle_diplomat_action((struct packet_diplomat_action *)packet);
    break;

  case PACKET_ADVANCE_FOCUS:
    handle_advance_focus((struct packet_generic_integer *)packet);
    break;
    
  case PACKET_CONN_INFO:
    handle_conn_info((struct packet_conn_info *)packet);
    break;
    
  case PACKET_CONN_PING:
    send_packet_generic_empty(&aconnection, PACKET_CONN_PONG);
    break;

  case PACKET_SELECT_NATION_OK:
    handle_select_nation(NULL);
    break;

  case PACKET_ATTRIBUTE_CHUNK:
    handle_player_attribute_chunk((struct packet_attribute_chunk *)
 				  packet);
    break;

  case PACKET_PROCESSING_STARTED:
    handle_processing_started();
    break;

  case PACKET_PROCESSING_FINISHED:
    handle_processing_finished();
    break;

  case PACKET_START_TURN:
    handle_start_turn();
    break;

  case PACKET_FREEZE_HINT:
    handle_freeze_hint();
    break;

  case PACKET_THAW_HINT:
    handle_thaw_hint();
    break;

  default:
    freelog(LOG_ERROR, "Received unknown packet (type %d) from server!", type);
    /* Old clients (<= some 1.11.5-devel, capstr +1.11) used to exit()
     * here, so server should not rely on client surviving.
     */
    break;
  }

  free(packet);
}

/**************************************************************************
...
**************************************************************************/
void user_ended_turn(void)
{
  send_turn_done();
}

/**************************************************************************
...
**************************************************************************/
void send_turn_done(void)
{
  struct packet_generic_message gen_packet;

  freelog(LOG_DEBUG, "send_turn_done() turn_done_button_state=%d",
	  get_turn_done_button_state());

  if (!get_turn_done_button_state()) {
    /*
     * The turn done button is disabled but the user may have press
     * the return key.
     */

    if (agents_busy()) {
      waiting_for_end_turn = TRUE;
    }

    return;
  }

  waiting_for_end_turn = FALSE;
  turn_done_sent = TRUE;

  attribute_flush();

  gen_packet.message[0] = '\0';

  send_packet_generic_message(&aconnection, PACKET_TURN_DONE, &gen_packet);

  update_turn_done_button_state();
}

/**************************************************************************
...
**************************************************************************/
void send_unit_info(struct unit *punit)
{
  struct packet_unit_info info;

  info.id=punit->id;
  info.owner=punit->owner;
  info.x=punit->x;
  info.y=punit->y;
  info.homecity=punit->homecity;
  info.veteran=punit->veteran;
  info.type=punit->type;
  info.movesleft=punit->moves_left;
  info.activity=punit->activity;
  info.activity_target=punit->activity_target;
  info.select_it = FALSE;
  info.packet_use = UNIT_INFO_IDENTITY;

  send_packet_unit_info(&aconnection, &info);
}

/**************************************************************************
...
**************************************************************************/
void send_move_unit(struct unit *punit)
{
  struct packet_move_unit move;

  move.unid=punit->id;
  move.x=punit->x;
  move.y=punit->y;

  send_packet_move_unit(&aconnection, &move);
}

/**************************************************************************
...
**************************************************************************/
void send_goto_unit(struct unit *punit, int dest_x, int dest_y)
{
  struct packet_unit_request req;

  req.unit_id = punit->id;
  req.name[0] = '\0';
  req.x = dest_x;
  req.y = dest_y;
  send_packet_unit_request(&aconnection, &req, PACKET_UNIT_GOTO_TILE);
}

/**************************************************************************
...
**************************************************************************/
void send_report_request(enum report_type type)
{
 struct packet_generic_integer pa;
  
  pa.value=type;
  send_packet_generic_integer(&aconnection, PACKET_REPORT_REQUEST, &pa);
}

/**************************************************************************
 called whenever client is changed to pre-game state.
**************************************************************************/
void client_game_init()
{
  conn_list_init(&game.all_connections);
  conn_list_init(&game.est_connections);
  conn_list_init(&game.game_connections);

  game_init();
  attribute_init();
  agents_init();
}

/**************************************************************************
...
**************************************************************************/
void client_game_free()
{
  client_remove_all_cli_conn();
  free_client_goto();
  free_help_texts();
  attribute_free();
  agents_free();
  game_free();
}

/**************************************************************************
...
**************************************************************************/
void set_client_state(enum client_states newstate)
{
  bool connect_error = (client_state == CLIENT_PRE_GAME_STATE)
      && (newstate == CLIENT_PRE_GAME_STATE);
  enum client_states oldstate = client_state;

  /*
   * We are currently ignoring the CLIENT_GAME_OVER_STATE state
   * because the client hasen't been changed to take care of it. So it
   * breaks the show-whole-map-at-the-end-of-the-game. Nevertheless
   * the server is so kind and sends the client this information. And
   * in the future the client can/should take advantage of this
   * information.
   *
   * FIXME: audit all client code to that it copes with
   * CLIENT_GAME_OVER_STATE and implement specific
   * CLIENT_GAME_OVER_STATE actions like history browsing. Then remove
   * the kludge below.
   */
  if (newstate == CLIENT_GAME_OVER_STATE) {
    /*
     * Extra kludge for end-game handling of the CMA.
     */
    city_list_iterate(game.player_ptr->cities, pcity) {
      if (cma_is_city_under_agent(pcity, NULL)) {
        cma_release_city(pcity);
      }
    } city_list_iterate_end;
    newstate = CLIENT_GAME_RUNNING_STATE;
  }

  if(client_state!=newstate) {

    /* If changing from pre-game state to _either_ select race
       or running state, then we have finished getting ruleset data,
       and should translate data, for joining running game or for
       selecting nations.  (Want translated nation names in nation
       select dialog.)
    */
    if (client_state==CLIENT_PRE_GAME_STATE
	&& (newstate==CLIENT_SELECT_RACE_STATE
	    || newstate==CLIENT_GAME_RUNNING_STATE)) {
      translate_data_names();
      audio_stop();		/* stop intro sound loop */
    }
      
    client_state=newstate;

    if(client_state==CLIENT_GAME_RUNNING_STATE) {
      load_ruleset_specific_options();
      create_event(-1, -1, E_GAME_START, _("Game started."));
      update_research(game.player_ptr);
      role_unit_precalcs();
      boot_help_texts();	/* reboot */
      update_unit_focus();
    }
    else if(client_state==CLIENT_PRE_GAME_STATE) {
      popdown_all_city_dialogs();
      close_all_diplomacy_dialogs();
      set_unit_focus_no_center(NULL);
      clear_notify_window();
      if (oldstate != CLIENT_BOOT_STATE) {
	client_game_free();
      }
      client_game_init();
    }
    update_menus();
  }
  if (client_state == CLIENT_PRE_GAME_STATE) {
    if (auto_connect) {
      if (connect_error) {
	freelog(LOG_NORMAL,
		_("There was an error while auto connecting; aborting."));
	exit(EXIT_FAILURE);
      } else {
	server_autoconnect();
        auto_connect = 0;  /* don't try this again */
      }
    } else {
      gui_server_connect();
    }
  }
  update_turn_done_button_state();
}


/**************************************************************************
...
**************************************************************************/
enum client_states get_client_state(void)
{
  return client_state;
}

/**************************************************************************
  Remove pconn from all connection lists in client, then free it.
**************************************************************************/
void client_remove_cli_conn(struct connection *pconn)
{
  if (pconn->player) {
    conn_list_unlink(&pconn->player->connections, pconn);
  }
  conn_list_unlink(&game.all_connections, pconn);
  conn_list_unlink(&game.est_connections, pconn);
  conn_list_unlink(&game.game_connections, pconn);
  free(pconn);
}

/**************************************************************************
  Remove (and free) all connections from all connection lists in client.
  Assumes game.all_connections is properly maintained with all connections.
**************************************************************************/
static void client_remove_all_cli_conn(void)
{
  while (conn_list_size(&game.all_connections) > 0) {
    struct connection *pconn = conn_list_get(&game.all_connections, 0);
    client_remove_cli_conn(pconn);
  }
}

void dealloc_id(int id); /* double kludge (suppress a possible warning) */
void dealloc_id(int id) { }/* kludge */

/**************************************************************************
..
**************************************************************************/
void send_attribute_block_request()
{
  struct packet_player_request packet;

  send_packet_player_request(&aconnection, &packet,
			     PACKET_PLAYER_ATTRIBUTE_BLOCK);
}

/**************************************************************************
..
**************************************************************************/
void wait_till_request_got_processed(int request_id)
{
  input_from_server_till_request_got_processed(aconnection.sock,
					       request_id);
}

/**************************************************************************
..
**************************************************************************/
bool client_is_observer(void)
{
  return aconnection.established && find_conn_by_id(game.conn_id)->observer;
}

/**************************************************************************
 This function should be called every 500ms. It lets the unit blink
 and update the timeout.
**************************************************************************/
void real_timer_callback(void)
{
  static bool flip = FALSE;

  if (get_client_state() != CLIENT_GAME_RUNNING_STATE) {
    return;
  }

  if (game.player_ptr->is_connected && game.player_ptr->is_alive &&
      !game.player_ptr->turn_done) {
    int is_waiting = 0, is_moving = 0;

    players_iterate(pplayer) {
      if (pplayer->is_alive && pplayer->is_connected) {
	if (pplayer->turn_done) {
	  is_waiting++;
	} else {
	  is_moving++;
	}
      }
    } players_iterate_end;

    if (is_moving == 1 && is_waiting > 0) {
      update_turn_done_button(FALSE);	/* stress the slow player! */
    }
  }

  blink_active_unit();

  if (flip) {
    update_timeout_label();
    if (seconds_to_turndone > 0) {
      seconds_to_turndone--;
    } else {
      seconds_to_turndone = 0;
    }
  }

  flip = !flip;
}
