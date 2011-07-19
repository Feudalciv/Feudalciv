/********************************************************************** 
 Freeciv - Copyright (C) 2002 - The Freeciv Project
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/
#ifndef FC__ADVDATA_H
#define FC__ADVDATA_H

/* utility */
#include "bitvector.h"
#include "support.h"            /* bool type */

/* common */
#include "fc_types.h"
#include "improvement.h"

/* 
 * This file and advdata.c contains global data structures for the AI
 * and some of the functions that fill them with useful values at the 
 * start of every turn. 
 */

enum ai_improvement_status {
  AI_IMPR_CALCULATE, /* Calculate exactly its effect */
  AI_IMPR_CALCULATE_FULL, /* Calculate including tile changes */
  AI_IMPR_ESTIMATE,  /* Estimate its effect using wild guesses */
  AI_IMPR_LAST
};

#if 0
struct ai_settler; /* see aisettler.c */
#endif

struct adv_dipl {
  /* Remember one example of each for text spam purposes. */
  bool allied_with_enemy;
};

struct adv_data {
  /* Whether adv_data_phase_init() has been called or not. */
  bool phase_is_initialized;

  /* The Wonder City */
  int wonder_city;

  /* Precalculated info about city improvements */
  enum ai_improvement_status impr_calc[MAX_NUM_ITEMS];
  enum req_range impr_range[MAX_NUM_ITEMS];

  /* Long-term threats, not to be confused with short-term danger */
  struct {
    bool invasions;   /* check if we need to consider invasions */
    bool *continent;  /* non-allied cities on continent? */
    bool *ocean;      /* non-allied offensive ships in ocean? */
    bool missile;     /* check for non-allied missiles */
    int nuclear;      /* nuke check: 0=no, 1=capability, 2=built */
    bool igwall;      /* enemies have igwall units */
  } threats;

  /* Keeps track of which continents are fully explored already */
  struct {
    bool *ocean;      /* are we done exploring this ocean? */
    bool *continent;  /* are we done exploring this continent? */
    bool land_done;   /* nothing more on land to explore anywhere */
    bool sea_done;    /* nothing more to explore at sea */
  } explore;

  /* This struct is used for statistical unit building, eg to ensure
   * that we don't build too few or too many units of a given type. */
  struct {
    /* Counts of specific types of units. */
    struct {
      /* Unit-flag counts. */
      int triremes, missiles;

      /* Move-type counts */
      int land, sea, amphibious;

      /* Upgradeable units */
      int upgradeable;

      int paratroopers;
    } units;
    int *workers;     /* cities to workers on continent*/
    int *cities;      /* number of cities we have on continent */
    int average_production;
  } stats;

  struct {
    struct adv_dipl **adv_dipl_slots;

    struct player *spacerace_leader; /* who is leading the space pack */
    struct player *production_leader;
  } dipl;

  int num_continents; /* last time we updated our continent data */
  int num_oceans; /* last time we updated our continent data */

  /* Dynamic weights used in addition to Syela's hardcoded weights */
  int shield_priority;
  int food_priority;
  int luxury_priority;
  int gold_priority;
  int science_priority;
  int happy_priority;
  int unhappy_priority;
  int angry_priority;
  int pollution_priority;

  /* Government data */
  int *government_want;
  short govt_reeval;

  /* Goals */
  struct {
    struct {
      struct government *gov;        /* The ideal government */
      int val;        /* Its value (relative to the current gov) */
      int req;        /* The tech requirement for the ideal gov */
    } govt;
    struct government *revolution;   /* The best gov of the now available */
  } goal;
  
  /* Whether science would benefit player at all */
  bool wants_science;

  /* If the AI celebrates. */
  bool celebrate;

  /* AI doesn't like having more than this number of cities */
  int max_num_cities;
};

enum choice_type {
  CT_NONE = 0,
  CT_BUILDING = 1,
  CT_CIVILIAN,
  CT_ATTACKER,
  CT_DEFENDER,
  CT_LAST
};

struct adv_choice {
  enum choice_type type;
  universals_u value; /* what the advisor wants */
  int want;              /* how much it wants it (0-100) */
  bool need_boat;        /* unit being built wants a boat */
};

void adv_data_init(struct player *pplayer);
void adv_data_default(struct player *pplayer);
void adv_data_close(struct player *pplayer);

bool adv_data_phase_init(struct player *pplayer, bool is_new_phase);
void adv_data_phase_done(struct player *pplayer);
bool is_adv_data_phase_open(struct player *pplayer);

void adv_data_analyze_rulesets(struct player *pplayer);

struct adv_data *adv_data_get(struct player *pplayer);

void adv_best_government(struct player *pplayer);

bool adv_wants_science(struct player *pplayer);

bool adv_is_player_dangerous(struct player *pplayer,
                             struct player *aplayer);

#endif /* FC__ADVDATA_H */
