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
  Reading and using the tilespec files, which describe
  the files and contents of tilesets.
***********************************************************************/
#ifndef FC__TILESPEC_H
#define FC__TILESPEC_H

#include "map.h"		/* NUM_DIRECTION_NSEW */

#include "citydlg_common.h"	/* enum citizen_type */
#include "colors_g.h"
#include "options.h"

struct Sprite;			/* opaque; gui-dep */
struct unit;
struct player;

struct drawn_sprite {
  struct Sprite *sprite;
  int offset_x, offset_y;	/* offset from tile origin */
};

const char **get_tileset_list(void);

void tilespec_read_toplevel(const char *tileset_name);
void tilespec_load_tiles(void);
void tilespec_free_tiles(void);

void tilespec_reread(const char *tileset_name);
void tilespec_reread_callback(struct client_option *option);

void tilespec_setup_unit_type(int id);
void tilespec_setup_impr_type(int id);
void tilespec_setup_tech_type(int id);
void tilespec_setup_tile_type(int id);
void tilespec_setup_government(int id);
void tilespec_setup_nation_flag(int id);
void tilespec_setup_city_tiles(int style);
void tilespec_alloc_city_tiles(int count);
void tilespec_free_city_tiles(int count);

/* Gfx support */

int fill_tile_sprite_array_iso(struct drawn_sprite *sprs,
			       struct Sprite **coasts,
			       struct Sprite **dither,
			       int x, int y, bool citymode,
			       bool *solid_bg);
int fill_tile_sprite_array(struct drawn_sprite *sprs, int abs_x0, int abs_y0,
			   bool citymode, bool *solid_bg,
			   struct player **pplayer);
int fill_unit_sprite_array(struct drawn_sprite *sprs,
			   struct unit *punit, bool *solid_bg);
int fill_city_sprite_array_iso(struct drawn_sprite *sprs,
			       struct city *pcity);

enum color_std player_color(struct player *pplayer);
enum color_std overview_tile_color(int x, int y);

void set_focus_unit_hidden_state(bool hide);
struct unit *get_drawable_unit(int x, int y, bool citymode);


/* This the way directional indices are now encoded: */

#define BIT_NORTH (0x01)
#define BIT_SOUTH (0x02)
#define BIT_EAST  (0x04)
#define BIT_WEST  (0x08)

#define INDEX_NSEW(n,s,e,w) (((n) ? BIT_NORTH : 0) | \
                             ((s) ? BIT_SOUTH : 0) | \
                             ((e) ? BIT_EAST  : 0) | \
                             ((w) ? BIT_WEST  : 0))

#define NUM_TILES_PROGRESS 8
#define NUM_TILES_CITIZEN CITIZEN_LAST
#define NUM_TILES_HP_BAR 11
#define NUM_TILES_DIGITS 10
#define MAX_NUM_CITIZEN_SPRITES 6

/* This could be moved to common/map.h if there's more use for it. */
enum direction4 {
  DIR4_NORTH = 0, DIR4_SOUTH, DIR4_EAST, DIR4_WEST
};

struct named_sprites {
  struct Sprite
    *bulb[NUM_TILES_PROGRESS],
    *warming[NUM_TILES_PROGRESS],
    *cooling[NUM_TILES_PROGRESS],
    *treaty_thumb[2],     /* 0=disagree, 1=agree */
    *right_arrow,

    *black_tile,      /* only used for isometric view */
    *dither_tile,     /* only used for isometric view */
    *coast_color;     /* only used for isometric view */

  struct {
    /* Each citizen type has up to MAX_NUM_CITIZEN_SPRITES different
     * sprites, as defined by the tileset. */
    int count;
    struct Sprite *sprite[MAX_NUM_CITIZEN_SPRITES];
  } citizen[NUM_TILES_CITIZEN];
  struct {
    struct Sprite
      *solar_panels,
      *life_support,
      *habitation,
      *structural,
      *fuel,
      *propulsion;
  } spaceship;
  struct {
    struct Sprite
      /* for roadstyle 0 */
      *dir[8],     /* all entries used */
      /* for roadstyle 1 */
      *cardinal[NUM_DIRECTION_NSEW],     /* first unused */
      *diagonal[NUM_DIRECTION_NSEW],     /* first unused */
      /* for roadstyle 0 and 1 */
      *isolated,
      *corner[NUM_DIRECTION_NSEW]; /* only diagonal directions used */
  } road, rail;
  struct {
    struct Sprite *nuke[3][3];	         /* row, column, from top-left */
    struct Sprite **unit;
    struct Sprite *iso_nuke;
  } explode;
  struct {
    struct Sprite
      *hp_bar[NUM_TILES_HP_BAR],
      *auto_attack,
      *auto_settler,
      *auto_explore,
      *fallout,
      *fortified,
      *fortifying,
      *fortress,
      *airbase,
      *go_to,			/* goto is a C keyword :-) */
      *irrigate,
      *mine,
      *pillage,
      *pollution,
      *road,
      *sentry,
      *stack,
      *transform,
      *connect,
      *patrol;
  } unit;
  struct {
    struct Sprite
      *food[2],
      *unhappy[2],
      *shield;
  } upkeep;
  struct {
    struct Sprite
      *occupied,
      *disorder,
      *size[NUM_TILES_DIGITS],
      *size_tens[NUM_TILES_DIGITS],		/* first unused */
      *tile_foodnum[NUM_TILES_DIGITS],
      *tile_shieldnum[NUM_TILES_DIGITS],
      *tile_tradenum[NUM_TILES_DIGITS],
      ***tile_wall,      /* only used for isometric view */
      ***tile;
  } city;
  struct {
    struct Sprite *attention;
  } user;
  struct {
    struct Sprite
      *farmland[NUM_DIRECTION_NSEW],
      *irrigation[NUM_DIRECTION_NSEW],
      *mine,
      *oil_mine,
      *pollution,
      *village,
      *fortress,
      *fortress_back,
      *airbase,
      *fallout,
      *fog,
      *spec_river[NUM_DIRECTION_NSEW],
      *darkness[NUM_DIRECTION_NSEW],         /* first unused */
      *river_outlet[4],		/* indexed by enum direction4 */
      /* for isometric */
      *spec_forest[NUM_DIRECTION_NSEW],
      *spec_mountain[NUM_DIRECTION_NSEW],
      *spec_hill[NUM_DIRECTION_NSEW],
      *coast_cape_iso[8][4], /* 4 = up down left right */
      /* for non-isometric */
      *coast_cape[NUM_DIRECTION_NSEW],	      /* first unused */
      *denmark[2][3];		/* row, column */
  } tx;				/* terrain extra */
};

extern struct named_sprites sprites;

struct Sprite *get_citizen_sprite(enum citizen_type type, int citizen_index,
				  struct city *pcity);

/* full pathnames: */
extern char *main_intro_filename;
extern char *minimap_intro_filename;

/* These variables contain the size of the tiles used within the game.
 *
 * "normal" tiles include most mapview graphics, particularly the basic
 * terrain graphics.
 *
 * "unit" tiles are those used for drawing units.  In iso view these are
 * larger than normal tiles to mimic a 3D effect.
 *
 * "small" tiles are used for extra "theme" graphics, particularly sprites
 * for citizens, governments, and other panel indicator icons.
 *
 * Various parts of the code may make additional assumptions, including:
 *   - in non-iso view:
 *     - NORMAL_TILE_WIDTH == NORMAL_TILE_HEIGHT
 *     - UNIT_TILE_WIDTH == NORMAL_TILE_WIDTH
 *     - UNIT_TILE_HEIGHT == NORMAL_TILE_HEIGHT
 *   - in iso-view:
 *     - NORMAL_TILE_WIDTH == 2 * NORMAL_TILE_HEIGHT
 *     - UNIT_TILE_WIDTH == NORMAL_TILE_WIDTH
 *     - UNIT_TILE_HEIGHT == NORMAL_TILE_HEIGHT * 3 / 2
 *     - NORMAL_TILE_WIDTH and NORMAL_TILE_HEIGHT are even
 */

extern int NORMAL_TILE_WIDTH;
extern int NORMAL_TILE_HEIGHT;
extern int UNIT_TILE_WIDTH;
extern int UNIT_TILE_HEIGHT;
extern int SMALL_TILE_WIDTH;
extern int SMALL_TILE_HEIGHT;

extern int OVERVIEW_TILE_WIDTH;
extern int OVERVIEW_TILE_HEIGHT;

extern bool is_isometric;

/* name of font to use to draw city names on main map */

extern char *city_names_font;

/* name of font to use to draw city productions on main map */

extern char *city_productions_font_name;

extern int num_tiles_explode_unit;

struct Sprite *load_sprite(const char *tag_name);
void unload_sprite(const char *tag_name);
bool sprite_exists(const char *tag_name);
void finish_loading_sprites(void);

#endif  /* FC__TILESPEC_H */
