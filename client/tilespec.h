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

#include "colors_g.h"
#include "options.h"

struct Sprite;			/* opaque; gui-dep */
struct unit;
struct player;

const char **get_tileset_list(void);

void tilespec_read_toplevel(const char *tileset_name);
void tilespec_load_tiles(void);
void tilespec_free_tiles(void);

void tilespec_reread(const char *tileset_name);
void tilespec_reread_callback(struct client_option *option);

void tilespec_setup_unit_type(int id);
void tilespec_setup_tile_type(int id);
void tilespec_setup_government(int id);
void tilespec_setup_nation_flag(int id);
void tilespec_setup_city_tiles(int style);
void tilespec_alloc_city_tiles(int count);
void tilespec_free_city_tiles(int count);

/* Gfx support */

int fill_tile_sprite_array_iso(struct Sprite **sprs, struct Sprite **coasts,
			       struct Sprite **dither,
			       int x, int y, bool citymode,
			       int *solid_bg);
int fill_tile_sprite_array(struct Sprite **sprs, int abs_x0, int abs_y0,
			   bool citymode, int *solid_bg, struct player **pplayer);
int fill_unit_sprite_array(struct Sprite **sprs, struct unit *punit, int *solid_bg);
int fill_city_sprite_array_iso(struct Sprite **sprs, struct city *pcity);

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
#define NUM_TILES_CITIZEN 11
#define NUM_TILES_HP_BAR 11
#define NUM_TILES_DIGITS 10

/* This could be moved to common/map.h if there's more use for it. */
enum direction4 {
  DIR4_NORTH = 0, DIR4_SOUTH, DIR4_EAST, DIR4_WEST
};

struct named_sprites {
  struct Sprite
    *bulb[NUM_TILES_PROGRESS],
    *warming[NUM_TILES_PROGRESS],
    *cooling[NUM_TILES_PROGRESS],
    *citizen[NUM_TILES_CITIZEN],   /* internal code... */
    *treaty_thumb[2],     /* 0=disagree, 1=agree */
    *right_arrow,

    *black_tile,      /* only used for isometric view */
    *dither_tile,     /* only used for isometric view */
    *coast_color;     /* only used for isometric view */
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
      *isolated,
      /* for isometric*/
      *dir[8],     /* first used! */
      /* for non-isometric */
      *cardinal[NUM_DIRECTION_NSEW],     /* first unused */
      *diagonal[NUM_DIRECTION_NSEW];     /* first unused */
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
      *farmland,
      *irrigation,
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

struct Sprite *get_citizen_sprite(int citizen);

extern char current_tile_set_name[512];

/* full pathnames: */
extern char *main_intro_filename;
extern char *minimap_intro_filename;

/* NOTE: The following comments are out of date and need to
 *       be revised!  -- dwp
 *
 * These variables contain thee size of the tiles used within the game.
 * Tiles for the units and city squares, etc, are usually 30x30.
 * Tiles for things like food production, etc, are usually 15x20.  We
 * say "usually" for two reasons:  
 *
 * First, it is feasible to replace the tiles in the .xpm files with
 * ones of some other size.  Mitch Davis (mjd@alphalink.com.au) has
 * done this, and replaced all the tiles with the ones from the
 * original Civ.  The tiles from the original civ are 32x32.  All that
 * is required is that these constants be changed.
 *
 * Second, although there is currently no "zoom" feature as in the
 * original Civ, we might add it some time in the future.  If and when
 * this happens, we'll have to stop using the constants, and go back
 * to using ints which change at runtime.  Note, this would require
 * quite a bit of memory and pixmap management work, so it seems like
 * a nasty task.
 *
 * BUG: pjunold informs me that there are hard-coded geometries in
 * the Freeciv.h file which will prevent ideal displaying of pixmaps
 * which are not of the original 30x30 size.  Also, the pixcomm widget
 * apparently also does not handle this well.  Truthfully, I hadn't
 * noticed at all! :-) (mjd)
 */

extern int NORMAL_TILE_WIDTH;
extern int NORMAL_TILE_HEIGHT;
extern int UNIT_TILE_WIDTH;
extern int UNIT_TILE_HEIGHT;
extern int SMALL_TILE_WIDTH;
extern int SMALL_TILE_HEIGHT;

extern bool is_isometric;

/* name of font to use to draw city names on main map */

extern char *city_names_font;

/* name of font to use to draw city productions on main map */

extern char *city_productions_font_name;

extern bool flags_are_transparent;

extern int num_tiles_explode_unit;

#endif  /* FC__TILESPEC_H */
