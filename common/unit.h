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
#ifndef FC__UNIT_H
#define FC__UNIT_H

#include "terrain.h"		/* enum tile_special_type */
#include "unittype.h"

struct player;
struct city;
struct goto_route;
struct tile;

#define BARBARIAN_LIFE    5

enum unit_activity {
  ACTIVITY_IDLE, ACTIVITY_POLLUTION, ACTIVITY_ROAD, ACTIVITY_MINE,
  ACTIVITY_IRRIGATE, ACTIVITY_FORTIFIED, ACTIVITY_FORTRESS, ACTIVITY_SENTRY,
  ACTIVITY_RAILROAD, ACTIVITY_PILLAGE, ACTIVITY_GOTO, ACTIVITY_EXPLORE,
  ACTIVITY_TRANSFORM, ACTIVITY_UNKNOWN, ACTIVITY_AIRBASE, ACTIVITY_FORTIFYING,
  ACTIVITY_FALLOUT, ACTIVITY_PATROL,
  ACTIVITY_LAST   /* leave this one last */
};

enum unit_focus_status {
  FOCUS_AVAIL, FOCUS_WAIT, FOCUS_DONE  
};

enum diplomat_actions {
  DIPLOMAT_BRIBE, DIPLOMAT_EMBASSY, DIPLOMAT_SABOTAGE,
  DIPLOMAT_STEAL, DIPLOMAT_INCITE, SPY_POISON, 
  DIPLOMAT_INVESTIGATE, SPY_SABOTAGE_UNIT,
  SPY_GET_SABOTAGE_LIST,
  DIPLOMAT_MOVE,	/* move onto city square - only for allied cities */
  DIPLOMAT_ANY_ACTION   /* leave this one last */
};

enum diplomat_client_actions {
  DIPLOMAT_CLIENT_POPUP_DIALOG
};

enum ai_unit_task { AIUNIT_NONE, AIUNIT_AUTO_SETTLER, AIUNIT_BUILD_CITY,
                    AIUNIT_DEFEND_HOME, AIUNIT_ATTACK, AIUNIT_FORTIFY,
                    AIUNIT_RUNAWAY, AIUNIT_ESCORT, AIUNIT_EXPLORE,
                    AIUNIT_PILLAGE, AIUNIT_RECOVER };

enum goto_move_restriction {
  GOTO_MOVE_ANY,
  GOTO_MOVE_CARDINAL_ONLY, /* No diagonal moves.  */
  GOTO_MOVE_STRAIGHTEST
};

enum goto_route_type {
  ROUTE_GOTO, ROUTE_PATROL
};

enum unit_move_result {
  MR_OK, MR_BAD_TYPE_FOR_CITY_TAKE_OVER, MR_NO_WAR, MR_ZOC,
  MR_BAD_ACTIVITY, MR_BAD_DESTINATION, MR_BAD_MAP_POSITION,
  MR_DESTINATION_OCCUPIED_BY_NON_ALLIED_UNIT,
  MR_NO_SEA_TRANSPORTER_CAPACITY,
  MR_DESTINATION_OCCUPIED_BY_NON_ALLIED_CITY
};

enum add_build_city_result {
  AB_BUILD_OK,			/* Unit OK to build city */
  AB_ADD_OK,			/* Unit OK to add to city */
  AB_NOT_BUILD_LOC,		/* City is not allowed to be built at
				   this location */
  AB_NOT_ADDABLE_UNIT,		/* Unit is not one that can be added
				   to cities */
  AB_NOT_BUILD_UNIT,		/* Unit is not one that can build
				   cities */
  AB_NO_MOVES_BUILD,		/* Unit does not have moves left to
				   build a city */
  AB_NO_MOVES_ADD,		/* Unit does not have moves left to
				   add to city */
  AB_NOT_OWNER,			/* Owner of unit is not owner of
				   city */
  AB_TOO_BIG,			/* City is too big to be added to */
  AB_NO_AQUEDUCT,		/* Adding takes city past limit for
				   aquaduct but city has no
				   aquaduct */
  AB_NO_SEWER			/* Adding takes city past limit for
				   sewer but city has no sewer */
};

struct unit_ai {
  bool control; /* 0: not automated    1: automated */
  enum ai_unit_task ai_role;
  /* The following are all unit ids */
  int ferryboat; /* the ferryboat assigned to us */
  int passenger; /* the unit assigned to this ferryboat */
  int bodyguard; /* the unit bodyguarding us */
  int charge; /* the unit this unit is bodyguarding */
};

struct unit {
  Unit_Type_id type;
  int id;
  int owner;
  int x, y;                           
  bool veteran;
  int homecity;
  int moves_left;
  int hp;
  int unhappiness;
  int upkeep;
  int upkeep_food;
  int upkeep_gold;
  bool foul;
  int fuel;
  int bribe_cost;
  struct unit_ai ai;
  enum unit_activity activity;
  int goto_dest_x, goto_dest_y;
  int activity_count;
  enum tile_special_type activity_target;
  enum unit_focus_status focus_status;
  int ord_map, ord_city;
  /* ord_map and ord_city are the order index of this unit in tile.units
     and city.units_supported; they are only used for save/reload */
  bool moved;
  bool paradropped;
  bool connecting;
  int transported_by;
  struct goto_route *pgr;
};


/* get 'struct unit_list' and related functions: */
#define SPECLIST_TAG unit
#define SPECLIST_TYPE struct unit
#include "speclist.h"

#define unit_list_iterate(unitlist, punit) \
    TYPED_LIST_ITERATE(struct unit, unitlist, punit)
#define unit_list_iterate_end  LIST_ITERATE_END
#define SINGLE_MOVE 3
#define MOVE_COST_RIVER 1
#define MOVE_COST_RAIL 0
#define MOVE_COST_ROAD 1
#define MOVE_COST_AIR 1

#define unit_list_iterate_safe(unitlist, punit) \
{ \
  int _size = unit_list_size(&unitlist); \
  if (_size > 0) { \
    int *_ids = fc_malloc(sizeof(int) * _size); \
    int _i = 0; \
    unit_list_iterate(unitlist, punit) { \
      _ids[_i++] = punit->id; \
    } unit_list_iterate_end; \
    for (_i=0; _i<_size; _i++) { \
      struct unit *punit = find_unit_by_id(_ids[_i]); \
      if (punit) { \

#define unit_list_iterate_safe_end \
      } \
    } \
    free(_ids); \
  } \
}

struct unit *unit_list_find(struct unit_list *This, int id);

void unit_list_sort_ord_map(struct unit_list *This);
void unit_list_sort_ord_city(struct unit_list *This);

bool diplomat_can_do_action(struct unit *pdiplomat,
			   enum diplomat_actions action, 
			   int destx, int desty);
bool is_diplomat_action_available(struct unit *pdiplomat,
                                 enum diplomat_actions action,
                                 int destx, int desty);

int unit_move_rate(struct unit *punit);
bool unit_can_help_build_wonder(struct unit *punit, struct city *pcity);
bool unit_can_help_build_wonder_here(struct unit *punit);
bool unit_can_est_traderoute_here(struct unit *punit);
bool unit_can_defend_here(struct unit *punit);
bool unit_can_airlift_to(struct unit *punit, struct city *pcity);

bool can_unit_paradrop(struct unit *punit);
bool can_unit_change_homecity(struct unit *punit);
bool can_unit_do_connect(struct unit *punit, enum unit_activity activity);
char* get_activity_text (int activity);
bool can_unit_continue_current_activity(struct unit *punit);
bool can_unit_do_activity(struct unit *punit, enum unit_activity activity);
bool can_unit_do_activity_targeted(struct unit *punit,
				   enum unit_activity activity,
				   enum tile_special_type target);
void set_unit_activity(struct unit *punit, enum unit_activity new_activity);
void set_unit_activity_targeted(struct unit *punit,
				enum unit_activity new_activity,
				enum tile_special_type new_target);
bool can_unit_do_auto(struct unit *punit); 
bool is_unit_activity_on_tile(enum unit_activity activity, int x, int y);
int get_unit_tile_pillage_set(int x, int y);
bool is_military_unit(struct unit *punit);           /* !set !dip !cara */
bool is_diplomat_unit(struct unit *punit);
bool is_square_threatened(struct player *pplayer, int x, int y);
bool is_field_unit(struct unit *punit);              /* ships+aero */
bool is_hiding_unit(struct unit *punit);
bool is_sailing_unit(struct unit *punit);
bool is_air_unit(struct unit *punit);
bool is_heli_unit(struct unit *punit);
bool is_ground_unit(struct unit *punit);
bool can_unit_add_to_city (struct unit *punit);
bool can_unit_build_city (struct unit *punit);
bool can_unit_add_or_build_city (struct unit *punit);
enum add_build_city_result test_unit_add_or_build_city(struct unit *punit);
bool kills_citizen_after_attack(struct unit *punit);

const char *unit_activity_text(struct unit *punit);
int ground_unit_transporter_capacity(int x, int y, struct player *pplayer);
int get_transporter_capacity(struct unit *punit);
bool is_ground_units_transport(struct unit *punit);
bool is_air_units_transport(struct unit *punit);
int missile_carrier_capacity(int x, int y, struct player *pplayer,
			     bool count_units_with_extra_fuel);
int airunit_carrier_capacity(int x, int y, struct player *pplayer,
			     bool count_units_with_extra_fuel);

struct player *unit_owner(struct unit *punit);

struct unit *is_allied_unit_tile(struct tile *ptile,
				 struct player *pplayer);
struct unit *is_enemy_unit_tile(struct tile *ptile,
				struct player *pplayer);
struct unit *is_non_allied_unit_tile(struct tile *ptile,
				     struct player *pplayer);
struct unit *is_non_attack_unit_tile(struct tile *ptile,
				     struct player *pplayer);

int trireme_loss_pct(struct player *pplayer, int x, int y);
int base_trireme_loss_pct(struct player *pplayer);

bool is_my_zoc(struct player *unit_owner, int x0, int y0);
bool unit_being_aggressive(struct unit *punit);
bool can_step_taken_wrt_to_zoc(Unit_Type_id type, struct player *unit_owner,
			      int src_x, int src_y, int dest_x,
			      int dest_y);
bool can_unit_move_to_tile(struct unit *punit, int dest_x, int dest_y,
			   bool igzoc);
enum unit_move_result test_unit_move_to_tile(Unit_Type_id type,
					     struct player *unit_owner,
					     enum unit_activity activity,
					     bool connecting, int src_x,
					     int src_y, int dest_x,
					     int dest_y, bool igzoc);
bool unit_type_really_ignores_zoc(Unit_Type_id type);
bool zoc_ok_move(struct unit *punit, int x, int y);

bool is_build_or_clean_activity(enum unit_activity activity);

struct unit *create_unit_virtual(struct player *pplayer, struct city *pcity,
                                 Unit_Type_id type, bool make_veteran);
void destroy_unit_virtual(struct unit *punit);

#endif  /* FC__UNIT_H */
