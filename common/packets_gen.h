
 /****************************************************************************
 *                       THIS FILE WAS GENERATED                             *
 * Script: common/generate_packets.py                                        *
 * Input:  common/packets.def                                                *
 *                       DO NOT CHANGE THIS FILE                             *
 ****************************************************************************/

struct packet_processing_started {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_processing_finished {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_freeze_hint {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_thaw_hint {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_server_join_req {
  char username[MAX_LEN_NAME];
  char capability[MAX_LEN_CAPSTR];
  char version_label[MAX_LEN_NAME];
  int major_version;
  int minor_version;
  int patch_version;
};

struct packet_server_join_reply {
  bool you_can_join;
  char message[MAX_LEN_MSG];
  char capability[MAX_LEN_CAPSTR];
  char challenge_file[MAX_LEN_PATH];
  int conn_id;
};

struct packet_authentication_req {
  enum authentication_type type;
  char message[MAX_LEN_MSG];
};

struct packet_authentication_reply {
  char password[MAX_LEN_PASSWORD];
};

struct packet_server_shutdown {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_nation_select_req {
  int player_no;
  Nation_type_id nation_no;
  bool is_male;
  char name[MAX_LEN_NAME];
  int city_style;
};

struct packet_player_ready {
  int player_no;
  bool is_ready;
};

struct packet_game_state {
  int value;
};

struct packet_endgame_report {
  int nscores;
  int id[MAX_NUM_PLAYERS];
  int score[MAX_NUM_PLAYERS];
  int pop[MAX_NUM_PLAYERS];
  int bnp[MAX_NUM_PLAYERS];
  int mfg[MAX_NUM_PLAYERS];
  int cities[MAX_NUM_PLAYERS];
  int techs[MAX_NUM_PLAYERS];
  int mil_service[MAX_NUM_PLAYERS];
  int wonders[MAX_NUM_PLAYERS];
  int research[MAX_NUM_PLAYERS];
  int landarea[MAX_NUM_PLAYERS];
  int settledarea[MAX_NUM_PLAYERS];
  int literacy[MAX_NUM_PLAYERS];
  int spaceship[MAX_NUM_PLAYERS];
};

struct packet_tile_info {
  int x;
  int y;
  Terrain_type_id type;
  int known;
  bool special[S_LAST];
  int owner;
  Continent_id continent;
  char spec_sprite[MAX_LEN_NAME];
};

struct packet_game_info {
  int gold;
  int tech;
  int skill_level;
  int aifill;
  bool is_new_game;
  float seconds_to_phasedone;
  int timeout;
  int turn;
  int phase;
  int year;
  int end_year;
  bool simultaneous_phases;
  int num_phases;
  int min_players;
  int max_players;
  int nplayers;
  int player_idx;
  int globalwarming;
  int heating;
  int warminglevel;
  int nuclearwinter;
  int cooling;
  int coolinglevel;
  int cityfactor;
  int diplcost;
  int freecost;
  int conquercost;
  int unhappysize;
  int angrycitizen;
  int techpenalty;
  int foodbox;
  int shieldbox;
  int sciencebox;
  int diplomacy;
  int dispersion;
  int tcptimeout;
  int netwait;
  int pingtimeout;
  int pingtime;
  int diplchance;
  int citymindist;
  int civilwarsize;
  int contactturns;
  int rapturedelay;
  int celebratesize;
  int barbarianrate;
  int onsetbarbarian;
  int occupychance;
  bool autoattack;
  bool spacerace;
  int aqueductloss;
  int killcitizen;
  int razechance;
  bool savepalace;
  bool natural_city_names;
  bool turnblock;
  bool fixedlength;
  bool auto_ai_toggle;
  bool fogofwar;
  int borders;
  int nbarbarians;
  bool happyborders;
  bool slow_invasions;
  int add_to_size_limit;
  int notradesize;
  int fulltradesize;
  int allowed_city_names;
  Impr_type_id palace_building;
  Impr_type_id land_defend_building;
  bool changable_tax;
  int forced_science;
  int forced_luxury;
  int forced_gold;
  int min_city_center_output[O_MAX];
  int min_dist_bw_cities;
  int init_vis_radius_sq;
  int hut_overflight;
  bool pillage_select;
  int nuke_contamination;
  int granary_food_ini[MAX_GRANARY_INIS];
  int granary_num_inis;
  int granary_food_inc;
  int tech_cost_style;
  int tech_leakage;
  int tech_cost_double_year;
  bool killstack;
  int autoupgrade_veteran_loss;
  int incite_improvement_factor;
  int incite_unit_factor;
  int incite_total_factor;
  int government_when_anarchy_id;
  int revolution_length;
  int base_pollution;
  int happy_cost;
  int food_cost;
  int base_bribe_cost;
  int base_incite_cost;
  int base_tech_cost;
  int ransom_gold;
  int save_nturns;
  int save_compress_level;
  char start_units[MAX_LEN_STARTUNIT];
  int num_teams;
  char team_names_orig[MAX_NUM_TEAMS][MAX_LEN_NAME];
  bool global_advances[A_LAST];
  int great_wonders[B_LAST];
};

struct packet_map_info {
  int xsize;
  int ysize;
  int topology_id;
};

struct packet_nuke_tile_info {
  int x;
  int y;
};

struct packet_chat_msg {
  char message[MAX_LEN_MSG];
  int x;
  int y;
  enum event_type event;
  int conn_id;
};

struct packet_chat_msg_req {
  char message[MAX_LEN_MSG];
};

struct packet_city_remove {
  int city_id;
};

struct packet_city_info {
  int id;
  int owner;
  int x;
  int y;
  char name[MAX_LEN_NAME];
  int size;
  int ppl_happy[5];
  int ppl_content[5];
  int ppl_unhappy[5];
  int ppl_angry[5];
  int specialists_size;
  int specialists[SP_MAX];
  int surplus[O_MAX];
  int waste[O_MAX];
  int unhappy_penalty[O_MAX];
  int prod[O_MAX];
  int citizen_base[O_MAX];
  int usage[O_MAX];
  int food_stock;
  int shield_stock;
  int trade[NUM_TRADEROUTES];
  int trade_value[NUM_TRADEROUTES];
  int pollution;
  bool production_is_unit;
  int production_value;
  int turn_last_built;
  int changed_from_id;
  bool changed_from_is_unit;
  int before_change_shields;
  int disbanded_shields;
  int caravan_shields;
  int last_turns_shield_surplus;
  struct worklist worklist;
  bv_imprs improvements;
  enum city_tile_type city_map[CITY_MAP_SIZE * CITY_MAP_SIZE];
  bool did_buy;
  bool did_sell;
  bool was_happy;
  bool airlift;
  bool diplomat_investigate;
  bv_city_options city_options;
  int turn_founded;
};

struct packet_city_short_info {
  int id;
  int owner;
  int x;
  int y;
  char name[MAX_LEN_NAME];
  int size;
  bool happy;
  bool unhappy;
  bv_imprs improvements;
  bool occupied;
  int tile_trade;
};

struct packet_city_sell {
  int city_id;
  int build_id;
};

struct packet_city_buy {
  int city_id;
};

struct packet_city_change {
  int city_id;
  int build_id;
  bool is_build_id_unit_id;
};

struct packet_city_worklist {
  int city_id;
  struct worklist worklist;
};

struct packet_city_make_specialist {
  int city_id;
  int worker_x;
  int worker_y;
};

struct packet_city_make_worker {
  int city_id;
  int worker_x;
  int worker_y;
};

struct packet_city_change_specialist {
  int city_id;
  Specialist_type_id from;
  Specialist_type_id to;
};

struct packet_city_rename {
  int city_id;
  char name[MAX_LEN_NAME];
};

struct packet_city_options_req {
  int city_id;
  bv_city_options options;
};

struct packet_city_refresh {
  int city_id;
};

struct packet_city_incite_inq {
  int city_id;
};

struct packet_city_incite_info {
  int city_id;
  int cost;
};

struct packet_city_name_suggestion_req {
  int unit_id;
};

struct packet_city_name_suggestion_info {
  int unit_id;
  char name[MAX_LEN_NAME];
};

struct packet_city_sabotage_list {
  int diplomat_id;
  int city_id;
  bv_imprs improvements;
};

struct packet_player_remove {
  int player_id;
};

struct packet_player_info {
  int playerno;
  char name[MAX_LEN_NAME];
  char username[MAX_LEN_NAME];
  int score;
  bool is_male;
  int government;
  int target_government;
  bool embassy[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];
  int city_style;
  Nation_type_id nation;
  int team;
  bool is_ready;
  bool phase_done;
  int nturns_idle;
  bool is_alive;
  struct player_diplstate diplstates[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];
  int gold;
  int tax;
  int science;
  int luxury;
  int bulbs_last_turn;
  int bulbs_researched;
  int techs_researched;
  int researching;
  int science_cost;
  int future_tech;
  int tech_goal;
  bool is_connected;
  int revolution_finishes;
  bool ai;
  int ai_skill_level;
  int barbarian_type;
  unsigned int gives_shared_vision;
  char inventions[A_LAST+1];
  int love[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];
  int small_wonders[B_LAST];
};

struct packet_player_phase_done {
  int turn;
};

struct packet_player_rates {
  int tax;
  int luxury;
  int science;
};

struct packet_player_change_government {
  int government;
};

struct packet_player_research {
  int tech;
};

struct packet_player_tech_goal {
  int tech;
};

struct packet_player_attribute_block {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_player_attribute_chunk {
  int offset;
  int total_length;
  int chunk_length;
  unsigned char data[ATTRIBUTE_CHUNK_SIZE];
};

struct packet_unit_remove {
  int unit_id;
};

struct packet_unit_info {
  int id;
  int owner;
  int x;
  int y;
  int homecity;
  int veteran;
  bool ai;
  bool paradropped;
  bool transported;
  bool done_moving;
  Unit_type_id type;
  int transported_by;
  int movesleft;
  int hp;
  int fuel;
  int activity_count;
  int unhappiness;
  int upkeep[O_MAX];
  int occupy;
  int goto_dest_x;
  int goto_dest_y;
  enum unit_activity activity;
  enum tile_special_type activity_target;
  bool has_orders;
  int orders_length;
  int orders_index;
  bool orders_repeat;
  bool orders_vigilant;
  enum unit_orders orders[MAX_LEN_ROUTE];
  enum direction8 orders_dirs[MAX_LEN_ROUTE];
  enum unit_activity orders_activities[MAX_LEN_ROUTE];
};

struct packet_unit_short_info {
  int id;
  int owner;
  int x;
  int y;
  Unit_type_id type;
  int veteran;
  bool occupied;
  bool goes_out_of_sight;
  bool transported;
  int hp;
  int activity;
  int transported_by;
  int packet_use;
  int info_city_id;
  int serial_num;
};

struct packet_unit_combat_info {
  int attacker_unit_id;
  int defender_unit_id;
  int attacker_hp;
  int defender_hp;
  bool make_winner_veteran;
};

struct packet_unit_move {
  int unit_id;
  int x;
  int y;
};

struct packet_unit_build_city {
  int unit_id;
  char name[MAX_LEN_NAME];
};

struct packet_unit_disband {
  int unit_id;
};

struct packet_unit_change_homecity {
  int unit_id;
  int city_id;
};

struct packet_unit_establish_trade {
  int unit_id;
};

struct packet_unit_help_build_wonder {
  int unit_id;
};

struct packet_unit_orders {
  int unit_id;
  int src_x;
  int src_y;
  int length;
  bool repeat;
  bool vigilant;
  enum unit_orders orders[MAX_LEN_ROUTE];
  enum direction8 dir[MAX_LEN_ROUTE];
  enum unit_activity activity[MAX_LEN_ROUTE];
  int dest_x;
  int dest_y;
};

struct packet_unit_autosettlers {
  int unit_id;
};

struct packet_unit_load {
  int cargo_id;
  int transporter_id;
};

struct packet_unit_unload {
  int cargo_id;
  int transporter_id;
};

struct packet_unit_upgrade {
  int unit_id;
};

struct packet_unit_nuke {
  int unit_id;
};

struct packet_unit_paradrop_to {
  int unit_id;
  int x;
  int y;
};

struct packet_unit_airlift {
  int unit_id;
  int city_id;
};

struct packet_unit_bribe_inq {
  int unit_id;
};

struct packet_unit_bribe_info {
  int unit_id;
  int cost;
};

struct packet_unit_type_upgrade {
  Unit_type_id type;
};

struct packet_unit_diplomat_action {
  int diplomat_id;
  enum diplomat_actions action_type;
  int target_id;
  int value;
};

struct packet_unit_diplomat_popup_dialog {
  int diplomat_id;
  int target_id;
};

struct packet_unit_change_activity {
  int unit_id;
  enum unit_activity activity;
  enum tile_special_type activity_target;
};

struct packet_diplomacy_init_meeting_req {
  int counterpart;
};

struct packet_diplomacy_init_meeting {
  int counterpart;
  int initiated_from;
};

struct packet_diplomacy_cancel_meeting_req {
  int counterpart;
};

struct packet_diplomacy_cancel_meeting {
  int counterpart;
  int initiated_from;
};

struct packet_diplomacy_create_clause_req {
  int counterpart;
  int giver;
  enum clause_type type;
  int value;
};

struct packet_diplomacy_create_clause {
  int counterpart;
  int giver;
  enum clause_type type;
  int value;
};

struct packet_diplomacy_remove_clause_req {
  int counterpart;
  int giver;
  enum clause_type type;
  int value;
};

struct packet_diplomacy_remove_clause {
  int counterpart;
  int giver;
  enum clause_type type;
  int value;
};

struct packet_diplomacy_accept_treaty_req {
  int counterpart;
};

struct packet_diplomacy_accept_treaty {
  int counterpart;
  bool I_accepted;
  bool other_accepted;
};

struct packet_diplomacy_cancel_pact {
  int other_player_id;
  enum clause_type clause;
};

struct packet_page_msg {
  char message[MAX_LEN_MSG];
  enum event_type event;
};

struct packet_report_req {
  enum report_type type;
};

struct packet_conn_info {
  int id;
  bool used;
  bool established;
  bool observer;
  int player_num;
  enum cmdlevel_id access_level;
  char username[MAX_LEN_NAME];
  char addr[MAX_LEN_ADDR];
  char capability[MAX_LEN_CAPSTR];
};

struct packet_conn_ping_info {
  int connections;
  int conn_id[MAX_NUM_CONNECTIONS];
  float ping_time[MAX_NUM_CONNECTIONS];
};

struct packet_conn_ping {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_conn_pong {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_end_phase {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_start_phase {
  int phase;
};

struct packet_new_year {
  int year;
  int turn;
};

struct packet_spaceship_launch {
  char __dummy;			/* to avoid malloc(0); */
};

struct packet_spaceship_place {
  enum spaceship_place_type type;
  int num;
};

struct packet_spaceship_info {
  int player_num;
  int sship_state;
  int structurals;
  int components;
  int modules;
  int fuel;
  int propulsion;
  int habitation;
  int life_support;
  int solar_panels;
  int launch_year;
  int population;
  int mass;
  char structure[NUM_SS_STRUCTURALS+1];
  float support_rate;
  float energy_rate;
  float success_rate;
  float travel_time;
};

struct packet_ruleset_unit {
  Unit_type_id id;
  char name[MAX_LEN_NAME];
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  char sound_move[MAX_LEN_NAME];
  char sound_move_alt[MAX_LEN_NAME];
  char sound_fight[MAX_LEN_NAME];
  char sound_fight_alt[MAX_LEN_NAME];
  int move_type;
  int unit_class_id;
  int build_cost;
  int pop_cost;
  int attack_strength;
  int defense_strength;
  int move_rate;
  int tech_requirement;
  int impr_requirement;
  int gov_requirement;
  int vision_radius_sq;
  int transport_capacity;
  int hp;
  int firepower;
  int obsoleted_by;
  int fuel;
  int happy_cost;
  int upkeep[O_MAX];
  int paratroopers_range;
  int paratroopers_mr_req;
  int paratroopers_mr_sub;
  char veteran_name[MAX_VET_LEVELS][MAX_LEN_NAME];
  float power_fact[MAX_VET_LEVELS];
  int move_bonus[MAX_VET_LEVELS];
  int bombard_rate;
  char helptext[MAX_LEN_PACKET];
  bv_flags flags;
  bv_roles roles;
};

struct packet_ruleset_game {
  int default_specialist;
  int global_init_techs[MAX_NUM_TECH_LIST];
  int trireme_loss_chance[MAX_VET_LEVELS];
  int work_veteran_chance[MAX_VET_LEVELS];
  int veteran_chance[MAX_VET_LEVELS];
};

struct packet_ruleset_specialist {
  Specialist_type_id id;
  char name[MAX_LEN_NAME];
  char short_name[MAX_LEN_NAME];
  int reqs_count;
  struct requirement reqs[MAX_NUM_REQS];
};

struct packet_ruleset_government_ruler_title {
  int gov;
  int id;
  Nation_type_id nation;
  char male_title[MAX_LEN_NAME];
  char female_title[MAX_LEN_NAME];
};

struct packet_ruleset_tech {
  int id;
  int req[2];
  int root_req;
  int flags;
  int preset_cost;
  int num_reqs;
  char name[MAX_LEN_NAME];
  char helptext[MAX_LEN_PACKET];
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
};

struct packet_ruleset_government {
  int id;
  int reqs_count;
  struct requirement reqs[MAX_NUM_REQS];
  int num_ruler_titles;
  char name[MAX_LEN_NAME];
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  char helptext[MAX_LEN_PACKET];
};

struct packet_ruleset_terrain_control {
  bool may_road;
  bool may_irrigate;
  bool may_mine;
  bool may_transform;
  int ocean_reclaim_requirement_pct;
  int land_channel_requirement_pct;
  enum special_river_move river_move_mode;
  int river_defense_bonus;
  int river_trade_incr;
  char river_help_text[MAX_LEN_PACKET];
  int fortress_defense_bonus;
  int road_superhighway_trade_bonus;
  int rail_tile_bonus[O_MAX];
  int pollution_tile_penalty[O_MAX];
  int fallout_tile_penalty[O_MAX];
};

struct packet_ruleset_nation {
  Nation_type_id id;
  char name[MAX_LEN_NAME];
  char name_plural[MAX_LEN_NAME];
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  char legend[MAX_LEN_MSG];
  int city_style;
  int init_techs[MAX_NUM_TECH_LIST];
  Unit_type_id init_units[MAX_NUM_UNIT_LIST];
  Impr_type_id init_buildings[MAX_NUM_BUILDING_LIST];
  int init_government;
  int leader_count;
  char leader_name[MAX_NUM_LEADERS][MAX_LEN_NAME];
  bool leader_sex[MAX_NUM_LEADERS];
  bool is_available;
  bool is_playable;
  bool is_barbarian;
  int group_count;
  char group_name[MAX_NUM_NATION_GROUPS][MAX_LEN_NAME];
};

struct packet_ruleset_city {
  int style_id;
  char name[MAX_LEN_NAME];
  char citizens_graphic[MAX_LEN_NAME];
  char citizens_graphic_alt[MAX_LEN_NAME];
  int reqs_count;
  struct requirement reqs[MAX_NUM_REQS];
  char graphic[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  int replaced_by;
};

struct packet_ruleset_building {
  Impr_type_id id;
  enum impr_genus_id genus;
  char name[MAX_LEN_NAME];
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  int reqs_count;
  struct requirement reqs[MAX_NUM_REQS];
  int obsolete_by;
  Impr_type_id replaced_by;
  int build_cost;
  int upkeep;
  int sabotage;
  int flags;
  char soundtag[MAX_LEN_NAME];
  char soundtag_alt[MAX_LEN_NAME];
  char helptext[MAX_LEN_PACKET];
};

struct packet_ruleset_terrain {
  Terrain_type_id id;
  bv_terrain_flags flags;
  char name_orig[MAX_LEN_NAME];
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  int movement_cost;
  int defense_bonus;
  int output[O_MAX];
  char special_1_name[MAX_LEN_NAME];
  int output_special_1[O_MAX];
  char graphic_str_special_1[MAX_LEN_NAME];
  char graphic_alt_special_1[MAX_LEN_NAME];
  char special_2_name[MAX_LEN_NAME];
  int output_special_2[O_MAX];
  char graphic_str_special_2[MAX_LEN_NAME];
  char graphic_alt_special_2[MAX_LEN_NAME];
  int road_trade_incr;
  int road_time;
  Terrain_type_id irrigation_result;
  int irrigation_food_incr;
  int irrigation_time;
  Terrain_type_id mining_result;
  int mining_shield_incr;
  int mining_time;
  Terrain_type_id transform_result;
  int transform_time;
  int rail_time;
  int airbase_time;
  int fortress_time;
  int clean_pollution_time;
  int clean_fallout_time;
  char helptext[MAX_LEN_PACKET];
};

struct packet_ruleset_control {
  int num_unit_types;
  int num_impr_types;
  int num_tech_types;
  int government_count;
  int nation_count;
  int styles_count;
  int terrain_count;
  int num_specialist_types;
};

struct packet_single_want_hack_req {
  char token[MAX_LEN_NAME];
};

struct packet_single_want_hack_reply {
  bool you_have_hack;
};

struct packet_ruleset_choices {
  int ruleset_count;
  char rulesets[MAX_NUM_RULESETS][MAX_RULESET_NAME_LENGTH];
};

struct packet_game_load {
  bool load_successful;
  int nplayers;
  char load_filename[MAX_LEN_PACKET];
  char name[MAX_NUM_PLAYERS][MAX_LEN_NAME];
  char username[MAX_NUM_PLAYERS][MAX_LEN_NAME];
  Nation_type_id nations[MAX_NUM_PLAYERS];
  bool is_alive[MAX_NUM_PLAYERS];
  bool is_ai[MAX_NUM_PLAYERS];
};

struct packet_options_settable_control {
  int num_settings;
  int num_categories;
  char category_names[256][MAX_LEN_NAME];
};

struct packet_options_settable {
  int id;
  char name[MAX_LEN_NAME];
  char short_help[MAX_LEN_PACKET];
  char extra_help[MAX_LEN_PACKET];
  enum sset_type type;
  enum sset_class class;
  bool is_visible;
  int val;
  int default_val;
  int min;
  int max;
  char strval[MAX_LEN_PACKET];
  char default_strval[MAX_LEN_PACKET];
  int category;
};

struct packet_ruleset_effect {
  enum effect_type effect_type;
  int effect_value;
};

struct packet_ruleset_effect_req {
  int effect_id;
  bool neg;
  enum req_source_type source_type;
  int source_value;
  enum req_range range;
  bool survives;
  bool negated;
};

enum packet_type {
  PACKET_PROCESSING_STARTED,             /* 0 */
  PACKET_PROCESSING_FINISHED,
  PACKET_FREEZE_HINT,
  PACKET_THAW_HINT,
  PACKET_SERVER_JOIN_REQ,
  PACKET_SERVER_JOIN_REPLY,
  PACKET_AUTHENTICATION_REQ,
  PACKET_AUTHENTICATION_REPLY,
  PACKET_SERVER_SHUTDOWN,
  PACKET_NATION_SELECT_REQ = 10,         /* 10 */
  PACKET_GAME_STATE = 12,
  PACKET_ENDGAME_REPORT,
  PACKET_TILE_INFO,
  PACKET_GAME_INFO,
  PACKET_MAP_INFO,
  PACKET_NUKE_TILE_INFO,
  PACKET_CHAT_MSG,
  PACKET_CHAT_MSG_REQ,
  PACKET_CITY_REMOVE,                    /* 20 */
  PACKET_CITY_INFO,
  PACKET_CITY_SHORT_INFO,
  PACKET_CITY_SELL,
  PACKET_CITY_BUY,
  PACKET_CITY_CHANGE,
  PACKET_CITY_WORKLIST,
  PACKET_CITY_MAKE_SPECIALIST,
  PACKET_CITY_MAKE_WORKER,
  PACKET_CITY_CHANGE_SPECIALIST,
  PACKET_CITY_RENAME,                    /* 30 */
  PACKET_CITY_OPTIONS_REQ,
  PACKET_CITY_REFRESH,
  PACKET_CITY_INCITE_INQ,
  PACKET_CITY_INCITE_INFO,
  PACKET_CITY_NAME_SUGGESTION_REQ,
  PACKET_CITY_NAME_SUGGESTION_INFO,
  PACKET_CITY_SABOTAGE_LIST,
  PACKET_PLAYER_REMOVE,
  PACKET_PLAYER_INFO,
  PACKET_PLAYER_PHASE_DONE,              /* 40 */
  PACKET_PLAYER_RATES,
  PACKET_PLAYER_CHANGE_GOVERNMENT = 43,
  PACKET_PLAYER_RESEARCH,
  PACKET_PLAYER_TECH_GOAL,
  PACKET_PLAYER_ATTRIBUTE_BLOCK,
  PACKET_PLAYER_ATTRIBUTE_CHUNK,
  PACKET_UNIT_REMOVE,
  PACKET_UNIT_INFO,
  PACKET_UNIT_SHORT_INFO,                /* 50 */
  PACKET_UNIT_COMBAT_INFO,
  PACKET_UNIT_MOVE,
  PACKET_UNIT_BUILD_CITY,
  PACKET_UNIT_DISBAND,
  PACKET_UNIT_CHANGE_HOMECITY,
  PACKET_UNIT_ESTABLISH_TRADE,
  PACKET_UNIT_HELP_BUILD_WONDER,
  PACKET_RULESET_SPECIALIST,
  PACKET_UNIT_ORDERS,
  PACKET_UNIT_AUTOSETTLERS,              /* 60 */
  PACKET_UNIT_UNLOAD,
  PACKET_UNIT_UPGRADE,
  PACKET_UNIT_NUKE,
  PACKET_UNIT_PARADROP_TO,
  PACKET_UNIT_AIRLIFT,
  PACKET_UNIT_BRIBE_INQ = 67,
  PACKET_UNIT_BRIBE_INFO,
  PACKET_UNIT_TYPE_UPGRADE,
  PACKET_UNIT_DIPLOMAT_ACTION,           /* 70 */
  PACKET_UNIT_DIPLOMAT_POPUP_DIALOG,
  PACKET_UNIT_CHANGE_ACTIVITY,
  PACKET_DIPLOMACY_INIT_MEETING_REQ,
  PACKET_DIPLOMACY_INIT_MEETING,
  PACKET_DIPLOMACY_CANCEL_MEETING_REQ,
  PACKET_DIPLOMACY_CANCEL_MEETING,
  PACKET_DIPLOMACY_CREATE_CLAUSE_REQ,
  PACKET_DIPLOMACY_CREATE_CLAUSE,
  PACKET_DIPLOMACY_REMOVE_CLAUSE_REQ,
  PACKET_DIPLOMACY_REMOVE_CLAUSE,        /* 80 */
  PACKET_DIPLOMACY_ACCEPT_TREATY_REQ,
  PACKET_DIPLOMACY_ACCEPT_TREATY,
  PACKET_DIPLOMACY_CANCEL_PACT,
  PACKET_PAGE_MSG,
  PACKET_REPORT_REQ,
  PACKET_CONN_INFO,
  PACKET_CONN_PING_INFO,
  PACKET_CONN_PING,
  PACKET_CONN_PONG,
  PACKET_END_PHASE,                      /* 90 */
  PACKET_START_PHASE,
  PACKET_NEW_YEAR,
  PACKET_SPACESHIP_LAUNCH,
  PACKET_SPACESHIP_PLACE,
  PACKET_SPACESHIP_INFO,
  PACKET_RULESET_UNIT,
  PACKET_RULESET_GAME,
  PACKET_RULESET_GOVERNMENT_RULER_TITLE,
  PACKET_RULESET_TECH,
  PACKET_RULESET_GOVERNMENT,             /* 100 */
  PACKET_RULESET_TERRAIN_CONTROL,
  PACKET_RULESET_NATION,
  PACKET_RULESET_CITY,
  PACKET_RULESET_BUILDING,
  PACKET_RULESET_TERRAIN,
  PACKET_RULESET_CONTROL,
  PACKET_UNIT_LOAD,
  PACKET_SINGLE_WANT_HACK_REQ,
  PACKET_SINGLE_WANT_HACK_REPLY,
  PACKET_GAME_LOAD = 111,
  PACKET_OPTIONS_SETTABLE_CONTROL,
  PACKET_OPTIONS_SETTABLE,
  PACKET_RULESET_CHOICES = 115,
  PACKET_PLAYER_READY,
  PACKET_RULESET_EFFECT = 122,
  PACKET_RULESET_EFFECT_REQ,

  PACKET_LAST  /* leave this last */
};

struct packet_processing_started *receive_packet_processing_started(struct connection *pc, enum packet_type type);
int send_packet_processing_started(struct connection *pc);

struct packet_processing_finished *receive_packet_processing_finished(struct connection *pc, enum packet_type type);
int send_packet_processing_finished(struct connection *pc);

struct packet_freeze_hint *receive_packet_freeze_hint(struct connection *pc, enum packet_type type);
int send_packet_freeze_hint(struct connection *pc);
void lsend_packet_freeze_hint(struct conn_list *dest);

struct packet_thaw_hint *receive_packet_thaw_hint(struct connection *pc, enum packet_type type);
int send_packet_thaw_hint(struct connection *pc);
void lsend_packet_thaw_hint(struct conn_list *dest);

struct packet_server_join_req *receive_packet_server_join_req(struct connection *pc, enum packet_type type);
int send_packet_server_join_req(struct connection *pc, const struct packet_server_join_req *packet);
int dsend_packet_server_join_req(struct connection *pc, const char *username, const char *capability, const char *version_label, int major_version, int minor_version, int patch_version);

struct packet_server_join_reply *receive_packet_server_join_reply(struct connection *pc, enum packet_type type);
int send_packet_server_join_reply(struct connection *pc, const struct packet_server_join_reply *packet);

struct packet_authentication_req *receive_packet_authentication_req(struct connection *pc, enum packet_type type);
int send_packet_authentication_req(struct connection *pc, const struct packet_authentication_req *packet);
int dsend_packet_authentication_req(struct connection *pc, enum authentication_type type, const char *message);

struct packet_authentication_reply *receive_packet_authentication_reply(struct connection *pc, enum packet_type type);
int send_packet_authentication_reply(struct connection *pc, const struct packet_authentication_reply *packet);

struct packet_server_shutdown *receive_packet_server_shutdown(struct connection *pc, enum packet_type type);
int send_packet_server_shutdown(struct connection *pc);
void lsend_packet_server_shutdown(struct conn_list *dest);

struct packet_nation_select_req *receive_packet_nation_select_req(struct connection *pc, enum packet_type type);
int send_packet_nation_select_req(struct connection *pc, const struct packet_nation_select_req *packet);
int dsend_packet_nation_select_req(struct connection *pc, int player_no, Nation_type_id nation_no, bool is_male, const char *name, int city_style);

struct packet_player_ready *receive_packet_player_ready(struct connection *pc, enum packet_type type);
int send_packet_player_ready(struct connection *pc, const struct packet_player_ready *packet);
int dsend_packet_player_ready(struct connection *pc, int player_no, bool is_ready);

struct packet_game_state *receive_packet_game_state(struct connection *pc, enum packet_type type);
int send_packet_game_state(struct connection *pc, const struct packet_game_state *packet);
void lsend_packet_game_state(struct conn_list *dest, const struct packet_game_state *packet);
int dsend_packet_game_state(struct connection *pc, int value);
void dlsend_packet_game_state(struct conn_list *dest, int value);

struct packet_endgame_report *receive_packet_endgame_report(struct connection *pc, enum packet_type type);
int send_packet_endgame_report(struct connection *pc, const struct packet_endgame_report *packet);
void lsend_packet_endgame_report(struct conn_list *dest, const struct packet_endgame_report *packet);

struct packet_tile_info *receive_packet_tile_info(struct connection *pc, enum packet_type type);
int send_packet_tile_info(struct connection *pc, const struct packet_tile_info *packet);
void lsend_packet_tile_info(struct conn_list *dest, const struct packet_tile_info *packet);

struct packet_game_info *receive_packet_game_info(struct connection *pc, enum packet_type type);
int send_packet_game_info(struct connection *pc, const struct packet_game_info *packet);

struct packet_map_info *receive_packet_map_info(struct connection *pc, enum packet_type type);
int send_packet_map_info(struct connection *pc, const struct packet_map_info *packet);
void lsend_packet_map_info(struct conn_list *dest, const struct packet_map_info *packet);

struct packet_nuke_tile_info *receive_packet_nuke_tile_info(struct connection *pc, enum packet_type type);
int send_packet_nuke_tile_info(struct connection *pc, const struct packet_nuke_tile_info *packet);
void lsend_packet_nuke_tile_info(struct conn_list *dest, const struct packet_nuke_tile_info *packet);
int dsend_packet_nuke_tile_info(struct connection *pc, int x, int y);
void dlsend_packet_nuke_tile_info(struct conn_list *dest, int x, int y);

struct packet_chat_msg *receive_packet_chat_msg(struct connection *pc, enum packet_type type);
int send_packet_chat_msg(struct connection *pc, const struct packet_chat_msg *packet);
void lsend_packet_chat_msg(struct conn_list *dest, const struct packet_chat_msg *packet);
int dsend_packet_chat_msg(struct connection *pc, const char *message, int x, int y, enum event_type event, int conn_id);
void dlsend_packet_chat_msg(struct conn_list *dest, const char *message, int x, int y, enum event_type event, int conn_id);

struct packet_chat_msg_req *receive_packet_chat_msg_req(struct connection *pc, enum packet_type type);
int send_packet_chat_msg_req(struct connection *pc, const struct packet_chat_msg_req *packet);
int dsend_packet_chat_msg_req(struct connection *pc, const char *message);

struct packet_city_remove *receive_packet_city_remove(struct connection *pc, enum packet_type type);
int send_packet_city_remove(struct connection *pc, const struct packet_city_remove *packet);
void lsend_packet_city_remove(struct conn_list *dest, const struct packet_city_remove *packet);
int dsend_packet_city_remove(struct connection *pc, int city_id);
void dlsend_packet_city_remove(struct conn_list *dest, int city_id);

struct packet_city_info *receive_packet_city_info(struct connection *pc, enum packet_type type);
int send_packet_city_info(struct connection *pc, const struct packet_city_info *packet);
void lsend_packet_city_info(struct conn_list *dest, const struct packet_city_info *packet);

struct packet_city_short_info *receive_packet_city_short_info(struct connection *pc, enum packet_type type);
int send_packet_city_short_info(struct connection *pc, const struct packet_city_short_info *packet);
void lsend_packet_city_short_info(struct conn_list *dest, const struct packet_city_short_info *packet);

struct packet_city_sell *receive_packet_city_sell(struct connection *pc, enum packet_type type);
int send_packet_city_sell(struct connection *pc, const struct packet_city_sell *packet);
int dsend_packet_city_sell(struct connection *pc, int city_id, int build_id);

struct packet_city_buy *receive_packet_city_buy(struct connection *pc, enum packet_type type);
int send_packet_city_buy(struct connection *pc, const struct packet_city_buy *packet);
int dsend_packet_city_buy(struct connection *pc, int city_id);

struct packet_city_change *receive_packet_city_change(struct connection *pc, enum packet_type type);
int send_packet_city_change(struct connection *pc, const struct packet_city_change *packet);
int dsend_packet_city_change(struct connection *pc, int city_id, int build_id, bool is_build_id_unit_id);

struct packet_city_worklist *receive_packet_city_worklist(struct connection *pc, enum packet_type type);
int send_packet_city_worklist(struct connection *pc, const struct packet_city_worklist *packet);
int dsend_packet_city_worklist(struct connection *pc, int city_id, struct worklist *worklist);

struct packet_city_make_specialist *receive_packet_city_make_specialist(struct connection *pc, enum packet_type type);
int send_packet_city_make_specialist(struct connection *pc, const struct packet_city_make_specialist *packet);
int dsend_packet_city_make_specialist(struct connection *pc, int city_id, int worker_x, int worker_y);

struct packet_city_make_worker *receive_packet_city_make_worker(struct connection *pc, enum packet_type type);
int send_packet_city_make_worker(struct connection *pc, const struct packet_city_make_worker *packet);
int dsend_packet_city_make_worker(struct connection *pc, int city_id, int worker_x, int worker_y);

struct packet_city_change_specialist *receive_packet_city_change_specialist(struct connection *pc, enum packet_type type);
int send_packet_city_change_specialist(struct connection *pc, const struct packet_city_change_specialist *packet);
int dsend_packet_city_change_specialist(struct connection *pc, int city_id, Specialist_type_id from, Specialist_type_id to);

struct packet_city_rename *receive_packet_city_rename(struct connection *pc, enum packet_type type);
int send_packet_city_rename(struct connection *pc, const struct packet_city_rename *packet);
int dsend_packet_city_rename(struct connection *pc, int city_id, const char *name);

struct packet_city_options_req *receive_packet_city_options_req(struct connection *pc, enum packet_type type);
int send_packet_city_options_req(struct connection *pc, const struct packet_city_options_req *packet);
int dsend_packet_city_options_req(struct connection *pc, int city_id, bv_city_options options);

struct packet_city_refresh *receive_packet_city_refresh(struct connection *pc, enum packet_type type);
int send_packet_city_refresh(struct connection *pc, const struct packet_city_refresh *packet);
int dsend_packet_city_refresh(struct connection *pc, int city_id);

struct packet_city_incite_inq *receive_packet_city_incite_inq(struct connection *pc, enum packet_type type);
int send_packet_city_incite_inq(struct connection *pc, const struct packet_city_incite_inq *packet);
int dsend_packet_city_incite_inq(struct connection *pc, int city_id);

struct packet_city_incite_info *receive_packet_city_incite_info(struct connection *pc, enum packet_type type);
int send_packet_city_incite_info(struct connection *pc, const struct packet_city_incite_info *packet);
int dsend_packet_city_incite_info(struct connection *pc, int city_id, int cost);

struct packet_city_name_suggestion_req *receive_packet_city_name_suggestion_req(struct connection *pc, enum packet_type type);
int send_packet_city_name_suggestion_req(struct connection *pc, const struct packet_city_name_suggestion_req *packet);
int dsend_packet_city_name_suggestion_req(struct connection *pc, int unit_id);

struct packet_city_name_suggestion_info *receive_packet_city_name_suggestion_info(struct connection *pc, enum packet_type type);
int send_packet_city_name_suggestion_info(struct connection *pc, const struct packet_city_name_suggestion_info *packet);
void lsend_packet_city_name_suggestion_info(struct conn_list *dest, const struct packet_city_name_suggestion_info *packet);
int dsend_packet_city_name_suggestion_info(struct connection *pc, int unit_id, const char *name);
void dlsend_packet_city_name_suggestion_info(struct conn_list *dest, int unit_id, const char *name);

struct packet_city_sabotage_list *receive_packet_city_sabotage_list(struct connection *pc, enum packet_type type);
int send_packet_city_sabotage_list(struct connection *pc, const struct packet_city_sabotage_list *packet);
void lsend_packet_city_sabotage_list(struct conn_list *dest, const struct packet_city_sabotage_list *packet);

struct packet_player_remove *receive_packet_player_remove(struct connection *pc, enum packet_type type);
int send_packet_player_remove(struct connection *pc, const struct packet_player_remove *packet);
void lsend_packet_player_remove(struct conn_list *dest, const struct packet_player_remove *packet);
int dsend_packet_player_remove(struct connection *pc, int player_id);
void dlsend_packet_player_remove(struct conn_list *dest, int player_id);

struct packet_player_info *receive_packet_player_info(struct connection *pc, enum packet_type type);
int send_packet_player_info(struct connection *pc, const struct packet_player_info *packet);

struct packet_player_phase_done *receive_packet_player_phase_done(struct connection *pc, enum packet_type type);
int send_packet_player_phase_done(struct connection *pc, const struct packet_player_phase_done *packet);
int dsend_packet_player_phase_done(struct connection *pc, int turn);

struct packet_player_rates *receive_packet_player_rates(struct connection *pc, enum packet_type type);
int send_packet_player_rates(struct connection *pc, const struct packet_player_rates *packet);
int dsend_packet_player_rates(struct connection *pc, int tax, int luxury, int science);

struct packet_player_change_government *receive_packet_player_change_government(struct connection *pc, enum packet_type type);
int send_packet_player_change_government(struct connection *pc, const struct packet_player_change_government *packet);
int dsend_packet_player_change_government(struct connection *pc, int government);

struct packet_player_research *receive_packet_player_research(struct connection *pc, enum packet_type type);
int send_packet_player_research(struct connection *pc, const struct packet_player_research *packet);
int dsend_packet_player_research(struct connection *pc, int tech);

struct packet_player_tech_goal *receive_packet_player_tech_goal(struct connection *pc, enum packet_type type);
int send_packet_player_tech_goal(struct connection *pc, const struct packet_player_tech_goal *packet);
int dsend_packet_player_tech_goal(struct connection *pc, int tech);

struct packet_player_attribute_block *receive_packet_player_attribute_block(struct connection *pc, enum packet_type type);
int send_packet_player_attribute_block(struct connection *pc);

struct packet_player_attribute_chunk *receive_packet_player_attribute_chunk(struct connection *pc, enum packet_type type);
int send_packet_player_attribute_chunk(struct connection *pc, const struct packet_player_attribute_chunk *packet);

struct packet_unit_remove *receive_packet_unit_remove(struct connection *pc, enum packet_type type);
int send_packet_unit_remove(struct connection *pc, const struct packet_unit_remove *packet);
void lsend_packet_unit_remove(struct conn_list *dest, const struct packet_unit_remove *packet);
int dsend_packet_unit_remove(struct connection *pc, int unit_id);
void dlsend_packet_unit_remove(struct conn_list *dest, int unit_id);

struct packet_unit_info *receive_packet_unit_info(struct connection *pc, enum packet_type type);
int send_packet_unit_info(struct connection *pc, const struct packet_unit_info *packet);
void lsend_packet_unit_info(struct conn_list *dest, const struct packet_unit_info *packet);

struct packet_unit_short_info *receive_packet_unit_short_info(struct connection *pc, enum packet_type type);
int send_packet_unit_short_info(struct connection *pc, const struct packet_unit_short_info *packet);
void lsend_packet_unit_short_info(struct conn_list *dest, const struct packet_unit_short_info *packet);

struct packet_unit_combat_info *receive_packet_unit_combat_info(struct connection *pc, enum packet_type type);
int send_packet_unit_combat_info(struct connection *pc, const struct packet_unit_combat_info *packet);
void lsend_packet_unit_combat_info(struct conn_list *dest, const struct packet_unit_combat_info *packet);

struct packet_unit_move *receive_packet_unit_move(struct connection *pc, enum packet_type type);
int send_packet_unit_move(struct connection *pc, const struct packet_unit_move *packet);
int dsend_packet_unit_move(struct connection *pc, int unit_id, int x, int y);

struct packet_unit_build_city *receive_packet_unit_build_city(struct connection *pc, enum packet_type type);
int send_packet_unit_build_city(struct connection *pc, const struct packet_unit_build_city *packet);
int dsend_packet_unit_build_city(struct connection *pc, int unit_id, const char *name);

struct packet_unit_disband *receive_packet_unit_disband(struct connection *pc, enum packet_type type);
int send_packet_unit_disband(struct connection *pc, const struct packet_unit_disband *packet);
int dsend_packet_unit_disband(struct connection *pc, int unit_id);

struct packet_unit_change_homecity *receive_packet_unit_change_homecity(struct connection *pc, enum packet_type type);
int send_packet_unit_change_homecity(struct connection *pc, const struct packet_unit_change_homecity *packet);
int dsend_packet_unit_change_homecity(struct connection *pc, int unit_id, int city_id);

struct packet_unit_establish_trade *receive_packet_unit_establish_trade(struct connection *pc, enum packet_type type);
int send_packet_unit_establish_trade(struct connection *pc, const struct packet_unit_establish_trade *packet);
int dsend_packet_unit_establish_trade(struct connection *pc, int unit_id);

struct packet_unit_help_build_wonder *receive_packet_unit_help_build_wonder(struct connection *pc, enum packet_type type);
int send_packet_unit_help_build_wonder(struct connection *pc, const struct packet_unit_help_build_wonder *packet);
int dsend_packet_unit_help_build_wonder(struct connection *pc, int unit_id);

struct packet_unit_orders *receive_packet_unit_orders(struct connection *pc, enum packet_type type);
int send_packet_unit_orders(struct connection *pc, const struct packet_unit_orders *packet);

struct packet_unit_autosettlers *receive_packet_unit_autosettlers(struct connection *pc, enum packet_type type);
int send_packet_unit_autosettlers(struct connection *pc, const struct packet_unit_autosettlers *packet);
int dsend_packet_unit_autosettlers(struct connection *pc, int unit_id);

struct packet_unit_load *receive_packet_unit_load(struct connection *pc, enum packet_type type);
int send_packet_unit_load(struct connection *pc, const struct packet_unit_load *packet);
int dsend_packet_unit_load(struct connection *pc, int cargo_id, int transporter_id);

struct packet_unit_unload *receive_packet_unit_unload(struct connection *pc, enum packet_type type);
int send_packet_unit_unload(struct connection *pc, const struct packet_unit_unload *packet);
int dsend_packet_unit_unload(struct connection *pc, int cargo_id, int transporter_id);

struct packet_unit_upgrade *receive_packet_unit_upgrade(struct connection *pc, enum packet_type type);
int send_packet_unit_upgrade(struct connection *pc, const struct packet_unit_upgrade *packet);
int dsend_packet_unit_upgrade(struct connection *pc, int unit_id);

struct packet_unit_nuke *receive_packet_unit_nuke(struct connection *pc, enum packet_type type);
int send_packet_unit_nuke(struct connection *pc, const struct packet_unit_nuke *packet);
int dsend_packet_unit_nuke(struct connection *pc, int unit_id);

struct packet_unit_paradrop_to *receive_packet_unit_paradrop_to(struct connection *pc, enum packet_type type);
int send_packet_unit_paradrop_to(struct connection *pc, const struct packet_unit_paradrop_to *packet);
int dsend_packet_unit_paradrop_to(struct connection *pc, int unit_id, int x, int y);

struct packet_unit_airlift *receive_packet_unit_airlift(struct connection *pc, enum packet_type type);
int send_packet_unit_airlift(struct connection *pc, const struct packet_unit_airlift *packet);
int dsend_packet_unit_airlift(struct connection *pc, int unit_id, int city_id);

struct packet_unit_bribe_inq *receive_packet_unit_bribe_inq(struct connection *pc, enum packet_type type);
int send_packet_unit_bribe_inq(struct connection *pc, const struct packet_unit_bribe_inq *packet);
int dsend_packet_unit_bribe_inq(struct connection *pc, int unit_id);

struct packet_unit_bribe_info *receive_packet_unit_bribe_info(struct connection *pc, enum packet_type type);
int send_packet_unit_bribe_info(struct connection *pc, const struct packet_unit_bribe_info *packet);
int dsend_packet_unit_bribe_info(struct connection *pc, int unit_id, int cost);

struct packet_unit_type_upgrade *receive_packet_unit_type_upgrade(struct connection *pc, enum packet_type type);
int send_packet_unit_type_upgrade(struct connection *pc, const struct packet_unit_type_upgrade *packet);
int dsend_packet_unit_type_upgrade(struct connection *pc, Unit_type_id type);

struct packet_unit_diplomat_action *receive_packet_unit_diplomat_action(struct connection *pc, enum packet_type type);
int send_packet_unit_diplomat_action(struct connection *pc, const struct packet_unit_diplomat_action *packet);
int dsend_packet_unit_diplomat_action(struct connection *pc, int diplomat_id, enum diplomat_actions action_type, int target_id, int value);

struct packet_unit_diplomat_popup_dialog *receive_packet_unit_diplomat_popup_dialog(struct connection *pc, enum packet_type type);
int send_packet_unit_diplomat_popup_dialog(struct connection *pc, const struct packet_unit_diplomat_popup_dialog *packet);
void lsend_packet_unit_diplomat_popup_dialog(struct conn_list *dest, const struct packet_unit_diplomat_popup_dialog *packet);
int dsend_packet_unit_diplomat_popup_dialog(struct connection *pc, int diplomat_id, int target_id);
void dlsend_packet_unit_diplomat_popup_dialog(struct conn_list *dest, int diplomat_id, int target_id);

struct packet_unit_change_activity *receive_packet_unit_change_activity(struct connection *pc, enum packet_type type);
int send_packet_unit_change_activity(struct connection *pc, const struct packet_unit_change_activity *packet);
int dsend_packet_unit_change_activity(struct connection *pc, int unit_id, enum unit_activity activity, enum tile_special_type activity_target);

struct packet_diplomacy_init_meeting_req *receive_packet_diplomacy_init_meeting_req(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_init_meeting_req(struct connection *pc, const struct packet_diplomacy_init_meeting_req *packet);
int dsend_packet_diplomacy_init_meeting_req(struct connection *pc, int counterpart);

struct packet_diplomacy_init_meeting *receive_packet_diplomacy_init_meeting(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_init_meeting(struct connection *pc, const struct packet_diplomacy_init_meeting *packet);
void lsend_packet_diplomacy_init_meeting(struct conn_list *dest, const struct packet_diplomacy_init_meeting *packet);
int dsend_packet_diplomacy_init_meeting(struct connection *pc, int counterpart, int initiated_from);
void dlsend_packet_diplomacy_init_meeting(struct conn_list *dest, int counterpart, int initiated_from);

struct packet_diplomacy_cancel_meeting_req *receive_packet_diplomacy_cancel_meeting_req(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_cancel_meeting_req(struct connection *pc, const struct packet_diplomacy_cancel_meeting_req *packet);
int dsend_packet_diplomacy_cancel_meeting_req(struct connection *pc, int counterpart);

struct packet_diplomacy_cancel_meeting *receive_packet_diplomacy_cancel_meeting(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_cancel_meeting(struct connection *pc, const struct packet_diplomacy_cancel_meeting *packet);
void lsend_packet_diplomacy_cancel_meeting(struct conn_list *dest, const struct packet_diplomacy_cancel_meeting *packet);
int dsend_packet_diplomacy_cancel_meeting(struct connection *pc, int counterpart, int initiated_from);
void dlsend_packet_diplomacy_cancel_meeting(struct conn_list *dest, int counterpart, int initiated_from);

struct packet_diplomacy_create_clause_req *receive_packet_diplomacy_create_clause_req(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_create_clause_req(struct connection *pc, const struct packet_diplomacy_create_clause_req *packet);
int dsend_packet_diplomacy_create_clause_req(struct connection *pc, int counterpart, int giver, enum clause_type type, int value);

struct packet_diplomacy_create_clause *receive_packet_diplomacy_create_clause(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_create_clause(struct connection *pc, const struct packet_diplomacy_create_clause *packet);
void lsend_packet_diplomacy_create_clause(struct conn_list *dest, const struct packet_diplomacy_create_clause *packet);
int dsend_packet_diplomacy_create_clause(struct connection *pc, int counterpart, int giver, enum clause_type type, int value);
void dlsend_packet_diplomacy_create_clause(struct conn_list *dest, int counterpart, int giver, enum clause_type type, int value);

struct packet_diplomacy_remove_clause_req *receive_packet_diplomacy_remove_clause_req(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_remove_clause_req(struct connection *pc, const struct packet_diplomacy_remove_clause_req *packet);
int dsend_packet_diplomacy_remove_clause_req(struct connection *pc, int counterpart, int giver, enum clause_type type, int value);

struct packet_diplomacy_remove_clause *receive_packet_diplomacy_remove_clause(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_remove_clause(struct connection *pc, const struct packet_diplomacy_remove_clause *packet);
void lsend_packet_diplomacy_remove_clause(struct conn_list *dest, const struct packet_diplomacy_remove_clause *packet);
int dsend_packet_diplomacy_remove_clause(struct connection *pc, int counterpart, int giver, enum clause_type type, int value);
void dlsend_packet_diplomacy_remove_clause(struct conn_list *dest, int counterpart, int giver, enum clause_type type, int value);

struct packet_diplomacy_accept_treaty_req *receive_packet_diplomacy_accept_treaty_req(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_accept_treaty_req(struct connection *pc, const struct packet_diplomacy_accept_treaty_req *packet);
int dsend_packet_diplomacy_accept_treaty_req(struct connection *pc, int counterpart);

struct packet_diplomacy_accept_treaty *receive_packet_diplomacy_accept_treaty(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_accept_treaty(struct connection *pc, const struct packet_diplomacy_accept_treaty *packet);
void lsend_packet_diplomacy_accept_treaty(struct conn_list *dest, const struct packet_diplomacy_accept_treaty *packet);
int dsend_packet_diplomacy_accept_treaty(struct connection *pc, int counterpart, bool I_accepted, bool other_accepted);
void dlsend_packet_diplomacy_accept_treaty(struct conn_list *dest, int counterpart, bool I_accepted, bool other_accepted);

struct packet_diplomacy_cancel_pact *receive_packet_diplomacy_cancel_pact(struct connection *pc, enum packet_type type);
int send_packet_diplomacy_cancel_pact(struct connection *pc, const struct packet_diplomacy_cancel_pact *packet);
int dsend_packet_diplomacy_cancel_pact(struct connection *pc, int other_player_id, enum clause_type clause);

struct packet_page_msg *receive_packet_page_msg(struct connection *pc, enum packet_type type);
int send_packet_page_msg(struct connection *pc, const struct packet_page_msg *packet);
void lsend_packet_page_msg(struct conn_list *dest, const struct packet_page_msg *packet);

struct packet_report_req *receive_packet_report_req(struct connection *pc, enum packet_type type);
int send_packet_report_req(struct connection *pc, const struct packet_report_req *packet);
int dsend_packet_report_req(struct connection *pc, enum report_type type);

struct packet_conn_info *receive_packet_conn_info(struct connection *pc, enum packet_type type);
int send_packet_conn_info(struct connection *pc, const struct packet_conn_info *packet);
void lsend_packet_conn_info(struct conn_list *dest, const struct packet_conn_info *packet);

struct packet_conn_ping_info *receive_packet_conn_ping_info(struct connection *pc, enum packet_type type);
int send_packet_conn_ping_info(struct connection *pc, const struct packet_conn_ping_info *packet);
void lsend_packet_conn_ping_info(struct conn_list *dest, const struct packet_conn_ping_info *packet);

struct packet_conn_ping *receive_packet_conn_ping(struct connection *pc, enum packet_type type);
int send_packet_conn_ping(struct connection *pc);

struct packet_conn_pong *receive_packet_conn_pong(struct connection *pc, enum packet_type type);
int send_packet_conn_pong(struct connection *pc);

struct packet_end_phase *receive_packet_end_phase(struct connection *pc, enum packet_type type);
int send_packet_end_phase(struct connection *pc);
void lsend_packet_end_phase(struct conn_list *dest);

struct packet_start_phase *receive_packet_start_phase(struct connection *pc, enum packet_type type);
int send_packet_start_phase(struct connection *pc, const struct packet_start_phase *packet);
void lsend_packet_start_phase(struct conn_list *dest, const struct packet_start_phase *packet);
int dsend_packet_start_phase(struct connection *pc, int phase);
void dlsend_packet_start_phase(struct conn_list *dest, int phase);

struct packet_new_year *receive_packet_new_year(struct connection *pc, enum packet_type type);
int send_packet_new_year(struct connection *pc, const struct packet_new_year *packet);
void lsend_packet_new_year(struct conn_list *dest, const struct packet_new_year *packet);

struct packet_spaceship_launch *receive_packet_spaceship_launch(struct connection *pc, enum packet_type type);
int send_packet_spaceship_launch(struct connection *pc);

struct packet_spaceship_place *receive_packet_spaceship_place(struct connection *pc, enum packet_type type);
int send_packet_spaceship_place(struct connection *pc, const struct packet_spaceship_place *packet);
int dsend_packet_spaceship_place(struct connection *pc, enum spaceship_place_type type, int num);

struct packet_spaceship_info *receive_packet_spaceship_info(struct connection *pc, enum packet_type type);
int send_packet_spaceship_info(struct connection *pc, const struct packet_spaceship_info *packet);
void lsend_packet_spaceship_info(struct conn_list *dest, const struct packet_spaceship_info *packet);

struct packet_ruleset_unit *receive_packet_ruleset_unit(struct connection *pc, enum packet_type type);
int send_packet_ruleset_unit(struct connection *pc, const struct packet_ruleset_unit *packet);
void lsend_packet_ruleset_unit(struct conn_list *dest, const struct packet_ruleset_unit *packet);

struct packet_ruleset_game *receive_packet_ruleset_game(struct connection *pc, enum packet_type type);
int send_packet_ruleset_game(struct connection *pc, const struct packet_ruleset_game *packet);
void lsend_packet_ruleset_game(struct conn_list *dest, const struct packet_ruleset_game *packet);

struct packet_ruleset_specialist *receive_packet_ruleset_specialist(struct connection *pc, enum packet_type type);
int send_packet_ruleset_specialist(struct connection *pc, const struct packet_ruleset_specialist *packet);
void lsend_packet_ruleset_specialist(struct conn_list *dest, const struct packet_ruleset_specialist *packet);

struct packet_ruleset_government_ruler_title *receive_packet_ruleset_government_ruler_title(struct connection *pc, enum packet_type type);
int send_packet_ruleset_government_ruler_title(struct connection *pc, const struct packet_ruleset_government_ruler_title *packet);
void lsend_packet_ruleset_government_ruler_title(struct conn_list *dest, const struct packet_ruleset_government_ruler_title *packet);

struct packet_ruleset_tech *receive_packet_ruleset_tech(struct connection *pc, enum packet_type type);
int send_packet_ruleset_tech(struct connection *pc, const struct packet_ruleset_tech *packet);
void lsend_packet_ruleset_tech(struct conn_list *dest, const struct packet_ruleset_tech *packet);

struct packet_ruleset_government *receive_packet_ruleset_government(struct connection *pc, enum packet_type type);
int send_packet_ruleset_government(struct connection *pc, const struct packet_ruleset_government *packet);
void lsend_packet_ruleset_government(struct conn_list *dest, const struct packet_ruleset_government *packet);

struct packet_ruleset_terrain_control *receive_packet_ruleset_terrain_control(struct connection *pc, enum packet_type type);
int send_packet_ruleset_terrain_control(struct connection *pc, const struct packet_ruleset_terrain_control *packet);
void lsend_packet_ruleset_terrain_control(struct conn_list *dest, const struct packet_ruleset_terrain_control *packet);

struct packet_ruleset_nation *receive_packet_ruleset_nation(struct connection *pc, enum packet_type type);
int send_packet_ruleset_nation(struct connection *pc, const struct packet_ruleset_nation *packet);
void lsend_packet_ruleset_nation(struct conn_list *dest, const struct packet_ruleset_nation *packet);

struct packet_ruleset_city *receive_packet_ruleset_city(struct connection *pc, enum packet_type type);
int send_packet_ruleset_city(struct connection *pc, const struct packet_ruleset_city *packet);
void lsend_packet_ruleset_city(struct conn_list *dest, const struct packet_ruleset_city *packet);

struct packet_ruleset_building *receive_packet_ruleset_building(struct connection *pc, enum packet_type type);
int send_packet_ruleset_building(struct connection *pc, const struct packet_ruleset_building *packet);
void lsend_packet_ruleset_building(struct conn_list *dest, const struct packet_ruleset_building *packet);

struct packet_ruleset_terrain *receive_packet_ruleset_terrain(struct connection *pc, enum packet_type type);
int send_packet_ruleset_terrain(struct connection *pc, const struct packet_ruleset_terrain *packet);
void lsend_packet_ruleset_terrain(struct conn_list *dest, const struct packet_ruleset_terrain *packet);

struct packet_ruleset_control *receive_packet_ruleset_control(struct connection *pc, enum packet_type type);
int send_packet_ruleset_control(struct connection *pc, const struct packet_ruleset_control *packet);
void lsend_packet_ruleset_control(struct conn_list *dest, const struct packet_ruleset_control *packet);

struct packet_single_want_hack_req *receive_packet_single_want_hack_req(struct connection *pc, enum packet_type type);
int send_packet_single_want_hack_req(struct connection *pc, const struct packet_single_want_hack_req *packet);

struct packet_single_want_hack_reply *receive_packet_single_want_hack_reply(struct connection *pc, enum packet_type type);
int send_packet_single_want_hack_reply(struct connection *pc, const struct packet_single_want_hack_reply *packet);
int dsend_packet_single_want_hack_reply(struct connection *pc, bool you_have_hack);

struct packet_ruleset_choices *receive_packet_ruleset_choices(struct connection *pc, enum packet_type type);
int send_packet_ruleset_choices(struct connection *pc, const struct packet_ruleset_choices *packet);

struct packet_game_load *receive_packet_game_load(struct connection *pc, enum packet_type type);
int send_packet_game_load(struct connection *pc, const struct packet_game_load *packet);
void lsend_packet_game_load(struct conn_list *dest, const struct packet_game_load *packet);

struct packet_options_settable_control *receive_packet_options_settable_control(struct connection *pc, enum packet_type type);
int send_packet_options_settable_control(struct connection *pc, const struct packet_options_settable_control *packet);
void lsend_packet_options_settable_control(struct conn_list *dest, const struct packet_options_settable_control *packet);

struct packet_options_settable *receive_packet_options_settable(struct connection *pc, enum packet_type type);
int send_packet_options_settable(struct connection *pc, const struct packet_options_settable *packet);
void lsend_packet_options_settable(struct conn_list *dest, const struct packet_options_settable *packet);

struct packet_ruleset_effect *receive_packet_ruleset_effect(struct connection *pc, enum packet_type type);
int send_packet_ruleset_effect(struct connection *pc, const struct packet_ruleset_effect *packet);
void lsend_packet_ruleset_effect(struct conn_list *dest, const struct packet_ruleset_effect *packet);

struct packet_ruleset_effect_req *receive_packet_ruleset_effect_req(struct connection *pc, enum packet_type type);
int send_packet_ruleset_effect_req(struct connection *pc, const struct packet_ruleset_effect_req *packet);
void lsend_packet_ruleset_effect_req(struct conn_list *dest, const struct packet_ruleset_effect_req *packet);


void delta_stats_report(void);
void delta_stats_reset(void);
void *get_packet_from_connection_helper(struct connection *pc, enum packet_type type);
