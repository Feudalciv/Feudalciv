/********************************************************************** 
 Freeciv - Copyright (C) 2001 - R. Falke
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
***********************************************************************/

#ifndef FC__CLIENT_AGENTS_CITY_MANAGEMENT_H
#define FC__CLIENT_AGENTS_CITY_MANAGEMENT_H

/*
 * CMA stands for citizen management agent.
 *
 * The purpose of this agent is to manage the citizens of a city. The
 * caller has to provide a goal (struct cma_parameter) which
 * determines in which way the citizens are allocated and placed. The
 * agent will also avoid disorder.
 *
 * The plan defines a minimal surplus. The agent will try to get the
 * required surplus. If there are citizens free after allocation of the
 * minimal surplus these citizens will get arranged to maximize the
 * sum over base*factor. The base depends upon the factor_target.
 */

#include "attribute.h"
#include "city.h"

enum factor_target {
  FT_SURPLUS,			/* will use the surplus as base */
  FT_EXTRA			/* will use (minimal_surplus-surplus) as base */
};

enum cma_stat { FOOD, SHIELD, TRADE, GOLD, LUXURY, SCIENCE, NUM_STATS };

/* A description of the goal. */
struct cma_parameter {
  int minimal_surplus[NUM_STATS];
  bool require_happy;

  enum factor_target factor_target;

  int factor[NUM_STATS];
  int happy_factor;
};

/* A result which can examined. */
struct cma_result {
  short int found_a_valid, disorder, happy;

  short int production[NUM_STATS];
  short int surplus[NUM_STATS];

  char worker_positions_used[CITY_MAP_SIZE][CITY_MAP_SIZE];
  short int entertainers, scientists, taxmen;
};

/*
 * Called once per client start.
 */
void cma_init(void);

void cma_free(void);

/*
 * Will try to meet the requirements and fill out the result. Caller
 * should test result->found_a_valid. cma_query_result will not change
 * the actual city setting.
 */
void cma_query_result(struct city *pcity,
		      const struct cma_parameter *const parameter,
		      struct cma_result *result);

/* Change the actual city setting. */
bool cma_apply_result(struct city *pcity,
		     const struct cma_result *const result);

/* Till a call of cma_release_city the city will be managed by the agent. */
void cma_put_city_under_agent(struct city *pcity,
			      const struct cma_parameter *const parameter);

/* Release the city from the agent. */
void cma_release_city(struct city *pcity);

/*
 * Test if the citizen in the given city are managed by the agent. The
 * given parameter is filled if pointer is non-NULL. The parameter is
 * only valid if cma_is_city_under_agent returns true.
 */
bool cma_is_city_under_agent(struct city *pcity,
			    struct cma_parameter *parameter);

/***************** utility methods *************************************/
const char *const cma_get_stat_name(enum cma_stat stat);
bool cma_are_parameter_equal(const struct cma_parameter *const p1,
			    const struct cma_parameter *const p2);
void cma_copy_parameter(struct cma_parameter *dest,
			const struct cma_parameter *const src);
bool cma_get_parameter(enum attr_city attr, int city_id,
		       struct cma_parameter *parameter);
void cma_set_parameter(enum attr_city attr, int city_id,
		       const struct cma_parameter *parameter);

#endif
