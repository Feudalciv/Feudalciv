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
#ifndef FC__MAP_H
#define FC__MAP_H

#include <assert.h>
#include <math.h>

#include "game.h"
#include "player.h"
#include "terrain.h"
#include "unit.h"

struct Sprite;			/* opaque; client-gui specific */

#define NUM_DIRECTION_NSEW 		16

/*
 * The value of MOVE_COST_FOR_VALID_SEA_STEP has no particular
 * meaning. The value is only used for comparison. The value must be
 * <0.
 */
#define MOVE_COST_FOR_VALID_SEA_STEP	(-3)

struct map_position {
  int x,y;
};

struct goto_route {
  int first_index; /* first valid tile pos */
  int last_index; /* point to the first non_legal pos. Note that the pos
		   is always alloced in the pos array (for coding reasons) */
  int length; /* length of pos array (use this as modulus when iterating)
		 Note that this is always at least 1 greater than the number
		 of valid positions, to make comparing first_index with
		 last_index during wrapped iteration easier. */
  struct map_position *pos;
};

/* For client Area Selection */
enum tile_hilite {
  HILITE_NONE = 0, HILITE_CITY
};

struct tile {
  enum tile_terrain_type terrain;
  enum tile_special_type special;
  struct city *city;
  struct unit_list units;
  unsigned int known;   /* A bitvector on the server side, an
			   enum known_type on the client side.
			   Player_no is index */
  int assigned; /* these can save a lot of CPU usage -- Syela */
  struct city *worked;      /* city working tile, or NULL if none */
  unsigned short continent;
  signed char move_cost[8]; /* don't know if this helps! */
  struct player *owner;     /* Player owning this tile, or NULL. */
  struct {
    /* Area Selection in client. */
    enum tile_hilite hilite;
  } client;
};


/****************************************************************
miscellaneous terrain information
*****************************************************************/

struct terrain_misc
{
  /* options */
  enum terrain_river_type river_style;
  bool may_road;       /* may build roads/railroads */
  bool may_irrigate;   /* may build irrigation/farmland */
  bool may_mine;       /* may build mines */
  bool may_transform;  /* may transform terrain */
  /* parameters */
  int ocean_reclaim_requirement;       /* # adjacent land tiles for reclaim */
  int land_channel_requirement;        /* # adjacent ocean tiles for channel */
  enum special_river_move river_move_mode;
  int river_defense_bonus;             /* % added to defense if Civ2 river */
  int river_trade_incr;                /* added to trade if Civ2 river */
  char *river_help_text;	       /* help for Civ2-style rivers */
  int fortress_defense_bonus;          /* % added to defense if fortress */
  int road_superhighway_trade_bonus;   /* % added to trade if road/s-highway */
  int rail_food_bonus;                 /* % added to food if railroad */
  int rail_shield_bonus;               /* % added to shield if railroad */
  int rail_trade_bonus;                /* % added to trade if railroad */
  int farmland_supermarket_food_bonus; /* % added to food if farm/s-market */
  int pollution_food_penalty;          /* % subtr. from food if polluted */
  int pollution_shield_penalty;        /* % subtr. from shield if polluted */
  int pollution_trade_penalty;         /* % subtr. from trade if polluted */
  int fallout_food_penalty;            /* % subtr. from food if fallout */
  int fallout_shield_penalty;          /* % subtr. from shield if fallout */
  int fallout_trade_penalty;           /* % subtr. from trade if fallout */
};

/****************************************************************
tile_type for each terrain type
expand with government bonuses??
*****************************************************************/
BV_DEFINE(bv_terrain_flags, TER_MAX);
struct tile_type {
  char terrain_name[MAX_LEN_NAME];     /* "" if unused */
  char terrain_name_orig[MAX_LEN_NAME];	/* untranslated copy */
  char graphic_str[MAX_LEN_NAME];
  char graphic_alt[MAX_LEN_NAME];
  struct Sprite *sprite[NUM_DIRECTION_NSEW];

  int movement_cost;
  int defense_bonus;

  int food;
  int shield;
  int trade;

  char special_1_name[MAX_LEN_NAME];   /* "" if none */
  char special_1_name_orig[MAX_LEN_NAME]; /* untranslated copy */
  int food_special_1;
  int shield_special_1;
  int trade_special_1;

  char special_2_name[MAX_LEN_NAME];   /* "" if none */
  char special_2_name_orig[MAX_LEN_NAME]; /* untranslated copy */
  int food_special_2;
  int shield_special_2;
  int trade_special_2;

  /* I would put the above special data in this struct too,
     but don't want to make unnecessary changes right now --dwp */
  struct {
    char graphic_str[MAX_LEN_NAME];
    char graphic_alt[MAX_LEN_NAME];
    struct Sprite *sprite;
  } special[2];

  int road_trade_incr;
  int road_time;

  enum tile_terrain_type irrigation_result;
  int irrigation_food_incr;
  int irrigation_time;

  enum tile_terrain_type mining_result;
  int mining_shield_incr;
  int mining_time;

  enum tile_terrain_type transform_result;
  int transform_time;

  bv_terrain_flags flags;

  char *helptext;
};

struct civ_map { 
  int topology_id;
  int xsize, ysize; /* native dimensions */
  int seed;
  int riches;
  bool is_earth;
  int huts;
  int landpercent;
  int grasssize;
  int swampsize;
  int deserts;
  int mountains;
  int riverlength;
  int forestsize;
  int generator;
  bool tinyisles;
  bool separatepoles;
  int num_start_positions;
  bool fixed_start_positions;
  bool have_specials;
  bool have_huts;
  bool have_rivers_overlay;	/* only applies if !have_specials */
  int num_continents;
  struct tile *tiles;

  /* Only used by server. */
  struct map_position *start_positions;	/* allocated at runtime */
};

enum topo_flag {
  /* Bit-values. */
  TF_WRAPX = 1,
  TF_WRAPY = 2,
  TF_ISO = 4
};

#define CURRENT_TOPOLOGY (map.topology_id)

#define topo_has_flag(flag) ((CURRENT_TOPOLOGY & (flag)) != 0)

bool map_is_empty(void);
void map_init(void);
void map_allocate(void);
void map_free(void);

const char *map_get_tile_info_text(int x, int y);
const char *map_get_tile_fpt_text(int x, int y);
struct tile *map_get_tile(int x, int y);

int map_distance(int x0, int y0, int x1, int y1);
int real_map_distance(int x0, int y0, int x1, int y1);
int sq_map_distance(int x0, int y0, int x1, int y1);
bool same_pos(int x1, int y1, int x2, int y2);
bool base_get_direction_for_step(int start_x, int start_y, int end_x,
				int end_y, int *dir);
int get_direction_for_step(int start_x, int start_y, int end_x, int end_y);

void map_set_continent(int x, int y, int val);
unsigned short map_get_continent(int x, int y);

void initialize_move_costs(void);
void reset_move_costs(int x, int y);

/* Maximum value of index (for sanity checks and allocations) */
#define MAX_MAP_INDEX map.xsize * map.ysize

#ifdef DEBUG
#define CHECK_MAP_POS(x,y) assert(is_normal_map_pos((x),(y)))
#define CHECK_INDEX(index) assert((index) >= 0 && (index) < MAX_MAP_INDEX)
#else
#define CHECK_MAP_POS(x,y) ((void)0)
#define CHECK_INDEX(index) ((void)0)
#endif

/* Obscure math.  See explanation in doc/HACKING. */
#define native_to_map_pos(pmap_x, pmap_y, nat_x, nat_y)                     \
  (topo_has_flag(TF_ISO)                                                    \
   ? (*(pmap_x) = ((nat_y) + ((nat_y) & 1)) / 2 + (nat_x),                  \
      *(pmap_y) = (nat_y) - *(pmap_x) + map.xsize)                          \
   : (*(pmap_x) = (nat_x), *(pmap_y) = (nat_y)))

#define map_to_native_pos(pnat_x, pnat_y, map_x, map_y)                     \
  (topo_has_flag(TF_ISO)                                                    \
   ? (*(pnat_y) = (map_x) + (map_y) - map.xsize,                            \
      *(pnat_x) = (2 * (map_x) - *(pnat_y) - (*(pnat_y) & 1)) / 2)          \
   : (*(pnat_x) = (map_x), *(pnat_y) = (map_y)))

/* Use map_to_native_pos instead unless you know what you're doing. */
#define map_pos_to_native_x(map_x, map_y)                                   \
  (topo_has_flag(TF_ISO)                                                    \
   ? (((map_x) - (map_y) + map.xsize                                        \
       - (((map_x) + (map_y) - map.xsize) & 1)) / 2)                        \
   : (map_x))
#define map_pos_to_native_y(map_x, map_y)                                   \
  (topo_has_flag(TF_ISO)                                                    \
   ? ((map_x) + (map_y) - map.xsize)                                        \
   : (map_y))

#define map_pos_to_index(map_x, map_y)        \
  (CHECK_MAP_POS((map_x), (map_y)),           \
   (map_pos_to_native_x(map_x, map_y)         \
    + map_pos_to_native_y(map_x, map_y) * map.xsize))

/* index_to_map_pos(int *, int *, int) inverts map_pos_to_index */
#define index_to_map_pos(pmap_x, pmap_y, index) \
  (CHECK_INDEX(index),                          \
   *(pmap_x) = (index) % map.xsize,             \
   *(pmap_y) = (index) / map.xsize,             \
   native_to_map_pos(pmap_x, pmap_y, *(pmap_x), *(pmap_y)))

#define DIRSTEP(dest_x, dest_y, dir)	\
(    (dest_x) = DIR_DX[(dir)],      	\
     (dest_y) = DIR_DY[(dir)])

/*
 * Returns true if the step yields a new valid map position. If yes
 * (dest_x, dest_y) is set to the new map position.
 *
 * Direct calls to DIR_DXY should be avoided and DIRSTEP should be
 * used. But to allow dest and src to be the same, as in
 *    MAPSTEP(x, y, x, y, dir)
 * we bend this rule here.
 */
#define MAPSTEP(dest_x, dest_y, src_x, src_y, dir)	\
(    (dest_x) = (src_x) + DIR_DX[(dir)],   		\
     (dest_y) = (src_y) + DIR_DY[(dir)],		\
     normalize_map_pos(&(dest_x), &(dest_y)))

struct player *map_get_owner(int x, int y);
void map_set_owner(int x, int y, struct player *pplayer);
struct city *map_get_city(int x, int y);
void map_set_city(int x, int y, struct city *pcity);
enum tile_terrain_type map_get_terrain(int x, int y);
enum tile_special_type map_get_special(int x, int y);
void map_set_terrain(int x, int y, enum tile_terrain_type ter);
void map_set_special(int x, int y, enum tile_special_type spe);
void map_clear_special(int x, int y, enum tile_special_type spe);
void map_clear_all_specials(int x, int y);
bool is_real_map_pos(int x, int y);
bool is_normal_map_pos(int x, int y);

/* implemented in server/maphand.c and client/climisc.c */
enum known_type map_get_known(int x, int y, struct player *pplayer);

/* special testing */
bool map_has_special(int x, int y, enum tile_special_type to_test_for);
bool tile_has_special(struct tile *ptile,
		      enum tile_special_type to_test_for);
bool contains_special(enum tile_special_type all,
		      enum tile_special_type to_test_for);

/*
 * A "border position" is any one that _may have_ positions within
 * map distance dist that are non-normal.  To see its correctness,
 * consider the case where dist is 1 or 0.
 *
 * TODO: implement this for iso-maps.
 */
#define IS_BORDER_MAP_POS(x, y, dist)                         \
  (topo_has_flag(TF_ISO)                                      \
   || (x) < (dist) || (x) >= map.xsize - (dist)               \
   || (y) < (dist) || (y) >= map.ysize - (dist))

bool normalize_map_pos(int *x, int *y);
void nearest_real_pos(int *x, int *y);
void map_distance_vector(int *dx, int *dy, int x0, int y0, int x1, int y1);
int map_num_tiles(void);

void rand_neighbour(int x0, int y0, int *x, int *y);
void rand_map_pos(int *x, int *y);

bool is_water_adjacent_to_tile(int x, int y);
bool is_tiles_adjacent(int x0, int y0, int x1, int y1);
bool is_move_cardinal(int start_x, int start_y, int end_x, int end_y);
int map_move_cost(struct unit *punit, int x, int y);
struct tile_type *get_tile_type(enum tile_terrain_type type);
void tile_types_free(void);
enum tile_terrain_type get_terrain_by_name(const char * name);
const char *get_terrain_name(enum tile_terrain_type type);
enum tile_special_type get_special_by_name(const char * name);
const char *get_special_name(enum tile_special_type type);
bool is_terrain_near_tile(int x, int y, enum tile_terrain_type t);
int count_terrain_near_tile(int x, int y, enum tile_terrain_type t);
enum terrain_flag_id terrain_flag_from_str(const char *s);
bool is_special_near_tile(int x, int y, enum tile_special_type spe);
int count_special_near_tile(int x, int y, enum tile_special_type spe);
bool is_coastline(int x,int y);
bool terrain_is_clean(int x, int y);
bool is_at_coast(int x, int y);
bool is_hut_close(int x, int y);
bool is_starter_close(int x, int y, int nr, int dist); 
int is_good_tile(int x, int y);
bool is_special_close(int x, int y);
bool is_sea_usable(int x, int y);
int get_tile_food_base(struct tile * ptile);
int get_tile_shield_base(struct tile * ptile);
int get_tile_trade_base(struct tile * ptile);
int get_tile_infrastructure_set(struct tile * ptile);
const char *map_get_infrastructure_text(int spe);
int map_get_infrastructure_prerequisite(int spe);
enum tile_special_type get_preferred_pillage(int pset);

void map_irrigate_tile(int x, int y);
void map_mine_tile(int x, int y);
void change_terrain(int x, int y, enum tile_terrain_type type);
void map_transform_tile(int x, int y);

int map_build_road_time(int x, int y);
int map_build_irrigation_time(int x, int y);
int map_build_mine_time(int x, int y);
int map_transform_time(int x, int y);
int map_build_rail_time(int x, int y);
int map_build_airbase_time(int x, int y);
int map_build_fortress_time(int x, int y);
int map_clean_pollution_time(int x, int y);
int map_clean_fallout_time(int x, int y);
int map_activity_time(enum unit_activity activity, int x, int y);

bool can_channel_land(int x, int y);
bool can_reclaim_ocean(int x, int y);

extern struct civ_map map;

extern struct terrain_misc terrain_control;
extern struct tile_type tile_types[T_LAST];

/* This iterates outwards from the starting point (Duh?).
   Every tile within max_dist will show up exactly once. (even takes
   into account wrap). All positions given correspond to real tiles.
   The values given are adjusted.
   You should make sure that the arguments passed to the macro are adjusted,
   or you could have some very nasty intermediate errors.

   Internally it works by for a given distance
   1) assume y positive and iterate over x
   2) assume y negative and iterate over x
   3) assume x positive and iterate over y
   4) assume x negative and iterate over y
   Where in this we are is decided by the variables xcycle and positive.
   each of there distances give a box of tiles; to ensure each tile is only
   returned once we only return the corner when iterating over x.
   As a special case positive is initialized as 0 (ie we start in step 2) ),
   as the center tile would else be returned by both step 1) and 2).
*/
#define iterate_outward(ARG_start_x, ARG_start_y, ARG_max_dist, ARG_x_itr, ARG_y_itr) \
{                                                                             \
  int ARG_x_itr, ARG_y_itr;                                                   \
  int MACRO_max_dx = map.xsize/2;                                             \
  int MACRO_min_dx = -MACRO_max_dx - 1 + (map.xsize % 2);                     \
  bool MACRO_xcycle = TRUE;                                                   \
  bool MACRO_positive = FALSE;                                                \
  bool MACRO_is_border = IS_BORDER_MAP_POS(ARG_start_x, ARG_start_y,          \
                                           ARG_max_dist);                     \
  int MACRO_dxy = 0, MACRO_do_xy;                                             \
  CHECK_MAP_POS(ARG_start_x, ARG_start_y);                                    \
  while(MACRO_dxy <= (ARG_max_dist)) {                                        \
    for (MACRO_do_xy = -MACRO_dxy; MACRO_do_xy <= MACRO_dxy; MACRO_do_xy++) { \
      if (MACRO_xcycle) {                                                     \
	ARG_x_itr = (ARG_start_x) + MACRO_do_xy;                              \
	if (MACRO_positive)                                                   \
	  ARG_y_itr = (ARG_start_y) + MACRO_dxy;                              \
	else                                                                  \
	  ARG_y_itr = (ARG_start_y) - MACRO_dxy;                              \
      } else { /* ! MACRO_xcycle */                                           \
        if (MACRO_dxy == MACRO_do_xy || MACRO_dxy == -MACRO_do_xy)            \
          continue;                                                           \
	ARG_y_itr = (ARG_start_y) + MACRO_do_xy;                              \
	if (MACRO_positive)                                                   \
	  ARG_x_itr = (ARG_start_x) + MACRO_dxy;                              \
	else                                                                  \
	  ARG_x_itr = (ARG_start_x) - MACRO_dxy;                              \
      }                                                                       \
      {                                                                       \
	int MACRO_dx = (ARG_start_x) - ARG_x_itr;                             \
	if (MACRO_dx > MACRO_max_dx || MACRO_dx < MACRO_min_dx)               \
	  continue;                                                           \
      }                                                                       \
      if (MACRO_is_border && !normalize_map_pos(&ARG_x_itr, &ARG_y_itr)) {    \
	continue;                                                             \
      }

#define iterate_outward_end                                                   \
    }                                                                         \
    if (!MACRO_positive) {                                                    \
      if (!MACRO_xcycle)                                                      \
	MACRO_dxy++;                                                          \
      MACRO_xcycle = !MACRO_xcycle;                                           \
    }                                                                         \
    MACRO_positive = !MACRO_positive;                                         \
  }                                                                           \
}

#define rectangle_iterate(map_x0, map_y0, map_width, map_height,            \
                          x_itr, y_itr)                                     \
{                                                                           \
  int RI_dx_itr, RI_dy_itr;                                                 \
  for (RI_dy_itr = 0; RI_dy_itr < (map_height); RI_dy_itr++) {              \
    for (RI_dx_itr = 0; RI_dx_itr < (map_width); RI_dx_itr++) {             \
      int x_itr = RI_dx_itr + (map_x0), y_itr = RI_dy_itr + (map_y0);       \
      if (normalize_map_pos(&x_itr, &y_itr)) {

#define rectangle_iterate_end                                               \
      }                                                                     \
    }                                                                       \
  }                                                                         \
}

/* 
 * Iterate through all tiles in a square with given center and radius.
 * The position (x_itr, y_itr) that is returned will be normalized;
 * unreal positions will be automatically discarded. (dx_itr, dy_itr)
 * is the standard distance vector between the position and the center
 * position. Note that when the square is larger than the map the
 * distance vector may not be the minimum distance vector.
 */
#define square_dxy_iterate(center_x, center_y, radius, x_itr, y_itr,          \
                           dx_itr, dy_itr)                                    \
{                                                                             \
  int dx_itr, dy_itr;                                                         \
  bool _is_border = IS_BORDER_MAP_POS((center_x), (center_y), (radius));      \
  CHECK_MAP_POS((center_x), (center_y));                                      \
  for (dy_itr = -(radius); dy_itr <= (radius); dy_itr++) {                    \
    for (dx_itr = -(radius); dx_itr <= (radius); dx_itr++) {                  \
      int x_itr = dx_itr + (center_x), y_itr = dy_itr + (center_y);           \
      if (_is_border && !normalize_map_pos(&x_itr, &y_itr)) {                 \
        continue;                                                             \
      }

#define square_dxy_iterate_end                                                \
    }                                                                         \
  }                                                                           \
}

/*
 * Iterate through all tiles in a square with given center and radius.
 * Positions returned will have adjusted x, and positions with illegal
 * y will be automatically discarded.
 */
#define square_iterate(center_x, center_y, radius, x_itr, y_itr)              \
{                                                                             \
  square_dxy_iterate(center_x, center_y, radius, x_itr, y_itr,                \
                     _dummy_x, _dummy_y);

#define square_iterate_end  square_dxy_iterate_end                            \
}

/* 
 * Iterate through all tiles in a circle with given center and squared
 * radius.  Positions returned will have adjusted (x, y); unreal
 * positions will be automatically discarded. 
 */
#define circle_iterate(center_x, center_y, sq_radius, x_itr, y_itr)           \
{                                                                             \
  int _cr_radius = (int)sqrt((double)(sq_radius));                            \
  square_dxy_iterate(center_x, center_y, _cr_radius,                          \
		     x_itr, y_itr, _dx, _dy) {                                \
    if (_dy * _dy + _dx * _dx <= (sq_radius)) {

#define circle_iterate_end                                                    \
    }                                                                         \
  } square_dxy_iterate_end;                                                   \
}									       

/* Iterate through all tiles adjacent to a tile */
#define adjc_iterate(RI_center_x, RI_center_y, RI_x_itr, RI_y_itr)            \
{                                                                             \
  square_iterate(RI_center_x, RI_center_y, 1, RI_x_itr, RI_y_itr) {           \
    if (RI_x_itr == RI_center_x && RI_y_itr == RI_center_y) {                 \
      continue;                                                               \
    }

#define adjc_iterate_end                                                      \
  } square_iterate_end;                                                       \
}

/* Iterate through all tiles adjacent to a tile.  dir_itr is the
   directional value (see DIR_D[XY]).  This assumes that center_x and
   center_y are normalized. --JDS */
#define adjc_dir_iterate(center_x, center_y, x_itr, y_itr, dir_itr)           \
{                                                                             \
  int x_itr, y_itr, dir_itr;                                                  \
  bool MACRO_border = IS_BORDER_MAP_POS((center_x), (center_y), 1);           \
  int MACRO_center_x = (center_x);                                            \
  int MACRO_center_y = (center_y);                                            \
  CHECK_MAP_POS(MACRO_center_x, MACRO_center_y);                              \
  for (dir_itr = 0; dir_itr < 8; dir_itr++) {                                 \
    DIRSTEP(x_itr, y_itr, dir_itr);                                           \
    x_itr += MACRO_center_x;                                                  \
    y_itr += MACRO_center_y;                                                  \
    if (MACRO_border && !normalize_map_pos(&x_itr, &y_itr)) {                 \
	continue;                                                             \
    }

#define adjc_dir_iterate_end                                                  \
  }                                                                           \
}

/* Iterate over all positions on the globe. */
#define whole_map_iterate(map_x, map_y)                                     \
{                                                                           \
  int WMI_index; /* We use index positions for cache efficiency. */         \
  for (WMI_index = 0; WMI_index < MAX_MAP_INDEX; WMI_index++) {             \
    int map_x, map_y;                                                       \
    index_to_map_pos(&map_x, &map_y, WMI_index);                            \
    {

#define whole_map_iterate_end                                               \
    }                                                                       \
  }                                                                         \
}

/*
used to compute neighboring tiles:
using
x1 = x + DIR_DX[dir];
y1 = y + DIR_DX[dir];
will give you the tile as shown below.
-------
|0|1|2|
|-+-+-|
|3| |4|
|-+-+-|
|5|6|7|
-------
Note that you must normalize x1 and y1 yourself.
*/

enum direction8 {
  /* FIXME: DIR8 is used to avoid conflict with
   * enum Directions in client/tilespec.h */
  DIR8_NORTHWEST = 0,
  DIR8_NORTH = 1,
  DIR8_NORTHEAST = 2,
  DIR8_WEST = 3,
  DIR8_EAST = 4,
  DIR8_SOUTHWEST = 5,
  DIR8_SOUTH = 6,
  DIR8_SOUTHEAST = 7
};

BV_DEFINE(dir_vector, 8);

/* return the reverse of the direction */
#define DIR_REVERSE(dir) (7 - (dir))

/* is the direction "cardinal"?  Cardinal directions
 * (also called cartesian) are the four main ones */
#define DIR_IS_CARDINAL(dir)                           \
  ((dir) == DIR8_NORTH || (dir) == DIR8_EAST ||        \
   (dir) == DIR8_WEST || (dir) == DIR8_SOUTH)

enum direction8 dir_cw(enum direction8 dir);
enum direction8 dir_ccw(enum direction8 dir);
const char* dir_get_name(enum direction8 dir);
bool is_valid_dir(enum direction8 dir);

extern const int DIR_DX[8];
extern const int DIR_DY[8];

/* like DIR_DX[] and DIR_DY[], only cartesian */
extern const int CAR_DIR_DX[4];
extern const int CAR_DIR_DY[4];

#define cartesian_adjacent_iterate(x, y, IAC_x, IAC_y)                        \
{                                                                             \
  int IAC_i;                                                                  \
  int IAC_x, IAC_y;                                                           \
  bool _is_border = IS_BORDER_MAP_POS(x, y, 1);                               \
  CHECK_MAP_POS(x, y);                                                        \
  for (IAC_i = 0; IAC_i < 4; IAC_i++) {                                       \
    IAC_x = x + CAR_DIR_DX[IAC_i];                                            \
    IAC_y = y + CAR_DIR_DY[IAC_i];                                            \
                                                                              \
    if (_is_border && !normalize_map_pos(&IAC_x, &IAC_y)) {                   \
      continue;                                                               \
    }

#define cartesian_adjacent_iterate_end                                        \
  }                                                                           \
}

/* Used for network transmission; do not change. */
#define MAP_TILE_OWNER_NULL	 MAX_UINT16

#define MAP_DEFAULT_HUTS         50
#define MAP_MIN_HUTS             0
#define MAP_MAX_HUTS             500

#define MAP_DEFAULT_WIDTH        80
#define MAP_MIN_WIDTH            40
#define MAP_MAX_WIDTH            200

#define MAP_DEFAULT_HEIGHT       50
#define MAP_MIN_HEIGHT           25
#define MAP_MAX_HEIGHT           100

#define MAP_ORIGINAL_TOPO        TF_WRAPX
#define MAP_DEFAULT_TOPO         TF_WRAPX
#define MAP_MIN_TOPO             0
#define MAP_MAX_TOPO             3

#define MAP_DEFAULT_SEED         0
#define MAP_MIN_SEED             0
#define MAP_MAX_SEED             (MAX_UINT32 >> 1)

#define MAP_DEFAULT_LANDMASS     30
#define MAP_MIN_LANDMASS         15
#define MAP_MAX_LANDMASS         85

#define MAP_DEFAULT_RICHES       250
#define MAP_MIN_RICHES           0
#define MAP_MAX_RICHES           1000

#define MAP_DEFAULT_MOUNTAINS    30
#define MAP_MIN_MOUNTAINS        10
#define MAP_MAX_MOUNTAINS        100

#define MAP_DEFAULT_GRASS       35
#define MAP_MIN_GRASS           20
#define MAP_MAX_GRASS           100

#define MAP_DEFAULT_SWAMPS       5
#define MAP_MIN_SWAMPS           0
#define MAP_MAX_SWAMPS           100

#define MAP_DEFAULT_DESERTS      5
#define MAP_MIN_DESERTS          0
#define MAP_MAX_DESERTS          100

#define MAP_DEFAULT_RIVERS       5
#define MAP_MIN_RIVERS           0
#define MAP_MAX_RIVERS           100

#define MAP_DEFAULT_FORESTS      20
#define MAP_MIN_FORESTS          0
#define MAP_MAX_FORESTS          100

#define MAP_DEFAULT_GENERATOR    1
#define MAP_MIN_GENERATOR        1
#define MAP_MAX_GENERATOR        5

#define MAP_DEFAULT_TINYISLES    FALSE
#define MAP_MIN_TINYISLES        FALSE
#define MAP_MAX_TINYISLES        TRUE

#define MAP_DEFAULT_SEPARATE_POLES   TRUE
#define MAP_MIN_SEPARATE_POLES       FALSE
#define MAP_MAX_SEPARATE_POLES       TRUE

#endif  /* FC__MAP_H */
