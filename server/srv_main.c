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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_TERMIO_H
#include <sys/termio.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_WINSOCK
#include <winsock.h>
#endif

#include "capability.h"
#include "capstr.h"
#include "city.h"
#include "dataio.h"
#include "effects.h"
#include "events.h"
#include "fciconv.h"
#include "fcintl.h"
#include "game.h"
#include "government.h"
#include "log.h"
#include "map.h"
#include "mem.h"
#include "nation.h"
#include "netintf.h"
#include "packets.h"
#include "player.h"
#include "rand.h"
#include "registry.h"
#include "shared.h"
#include "support.h"
#include "tech.h"
#include "timing.h"
#include "version.h"

#include "barbarian.h"
#include "cityhand.h"
#include "citytools.h"
#include "cityturn.h"
#include "connecthand.h"
#include "console.h"
#include "diplhand.h"
#include "gamehand.h"
#include "gamelog.h"
#include "handchat.h"
#include "maphand.h"
#include "meta.h"
#include "plrhand.h"
#include "report.h"
#include "ruleset.h"
#include "sanitycheck.h"
#include "savegame.h"
#include "score.h"
#include "script_signal.h"
#include "sernet.h"
#include "settlers.h"
#include "spacerace.h"
#include "stdinhand.h"
#include "unithand.h"
#include "unittools.h"

#include "advdiplomacy.h"
#include "advmilitary.h"
#include "aicity.h"
#include "aidata.h"
#include "aihand.h"
#include "aisettler.h"
#include "citymap.h"

#include "mapgen.h"

#include "srv_main.h"


static void end_turn(void);
static void save_game_auto(const char *save_reason);
static void announce_player(struct player *pplayer);
static void srv_loop(void);


/* this is used in strange places, and is 'extern'd where
   needed (hence, it is not 'extern'd in srv_main.h) */
bool is_server = TRUE;

/* command-line arguments to server */
struct server_arguments srvarg;

/* server state information */
enum server_states server_state = PRE_GAME_STATE;
bool nocity_send = FALSE;

/* this global is checked deep down the netcode. 
   packets handling functions can set it to none-zero, to
   force end-of-tick asap
*/
bool force_end_of_sniff;

/* this counter creates all the id numbers used */
/* use get_next_id_number()                     */
static unsigned short global_id_counter=100;
static unsigned char used_ids[8192]={0};

/* server initialized flag */
static bool has_been_srv_init = FALSE;

/**************************************************************************
  Initialize the game seed.  This may safely be called multiple times.
**************************************************************************/
void init_game_seed(void)
{
  if (game.seed == 0) {
    /* We strip the high bit for now because neither game file nor
       server options can handle unsigned ints yet. - Cedric */
    game.seed = time(NULL) & (MAX_UINT32 >> 1);
    freelog(LOG_DEBUG, "Setting game.seed:%d", game.seed);
  }
 
  if (!myrand_is_init()) {
    mysrand(game.seed);
  }
}

/**************************************************************************
...
**************************************************************************/
void srv_init(void)
{
  /* NLS init */
  init_nls();

  /* init server arguments... */

  srvarg.metaserver_no_send = DEFAULT_META_SERVER_NO_SEND;
  sz_strlcpy(srvarg.metaserver_addr, DEFAULT_META_SERVER_ADDR);

  srvarg.bind_addr = NULL;
  srvarg.port = DEFAULT_SOCK_PORT;

  srvarg.loglevel = LOG_NORMAL;

  srvarg.log_filename = NULL;
  srvarg.gamelog_filename = NULL;
  srvarg.load_filename[0] = '\0';
  srvarg.script_filename = NULL;
  srvarg.saves_pathname = "";

  srvarg.quitidle = 0;

  srvarg.auth_enabled = FALSE;
  srvarg.auth_allow_guests = FALSE;
  srvarg.auth_allow_newusers = FALSE;

  /* initialize teams */
  team_init();

  /* mark as initialized */
  has_been_srv_init = TRUE;

  /* init character encodings. */
  init_character_encodings(FC_DEFAULT_DATA_ENCODING, FALSE);

  /* Initialize callbacks. */
  game.callbacks.unit_deallocate = dealloc_id;

  /* done */
  return;
}

/**************************************************************************
  Returns TRUE if any one game end condition is fulfilled, FALSE otherwise
**************************************************************************/
bool is_game_over(void)
{
  int barbs = 0, alive = 0, observers = 0;
  bool all_allied;
  struct player *victor = NULL;

  /* quit if we are past the year limit */
  if (game.info.year > game.info.end_year) {
    notify_conn_ex(game.est_connections, NULL, E_GAME_END, 
		   _("Game ended in a draw as end year exceeded"));
    gamelog(GAMELOG_JUDGE, GL_DRAW, 
            "Game ended in a draw as end year exceeded");
    return TRUE;
  }

  /* count barbarians and observers */
  players_iterate(pplayer) {
    if (is_barbarian(pplayer)) {
      barbs++;
    }
    if (pplayer->is_observer) {
      observers++;
    }
  } players_iterate_end;

  /* count the living */
  players_iterate(pplayer) {
    if (pplayer->is_alive
        && !is_barbarian(pplayer)
        && !pplayer->surrendered) {
      alive++;
      victor = pplayer;
    }
  } players_iterate_end;

  /* the game does not quit if we are playing solo */
  if (game.info.nplayers == (observers + barbs + 1)
      && alive >= 1) {
    return FALSE;
  }

  /* quit if we have team victory */
  team_iterate(pteam) {
    bool win = TRUE; /* optimistic */

    /* If there are any players alive and unconceded outside our
     * team, we have not yet won. */
    players_iterate(pplayer) {
      if (pplayer->is_alive
          && !pplayer->surrendered
          && pplayer->team != pteam->id) {
        win = FALSE;
        break;
      }
    } players_iterate_end;
    if (win) {
      notify_conn_ex(game.est_connections, NULL, E_GAME_END,
		     _("Team victory to %s"), pteam->name);
      gamelog(GAMELOG_JUDGE, GL_TEAMWIN, pteam);
      return TRUE;
    }
  } team_iterate_end;

  /* quit if only one player is left alive */
  if (alive == 1) {
    notify_conn_ex(game.est_connections, NULL, E_GAME_END,
		   _("Game ended in victory for %s"), victor->name);
    gamelog(GAMELOG_JUDGE, GL_LONEWIN, victor);
    return TRUE;
  } else if (alive == 0) {
    notify_conn_ex(game.est_connections, NULL, E_GAME_END, 
		   _("Game ended in a draw"));
    gamelog(GAMELOG_JUDGE, GL_DRAW);
    return TRUE;
  }

  /* quit if all remaining players are allied to each other */
  all_allied = TRUE;
  players_iterate(pplayer) {
    players_iterate(aplayer) {
      if (!pplayers_allied(pplayer, aplayer)
          && pplayer->is_alive
          && aplayer->is_alive
          && !pplayer->surrendered
          && !aplayer->surrendered) {
        all_allied = FALSE;
        break;
      }
    } players_iterate_end;
    if (!all_allied) {
      break;
    }
  } players_iterate_end;
  if (all_allied) {
    notify_conn_ex(game.est_connections, NULL, E_GAME_END, 
		   _("Game ended in allied victory"));
    gamelog(GAMELOG_JUDGE, GL_ALLIEDWIN);
    return TRUE;
  }

  return FALSE;
}

/**************************************************************************
  Send all information for when game starts or client reconnects.
  Ruleset information should have been sent before this.
**************************************************************************/
void send_all_info(struct conn_list *dest)
{
  conn_list_iterate(dest, pconn) {
      send_attribute_block(pconn->player,pconn);
  }
  conn_list_iterate_end;

  send_game_info(dest);
  send_map_info(dest);
  send_player_info_c(NULL, dest);
  send_conn_info(game.est_connections, dest);
  send_spaceship_info(NULL, dest);
  send_all_known_tiles(dest);
  send_all_known_cities(dest);
  send_all_known_units(dest);
  send_player_turn_notifications(dest);
}

/**************************************************************************
  Give map information to players with EFT_REVEAL_CITIES or
  EFT_REVEAL_MAP effects (traditionally from the Apollo Program).
**************************************************************************/
static void do_reveal_effects(void)
{
  phase_players_iterate(pplayer) {
    if (get_player_bonus(pplayer, EFT_REVEAL_CITIES) > 0) {
      players_iterate(other_player) {
	city_list_iterate(other_player->cities, pcity) {
	  show_area(pplayer, pcity->tile, 0);
	} city_list_iterate_end;
      } players_iterate_end;
    }
    if (get_player_bonus(pplayer, EFT_REVEAL_MAP) > 0) {
      /* map_know_all will mark all unknown tiles as known and send
       * tile, unit, and city updates as necessary.  No other actions are
       * needed. */
      map_know_all(pplayer);
    }
  } phase_players_iterate_end;
}

/**************************************************************************
  Give contact to players with the EFT_HAVE_EMBASSIES effect (traditionally
  from Marco Polo's Embassy).
**************************************************************************/
static void do_have_embassies_effect(void)
{
  phase_players_iterate(pplayer) {
    if (get_player_bonus(pplayer, EFT_HAVE_EMBASSIES) > 0) {
      players_iterate(pother) {
	/* Note this gives pplayer contact with pother, but doesn't give
	 * pother contact with pplayer.  This may cause problems in other
	 * parts of the code if we're not careful. */
	make_contact(pplayer, pother, NULL);
      } players_iterate_end;
    }
  } phase_players_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
static void update_environmental_upset(enum tile_special_type cause,
				       int *current, int *accum, int *level,
				       void (*upset_action_fn)(int))
{
  int count;

  count = 0;
  whole_map_iterate(ptile) {
    if (tile_has_special(ptile, cause)) {
      count++;
    }
  } whole_map_iterate_end;

  *current = count;
  *accum += count;
  if (*accum < *level) {
    *accum = 0;
  } else {
    *accum -= *level;
    if (myrand((map_num_tiles() + 19) / 20) <= *accum) {
      upset_action_fn((map.xsize / 10) + (map.ysize / 10) + ((*accum) * 5));
      *accum = 0;
      *level += (map_num_tiles() + 999) / 1000;
    }
  }

  freelog(LOG_DEBUG,
	  "environmental_upset: cause=%-4d current=%-2d level=%-2d accum=%-2d",
	  cause, *current, *level, *accum);
}

/**************************************************************************
 check for cease-fires running out; update cancelling reasons
**************************************************************************/
static void update_diplomatics(void)
{
  players_iterate(player1) {
    players_iterate(player2) {
      struct player_diplstate *pdiplstate =
	  &player1->diplstates[player2->player_no];

      pdiplstate->has_reason_to_cancel =
	  MAX(pdiplstate->has_reason_to_cancel - 1, 0);

      pdiplstate->contact_turns_left =
	  MAX(pdiplstate->contact_turns_left - 1, 0);

      if(pdiplstate->type == DS_CEASEFIRE) {
	switch(--pdiplstate->turns_left) {
	case 1:
	  notify_player(player1,
			_("Concerned citizens point "
  			  "out that the cease-fire with %s will run out soon."),
			player2->name);
  	  break;
  	case 0:
	  notify_player(player1,
  			_("The cease-fire with %s has "
  			  "run out. You are now neutral towards the %s."),
			player2->name,
			get_nation_name_plural(player2->nation));
	  pdiplstate->type = DS_NEUTRAL;
	  check_city_workers(player1);
	  check_city_workers(player2);
  	  break;
  	}
        }
    } players_iterate_end;
  } players_iterate_end;
}

/**************************************************************************
  Called at the start of each (new) phase to do AI activities.
**************************************************************************/
static void ai_start_phase(void)
{
  phase_players_iterate(pplayer) {
    if (pplayer->ai.control) {
      ai_do_first_activities(pplayer);
      flush_packets();			/* AIs can be such spammers... */
    }
  } phase_players_iterate_end;
  kill_dying_players();
}

/**************************************************************************
Handle the beginning of each turn.
Note: This does not give "time" to any player;
      it is solely for updating turn-dependent data.
**************************************************************************/
static void begin_turn(bool is_new_turn)
{
  freelog(LOG_DEBUG, "Begin turn");

  /* Reset this each turn. */
  if (is_new_turn) {
    game.info.simultaneous_phases = game.simultaneous_phases_stored;
  }
  if (game.info.simultaneous_phases) {
    game.info.num_phases = 1;
  } else {
    game.info.num_phases = game.info.nplayers;
  }
  send_game_info(game.game_connections);

  if (is_new_turn) {
    /* We build scores at the beginning of every turn.  We have to
     * build them at the beginning so that the AI can use the data,
     * and we are sure to have it when we need it. */
    players_iterate(pplayer) {
      calc_civ_score(pplayer);
    } players_iterate_end;
  }

  /* See if the value of fog of war has changed */
  if (is_new_turn && game.info.fogofwar != game.fogofwar_old) {
    if (game.info.fogofwar) {
      enable_fog_of_war();
      game.fogofwar_old = TRUE;
    } else {
      disable_fog_of_war();
      game.fogofwar_old = FALSE;
    }
  }

  if (is_new_turn && game.info.simultaneous_phases) {
    freelog(LOG_DEBUG, "Shuffleplayers");
    shuffle_players();
  }

  if (is_new_turn) {
    game.info.phase = 0;
  }

  sanity_check();
}

/**************************************************************************
  Begin a phase of movement.  This handles all beginning-of-phase actions
  for one or more players.
**************************************************************************/
static void begin_phase(bool is_new_phase)
{
  freelog(LOG_DEBUG, "Begin phase");

  conn_list_do_buffer(game.game_connections);
  ai_data_movemap_recalculate();

  phase_players_iterate(pplayer) {
    pplayer->phase_done = FALSE;
  } phase_players_iterate_end;
  send_player_info(NULL, NULL);

  send_start_phase_to_clients();

  if (is_new_phase) {
    /* Unit "end of turn" activities - of course these actually go at
     * the start of the turn! */
    phase_players_iterate(pplayer) {
      update_unit_activities(pplayer); /* major network traffic */
      flush_packets();
    } phase_players_iterate_end;
  }

  phase_players_iterate(pplayer) {
    freelog(LOG_DEBUG, "beginning player turn for #%d (%s)",
	    pplayer->player_no, pplayer->name);
    /* human players also need this for building advice */
    ai_data_phase_init(pplayer, is_new_phase);
    if (!pplayer->ai.control) {
      ai_manage_buildings(pplayer); /* building advisor */
    }
  } phase_players_iterate_end;

  phase_players_iterate(pplayer) {
    send_player_cities(pplayer);
  } phase_players_iterate_end;

  flush_packets();  /* to curb major city spam */
  conn_list_do_unbuffer(game.game_connections);

  phase_players_iterate(pplayer) {
    update_revolution(pplayer);
  } phase_players_iterate_end;

  if (is_new_phase) {
    /* Try to avoid hiding events under a diplomacy dialog */
    phase_players_iterate(pplayer) {
      if (pplayer->ai.control && !is_barbarian(pplayer)) {
	ai_diplomacy_actions(pplayer);
      }
    } phase_players_iterate_end;

    freelog(LOG_DEBUG, "Aistartturn");
    ai_start_phase();
  }

  sanity_check();

  game.info.seconds_to_phasedone = (double)game.info.timeout;
  game.phase_timer = renew_timer_start(game.phase_timer,
				       TIMER_USER, TIMER_ACTIVE);
  send_game_info(NULL);
}

/**************************************************************************
  End a phase of movement.  This handles all end-of-phase actions
  for one or more players.
**************************************************************************/
static void end_phase(void)
{
  freelog(LOG_DEBUG, "Endphase");
 
  /* 
   * This empties the client Messages window; put this before
   * everything else below, since otherwise any messages from the
   * following parts get wiped out before the user gets a chance to
   * see them.  --dwp
   */
  phase_players_iterate(pplayer) {
    /* Unlike the start_phase packet we only send this one to the active
     * player. */
    lsend_packet_end_phase(pplayer->connections);
  } phase_players_iterate_end;

  phase_players_iterate(pplayer) {
    if (pplayer->research->researching == A_UNSET) {
      choose_random_tech(pplayer);
      update_tech(pplayer, 0);
    }
  } phase_players_iterate_end;

  /* Freeze sending of cities. */
  nocity_send = TRUE;

  /* AI end of turn activities */
  players_iterate(pplayer) {
    unit_list_iterate(pplayer->units, punit) {
      punit->ai.hunted = 0;
    } unit_list_iterate_end;
  } players_iterate_end;
  phase_players_iterate(pplayer) {
    if (pplayer->ai.control) {
      ai_settler_init(pplayer);
    }
    auto_settlers_player(pplayer);
    if (pplayer->ai.control) {
      ai_do_last_activities(pplayer);
    }
  } phase_players_iterate_end;

  /* Refresh cities */
  phase_players_iterate(pplayer) {
    pplayer->research->got_tech = FALSE;
  } phase_players_iterate_end;
  
  phase_players_iterate(pplayer) {
    do_tech_parasite_effect(pplayer);
    player_restore_units(pplayer);
    update_city_activities(pplayer);
    pplayer->research->changed_from=-1;
    flush_packets();
  } phase_players_iterate_end;

  kill_dying_players();

  /* Unfreeze sending of cities. */
  nocity_send = FALSE;
  phase_players_iterate(pplayer) {
    send_player_cities(pplayer);
  } phase_players_iterate_end;
  flush_packets();  /* to curb major city spam */

  do_reveal_effects();
  do_have_embassies_effect();
}

/**************************************************************************
  Handle the end of each turn.
**************************************************************************/
static void end_turn(void)
{
  freelog(LOG_DEBUG, "Endturn");

  /* Output some ranking and AI debugging info here. */
  if (game.info.turn % 10 == 0) {
    players_iterate(pplayer) {
      gamelog(GAMELOG_INFO, pplayer);
    } players_iterate_end;
  }

  freelog(LOG_DEBUG, "Season of native unrests");
  summon_barbarians(); /* wild guess really, no idea where to put it, but
			  I want to give them chance to move their units */

  update_environmental_upset(S_POLLUTION, &game.info.heating,
			     &game.info.globalwarming, &game.info.warminglevel,
			     global_warming);
  update_environmental_upset(S_FALLOUT, &game.info.cooling,
			     &game.info.nuclearwinter, &game.info.coolinglevel,
			     nuclear_winter);
  update_diplomatics();
  make_history_report();
  stdinhand_turn();
  send_player_turn_notifications(NULL);

  freelog(LOG_DEBUG, "Gamenextyear");
  game_advance_year();

  freelog(LOG_DEBUG, "Updatetimeout");
  update_timeout();

  check_spaceship_arrivals();

  freelog(LOG_DEBUG, "Sendplayerinfo");
  send_player_info(NULL, NULL);

  freelog(LOG_DEBUG, "Sendgameinfo");
  send_game_info(NULL);

  freelog(LOG_DEBUG, "Sendyeartoclients");
  send_year_to_clients(game.info.year);
}

/**************************************************************************
Unconditionally save the game, with specified filename.
Always prints a message: either save ok, or failed.

Note that if !HAVE_LIBZ, then game.info.save_compress_level should never
become non-zero, so no need to check HAVE_LIBZ explicitly here as well.
**************************************************************************/
void save_game(char *orig_filename, const char *save_reason)
{
  char filename[600];
  char *dot;
  struct section_file file;
  struct timer *timer_cpu, *timer_user;

  if (!orig_filename) {
    filename[0] = '\0';
  } else {
    sz_strlcpy(filename, orig_filename);
  }

  /* Strip extension. */
  if ((dot = strchr(filename, '.'))) {
    *dot = '\0';
  }

  /* If orig_filename is NULL or empty, use "civgame.info.year>m". */
  if (filename[0] == '\0'){
    my_snprintf(filename, sizeof(filename),
	"%s%+05dm", game.save_name, game.info.year);
  }
  
  timer_cpu = new_timer_start(TIMER_CPU, TIMER_ACTIVE);
  timer_user = new_timer_start(TIMER_USER, TIMER_ACTIVE);
    
  section_file_init(&file);
  game_save(&file, save_reason);

  /* Append ".sav" to filename. */
  sz_strlcat(filename, ".sav");

  if (game.info.save_compress_level > 0) {
    /* Append ".gz" to filename. */
    sz_strlcat(filename, ".gz");
  }

  if (!path_is_absolute(filename)) {
    char tmpname[600];

    /* Ensure the saves directory exists. */
    make_dir(srvarg.saves_pathname);

    sz_strlcpy(tmpname, srvarg.saves_pathname);
    if (tmpname[0] != '\0') {
      sz_strlcat(tmpname, "/");
    }
    sz_strlcat(tmpname, filename);
    sz_strlcpy(filename, tmpname);
  }

  if(!section_file_save(&file, filename, game.info.save_compress_level))
    con_write(C_FAIL, _("Failed saving game as %s"), filename);
  else
    con_write(C_OK, _("Game saved as %s"), filename);

  section_file_free(&file);

  freelog(LOG_VERBOSE, "Save time: %g seconds (%g apparent)",
	  read_timer_seconds_free(timer_cpu),
	  read_timer_seconds_free(timer_user));
}

/**************************************************************************
Save game with autosave filename, and call gamelog_save().
**************************************************************************/
static void save_game_auto(const char *save_reason)
{
  char filename[512];

  assert(strlen(game.save_name)<256);
  
  my_snprintf(filename, sizeof(filename),
	      "%s%+05d.sav", game.save_name, game.info.year);
  save_game(filename, save_reason);
  gamelog(GAMELOG_STATUS);
}

/**************************************************************************
...
**************************************************************************/
void start_game(void)
{
  if(server_state!=PRE_GAME_STATE) {
    con_puts(C_SYNTAX, _("The game is already running."));
    return;
  }

  con_puts(C_OK, _("Starting game."));

  server_state = RUN_GAME_STATE; /* loaded ??? */
  force_end_of_sniff = TRUE;
}

/**************************************************************************
 Quit the server and exit.
**************************************************************************/
void server_quit(void)
{
  server_game_free();
  diplhand_free();
  stdinhand_free();
  close_connections_and_socket();
  exit(EXIT_SUCCESS);
}

/**************************************************************************
...
**************************************************************************/
void handle_report_req(struct connection *pconn, enum report_type type)
{
  struct conn_list *dest = pconn->self;
  
  if (server_state != RUN_GAME_STATE && server_state != GAME_OVER_STATE
      && type != REPORT_SERVER_OPTIONS1 && type != REPORT_SERVER_OPTIONS2) {
    freelog(LOG_ERROR, "Got a report request %d before game start", type);
    return;
  }

  switch(type) {
   case REPORT_WONDERS_OF_THE_WORLD:
    report_wonders_of_the_world(dest);
    break;
   case REPORT_TOP_5_CITIES:
    report_top_five_cities(dest);
    break;
   case REPORT_DEMOGRAPHIC:
    report_demographics(pconn);
    break;
  case REPORT_SERVER_OPTIONS1:
    report_settable_server_options(pconn, 1);
    break;
  case REPORT_SERVER_OPTIONS2:
    report_settable_server_options(pconn, 2);
    break;
  case REPORT_SERVER_OPTIONS: /* obsolete */
  default:
    notify_conn(dest, _("request for unknown report (type %d)"), type);
  }
}

/**************************************************************************
...
**************************************************************************/
void dealloc_id(int id)
{
  used_ids[id/8]&= 0xff ^ (1<<(id%8));
}

/**************************************************************************
...
**************************************************************************/
static bool is_id_allocated(int id)
{
  return TEST_BIT(used_ids[id / 8], id % 8);
}

/**************************************************************************
...
**************************************************************************/
void alloc_id(int id)
{
  used_ids[id/8]|= (1<<(id%8));
}

/**************************************************************************
...
**************************************************************************/

int get_next_id_number(void)
{
  while (is_id_allocated(++global_id_counter) || global_id_counter == 0) {
    /* nothing */
  }
  return global_id_counter;
}

/**************************************************************************
Returns 0 if connection should be closed (because the clients was
rejected). Returns 1 else.
**************************************************************************/
bool handle_packet_input(struct connection *pconn, void *packet, int type)
{
  struct player *pplayer;

  /* a NULL packet can be returned from receive_packet_goto_route() */
  if (!packet)
    return TRUE;

  /* 
   * Old pre-delta clients (before 2003-11-28) send a
   * PACKET_LOGIN_REQUEST (type 0) to the server. We catch this and
   * reply with an old reject packet. Since there is no struct for
   * this old packet anymore we build it by hand.
   */
  if (type == 0) {
    unsigned char buffer[4096];
    struct data_out dout;

    freelog(LOG_ERROR,
	    _("Warning: rejecting old client %s"), conn_description(pconn));

    dio_output_init(&dout, buffer, sizeof(buffer));
    dio_put_uint16(&dout, 0);

    /* 1 == PACKET_LOGIN_REPLY in the old client */
    dio_put_uint8(&dout, 1);

    dio_put_bool32(&dout, FALSE);
    dio_put_string(&dout, _("Your client is too old. To use this server "
			    "please upgrade your client to a CVS version "
			    "later than 2003-11-28 or Freeciv 1.15.0 or "
			    "later."));
    dio_put_string(&dout, "");

    {
      size_t size = dio_output_used(&dout);
      dio_output_rewind(&dout);
      dio_put_uint16(&dout, size);

      /* 
       * Use send_connection_data instead of send_packet_data to avoid
       * compression.
       */
      send_connection_data(pconn, buffer, size);
    }

    return FALSE;
  }

  if (type == PACKET_SERVER_JOIN_REQ) {
    return handle_login_request(pconn,
				(struct packet_server_join_req *) packet);
  }

  /* May be received on a non-established connection. */
  if (type == PACKET_AUTHENTICATION_REPLY) {
    return handle_authentication_reply(pconn,
				((struct packet_authentication_reply *)
				 packet)->password);
  }

  if (type == PACKET_CONN_PONG) {
    handle_conn_pong(pconn);
    return TRUE;
  }

  if (!pconn->established) {
    freelog(LOG_ERROR, "Received game packet from unaccepted connection %s",
	    conn_description(pconn));
    return TRUE;
  }
  
  /* valid packets from established connections but non-players */
  if (type == PACKET_CHAT_MSG_REQ) {
    handle_chat_msg_req(pconn,
			((struct packet_chat_msg_req *) packet)->message);
    return TRUE;
  }

  if (type == PACKET_SINGLE_WANT_HACK_REQ) {
    handle_single_want_hack_req(pconn,
		                (struct packet_single_want_hack_req *) packet);
    return TRUE;
  }

  pplayer = pconn->player;

  if(!pplayer) {
    /* don't support these yet */
    freelog(LOG_ERROR, "Received packet from non-player connection %s",
 	    conn_description(pconn));
    return TRUE;
  }

  if (server_state != RUN_GAME_STATE
      && type != PACKET_NATION_SELECT_REQ
      && type != PACKET_CONN_PONG
      && type != PACKET_REPORT_REQ) {
    if (server_state == GAME_OVER_STATE) {
      /* This can happen by accident, so we don't want to print
	 out lots of error messages. Ie, we use LOG_DEBUG. */
      freelog(LOG_DEBUG, "got a packet of type %d "
			  "in GAME_OVER_STATE", type);
    } else {
      freelog(LOG_ERROR, "got a packet of type %d "
	                 "outside RUN_GAME_STATE", type);
    }
    return TRUE;
  }

  pplayer->nturns_idle=0;

  if((!pplayer->is_alive || pconn->observer)
     && !(type == PACKET_REPORT_REQ || type == PACKET_CONN_PONG)) {
    freelog(LOG_ERROR, _("Got a packet of type %d from a "
			 "dead or observer player"), type);
    return TRUE;
  }
  
  /* Make sure to set this back to NULL before leaving this function: */
  pplayer->current_conn = pconn;

  if (!server_handle_packet(type, packet, pplayer, pconn)) {
    freelog(LOG_ERROR, "Received unknown packet %d from %s",
	    type, conn_description(pconn));
  }

  if (server_state == RUN_GAME_STATE) {
    kill_dying_players();
  }

  pplayer->current_conn = NULL;
  return TRUE;
}

/**************************************************************************
...
**************************************************************************/
void check_for_full_turn_done(void)
{
  bool connected = FALSE;

  /* fixedlength is only applicable if we have a timeout set */
  if (game.info.fixedlength && game.info.timeout != 0) {
    return;
  }

  /* If there are no connected players, don't automatically advance.  This is
   * a hack to prevent all-AI games from running rampant.  Note that if
   * timeout is set to -1 this function call is skipped entirely and the
   * server will run rampant. */
  players_iterate(pplayer) {
    if (pplayer->is_connected) {
      connected = TRUE;
      break;
    }
  } players_iterate_end;
  if (!connected) {
    return;
  }

  phase_players_iterate(pplayer) {
    if (game.info.turnblock && !pplayer->ai.control && pplayer->is_alive
	&& !pplayer->phase_done) {
      /* If turnblock is enabled check for human players, connected
       * or not. */
      return;
    } else if (pplayer->is_connected && pplayer->is_alive
	       && !pplayer->phase_done) {
      /* In all cases, we wait for any connected players. */
      return;
    }
  } phase_players_iterate_end;

  force_end_of_sniff = TRUE;
}

/**************************************************************************
  Checks if the player name belongs to the default player names of a
  particular player.
**************************************************************************/
static bool is_default_nation_name(const char *name,
				   Nation_type_id nation_id)
{
  const struct nation_type *nation = get_nation_by_idx(nation_id);

  int choice;

  for (choice = 0; choice < nation->leader_count; choice++) {
    if (mystrcasecmp(name, nation->leaders[choice].name) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

/**************************************************************************
  Check if this name is allowed for the player.  Fill out the error message
  (a translated string to be sent to the client) if not.
**************************************************************************/
static bool is_allowed_player_name(struct player *pplayer,
				   Nation_type_id nation,
				   const char *name,
				   char *error_buf, size_t bufsz)
{
  /* An empty name is surely not allowed. */
  if (strlen(name) == 0) {
    if (error_buf) {
      my_snprintf(error_buf, bufsz, _("Please choose a non-blank name."));
    }
    return FALSE;
  }

  /* Any name already taken is not allowed. */
  players_iterate(other_player) {
    if (other_player->nation == nation) {
      if (error_buf) {
	my_snprintf(error_buf, bufsz, _("That nation is already in use."));
      }
      return FALSE;
    } else {
      /* Check to see if name has been taken.
       * Ignore case because matches elsewhere are case-insenstive.
       * Don't limit this check to just players with allocated nation:
       * otherwise could end up with same name as pre-created AI player
       * (which have no nation yet, but will keep current player name).
       * Also want to keep all player names strictly distinct at all
       * times (for server commands etc), including during nation
       * allocation phase.
       */
      if (other_player->player_no != pplayer->player_no
	  && mystrcasecmp(other_player->name, name) == 0) {
	if (error_buf) {
	  my_snprintf(error_buf, bufsz,
		      _("Another player already has the name '%s'.  Please "
			"choose another name."), name);
	}
	return FALSE;
      }
    }
  } players_iterate_end;

  /* Any name from the default list is always allowed. */
  if (is_default_nation_name(name, nation)) {
    return TRUE;
  }

  /* To prevent abuse, only players with HACK access (usually local
   * connections) can use non-ascii names.  Otherwise players could use
   * confusing garbage names in multi-player games. */
  if (!is_ascii_name(name)
      && find_conn_by_user(pplayer->username)->access_level != ALLOW_HACK) {
    if (error_buf) {
      my_snprintf(error_buf, bufsz, _("Please choose a name containing "
				      "only ASCII characters."));
    }
    return FALSE;
  }

  return TRUE;
}

/****************************************************************************
  Send unavailable/used information for this nation out to everyone.
****************************************************************************/
static void send_nation_available(struct nation_type *nation)
{
  struct packet_nation_available packet;

  packet.id = nation->index;
  packet.is_unavailable = nation->is_unavailable;
  packet.is_used = nation->is_used;

  lsend_packet_nation_available(game.est_connections, &packet);
}

/****************************************************************************
  Initialize the list of available nations.

  Call this on server start, or when loading a scenario.
****************************************************************************/
static void init_available_nations(void)
{
  bool start_nations;
  int i;

  if (map.num_start_positions > 0) {
    start_nations = TRUE;

    for (i = 0; i < map.num_start_positions; i++) {
      if (map.start_positions[i].nation == NO_NATION_SELECTED) {
	start_nations = FALSE;
	break;
      }
    }
  } else {
    start_nations = FALSE;
  }

  if (start_nations) {
    nations_iterate(nation) {
      nation->is_unavailable = TRUE;
    } nations_iterate_end;
    for (i = 0; i < map.num_start_positions; i++) {
      Nation_type_id nation_no = map.start_positions[i].nation;
      struct nation_type *nation = get_nation_by_idx(nation_no);

      nation->is_unavailable = FALSE;
    }
  } else {
    nations_iterate(nation) {
      nation->is_unavailable = FALSE;
    } nations_iterate_end;
  }
  nations_iterate(nation) {
    nation->is_used = FALSE;
    send_nation_available(nation);
  } nations_iterate_end;
}

/**************************************************************************
...
**************************************************************************/
void handle_nation_select_req(struct player *pplayer,
			      Nation_type_id nation_no, bool is_male,
			      char *name, int city_style)
{
  const Nation_type_id old_nation_no = pplayer->nation;

  if (server_state != PRE_GAME_STATE) {
    freelog(LOG_ERROR, _("Trying to alloc nation outside of pregame!"));
    return;
  }

  if (nation_no != NO_NATION_SELECTED) {
    char message[1024];
    struct nation_type *nation;

    /* check sanity of the packet sent by client */
    if (nation_no < 0 || nation_no >= game.control.nation_count
	|| city_style < 0 || city_style >= game.control.styles_count
	|| city_style_has_requirements(&city_styles[city_style])) {
      return;
    }

    nation = get_nation_by_idx(nation_no);
    if (nation->is_unavailable) {
      notify_conn_ex(pplayer->connections, NULL, E_NATION_SELECTED,
		     _("%s nation is not available in this scenario."),
		     nation->name);
      return;
    }
    if (nation->is_unavailable) {
      notify_conn_ex(pplayer->connections, NULL, E_NATION_SELECTED,
		     _("%s nation is already in use."),
		     nation->name);
      return;
    }

    remove_leading_trailing_spaces(name);

    if (!is_allowed_player_name(pplayer, nation_no, name,
				message, sizeof(message))) {
      notify_conn_ex(pplayer->connections, NULL, E_NATION_SELECTED,
		     "%s", message);
      return;
    }

    name[0] = my_toupper(name[0]);

    notify_conn_ex(game.game_connections, NULL, E_NATION_SELECTED,
		   _("%s is the %s ruler %s."), pplayer->username,
		   get_nation_name(nation_no), name);

    sz_strlcpy(pplayer->name, name);
    pplayer->is_male = is_male;
    pplayer->city_style = city_style;

    nation->is_used = TRUE;
    send_nation_available(nation);
  }

  pplayer->nation = nation_no;
  send_player_info_c(pplayer, game.est_connections);

  if (old_nation_no != NO_NATION_SELECTED) {
    struct nation_type *old_nation = get_nation_by_idx(old_nation_no);

    old_nation->is_used = FALSE;
    send_nation_available(old_nation);
  }
}

/**************************************************************************
  Returns how much two nations looks good in the same game
**************************************************************************/
static int nations_match(struct nation_type* n1, struct nation_type* n2)
{
  int i;
  int sum = 0;
  for (i = 0; i < n1->num_groups; i++) {
    if (nation_in_group(n2, n1->groups[i]->name)) {
      sum += n1->groups[i]->match;
    }
  }
  return sum;
}

/**************************************************************************
  Select a random available nation.
**************************************************************************/
static Nation_type_id select_random_nation()
{
  Nation_type_id i, available[game.control.playable_nation_count];
  int count = 0;
  int V[game.control.playable_nation_count];
  int sum = 0;
  int x;
  
  /* Determine which nations are available. */
  for (i = 0; i < game.control.playable_nation_count; i++) {
    struct nation_type *nation = get_nation_by_idx(i);

    if (!nation->is_unavailable && !nation->is_used) {
      available[count] = i;
      
      /* Increase the probablity of selecting those which have higher
       * values of nations_match() */
      players_iterate(aplayer) {
        if (aplayer->nation == NO_NATION_SELECTED) {
	  continue;
	}
	sum+= nations_match(get_nation_by_idx(aplayer->nation),
	                    get_nation_by_idx(i)) * 100;
      } players_iterate_end;
      sum++;
      V[count] = sum;
      count++;
    }
  }

  /* Then pick one */  
  x = myrand(sum);
  for (i = 0; i < count; i++) {
    if (V[i] >= x) break;
  }
  return available[i];
}

/**************************************************************************
generate_ai_players() - Selects a nation for players created with
   server's "create <PlayerName>" command.  If <PlayerName> matches
   one of the leader names for some nation, we choose that nation.
   (I.e. if we issue "create Shaka" then we will make that AI player's
   nation the Zulus if the Zulus have not been chosen by anyone else.
   If they have, then we pick an available nation at random.)

   After that, we check to see if the server option "aifill" is greater
   than the number of players currently connected.  If so, we create the
   appropriate number of players (game.aifill - game.info.nplayers) from
   scratch, choosing a random nation and appropriate name for each.
   
   If the AI player name is one of the leader names for the AI player's
   nation, the player sex is set to the sex for that leader, else it
   is chosen randomly.  (So if English are ruled by Elisabeth, she is
   female, but if "Player 1" rules English, may be male or female.)
**************************************************************************/
static void generate_players(void)
{
  Nation_type_id nation;
  char player_name[MAX_LEN_NAME];
  int i, old_nplayers;

  /* Select nations for AI players generated with server
   * 'create <name>' command
   */
  players_iterate(pplayer) {
    ai_data_analyze_rulesets(pplayer);
    
    if (pplayer->nation != NO_NATION_SELECTED) {
      continue;
    }

    /* See if the player name matches a known leader name. */
    for (nation = 0; nation < game.control.playable_nation_count; nation++) {
      struct nation_type *n = get_nation_by_idx(nation);

      if (check_nation_leader_name(nation, pplayer->name)
	  && !n->is_unavailable && !n->is_used) {
	pplayer->nation = nation;
	pplayer->city_style = get_nation_city_style(nation);
	pplayer->is_male = get_nation_leader_sex(nation, pplayer->name);
	break;
      }
    }
    if (pplayer->nation != NO_NATION_SELECTED) {
      continue;
    }

    nation = select_random_nation();
    assert(nation != NO_NATION_SELECTED);

    get_nation_by_idx(nation)->is_used = TRUE;
    pplayer->nation = nation;
    pplayer->city_style = get_nation_city_style(nation);

    pplayer->is_male = (myrand(2) == 1);
    if (pplayer->is_connected) {
      /* FIXME: need to generate a leader name. */
    }

    announce_player(pplayer);
  } players_iterate_end;
  
  /* Create and pick nation and name for AI players needed to bring the
   * total number of players to equal game.aifill
   */

  if (game.control.playable_nation_count < game.aifill) {
    game.aifill = game.control.playable_nation_count;
    freelog(LOG_NORMAL,
	     _("Nation count smaller than aifill; aifill reduced to %d."),
             game.control.playable_nation_count);
  }

  if (game.info.max_players < game.aifill) {
    game.aifill = game.info.max_players;
    freelog(LOG_NORMAL,
	     _("Maxplayers smaller than aifill; aifill reduced to %d."),
             game.info.max_players);
  }

  /* we don't want aifill to count global observers unless 
   * aifill = MAX_NUM_PLAYERS */
  i = 0;
  players_iterate(pplayer) {
    if (pplayer->is_observer) {
      i++;
    }
  } players_iterate_end;
  if (game.aifill == MAX_NUM_PLAYERS) {
    i = 0;
  }

  for(;game.info.nplayers < game.aifill + i;) {
    struct player *pplayer;

    nation = select_random_nation();
    assert(nation != NO_NATION_SELECTED);
    get_nation_by_idx(nation)->is_used = TRUE;
    pick_random_player_name(nation, player_name);

    old_nplayers = game.info.nplayers;
    pplayer = get_player(old_nplayers);
     
    sz_strlcpy(pplayer->name, player_name);
    sz_strlcpy(pplayer->username, ANON_USER_NAME);

    pplayer->ai.skill_level = game.info.skill_level;
    freelog(LOG_NORMAL, _("%s has been added as %s level AI-controlled player."),
            player_name, name_of_skill_level(pplayer->ai.skill_level));
    notify_player(NULL,
                  _("%s has been added as %s level AI-controlled player."),
                  player_name, name_of_skill_level(pplayer->ai.skill_level));

    game.info.nplayers++;

    if (!((game.info.nplayers == old_nplayers+1)
	  && strcmp(player_name, pplayer->name)==0)) {
      con_write(C_FAIL, _("Error creating new AI player: %s\n"),
		player_name);
      break;			/* don't loop forever */
    }
      
    pplayer->nation = nation;
    pplayer->city_style = get_nation_city_style(nation);
    pplayer->ai.control = TRUE;
    if (check_nation_leader_name(nation, player_name)) {
      pplayer->is_male = get_nation_leader_sex(nation, player_name);
    } else {
      pplayer->is_male = (myrand(2) == 1);
    }
    set_ai_level_directer(pplayer, pplayer->ai.skill_level);
  }
  (void) send_server_info_to_metaserver(META_INFO);
}

/*************************************************************************
 Used in pick_random_player_name() below; buf has size at least MAX_LEN_NAME;
*************************************************************************/
static bool good_name(char *ptry, char *buf) {
  if (!(find_player_by_name(ptry) || find_player_by_user(ptry))) {
     (void) mystrlcpy(buf, ptry, MAX_LEN_NAME);
     return TRUE;
  }
  return FALSE;
}

/*************************************************************************
  Returns a random ruler name picked from given nation
     ruler names, given that nation's number. If that player name is already 
     taken, iterates through all leader names to find unused one. If it fails
     it iterates through "Player 1", "Player 2", ... until an unused name
     is found.
 newname should point to a buffer of size at least MAX_LEN_NAME.
*************************************************************************/
void pick_random_player_name(Nation_type_id nation, char *newname) 
{
   int i, names_count;
   struct leader *leaders;

   leaders = get_nation_leaders(nation, &names_count);

   /* Try random names (scattershot), then all available,
    * then "Player 1" etc:
    */
   for(i=0; i<names_count; i++) {
     if (good_name(leaders[myrand(names_count)].name, newname)) {
       return;
     }
   }
   
   for(i=0; i<names_count; i++) {
     if (good_name(leaders[i].name, newname)) {
       return;
     }
   }
   
   for(i=1; /**/; i++) {
     char tempname[50];
     my_snprintf(tempname, sizeof(tempname), _("Player %d"), i);
     if (good_name(tempname, newname)) return;
   }
}

/*************************************************************************
...
*************************************************************************/
static void announce_player (struct player *pplayer)
{
   freelog(LOG_NORMAL,
	   _("%s rules the %s."), pplayer->name,
	   get_nation_name_plural(pplayer->nation));

  players_iterate(other_player) {
    notify_player(other_player,
		  _("%s rules the %s."), pplayer->name,
		  get_nation_name_plural(pplayer->nation));
  } players_iterate_end;
}

/**************************************************************************
Play the game! Returns when server_state == GAME_OVER_STATE.
**************************************************************************/
static void main_loop(void)
{
  struct timer *eot_timer;	/* time server processing at end-of-turn */
  int save_counter = 0;
  bool is_new_turn = game.is_new_game;

  /* We may as well reset is_new_game now. */
  game.is_new_game = FALSE;

  eot_timer = new_timer_start(TIMER_CPU, TIMER_ACTIVE);

  /* 
   * This will freeze the reports and agents at the client.
   * 
   * Do this before the body so that the PACKET_THAW_HINT packet is
   * balanced. 
   */
  lsend_packet_freeze_hint(game.game_connections);

  while(server_state==RUN_GAME_STATE) {
    /* The beginning of a turn.
     *
     * We have to initialize data as well as do some actions.  However when
     * loading a game we don't want to do these actions (like AI unit
     * movement and AI diplomacy). */
    begin_turn(is_new_turn);

    for (; game.info.phase < game.info.num_phases; game.info.phase++) {
      freelog(LOG_DEBUG, "Starting phase %d/%d.", game.info.phase,
	      game.info.num_phases);
      begin_phase(is_new_turn);
      is_new_turn = TRUE;

      force_end_of_sniff = FALSE;

      /* 
       * This will thaw the reports and agents at the client.
       */
      lsend_packet_thaw_hint(game.game_connections);

      /* Before sniff (human player activites), report time to now: */
      freelog(LOG_VERBOSE, "End/start-turn server/ai activities: %g seconds",
	      read_timer_seconds(eot_timer));

      /* Do auto-saves just before starting sniff_packets(), so that
       * autosave happens effectively "at the same time" as manual
       * saves, from the point of view of restarting and AI players.
       * Post-increment so we don't count the first loop.
       */
      if (game.info.phase == 0) {
	if (save_counter >= game.info.save_nturns && game.info.save_nturns > 0) {
	  save_counter = 0;
	  save_game_auto("Autosave");
	}
	save_counter++;
      }

      freelog(LOG_DEBUG, "sniffingpackets");
      check_for_full_turn_done(); /* HACK: don't wait during AI phases */
      while (sniff_packets() == 1) {
	/* nothing */
      }

      /* After sniff, re-zero the timer: (read-out above on next loop) */
      clear_timer_start(eot_timer);

      conn_list_do_buffer(game.game_connections);

      sanity_check();

      /* 
       * This will freeze the reports and agents at the client.
       */
      lsend_packet_freeze_hint(game.game_connections);

      end_phase();

      conn_list_do_unbuffer(game.game_connections);
    }
    end_turn();
    freelog(LOG_DEBUG, "Sendinfotometaserver");
    (void) send_server_info_to_metaserver(META_REFRESH);

    if (server_state != GAME_OVER_STATE && is_game_over()) {
      server_state=GAME_OVER_STATE;
    }
  }

  /* 
   * This will thaw the reports and agents at the client.
   */
  lsend_packet_thaw_hint(game.game_connections);

  free_timer(eot_timer);
}

/**************************************************************************
  Server initialization.
**************************************************************************/
void srv_main(void)
{
  /* make sure it's initialized */
  if (!has_been_srv_init) {
    srv_init();
  }

  my_init_network();

  con_log_init(srvarg.log_filename, srvarg.loglevel);
  gamelog_init(srvarg.gamelog_filename);
  gamelog_set_level(GAMELOG_FULL);
  gamelog(GAMELOG_BEGIN);
  
#if IS_BETA_VERSION
  con_puts(C_COMMENT, "");
  con_puts(C_COMMENT, beta_message());
  con_puts(C_COMMENT, "");
#endif
  
  con_flush();

  server_game_init();
  stdinhand_init();
  diplhand_init();

  /* init network */  
  init_connections(); 
  server_open_socket();

  /* load a saved game */
  if (srvarg.load_filename[0] != '\0') {
    (void) load_command(NULL, srvarg.load_filename, FALSE);
  } 

  if(!(srvarg.metaserver_no_send)) {
    freelog(LOG_NORMAL, _("Sending info to metaserver [%s]"),
	    meta_addr_port());
    server_open_meta(); /* open socket for meta server */ 
  }

  (void) send_server_info_to_metaserver(META_INFO);

  /* accept new players, wait for serverop to start..*/
  server_state = PRE_GAME_STATE;

  /* load a script file */
  if (srvarg.script_filename
      && !read_init_script(NULL, srvarg.script_filename)) {
    exit(EXIT_FAILURE);
  }

  /* Run server loop */
  while (TRUE) {
    srv_loop();

    send_game_state(game.game_connections, CLIENT_GAME_OVER_STATE);
    report_final_scores();
    show_map_to_all();
    notify_player(NULL, _("The game is over..."));
    gamelog(GAMELOG_JUDGE, GL_NONE);
    send_server_info_to_metaserver(META_INFO);
    if (game.info.save_nturns > 0) {
      save_game_auto("Game over");
    }
    gamelog(GAMELOG_END);

    /* Remain in GAME_OVER_STATE until players log out */
    while (conn_list_size(game.est_connections) > 0) {
      (void) sniff_packets();
    }

    if (game.info.timeout == -1 || srvarg.exit_on_end) {
      /* For autogames or if the -e option is specified, exit the server. */
      server_quit();
    }

    /* Reset server */
    server_game_free();
    server_game_init();
    game.is_new_game = TRUE;
    server_state = PRE_GAME_STATE;
  }

  /* Technically, we won't ever get here. We exit via server_quit. */
}

/**************************************************************************
  Server loop, run to set up one game.
**************************************************************************/
static void final_ruleset_adjustments()
{
  int i;

  for (i = 0; i < MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS; i++) {
    if (game.players[i].government == G_MAGIC) {
      game.players[i].government = game.control.default_government;
    }
  }

  if (game.control.default_government == game.info.government_when_anarchy) {
    players_iterate(pplayer) {
      /* If we do not do this, an assert will trigger. This enables us to
       * select a valid government on game start. */
      pplayer->revolution_finishes = 0;
    } players_iterate_end;
  }
}

/**************************************************************************
  Server loop, run to set up one game.
**************************************************************************/
static void srv_loop(void)
{
  init_available_nations();

  freelog(LOG_NORMAL, _("Now accepting new client connections."));
  while(server_state == PRE_GAME_STATE) {
    sniff_packets(); /* Accepting commands. */
  }

  (void) send_server_info_to_metaserver(META_INFO);

  if (game.info.auto_ai_toggle) {
    players_iterate(pplayer) {
      if (!pplayer->is_connected && !pplayer->ai.control) {
	toggle_ai_player_direct(NULL, pplayer);
      }
    } players_iterate_end;
  }

  init_game_seed();

#ifdef TEST_RANDOM /* not defined anywhere, set it if you want it */
  test_random1(200);
  test_random1(2000);
  test_random1(20000);
  test_random1(200000);
#endif

  if (game.is_new_game) {
    generate_players();
  }
  final_ruleset_adjustments();
   
  /* If we have a tile map, and map.generator==0, call map_fractal_generate
   * anyway to make the specials, huts and continent numbers. */
  if (map_is_empty() || (map.generator == 0 && game.is_new_game)) {
    map_fractal_generate(TRUE);
  }

  gamelog(GAMELOG_MAP);
  /* start the game */

  server_state = RUN_GAME_STATE;
  (void) send_server_info_to_metaserver(META_INFO);

  if(game.is_new_game) {
    /* Before the player map is allocated (and initiailized)! */
    game.fogofwar_old = game.info.fogofwar;

    players_iterate(pplayer) {
      player_map_allocate(pplayer);
      init_tech(pplayer, game.info.tech);
      player_limit_to_government_rates(pplayer);
      pplayer->economic.gold = game.info.gold;
    } players_iterate_end;
    if(game.is_new_game) {
      /* If we're starting a new game, reset the rules.max_players to be the
       * number of players currently in the game.  But when loading a game
       * we don't want to change it. */
      game.info.max_players = game.info.nplayers;
    }
  }

  /* Set up alliances based on team selections */
  if (game.is_new_game) {
   players_iterate(pplayer) {
     players_iterate(pdest) {
      if (players_on_same_team(pplayer, pdest)
          && pplayer->player_no != pdest->player_no) {
        pplayer->diplstates[pdest->player_no].type = DS_TEAM;
        give_shared_vision(pplayer, pdest);
	BV_SET(pplayer->embassy, pdest->player_no);
      }
    } players_iterate_end;
   } players_iterate_end;
  }
  
  players_iterate(pplayer) {
    players_iterate(pdest) {
      if (players_on_same_team(pplayer, pdest)
          && pplayer->player_no != pdest->player_no) {
    	merge_players_research(pplayer, pdest);
      }
    } players_iterate_end;
  } players_iterate_end;
  /* tell the gamelog about the players */
  players_iterate(pplayer) {
    gamelog(GAMELOG_PLAYER, pplayer);
  } players_iterate_end;

  /* tell the gamelog who is whose team */
  team_iterate(pteam) {
    gamelog(GAMELOG_TEAM, pteam);
  } team_iterate_end;

  ai_data_movemap_init();

  if (!game.is_new_game) {
    players_iterate(pplayer) {
      if (pplayer->ai.control) {
	set_ai_level_direct(pplayer, pplayer->ai.skill_level);
      }
    } players_iterate_end;
  } else {
    players_iterate(pplayer) {
      ai_data_init(pplayer); /* Initialize this at last moment */
    } players_iterate_end;
  }

  lsend_packet_freeze_hint(game.game_connections);
  send_all_info(game.game_connections);
  lsend_packet_thaw_hint(game.game_connections);
  
  if(game.is_new_game) {
    init_new_game();

    /* give global observers the entire map */
    players_iterate(pplayer) {
      if (pplayer->is_observer) {
        map_know_and_see_all(pplayer);
      }
    } players_iterate_end;
  }

  send_game_state(game.game_connections, CLIENT_GAME_RUNNING_STATE);

  /*** Where the action is. ***/
  main_loop();

  /* Clean up some AI game data. */
  ai_data_movemap_done();
}

/**************************************************************************
  Initialize game data for the server (corresponds to server_game_free).
**************************************************************************/
void server_game_init(void)
{
  game_init();

  /* Rulesets are loaded on game initialization, but may be changed later
   * if /load or /rulesetdir is done. */
  load_rulesets();
}

/**************************************************************************
  Free game data that we reinitialize as part of a server soft restart.
  Bear in mind that this function is called when the 'load' command is
  used, for instance.
**************************************************************************/
void server_game_free()
{
  players_iterate(pplayer) {
    player_map_free(pplayer);
  } players_iterate_end;
  game_free();
}
