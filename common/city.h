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
#ifndef FC__CITY_H
#define FC__CITY_H

#include "improvement.h"
#include "unit.h"		/* struct unit_list */
#include "worklist.h"

struct player;
struct government;
struct tile;

enum production_class_type {
  TYPE_UNIT, TYPE_NORMAL_IMPROVEMENT, TYPE_WONDER
};

enum specialist_type {
  SP_ELVIS, SP_SCIENTIST, SP_TAXMAN
};

enum city_tile_type {
  C_TILE_EMPTY, C_TILE_WORKER, C_TILE_UNAVAILABLE
};

enum city_options {
  /* The first 4 are whether to auto-attack versus each unit move_type
   * from with auto-attack units within this city.  Note that these
   * should stay the first four, and must stay in the same order as
   * enum unit_move_type.  
   *
   * The next is whether building a settler at size 1 disbands a city.
   *
   * The following 2 are what to do of new citizens when the city grows:
   * make them workers, scientists, or taxmen. Should have only one set,
   * or if neither is set, that means make workers.
   *
   * Any more than 8 options requires a protocol extension, since
   * we only send 8 bits.
   */
  CITYO_ATT_LAND=0, CITYO_ATT_SEA, CITYO_ATT_HELI, CITYO_ATT_AIR,
  CITYO_DISBAND, CITYO_NEW_EINSTEIN, CITYO_NEW_TAXMAN
};

/* first four bits are for auto-attack: */
#define CITYOPT_AUTOATTACK_BITS 0xF

/* for new city: default auto-attack options all on, others off: */
#define CITYOPT_DEFAULT (CITYOPT_AUTOATTACK_BITS)

/* Diameter of the workable city area. Must be an odd number.
   Some places in the code hardcodes this number, fx 
   city_map_iterate_outwards_indices */
#define CITY_MAP_SIZE 5

/* Number of tiles a city can use */
#define CITY_TILES (CITY_MAP_SIZE * CITY_MAP_SIZE - 4)

#define INCITE_IMPOSSIBLE_COST (1000 * 1000 * 1000)

/*
 * Number of traderoutes a city can have.
 */
#define NUM_TRADEROUTES		4

/*
 * Size of the biggest possible city.
 *
 * The constant may be changed since it isn't externally visible.
 */
#define MAX_CITY_SIZE					100

/* Iterate a city map */

#define city_map_iterate(x, y)                     \
{                                                  \
  int x, y;                                        \
  for (y = 0; y < CITY_MAP_SIZE; y++)              \
    for (x = 0; x < CITY_MAP_SIZE; x++)            \
      if (! ((x == 0 || x == (CITY_MAP_SIZE-1)) && \
	     (y == 0 || y == (CITY_MAP_SIZE-1))) )

#define city_map_iterate_end                       \
}

/* Iterate a city map, from the center (the city) outwards */

extern int city_map_iterate_outwards_indices[CITY_TILES][2];

/* Iterate a city map, from the center (the city) outwards. x and y
   will be elements of [0, CITY_MAP_SIZE). */
#define city_map_iterate_outwards(x, y) {                                      \
  int x, y;                                                                    \
  int city_map_iterate_outwards_index;                                         \
  for                                                                          \
  (                                                                            \
    city_map_iterate_outwards_index = 0;                                       \
    city_map_iterate_outwards_index < CITY_TILES;                              \
    city_map_iterate_outwards_index++                                          \
  )                                                                            \
  {                                                                            \
    x = city_map_iterate_outwards_indices[city_map_iterate_outwards_index][0]; \
    y = city_map_iterate_outwards_indices[city_map_iterate_outwards_index][1];

#define city_map_iterate_outwards_end                                          \
  }                                                                            \
}

/*
 * Iterate a city map in checked real map coordinates. The center of
 * the city is given as a map position (x0,y0). cx and cy will be
 * elements of [0,CITY_MAP_SIZE). mx and my will form the map position
 * (mx,my).
 */
#define city_map_checked_iterate(x0, y0, cx, cy, mx, my) {     \
  city_map_iterate_outwards(cx, cy) {                          \
    int mx, my;                                                \
    if (base_city_map_to_map(&mx, &my, x0, y0, cx, cy)) {

#define city_map_checked_iterate_end \
    }                                \
  } city_map_iterate_outwards_end    \
}

/* Does the same thing as city_map_checked_iterate, but keeps the city
 * coordinates hidden. */
#define map_city_radius_iterate(city_x, city_y, map_x, map_y)     \
{                                                                 \
  city_map_checked_iterate(city_x, city_y, _cx, _cy, map_x, map_y) { 

#define map_city_radius_iterate_end                               \
  } city_map_checked_iterate_end;                                 \
}


enum choice_type { CT_NONE = 0, CT_BUILDING = 0, CT_NONMIL, CT_ATTACKER,
                   CT_DEFENDER, CT_LAST };

/* FIXME:

   This should detect also cases where type is just initialized with
   CT_NONE (probably in order to silence compiler warnings), but no real value
   is given. You have to change value of CT_BUILDING into 1 before you
   can add this check. It's left this way for now, is case hardcoded
   value 0 is still used somewhere instead of CT_BUILDING.

   -- Caz
*/
#define ASSERT_REAL_CHOICE_TYPE(type)                                    \
        assert(type >= 0 && type < CT_LAST /* && type != CT_NONE */ );


struct ai_choice {
  int choice;            /* what the advisor wants */
  int want;              /* how bad it wants it (0-100) */
  int type;              /* unit/building or other depending on question */
};

struct ai_city {
  int workremain;

  /* building desirabilities - easiest to handle them here -- Syela */
  int building_want[B_LAST];    /* not sure these will always be < 256 */
  int danger;                   /* danger to be compared to assess_defense */
  bool diplomat_threat;         /* enemy diplomat or spy is near the city */
  bool has_diplomat;            /* this city has diplomat or spy defender */
  int urgency;                  /* how close the danger is; if zero, 
                                   bodyguards can leave */
  int grave_danger;             /* danger, should show positive feedback */
  int wallvalue;                /* how much it helps for defenders to be 
                                   ground units */
  int trade_want;               /* saves a zillion calculations */
  struct ai_choice choice;      /* to spend gold in the right place only */
  int downtown;                 /* distance from neighbours, for locating 
                                   wonders wisely */
  int distance_to_wonder_city;  /* wondercity will set this for us, 
                                   avoiding paradox */

  /* Used for caching when settlers evalueate which tile to improve,
     and when we place workers. */
  signed short int detox[5][5];
  signed short int derad[5][5];
  signed short int mine[5][5];
  signed short int irrigate[5][5];
  signed short int road[5][5];
  signed short int railroad[5][5];
  signed short int transform[5][5];
  signed short int tile_value[5][5];

  /* so we can contemplate with warmap fresh and decide later */
  int settler_want, founder_want; /* for builder (F_SETTLERS) and founder (F_CITIES) */
  int invasion; /* who's coming to kill us, for attack co-ordination */
  int attack, bcost; /* This is also for invasion - total power and value of
                      * all units coming to kill us. */

  /* Used by _other_ cities temporarily while assigning diplomat targets */
  bool already_considered_for_diplomat;
};

struct city {
  int id;
  int owner;
  int x, y;
  char name[MAX_LEN_NAME];

  /* the people */
  int size;

  /* How the citizens feel:
     ppl_*[0] is distribution before any of the modifiers below.
     ppl_*[1] is distribution after luxury.
     ppl_*[2] is distribution after after building effects.
     ppl_*[3] is distribution after units enfored martial order.
     ppl_*[4] is distribution after wonders. (final result.) */
  int ppl_happy[5], ppl_content[5], ppl_unhappy[5], ppl_angry[5];

  /* Specialists */
  int ppl_elvis, ppl_scientist, ppl_taxman;

  /* trade routes */
  int trade[NUM_TRADEROUTES], trade_value[NUM_TRADEROUTES];

  /* the productions */
  int food_prod, food_surplus;
  int shield_prod, shield_surplus;
  int trade_prod, corruption, tile_trade;
  int shield_bonus, tax_bonus, science_bonus; /* more CPU savings! */

  /* the totals */
  int luxury_total, tax_total, science_total;
  
  /* the physics */
  int food_stock;
  int shield_stock;
  int pollution;
  /* city can't be incited if INCITE_IMPOSSIBLE_COST */
  int incite_revolt_cost;      
   
  bool is_building_unit;    /* boolean unit/improvement */
  int currently_building;
  
  Impr_Status improvements[B_LAST];
  
  struct worklist worklist;

  enum city_tile_type city_map[CITY_MAP_SIZE][CITY_MAP_SIZE];

  struct unit_list units_supported;

  /* TRUE iff there units in the town. Only set at the client. */
  bool occupied;	      

  int steal;		      /* diplomats steal once; for spies, gets harder */
  /* turn states */
  bool did_buy;
  bool did_sell, is_updated;
  int turn_last_built;	      /* The last year in which something was built */
  int turn_changed_target;    /* Suffer shield loss at most once per turn */
  int changed_from_id;	      /* If changed this turn, what changed from (id) */
  bool changed_from_is_unit;   /* If changed this turn, what changed from (unit?) */
  int disbanded_shields;      /* If you disband unit in a city. Count them */
  int caravan_shields;        /* If caravan has helped city to build wonder. */
  int before_change_shields;  /* If changed this turn, shields before penalty */
  int anarchy;		      /* anarchy rounds count */ 
  int rapture;                /* rapture rounds count */ 
  bool was_happy;
  bool airlift;
  int original;			/* original owner */
  int city_options;		/* bitfield; positions as enum city_options */

  /* server variable. indicates if the city map is synced with the client. */
  bool synced;
    
  int turn_founded;		/* In which turn was the city founded? */

  /* info for dipl/spy investigation -- used only in client */
  struct unit_list info_units_supported;
  struct unit_list info_units_present;

  struct ai_city ai;
};

/* city drawing styles */

#define MAX_CITY_TILES 8

struct citystyle {
  char name[MAX_LEN_NAME];
  char name_orig[MAX_LEN_NAME];	      /* untranslated */
  char graphic[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  int techreq;                  /* tech required to use a style      */
  int replaced_by;              /* index to replacing style          */
                                /* client side-only:                 */
  int tresh[MAX_CITY_TILES];    /* treshholds - what city size to use a tile */
  int tiles_num;                /* number of "normal" city tiles,    */
};                              /* not incl. wall and occupied tiles */

extern struct citystyle *city_styles;

/* get 'struct city_list' and related functions: */
#define SPECLIST_TAG city
#define SPECLIST_TYPE struct city
#include "speclist.h"

#define city_list_iterate(citylist, pcity) \
    TYPED_LIST_ITERATE(struct city, citylist, pcity)
#define city_list_iterate_end  LIST_ITERATE_END

/* properties */

struct player *city_owner(struct city *pcity);
int city_population(struct city *pcity);
int city_gold_surplus(struct city *pcity);
int city_buy_cost(struct city *pcity);
bool city_happy(struct city *pcity);  /* generally use celebrating instead */
bool city_unhappy(struct city *pcity);                /* anarchy??? */
bool base_city_celebrating(struct city *pcity);
bool city_celebrating(struct city *pcity);            /* love the king ??? */
bool city_rapture_grow(struct city *pcity);

/* city related improvement and unit functions */

bool city_has_terr_spec_gate(struct city *pcity, Impr_Type_id id); 
int improvement_upkeep(struct city *pcity, Impr_Type_id i); 
bool can_build_improvement(struct city *pcity, Impr_Type_id id);
bool can_eventually_build_improvement(struct city *pcity, Impr_Type_id id);
bool can_build_unit(struct city *pcity, Unit_Type_id id);
bool can_build_unit_direct(struct city *pcity, Unit_Type_id id);
bool can_eventually_build_unit(struct city *pcity, Unit_Type_id id);
bool city_got_building(struct city *pcity,  Impr_Type_id id); 
bool city_affected_by_wonder(struct city *pcity, Impr_Type_id id);
bool city_got_effect(struct city *pcity, Impr_Type_id id);
bool city_got_citywalls(struct city *pcity);
bool wonder_replacement(struct city *pcity, Impr_Type_id id);
int city_change_production_penalty(struct city *pcity,
				   int target, bool is_unit, bool apply_it);
int city_turns_to_build(struct city *pcity, int id, bool id_is_unit,
                        bool include_shield_stock );
int city_turns_to_grow(struct city *pcity);

/* textual representation of buildings */

const char *get_impr_name_ex(struct city *pcity, Impr_Type_id id);

/* tile production functions */

int get_shields_tile(int x, int y); /* shield on spot */
int get_trade_tile(int x, int y);   /* trade  on spot */
int get_food_tile(int x, int y);    /* food   on spot */

/* city map functions */

bool is_valid_city_coords(const int city_x, const int city_y);
bool is_city_center(int city_x, int city_y);
bool map_to_city_map(int *city_map_x, int *city_map_y,
		    const struct city *const pcity, int map_x, int map_y);

bool base_city_map_to_map(int *map_x, int *map_y, int city_center_x,
			 int city_center_y, int city_map_x,
			 int city_map_y);
bool city_map_to_map(int *map_x, int *map_y, const struct city *const pcity,
		    int city_map_x, int city_map_y);

/* shield on spot */
int city_get_shields_tile(int x, int y, struct city *pcity);
int base_city_get_shields_tile(int x, int y, struct city *pcity,
			       bool is_celebrating);
/* trade  on spot */
int city_get_trade_tile(int x, int y, struct city *pcity);
int base_city_get_trade_tile(int x, int y, struct city *pcity,
			     bool is_celebrating);
/* food   on spot */
int city_get_food_tile(int x, int y, struct city *pcity);
int base_city_get_food_tile(int x, int y, struct city *pcity,
			    bool is_celebrating);

void set_worker_city(struct city *pcity, int city_x, int city_y,
		     enum city_tile_type type); 
enum city_tile_type get_worker_city(struct city *pcity, int city_x,
				    int city_y);
void get_worker_on_map_position(int map_x, int map_y,
				enum city_tile_type *result_city_tile_type,
				struct city **result_pcity);
bool is_worker_here(struct city *pcity, int city_x, int city_y);

bool city_can_be_built_here(int x, int y);

/* trade functions */
bool can_establish_trade_route(struct city *pc1, struct city *pc2);
int trade_between_cities(struct city *pc1, struct city *pc2);
int city_num_trade_routes(struct city *pcity);

/* list functions */
struct city *city_list_find_id(struct city_list *This, int id);
struct city *city_list_find_name(struct city_list *This, const char *name);

int city_name_compare(const void *p1, const void *p2);

/* city free cost values depending on government: */
int citygov_free_shield(struct city *pcity, struct government *gov);
int citygov_free_happy(struct city *pcity, struct government *gov);
int citygov_free_food(struct city *pcity, struct government *gov);

/* city style functions */
int get_city_style(struct city *pcity);
int get_player_city_style(struct player *plr);
int get_style_by_name(const char *);

struct city *is_enemy_city_tile(struct tile *ptile, struct player *pplayer);
struct city *is_allied_city_tile(struct tile *ptile,
				 struct player *pplayer);
struct city *is_non_attack_city_tile(struct tile *ptile,
				     struct player *pplayer);
struct city *is_non_allied_city_tile(struct tile *ptile,
				     struct player *pplayer);

bool is_unit_near_a_friendly_city(struct unit *punit);
bool is_friendly_city_near(struct player *owner, int x, int y);
bool city_exists_within_city_radius(int x, int y, bool may_be_on_center);

/* granary size as a function of city size */
int city_granary_size(int city_size);

void city_add_improvement(struct city *pcity,Impr_Type_id impr);
void city_remove_improvement(struct city *pcity,Impr_Type_id impr);

/* city update functions */
void generic_city_refresh(struct city *pcity,
			  bool refresh_trade_route_cities);
void adjust_city_free_cost(int *num_free, int *this_cost);
int city_corruption(struct city *pcity, int trade);
int city_specialists(struct city *pcity);                 /* elv+tax+scie */
int get_temple_power(struct city *pcity);
int get_cathedral_power(struct city *pcity);
int get_colosseum_power(struct city *pcity);
int get_city_tax_bonus(struct city *pcity);
int get_city_shield_bonus(struct city *pcity);
int get_city_science_bonus(struct city *pcity);

/* misc */
bool is_city_option_set(struct city *pcity, enum city_options option);
void city_styles_alloc(int num);
void city_styles_free(void);

/*
 * Iterates over all improvements which are built in the given city.
 */
#define built_impr_iterate(m_pcity, m_i)                                      \
  impr_type_iterate(m_i) {                                                    \
    if((m_pcity)->improvements[m_i] == I_NONE) {                              \
      continue;                                                               \
    }

#define built_impr_iterate_end                                                \
  } impr_type_iterate_end;

#endif  /* FC__CITY_H */
