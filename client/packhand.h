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
#ifndef FC__PACKHAND_H
#define FC__PACKHAND_H

#include "packets.h"

void handle_login_reply(struct packet_login_reply *packet);

void handle_tile_info(struct packet_tile_info *packet);
void handle_player_info(struct packet_player_info *pinfo);
void handle_conn_info(struct packet_conn_info *pinfo);
void handle_ping_info(struct packet_ping_info *packet);
void handle_game_info(struct packet_game_info *pinfo);
void handle_map_info(struct packet_map_info *pinfo);
void handle_select_nation(struct packet_nations_used *packet);
void handle_unit_info(struct packet_unit_info *packet);
void handle_chat_msg(struct packet_generic_message *packet);

void handle_remove_city(struct packet_generic_integer *packet);
void handle_remove_unit(struct packet_generic_integer *packet);
void handle_incite_cost(struct packet_generic_values *packet);

void handle_city_options(struct packet_generic_values *preq);

void handle_spaceship_info(struct packet_spaceship_info *p);

void handle_move_unit(void);
void handle_new_year(struct packet_new_year *ppacket);
void handle_city_info(struct packet_city_info *packet);
void handle_short_city(struct packet_short_city *packet);
void handle_unit_combat(struct packet_unit_combat *packet);
void handle_game_state(struct packet_generic_integer *packet);
void handle_nuke_tile(struct packet_nuke_tile *packet);
void handle_page_msg(struct packet_generic_message *packet);
void handle_before_new_year(void);
void handle_remove_player(struct packet_generic_integer *packet);
void handle_ruleset_control(struct packet_ruleset_control *packet);
void handle_ruleset_unit(struct packet_ruleset_unit *p);
void handle_ruleset_tech(struct packet_ruleset_tech *p);
void handle_ruleset_building(struct packet_ruleset_building *p);
void handle_ruleset_terrain(struct packet_ruleset_terrain *p);
void handle_ruleset_terrain_control(struct terrain_misc *p);
void handle_ruleset_government(struct packet_ruleset_government *p);
void handle_ruleset_government_ruler_title(struct packet_ruleset_government_ruler_title *p);
void handle_city_name_suggestion(struct packet_city_name_suggestion *packet);
void handle_ruleset_nation(struct packet_ruleset_nation *p);
void handle_ruleset_city(struct packet_ruleset_city *packet);
void handle_ruleset_game(struct packet_ruleset_game *packet);
void handle_diplomat_action(struct packet_diplomat_action *packet);
void handle_sabotage_list(struct packet_sabotage_list *packet);
void handle_player_attribute_chunk(struct packet_attribute_chunk *chunk);
void handle_processing_started(void);
void handle_processing_finished(void);
void handle_start_turn(void);
void handle_freeze_hint(void);
void handle_thaw_hint(void);

void notify_about_incoming_packet(struct connection *pc,
				   int packet_type, int size);
void notify_about_outgoing_packet(struct connection *pc,
				  int packet_type, int size,
				  int request_id);
void set_reports_thaw_request(int request_id);

void target_government_init(void);
void set_government_choice(int government);
void start_revolution(void);

#endif /* FC__PACKHAND_H */
