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
#ifndef FC__PLAYER_H
#define FC__PLAYER_H

#include "city.h"
#include "connection.h"
#include "fc_types.h"
#include "improvement.h"	/* Impr_Status */
#include "nation.h"
#include "shared.h"
#include "spaceship.h"
#include "tech.h"
#include "unit.h"

#define PLAYER_DEFAULT_TAX_RATE 0
#define PLAYER_DEFAULT_SCIENCE_RATE 100
#define PLAYER_DEFAULT_LUXURY_RATE 0

#define ANON_PLAYER_NAME "noname"
#define ANON_USER_NAME  "Unassigned"
#define OBSERVER_NAME	"Observer"

/*
 * pplayer->ai.barbarian_type uses this enum. Note that the values
 * have to stay since they are used in savegames.
 */
enum barbarian_type {
  NOT_A_BARBARIAN = 0,
  LAND_BARBARIAN = 1,
  SEA_BARBARIAN = 2
};

enum handicap_type {
  H_NONE = 0,         /* No handicaps */
  H_DIPLOMAT = 1,     /* Can't build offensive diplomats */
  H_AWAY = 2,         /* Away mode */
  H_LIMITEDHUTS = 4,  /* Can get only 25 gold and barbs from huts */
  H_DEFENSIVE = 8,    /* Build defensive buildings without calculating need */
  H_EXPERIMENTAL = 16,/* Enable experimental AI features (for testing) */
  H_RATES = 32,       /* Can't set its rates beyond government limits */
  H_TARGETS = 64,     /* Can't target anything it doesn't know exists */
  H_HUTS = 128,       /* Doesn't know which unseen tiles have huts on them */
  H_FOG = 256,        /* Can't see through fog of war */
  H_NOPLANES = 512,   /* Doesn't build air units */
  H_MAP = 1024,       /* Only knows map_is_known tiles */
  H_DIPLOMACY = 2048, /* Not very good at diplomacy */
  H_REVOLUTION = 4096 /* Cannot skip anarchy */
};

BV_DEFINE(bv_player, MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS);

struct player_economic {
  int gold;
  int tax;
  int science;
  int luxury;
};

struct player_research {
  /* The number of techs and future techs the player has
   * researched/acquired. */
  int techs_researched, future_tech;

  /* Invention being researched in. Valid values for researching are:
   *  - any existing tech but not A_NONE or
   *  - A_FUTURE.
   * In addition A_NOINFO is allowed at the client for enemies.
   *
   * bulbs_researched tracks how many bulbs have been accumulated toward
   * this research target. */
  Tech_type_id researching;        
  int bulbs_researched;

  /* If the player changes his research target in a turn, he loses some or
   * all of the bulbs he's accumulated toward that target.  We save the
   * original info from the start of the turn so that if he changes back
   * he will get the bulbs back. */
  Tech_type_id changed_from;
  int bulbs_researched_before;

  /* If the player completed a research this turn, this value is turned on
   * and changing targets may be done without penalty. */
  bool got_tech;

  struct {
    /* One of TECH_UNKNOWN, TECH_KNOWN or TECH_REACHABLE. */
    enum tech_state state;

    /* 
     * required_techs, num_required_techs and bulbs_required are
     * cached values. Updated from build_required_techs (which is
     * called by update_research).
     */
    tech_vector required_techs;
    int num_required_techs, bulbs_required;
  } inventions[A_LAST];

  /* Tech goal (similar to worklists; when one tech is researched the next
   * tech toward the goal will be chosen).  May be A_NONE. */
  Tech_type_id tech_goal;

  /*
   * Cached values. Updated by update_research.
   */
  int num_known_tech_with_flag[TF_LAST];
};

struct player_score {
  int happy;
  int content;
  int unhappy;
  int angry;
  int specialists[SP_MAX];
  int wonders;
  int techs;
  int techout;
  int landarea;
  int settledarea;
  int population; 	/* in thousand of citizen */
  int cities;
  int units;
  int pollution;
  int literacy;
  int bnp;
  int mfg;
  int spaceship;
  int game;             /* total score you get in player dialog */
};

struct player_ai {
  bool control;

  /* 
   * Valid values for tech_goal are:
   *  - any existing tech but not A_NONE or
   *  - A_UNSET.
   */
  int prev_gold;
  int maxbuycost;
  int est_upkeep; /* estimated upkeep of buildings in cities */
  int tech_want[A_LAST+1];
  int handicap;			/* sum of enum handicap_type */
  int skill_level;		/* 0-10 value for save/load/display */
  int fuzzy;			/* chance in 1000 to mis-decide */
  int expand;			/* percentage factor to value new cities */
  int science_cost;             /* Cost in bulbs to get new tech, relative
                                   to non-AI players (100: Equal cost) */
  int warmth, frost; /* threat of global warming / nuclear winter */
  enum barbarian_type barbarian_type;

  int love[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];
};

/* Diplomatic states (how one player views another).
 * (Some diplomatic states are "pacts" (mutual agreements), others aren't.)
 *
 * Adding to or reordering this array will break many things.
 */
enum diplstate_type {
  DS_NEUTRAL = 0,
  DS_WAR,
  DS_CEASEFIRE,
  DS_PEACE,
  DS_ALLIANCE,
  DS_NO_CONTACT,
  DS_TEAM,
  DS_LAST	/* leave this last */
};

struct player_diplstate {
  enum diplstate_type type;	/* this player's disposition towards other */
  /* the following are for "pacts" */
  int turns_left;		/* until pact (e.g., cease-fire) ends */
  int has_reason_to_cancel;	/* 0: no, 1: this turn, 2: this or next turn */
  int contact_turns_left;	/* until contact ends */
};

/***************************************************************************
  On the distinction between nations(formerly races), players, and users,
  see doc/HACKING
***************************************************************************/

enum player_debug_types {
  PLAYER_DEBUG_DIPLOMACY, PLAYER_DEBUG_TECH, PLAYER_DEBUG_LAST
};

BV_DEFINE(bv_debug, PLAYER_DEBUG_LAST);
struct player {
  int player_no;
  char name[MAX_LEN_NAME];
  char username[MAX_LEN_NAME];
  bool is_male;
  int government;
  int target_government;
  Nation_type_id nation;
  struct team *team;
  bool is_ready; /* Did the player click "start" yet? */
  bool phase_done;
  int nturns_idle;
  bool is_alive;
  bool is_observer; /* is the player a global observer */ 
  bool is_dying; /* set once the player is in the process of dying */
  bool surrendered; /* has indicated willingness to surrender */

  /* Turn in which the player's revolution is over; see update_revolution. */
  int revolution_finishes;

  bool capital; /* used to give player init_buildings in first city. */
  bv_player embassy;
  struct player_diplstate diplstates[MAX_NUM_PLAYERS + MAX_NUM_BARBARIANS];
  int city_style;
  struct unit_list *units;
  struct city_list *cities;
  struct player_score score;
  struct player_economic economic;
  struct player_research* research;
  int bulbs_last_turn;    /* # bulbs researched last turn only */
  struct player_spaceship spaceship;
  struct player_ai ai;
  bool was_created;                    /* if the player was /created */
  bool is_connected;		       /* observers don't count */
  struct connection *current_conn;     /* non-null while handling packet */
  struct conn_list *connections;       /* will replace conn */
  struct worklist worklists[MAX_NUM_WORKLISTS];
  struct player_tile *private_map;
  unsigned int gives_shared_vision; /* bitvector those that give you shared vision */
  unsigned int really_gives_vision; /* takes into account that p3 may see what p1 has via p2 */
  int small_wonders[B_LAST];              /* contains city id's */
         /* wonders[] may also be (-1), or the id of a city
	    which no longer exists, if the wonder has been destroyed */
  struct {
    int length;
    void *data;
  } attribute_block;
  bv_debug debug;
};

void player_research_init(struct player_research* research);
void player_init(struct player *plr);
struct player *find_player_by_name(const char *name);
struct player *find_player_by_name_prefix(const char *name,
					  enum m_pre_result *result);
struct player *find_player_by_user(const char *name);
void player_set_unit_focus_status(struct player *pplayer);
bool player_has_embassy(const struct player *pplayer,
			const struct player *pplayer2);

bool can_player_see_unit(const struct player *pplayer,
			 const struct unit *punit);
bool can_player_see_unit_at(const struct player *pplayer,
			    const struct unit *punit,
			    const struct tile *ptile);

bool can_player_see_units_in_city(const struct player *pplayer,
				  const struct city *pcity);
bool can_player_see_city_internals(const struct player *pplayer,
				   const struct city *pcity);

bool player_owns_city(const struct player *pplayer,
		      const struct city *pcity);

struct city *player_find_city_by_id(const struct player *pplayer,
				    int city_id);
struct unit *player_find_unit_by_id(const struct player *pplayer,
				    int unit_id);

bool player_in_city_radius(const struct player *pplayer,
			   const struct tile *ptile);
bool player_knows_techs_with_flag(const struct player *pplayer,
				  enum tech_flag_id flag);
int num_known_tech_with_flag(const struct player *pplayer,
			     enum tech_flag_id flag);
int player_get_expected_income(const struct player *pplayer);

void player_limit_to_government_rates(struct player *pplayer);

struct city *find_palace(const struct player *pplayer);

bool ai_handicap(const struct player *pplayer, enum handicap_type htype);
bool ai_fuzzy(const struct player *pplayer, bool normal_decision);

const char *diplstate_text(const enum diplstate_type type);
const char *love_text(const int love);

const struct player_diplstate *pplayer_get_diplstate(const struct player
						     *pplayer,
						     const struct player
						     *pplayer2);
bool are_diplstates_equal(const struct player_diplstate *pds1,
			  const struct player_diplstate *pds2);
bool pplayer_can_ally(const struct player *p1, const struct player *p2);
bool pplayers_at_war(const struct player *pplayer,
		    const struct player *pplayer2);
bool pplayers_allied(const struct player *pplayer,
		    const struct player *pplayer2);
bool pplayers_in_peace(const struct player *pplayer,
                    const struct player *pplayer2);
bool pplayers_non_attack(const struct player *pplayer,
			const struct player *pplayer2);
bool players_on_same_team(const struct player *pplayer1,
                          const struct player *pplayer2);
int player_in_territory(const struct player *pplayer,
			const struct player *pplayer2);

bool is_barbarian(const struct player *pplayer);

bool gives_shared_vision(const struct player *me, const struct player *them);

void merge_players_research(struct player* p1, struct player* p2);
void clean_players_research(void);
#define players_iterate(PI_player)                                            \
{                                                                             \
  struct player *PI_player;                                                   \
  int PI_p_itr;                                                               \
  for (PI_p_itr = 0; PI_p_itr < game.info.nplayers; PI_p_itr++) {            \
    PI_player = get_player(PI_p_itr);

#define players_iterate_end                                                   \
  }                                                                           \
}

/* ai love values should be in range [-MAX_AI_LOVE..MAX_AI_LOVE] */
#define MAX_AI_LOVE 1000


/* User functions. */
bool is_valid_username(const char *name);


#endif  /* FC__PLAYER_H */
