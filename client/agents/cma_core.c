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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "agents.h"
#include "attribute.h"
#include "chatline_g.h"
#include "city.h"
#include "civclient.h"
#include "climisc.h"
#include "clinet.h"
#include "dataio.h"
#include "events.h"
#include "fcintl.h"
#include "government.h"
#include "hash.h"
#include "log.h"
#include "mem.h"
#include "messagewin_g.h"
#include "packets.h"
#include "packhand.h"
#include "shared.h"		/* for MIN() */
#include "support.h"
#include "timing.h"

#include "cma_core.h"

/*
 * Terms used
 * ==========
 *
 * Primary Stats: food, shields and trade
 *
 * Secondary Stats: luxury, science and gold
 *
 * Happy State: disorder (unhappy), content (!unhappy && !happy) and
 * happy (happy)
 *
 * Combination: A combination is a distribution of workers on the city
 * map. There are several realisations of a certain combination. Each
 * realisation has a different number of specialists. All realisations
 * of a certain combination have the same primary stats.
 *
 * Simple Primary Stats: Primary Stats which are calculated as the sum
 * over all city tiles which are used by a worker.
 *
 * Rough description of the alogrithm
 * ==================================
 *
 * 1) for i in [0..max number of workers]:
 * 2)  list_i = generate all possible combinations with use i workers
 * 3)  list_i = filter list_i to discard combinations which are \
 *              worse than others in list_i
 * 4) best_r = null
 * 5) for c in concatenation of all list_i:
 * 6)   x = best realisation of all possible realisations of c
 * 7)   if fitness(x) > fitness(best_r):
 * 8)      best_r = x
 *
 * Reducing expensive calls
 * ========================
 *
 * As it can seen in the outline above the alogrithm is quite
 * computationally expensive. So we want to avoid calculating information
 * a second or third time. The bottleneck here is generic_city_refresh
 * calls. generic_city_refresh recalculates the city locally. This is
 * a quite complex calculation and it can be expected that with the
 * full implementation of generalized improvements it will become even
 * more expensive. generic_city_refresh will calculate based on the
 * worker allocation, the number of specialists, the government,
 * rates of the player, existing traderoutes, the primary and
 * secondary stats, and also the happy state. Fortunately
 * generic_city_refresh has properties which make it possible to avoid
 * calling it:
 * 
 *  a) the primary stats as returned by generic_city_refresh are always
 *  greater than or equal to the simple primary stats (which can be
 *  computed cheaply).
 *  b) the primary stats as computed by generic_city_refresh only
 *  depends on the simple primary stats. So simple primary stats will
 *  yield same primary stats.
 *  c) the secondary stats as computed by generic_city_refresh only
 *  depend on the trade and the number of specialists.
 *  d) the happy state as computed by generic_city_refresh only
 *  depend on the luxury and the number of workers.
 *
 * a) and b) allow the fast comparison of certain combinations in step
 * 3) above by comparing the simple primary stats of the combinations.
 *
 * b) allows it to only have to call generic_city_refresh one time to
 * yield the primary stats of a certain combination and so also of all
 * its realisations.
 *
 * c) and d) allow the almost complete caching of the secondary stats.
 *
 * Top-down description of the alogrithm
 * ==============================================
 * 
 * Main entry point is cma_query_result which calls optimize_final.
 *
 * optimize_final implements all of the above mentioned steps
 * 1)-8). It will use build_cache3 to do steps 1)-3). It will use
 * find_best_specialist_arrangement to do step 6). optimize_final will
 * also test if the realisation --- which makes the spare workers
 * entertainers --- can meet the requirements for the primary stats. The
 * user given goal can only be satisfied if this test is true.
 *
 * build_cache3 will create all possible combinations for a given
 * city. There are at most 2^MAX_FIELDS_USED possible
 * combinations. Usually the number is smaller because a certain
 * combination is worse than another. Only combinations which have the
 * same number of workers can be compared this way. Example: two
 * combinations which both use 2 tiles/worker. The first one yields
 * (food=3, shield=4, trade=2) the second one (food=3, shield=3,
 * trade=1). The second one will be discarded because it is worse than
 * the first one.
 *
 * find_best_specialist_arrangement will try all realisations for a
 * given combination. It will find the best one (according to the
 * fitness function) and will return this one. It may be the case that
 * no realisation can meet the requirements.
 *
 * Outside the algorithm
 * =====================
 *
 * The CMA is also an agent. The CMA will subscribe itself to all city
 * events. So if a city changes the callback function city_changed is
 * called. handle_city will be called from city_changed to update the
 * given city. handle_city will call cma_query_result and
 * apply_result_on_server to update the server city state.
 */

/****************************************************************************
 defines, structs, globals, forward declarations
*****************************************************************************/

#define NUM_PRIMARY_STATS				3

#define OPTIMIZE_FINAL_LOG_LEVEL			LOG_DEBUG
#define OPTIMIZE_FINAL_LOG_LEVEL2			LOG_DEBUG
#define FIND_BEST_SPECIALIST_ARRANGEMENT_LOG_LEVEL	LOG_DEBUG
#define APPLY_RESULT_LOG_LEVEL				LOG_DEBUG
#define CMA_QUERY_RESULT_LOG_LEVEL			LOG_DEBUG
#define HANDLE_CITY_LOG_LEVEL				LOG_DEBUG
#define HANDLE_CITY_LOG_LEVEL2				LOG_DEBUG
#define RESULTS_ARE_EQUAL_LOG_LEVEL			LOG_DEBUG
#define CALC_FITNESS_LOG_LEVEL				LOG_DEBUG
#define CALC_FITNESS_LOG_LEVEL2				LOG_DEBUG
#define EXPAND_CACHE3_LOG_LEVEL				LOG_DEBUG

#define SHOW_EXPAND_CACHE3_RESULT                       FALSE
#define SHOW_CACHE_STATS                                FALSE
#define SHOW_TIME_STATS                                 FALSE
#define SHOW_APPLY_RESULT_ON_SERVER_ERRORS              FALSE
#define DISABLE_CACHE3                                  FALSE
#define ALWAYS_APPLY_AT_SERVER                          FALSE

#define NUM_SPECIALISTS_ROLES				3
#define MAX_FIELDS_USED	       	(CITY_MAP_SIZE * CITY_MAP_SIZE - 4 - 1)
#define MAX_COMBINATIONS				100

#define SAVED_PARAMETER_SIZE				29

/* Maps scientists and taxmen to result for a certain combination. */
static struct {
  int hits, misses;
} cache1;

/*
 * Maps (trade, taxmen) -> (gold_production, gold_surplus)
 * Maps (trade, entertainers) -> (luxury_production, luxury_surplus)
 * Maps (trade, scientists) -> (science_production, science_surplus)
 * Maps (luxury, workers) -> (city_is_in_disorder, city_is_happy)
 */
static struct {
  int allocated_trade, allocated_size, allocated_luxury;
  int hits, misses;

  struct secondary_stat {
    short int is_valid, production, surplus;
  } *secondary_stats;
  struct city_status {
    short int is_valid, disorder, happy;
  } *city_status;
} cache2;

/* 
 * Contains all combinations. Caches all the data about a city across
 * multiple cma_query_result calls about the same city.
 */
static struct {
  int fields_available_total;

  struct {
    struct combination {
      int is_valid, max_scientists, max_taxmen, worker;
      int production2[NUM_PRIMARY_STATS];
      enum city_tile_type worker_positions[CITY_MAP_SIZE][CITY_MAP_SIZE];
      struct cma_result *cache1;
      struct cma_result all_entertainer;
    } combinations[MAX_COMBINATIONS];
  } results[MAX_FIELDS_USED + 1];

  int hits, misses;
  struct city *pcity;
} cache3;

/*
 * Misc statistic to analyze performance.
 */
static struct {
  struct timer *wall_timer;
  int queries, apply_result_ignored, apply_result_applied, refresh_forced;
} stats;

/*
 * Cached results of city_get_{food,trade,shield}_tile calls. Indexed
 * by city map.
 */
struct tile_stats {
  struct {
    short int stats[NUM_PRIMARY_STATS];
    short int is_valid;
  } tiles[CITY_MAP_SIZE][CITY_MAP_SIZE];
};

#define my_city_map_iterate(pcity, cx, cy) {                           \
  city_map_checked_iterate(pcity->x, pcity->y, cx, cy, map_x, map_y) { \
    if(!is_city_center(cx, cy)) {

#define my_city_map_iterate_end \
    }                                \
  } city_map_checked_iterate_end;    \
}

/****************************************************************************
 * implementation of utility functions (these are relatively independent
 * of the algorithms used)
 ****************************************************************************/

/****************************************************************************
 Returns the number of workers of the given result. The given result
 has to be a result for the given city.
*****************************************************************************/
static int count_worker(struct city *pcity,
			const struct cma_result *const result)
{
  int worker = 0;

  my_city_map_iterate(pcity, x, y) {
    if (result->worker_positions_used[x][y]) {
      worker++;
    }
  } my_city_map_iterate_end;

  return worker;
}

#define T(x) if (result1->x != result2->x) { \
	freelog(RESULTS_ARE_EQUAL_LOG_LEVEL, #x); \
	return FALSE; }

/****************************************************************************
 Returns TRUE iff the two results are equal. Both results have to be
 results for the given city.
*****************************************************************************/
static bool results_are_equal(struct city *pcity,
			     const struct cma_result *const result1,
			     const struct cma_result *const result2)
{
  T(disorder);
  T(happy);
  T(entertainers);
  T(scientists);
  T(taxmen);

  T(production[FOOD]);
  T(production[SHIELD]);
  T(production[TRADE]);
  T(production[GOLD]);
  T(production[LUXURY]);
  T(production[SCIENCE]);

  T(surplus[FOOD]);
  T(surplus[SHIELD]);
  T(surplus[TRADE]);
  T(surplus[GOLD]);
  T(surplus[LUXURY]);
  T(surplus[SCIENCE]);

  my_city_map_iterate(pcity, x, y) {
    if (result1->worker_positions_used[x][y] !=
	result2->worker_positions_used[x][y]) {
      freelog(RESULTS_ARE_EQUAL_LOG_LEVEL, "worker_positions_used");
      return FALSE;
    }
  } my_city_map_iterate_end;

  return TRUE;
}

#undef T

/****************************************************************************
 Returns the number of valid combinations which use the given number
 of fields/tiles.
*****************************************************************************/
static int count_valid_combinations(int fields_used)
{
  int i, result = 0;

  for (i = 0; i < MAX_COMBINATIONS; i++) {
    struct combination *current =
	&cache3.results[fields_used].combinations[i];

    if (current->is_valid) {
      result++;
    }
  }
  return result;
}

/****************************************************************************
 Simple send_packet_* wrapper. Will return the id of the request.
*****************************************************************************/
static int set_worker(struct city *pcity, int x, int y, bool set_clear)
{
  struct packet_city_request packet;

  freelog(LOG_DEBUG, "set_worker(city='%s'(%d), x=%d, y=%d, %s)",
	  pcity->name, pcity->id, x, y, set_clear ? "set" : "clear");

  packet.city_id = pcity->id;
  packet.worker_x = x;
  packet.worker_y = y;
  return send_packet_city_request(&aconnection, &packet,
				  (set_clear ? PACKET_CITY_MAKE_WORKER :
				   PACKET_CITY_MAKE_SPECIALIST));
}

/****************************************************************************
 Returns TRUE iff the given field can be used for a worker.
*****************************************************************************/
static bool can_field_be_used_for_worker(struct city *pcity, int x, int y)
{
  enum known_type known;
  int map_x, map_y;
  bool is_real;

  assert(is_valid_city_coords(x, y));

  if (pcity->city_map[x][y] == C_TILE_WORKER) {
    return TRUE;
  }

  if (pcity->city_map[x][y] == C_TILE_UNAVAILABLE) {
    return FALSE;
  }

  is_real = city_map_to_map(&map_x, &map_y, pcity, x, y);
  assert(is_real);

  known = tile_get_known(map_x, map_y);
  assert(known == TILE_KNOWN);

  return TRUE;
}

/****************************************************************************
 Returns TRUE iff if the given city can use this kind of specialists.
*****************************************************************************/
static bool can_use_specialist(struct city *pcity,
			       enum specialist_type specialist_type)
{
  if (specialist_type == SP_ELVIS) {
    return TRUE;
  }
  if (pcity->size >= 5) {
    return TRUE;
  }
  return FALSE;
}

/****************************************************************************
 Returns TRUE iff is the result has the required surplus and the city
 isn't in disorder and the city is happy if this is required.
*****************************************************************************/
static bool is_valid_result(const struct cma_parameter *const parameter,
			    const struct cma_result *const result)
{
  int i;

  if (result->disorder) {
    return FALSE;
  }
  if (parameter->require_happy && !result->happy) {
    return FALSE;
  }

  for (i = 0; i < NUM_STATS; i++) {
    if (result->surplus[i] < parameter->minimal_surplus[i]) {
      return FALSE;
    }
  }
  return TRUE;
}

/****************************************************************************
 Print the current state of the given city via
 freelog(LOG_NORMAL,...).
*****************************************************************************/
static void print_city(struct city *pcity)
{
  freelog(LOG_NORMAL, "print_city(city='%s'(id=%d))",
	  pcity->name, pcity->id);
  freelog(LOG_NORMAL,
	  "  size=%d, entertainers=%d, scientists=%d, taxmen=%d",
	  pcity->size, pcity->ppl_elvis, pcity->ppl_scientist,
	  pcity->ppl_taxman);
  freelog(LOG_NORMAL, "  workers at:");
  my_city_map_iterate(pcity, x, y) {
    if (pcity->city_map[x][y] == C_TILE_WORKER) {
      freelog(LOG_NORMAL, "    (%2d,%2d)", x, y);
    }
  } my_city_map_iterate_end;

  freelog(LOG_NORMAL, "  food    = %3d (%+3d)",
	  pcity->food_prod, pcity->food_surplus);
  freelog(LOG_NORMAL, "  shield  = %3d (%+3d)",
	  pcity->shield_prod, pcity->shield_surplus);
  freelog(LOG_NORMAL, "  trade   = %3d (%+3d)",
	  pcity->trade_prod + pcity->corruption, pcity->trade_prod);

  freelog(LOG_NORMAL, "  gold    = %3d (%+3d)", pcity->tax_total,
	  city_gold_surplus(pcity));
  freelog(LOG_NORMAL, "  luxury  = %3d", pcity->luxury_total);
  freelog(LOG_NORMAL, "  science = %3d", pcity->science_total);
}

/****************************************************************************
 Print the given result via freelog(LOG_NORMAL,...). The given result
 has to be a result for the given city.
*****************************************************************************/
static void print_result(struct city *pcity,
			 const struct cma_result *const result)
{
  int y, i, worker = count_worker(pcity, result);

  freelog(LOG_NORMAL, "print_result(result=%p)", result);
  freelog(LOG_NORMAL,
	  "print_result:  found_a_valid=%d disorder=%d happy=%d",
	  result->found_a_valid, result->disorder, result->happy);
#if UNUSED
  freelog(LOG_NORMAL, "print_result:  workers at:");
  my_city_map_iterate(pcity, x, y) {
    if (result->worker_positions_used[x][y]) {
      freelog(LOG_NORMAL, "print_result:    (%2d,%2d)", x, y);
    }
  } my_city_map_iterate_end;
#endif

  for (y = 0; y < CITY_MAP_SIZE; y++) {
    char line[CITY_MAP_SIZE + 1];
    int x;

    line[CITY_MAP_SIZE] = 0;

    for (x = 0; x < CITY_MAP_SIZE; x++) {
      if (!is_valid_city_coords(x, y)) {
	line[x] = '-';
      } else if (is_city_center(x, y)) {
	line[x] = 'c';
      } else if (result->worker_positions_used[x][y]) {
	line[x] = 'w';
      } else {
	line[x] = '.';
      }
    }
    freelog(LOG_NORMAL, "print_result: %s", line);
  }

  freelog(LOG_NORMAL,
	  "print_result:  people: W/E/S/T %d/%d/%d/%d",
	  worker, result->entertainers, result->scientists,
	  result->taxmen);

  for (i = 0; i < NUM_STATS; i++) {
    freelog(LOG_NORMAL,
	    "print_result:  %10s production=%d surplus=%d",
	    cma_get_stat_name(i), result->production[i],
	    result->surplus[i]);
  }
}

/****************************************************************************
 Print the given combination via freelog(LOG_NORMAL,...). The given
 combination has to be a result for the given city.
*****************************************************************************/
static void print_combination(struct city *pcity,
			      struct combination *combination)
{
  assert(combination->is_valid);

  freelog(LOG_NORMAL, "combination:  workers at:");
  my_city_map_iterate(pcity, x, y) {
    if (combination->worker_positions[x][y] == C_TILE_WORKER) {
      freelog(LOG_NORMAL, "combination:    (%2d,%2d)", x, y);
    }
  } my_city_map_iterate_end;

  freelog(LOG_NORMAL,
	  "combination:  food=%d shield=%d trade=%d",
	  combination->production2[FOOD], combination->production2[SHIELD],
	  combination->production2[TRADE]);
}

/****************************************************************************
 Copy the current production stats and happy status of the given city
 to the result.
*****************************************************************************/
static void copy_stats(struct city *pcity, struct cma_result *result)
{
  result->production[FOOD] = pcity->food_prod;
  result->production[SHIELD] = pcity->shield_prod;
  result->production[TRADE] = pcity->trade_prod + pcity->corruption;

  result->surplus[FOOD] = pcity->food_surplus;
  result->surplus[SHIELD] = pcity->shield_surplus;
  result->surplus[TRADE] = pcity->trade_prod;

  result->production[GOLD] = pcity->tax_total;
  result->production[LUXURY] = pcity->luxury_total;
  result->production[SCIENCE] = pcity->science_total;

  result->surplus[GOLD] = city_gold_surplus(pcity);
  result->surplus[LUXURY] = result->production[LUXURY];
  result->surplus[SCIENCE] = result->production[SCIENCE];

  result->disorder = city_unhappy(pcity);
  result->happy = city_happy(pcity);
}

/****************************************************************************
 Copy the current city state (citizen assignment, production stats and
 happy state) in the given result.
*****************************************************************************/
static void get_current_as_result(struct city *pcity,
				  struct cma_result *result)
{
  int worker = 0;

  memset(result->worker_positions_used, 0,
	 sizeof(result->worker_positions_used));

  my_city_map_iterate(pcity, x, y) {
    result->worker_positions_used[x][y] =
	(pcity->city_map[x][y] == C_TILE_WORKER);
    if (result->worker_positions_used[x][y]) {
      worker++;
    }
  } my_city_map_iterate_end;

  result->entertainers = pcity->ppl_elvis;
  result->scientists = pcity->ppl_scientist;
  result->taxmen = pcity->ppl_taxman;

  assert(worker + result->entertainers + result->scientists +
	 result->taxmen == pcity->size);

  result->found_a_valid = TRUE;

  copy_stats(pcity, result);
}

/****************************************************************************
 Invalidate cache3 if the given city is the one which is cached by
 cache3. The other caches (cache1, cache2 and tile_stats) doesn't have
 to be invalidated since they are chained on cache3.
*****************************************************************************/
static void clear_caches(struct city *pcity)
{
  freelog(LOG_DEBUG, "clear_caches(city='%s'(%d))", pcity->name,
	  pcity->id);

  if (cache3.pcity == pcity) {
    int i, j;
    for (i = 0; i < MAX_FIELDS_USED + 1; i++) {
      for (j = 0; j < MAX_COMBINATIONS; j++) {
	if (!cache3.results[i].combinations[j].is_valid) {
	  continue;
	}
	if (cache3.results[i].combinations[j].cache1) {
	  free(cache3.results[i].combinations[j].cache1);
	  cache3.results[i].combinations[j].cache1 = NULL;
	}
      }
    }
    cache3.pcity = NULL;
  }
}

/****************************************************************************
  Returns TRUE if the city is valid for CMA. Fills parameter if TRUE
  is returned. Parameter can be NULL.
*****************************************************************************/
static bool check_city(int city_id, struct cma_parameter *parameter)
{
  struct city *pcity = find_city_by_id(city_id);
  struct cma_parameter dummy;

  if (!parameter) {
    parameter = &dummy;
  }

  if (!pcity
      || !cma_get_parameter(ATTR_CITY_CMA_PARAMETER, city_id, parameter)) {
    return FALSE;
  }

  if (city_owner(pcity) != game.player_ptr) {
    cma_release_city(pcity);
    create_event(pcity->x, pcity->y, E_CITY_CMA_RELEASE,
		 _("CMA: You lost control of %s. Detaching from city."),
		 pcity->name);
    return FALSE;
  }

  return TRUE;
}  

/****************************************************************************
 Change the actual city setting to the given result. Returns TRUE iff
 the actual data matches the calculated one.
*****************************************************************************/
static bool apply_result_on_server(struct city *pcity,
				   const struct cma_result *const result)
{
  struct packet_city_request packet;
  int first_request_id = 0, last_request_id = 0, i, worker;
  struct cma_result current_state;
  bool success;

  get_current_as_result(pcity, &current_state);

  if (results_are_equal(pcity, result, &current_state)
      && !ALWAYS_APPLY_AT_SERVER) {
    stats.apply_result_ignored++;
    return TRUE;
  }

  stats.apply_result_applied++;

  freelog(APPLY_RESULT_LOG_LEVEL, "apply_result(city='%s'(%d))",
	  pcity->name, pcity->id);

  connection_do_buffer(&aconnection);

  packet.city_id = pcity->id;

  /* Do checks */
  worker = count_worker(pcity, result);
  if (pcity->size !=
      (worker + result->entertainers + result->scientists +
       result->taxmen)) {
    print_city(pcity);
    print_result(pcity, result);
    assert(0);
  }

  /* Remove all surplus workers */
  my_city_map_iterate(pcity, x, y) {
    if ((pcity->city_map[x][y] == C_TILE_WORKER) &&
	!result->worker_positions_used[x][y]) {
      last_request_id = set_worker(pcity, x, y, FALSE);
      if (first_request_id == 0) {
	first_request_id = last_request_id;
      }
    }
  } my_city_map_iterate_end;

  /* Change surplus scientists to entertainers */
  for (i = 0; i < pcity->ppl_scientist - result->scientists; i++) {
    packet.specialist_from = SP_SCIENTIST;
    packet.specialist_to = SP_ELVIS;
    last_request_id = send_packet_city_request(&aconnection, &packet,
					       PACKET_CITY_CHANGE_SPECIALIST);
    if (first_request_id == 0) {
      first_request_id = last_request_id;
    }
  }

  /* Change surplus taxmen to entertainers */
  for (i = 0; i < pcity->ppl_taxman - result->taxmen; i++) {
    packet.specialist_from = SP_TAXMAN;
    packet.specialist_to = SP_ELVIS;
    last_request_id = send_packet_city_request(&aconnection, &packet,
					       PACKET_CITY_CHANGE_SPECIALIST);
    if (first_request_id == 0) {
      first_request_id = last_request_id;
    }
  }

  /* now all surplus people are enterainers */

  /* Set workers */
  my_city_map_iterate(pcity, x, y) {
    if (result->worker_positions_used[x][y] &&
	pcity->city_map[x][y] != C_TILE_WORKER) {
      last_request_id = set_worker(pcity, x, y, TRUE);
      if (first_request_id == 0) {
	first_request_id = last_request_id;
      }
    }
  } my_city_map_iterate_end;

  /* Set scientists. */
  for (i = 0; i < result->scientists - pcity->ppl_scientist; i++) {
    packet.specialist_from = SP_ELVIS;
    packet.specialist_to = SP_SCIENTIST;
    last_request_id = send_packet_city_request(&aconnection, &packet,
					       PACKET_CITY_CHANGE_SPECIALIST);
    if (first_request_id == 0) {
      first_request_id = last_request_id;
    }
  }

  /* Set taxmen. */
  for (i = 0; i < result->taxmen - pcity->ppl_taxman; i++) {
    packet.specialist_from = SP_ELVIS;
    packet.specialist_to = SP_TAXMAN;
    last_request_id = send_packet_city_request(&aconnection, &packet,
					       PACKET_CITY_CHANGE_SPECIALIST);
    if (first_request_id == 0) {
      first_request_id = last_request_id;
    }
  }

  if (last_request_id == 0 || ALWAYS_APPLY_AT_SERVER) {
      /*
       * If last_request is 0 no change request was send. But it also
       * means that the results are different or the results_are_equal
       * test at the start of the function would be true. So this
       * means that the client has other results for the same
       * allocation of citizen than the server. We just send a
       * PACKET_CITY_REFRESH to bring them in sync.
       */
    struct packet_generic_integer packet;

    packet.value = pcity->id;
    first_request_id = last_request_id =
	send_packet_generic_integer(&aconnection, PACKET_CITY_REFRESH,
				    &packet);
    stats.refresh_forced++;
  }
  reports_freeze_till(last_request_id);

  connection_do_unbuffer(&aconnection);

  if (last_request_id != 0) {
    int city_id = pcity->id;

    wait_for_requests("CMA", first_request_id, last_request_id);
    if (!check_city(city_id, NULL)) {
      return FALSE;
    }
  }

  /* Return. */
  get_current_as_result(pcity, &current_state);

  freelog(APPLY_RESULT_LOG_LEVEL, "apply_result: return");

  success = results_are_equal(pcity, result, &current_state);
  if (!success) {
    clear_caches(pcity);

    if (SHOW_APPLY_RESULT_ON_SERVER_ERRORS) {
      freelog(LOG_NORMAL, "expected");
      print_result(pcity, result);
      freelog(LOG_NORMAL, "got");
      print_result(pcity, &current_state);
    }
  }
  return success;
}

/****************************************************************************
 Wraps the array access to cache2.secondary_stats.
*****************************************************************************/
static struct secondary_stat *get_secondary_stat(int trade, int specialists,
						 enum specialist_type
						 specialist_type)
{
  freelog(LOG_DEBUG, "second: trade=%d spec=%d type=%d", trade, specialists,
	  specialist_type);

  assert(trade <= cache2.allocated_trade);
  assert(specialists <= cache2.allocated_size);

  return &cache2.secondary_stats[NUM_SPECIALISTS_ROLES *
				 (cache2.allocated_size * (trade) +
				  specialists) + specialist_type];
}

/****************************************************************************
 Wraps the array access to cache2.city_status.
*****************************************************************************/
static struct city_status *get_city_status(int luxury, int workers)
{
  freelog(LOG_DEBUG, "status: lux=%d worker=%d", luxury, workers);

  assert(luxury <= cache2.allocated_luxury);
  assert(workers <= cache2.allocated_size);

  return &cache2.city_status[cache2.allocated_size * luxury + workers];
}

/****************************************************************************
 Update the cache2 according to the filled out result. If the info is
 already in the cache check that the two match.
*****************************************************************************/
static void update_cache2(struct city *pcity,
			  const struct cma_result *const result)
{
  struct secondary_stat *p;
  struct city_status *q;

  /*
   * Science is set to 0 if the city is unhappy/in disorder. See
   * unhappy_city_check.
   */
  if (!result->disorder) {
    p = get_secondary_stat(result->production[TRADE], result->scientists,
			   SP_SCIENTIST);
    if (!p->is_valid) {
      p->production = result->production[SCIENCE];
      p->surplus = result->surplus[SCIENCE];
      p->is_valid = TRUE;
    } else {
      assert(p->production == result->production[SCIENCE] &&
	     p->surplus == result->surplus[SCIENCE]);
    }
  }

  /*
   * Gold is set to 0 if the city is unhappy/in disorder. See
   * unhappy_city_check.
   */
  if (!result->disorder) {
    p = get_secondary_stat(result->production[TRADE], result->taxmen,
			   SP_TAXMAN);
    if (!p->is_valid && !result->disorder) {
      p->production = result->production[GOLD];
      p->surplus = result->surplus[GOLD];
      p->is_valid = TRUE;
    } else {
      assert(p->production == result->production[GOLD] &&
	     p->surplus == result->surplus[GOLD]);
    }
  }

  p = get_secondary_stat(result->production[TRADE], result->entertainers,
			 SP_ELVIS);
  if (!p->is_valid) {
    p->production = result->production[LUXURY];
    p->surplus = result->surplus[LUXURY];
    p->is_valid = TRUE;
  } else {
    if (!result->disorder) {
      assert(p->production == result->production[LUXURY] &&
	     p->surplus == result->surplus[LUXURY]);
    }
  }

  q = get_city_status(result->production[LUXURY],
		      count_worker(pcity, result));
  if (!q->is_valid) {
    q->disorder = result->disorder;
    q->happy = result->happy;
    q->is_valid = TRUE;
  } else {
    assert(q->disorder == result->disorder && q->happy == result->happy);
  }
}

/****************************************************************************
 Uses worker_positions_used, entertainers, scientists and taxmen to
 get the remaining stats.
*****************************************************************************/
static void real_fill_out_result(struct city *pcity,
				 struct cma_result *result)
{
  int worker = count_worker(pcity, result);
  struct city backup;

  freelog(LOG_DEBUG, "real_fill_out_result(city='%s'(%d))", pcity->name,
	  pcity->id);

  /* Do checks */
  if (pcity->size !=
      (worker + result->entertainers + result->scientists +
       result->taxmen)) {
    print_city(pcity);
    print_result(pcity, result);
    assert(0);
  }

  /* Backup */
  memcpy(&backup, pcity, sizeof(struct city));

  /* Set new state */
  my_city_map_iterate(pcity, x, y) {
    if (pcity->city_map[x][y] == C_TILE_WORKER) {
      pcity->city_map[x][y] = C_TILE_EMPTY;
    }
  } my_city_map_iterate_end;

  my_city_map_iterate(pcity, x, y) {
    if (result->worker_positions_used[x][y]) {
      pcity->city_map[x][y] = C_TILE_WORKER;
    }
  } my_city_map_iterate_end;

  pcity->ppl_elvis = result->entertainers;
  pcity->ppl_scientist = result->scientists;
  pcity->ppl_taxman = result->taxmen;

  /* Do a local recalculation of the city */
  generic_city_refresh(pcity, FALSE);

  copy_stats(pcity, result);

  /* Restore */
  memcpy(pcity, &backup, sizeof(struct city));

  freelog(LOG_DEBUG, "xyz: w=%d e=%d s=%d t=%d trade=%d "
	  "sci=%d lux=%d tax=%d dis=%s happy=%s",
	  count_worker(pcity, result), result->entertainers,
	  result->scientists, result->taxmen,
	  result->production[TRADE],
	  result->production[SCIENCE],
	  result->production[LUXURY],
	  result->production[GOLD],
	  result->disorder ? "yes" : "no", result->happy ? "yes" : "no");
  update_cache2(pcity, result);
}

/****************************************************************************
 Estimates the fitness of the given result with respect to the given
 parameters. Will fill out major fitnes and minor fitness.

 The minor fitness should be used if the major fitness are equal.
*****************************************************************************/
static void calc_fitness(struct city *pcity,
			 const struct cma_parameter *const parameter,
			 const struct cma_result *const result,
			 int *major_fitness, int *minor_fitness)
{
  int i;

  *major_fitness = 0;
  *minor_fitness = 0;

  for (i = 0; i < NUM_STATS; i++) {
    int base;
    if (parameter->factor_target == FT_SURPLUS) {
      base = result->surplus[i];
    } else if (parameter->factor_target == FT_EXTRA) {
      base = parameter->minimal_surplus[i] - result->surplus[i];
    } else {
      base = 0;
      assert(0);
    }

    *major_fitness += base * parameter->factor[i];
    *minor_fitness += result->surplus[i];
  }

  if (result->happy) {
    *major_fitness += parameter->happy_factor;
  }

  freelog(CALC_FITNESS_LOG_LEVEL2, "calc_fitness()");
  freelog(CALC_FITNESS_LOG_LEVEL,
	  "calc_fitness:   surplus={food=%d, shields=%d, trade=%d",
	  result->surplus[FOOD], result->surplus[SHIELD],
	  result->surplus[TRADE]);
  freelog(CALC_FITNESS_LOG_LEVEL,
	  "calc_fitness:     tax=%d, luxury=%d, science=%d}",
	  result->surplus[GOLD], result->surplus[LUXURY],
	  result->surplus[SCIENCE]);
  freelog(CALC_FITNESS_LOG_LEVEL2,
	  "calc_fitness:   factor={food=%d, shields=%d, trade=%d",
	  parameter->factor[FOOD], parameter->factor[SHIELD],
	  parameter->factor[TRADE]);
  freelog(CALC_FITNESS_LOG_LEVEL2,
	  "calc_fitness:     tax=%d, luxury=%d, science=%d}",
	  parameter->factor[GOLD], parameter->factor[LUXURY],
	  parameter->factor[SCIENCE]);
  freelog(CALC_FITNESS_LOG_LEVEL,
	  "calc_fitness: fitness = %d, minor_fitness=%d", *major_fitness,
	  *minor_fitness);
}

/****************************************************************************
 Prints the data of the stats struct via freelog(LOG_NORMAL,...).
*****************************************************************************/
static void report_stats(void)
{
#if SHOW_TIME_STATS
  int total, per_mill;

  freelog(LOG_NORMAL, "CMA: overall=%fs queries=%d %fms / query",
	  read_timer_seconds(stats.wall_timer), stats.queries,
	  (1000.0 * read_timer_seconds(stats.wall_timer)) /
	  ((double) stats.queries));
  total = stats.apply_result_ignored + stats.apply_result_applied;
  per_mill = (stats.apply_result_ignored * 1000) / (total ? total : 1);

  freelog(LOG_NORMAL,
	  "CMA: apply_result: ignored=%2d.%d%% (%d) "
	  "applied=%2d.%d%% (%d) total=%d",
	  per_mill / 10, per_mill % 10, stats.apply_result_ignored,
	  (1000 - per_mill) / 10, (1000 - per_mill) % 10,
	  stats.apply_result_applied, total);
#endif

#if SHOW_CACHE_STATS
  total = cache1.hits + cache1.misses;
  if (total) {
    per_mill = (cache1.hits * 1000) / total;
  } else {
    per_mill = 0;
  }
  freelog(LOG_NORMAL,
	  "CMA: CACHE1: hits=%2d.%d%% misses=%2d.%d%% total=%d",
	  per_mill / 10, per_mill % 10, (1000 - per_mill) / 10,
	  (1000 - per_mill) % 10, total);

  total = cache2.hits + cache2.misses;
  if (total) {
    per_mill = (cache2.hits * 1000) / total;
  } else {
    per_mill = 0;
  }
  freelog(LOG_NORMAL,
	  "CMA: CACHE2: hits=%2d.%d%% misses=%2d.%d%% total=%d",
	  per_mill / 10, per_mill % 10, (1000 - per_mill) / 10,
	  (1000 - per_mill) % 10, total);

  total = cache3.hits + cache3.misses;
  if (total) {
    per_mill = (cache3.hits * 1000) / total;
  } else {
    per_mill = 0;
  }
  freelog(LOG_NORMAL,
	  "CMA: CACHE3: hits=%2d.%d%% misses=%2d.%d%% total=%d",
	  per_mill / 10, per_mill % 10, (1000 - per_mill) / 10,
	  (1000 - per_mill) % 10, total);
#endif
}

/****************************************************************************
...
*****************************************************************************/
static void release_city(int city_id)
{
  attr_city_set(ATTR_CITY_CMA_PARAMETER, city_id, 0, NULL);
}

/****************************************************************************
                           algorithmic functions
*****************************************************************************/

/****************************************************************************
 Frontend cache for real_fill_out_result. This method tries to avoid
 calling real_fill_out_result by all means.
*****************************************************************************/
static void fill_out_result(struct city *pcity, struct cma_result *result,
			    struct combination *base_combination,
			    int scientists, int taxmen)
{
  struct cma_result *slot;
  bool got_all;

  assert(base_combination->is_valid);

  /*
   * First try to get a filled out result from cache1 or from the
   * all_entertainer result.
   */
  if (scientists == 0 && taxmen == 0) {
    slot = &base_combination->all_entertainer;
  } else {
    assert(scientists <= base_combination->max_scientists);
    assert(taxmen <= base_combination->max_taxmen);
    assert(base_combination->cache1 != NULL);
    assert(base_combination->all_entertainer.found_a_valid);

    slot = &base_combination->cache1[scientists *
				     (base_combination->max_taxmen + 1) +
				     taxmen];
  }

  freelog(LOG_DEBUG,
	  "fill_out_result(base_comb=%p (w=%d), scientists=%d, taxmen=%d) %s",
	  base_combination, base_combination->worker, scientists,
	  taxmen, slot->found_a_valid ? "CACHED" : "unknown");

  if (slot->found_a_valid) {
    /* Cache1 contains the result */
    cache1.hits++;
    memcpy(result, slot, sizeof(struct cma_result));
    return;
  }
  cache1.misses++;

  my_city_map_iterate(pcity, x, y) {
    result->worker_positions_used[x][y] =
	(base_combination->worker_positions[x][y] == C_TILE_WORKER);
  } my_city_map_iterate_end;

  result->scientists = scientists;
  result->taxmen = taxmen;
  result->entertainers =
      pcity->size - (base_combination->worker + scientists + taxmen);

  freelog(LOG_DEBUG,
	  "fill_out_result(city='%s'(%d), entrt.s=%d, scien.s=%d, taxmen=%d)",
	  pcity->name, pcity->id, result->entertainers,
	  result->scientists, result->taxmen);

  /* try to fill result from cache2 */
  if (!base_combination->all_entertainer.found_a_valid) {
    got_all = FALSE;
  } else {
    struct secondary_stat *p;
    struct city_status *q;
    int i;

    got_all = TRUE;

    /*
     * fill out the primary stats that are known from the
     * all_entertainer result
     */
    for (i = 0; i < NUM_PRIMARY_STATS; i++) {
      result->production[i] =
	  base_combination->all_entertainer.production[i];
      result->surplus[i] = base_combination->all_entertainer.surplus[i];
    }

    p = get_secondary_stat(result->production[TRADE], result->scientists,
			   SP_SCIENTIST);
    if (!p->is_valid) {
      got_all = FALSE;
    } else {
      result->production[SCIENCE] = p->production;
      result->surplus[SCIENCE] = p->surplus;
    }

    p = get_secondary_stat(result->production[TRADE], result->taxmen,
			   SP_TAXMAN);
    if (!p->is_valid) {
      got_all = FALSE;
    } else {
      result->production[GOLD] = p->production;
      result->surplus[GOLD] = p->surplus;
    }

    p = get_secondary_stat(result->production[TRADE], result->entertainers,
			   SP_ELVIS);
    if (!p->is_valid) {
      got_all = FALSE;
    } else {
      result->production[LUXURY] = p->production;
      result->surplus[LUXURY] = p->surplus;
    }

    q = get_city_status(result->production[LUXURY],
			base_combination->worker);
    if (!q->is_valid) {
      got_all = FALSE;
    } else {
      result->disorder = q->disorder;
      result->happy = q->happy;
    }
  }

  if (got_all) {
    /*
     * All secondary stats and the city status have been filled from
     * cache2.
     */

    cache2.hits++;
    memcpy(slot, result, sizeof(struct cma_result));
    slot->found_a_valid = TRUE;
    return;
  }

  cache2.misses++;

  /*
   * Result can't be constructed from caches. Do the slow
   * re-calculation.
   */
  real_fill_out_result(pcity, result);

  /* Update cache1 */
  memcpy(slot, result, sizeof(struct cma_result));
  slot->found_a_valid = TRUE;
}


/****************************************************************************
 The given combination is added only if no other better combination is
 already known. add_combination will also remove any combination which
 may have become worse than the inserted.
*****************************************************************************/
static void add_combination(int fields_used,
			    struct combination *combination)
{
  static int max_used = 0;
  int i, used;
  /* This one is cached for later. Avoids another loop. */
  struct combination *invalid_slot_for_insert = NULL;

  /* Try to find a better combination. */
  for (i = 0; i < MAX_COMBINATIONS; i++) {
    struct combination *current =
	&cache3.results[fields_used].combinations[i];

    if (!current->is_valid) {
      if (!invalid_slot_for_insert) {
	invalid_slot_for_insert = current;
      }
      continue;
    }

    if (current->production2[FOOD] >= combination->production2[FOOD] &&
	current->production2[SHIELD] >= combination->production2[SHIELD] &&
	current->production2[TRADE] >= combination->production2[TRADE]) {
      /*
         freelog(LOG_NORMAL, "found a better combination:");
         print_combination(current);
       */
      return;
    }
  }

  /*
   * There is no better combination. Remove any combinations which are
   * worse than the given.
   */

  /*
     freelog(LOG_NORMAL, "add_combination()");
     print_combination(combination);
   */

  for (i = 0; i < MAX_COMBINATIONS; i++) {
    struct combination *current =
	&cache3.results[fields_used].combinations[i];

    if (!current->is_valid) {
      continue;
    }

    if (current->production2[FOOD] <= combination->production2[FOOD] &&
	current->production2[SHIELD] <= combination->production2[SHIELD] &&
	current->production2[TRADE] <= combination->production2[TRADE]) {
      /*
         freelog(LOG_NORMAL, "the following is now obsolete:");
         print_combination(current);
       */
      current->is_valid = FALSE;
    }
  }

  /* Insert the given combination. */
  if (invalid_slot_for_insert == NULL) {
    freelog(LOG_FATAL,
	    "No more free combinations left. You may increase "
	    "MAX_COMBINATIONS or \nreport this error to "
	    "freeciv-dev@freeciv.org.\nCurrent MAX_COMBINATIONS=%d",
	    MAX_COMBINATIONS);
    exit(EXIT_FAILURE);
  }

  memcpy(invalid_slot_for_insert, combination, sizeof(struct combination));
  invalid_slot_for_insert->all_entertainer.found_a_valid = FALSE;
  invalid_slot_for_insert->cache1 = NULL;

  used = count_valid_combinations(fields_used);
  if (used > (MAX_COMBINATIONS * 9) / 10
      && (used > max_used || max_used == 0)) {
    max_used = used;
    freelog(LOG_ERROR,
	    "Warning: there are currently %d out of %d combinations used",
	    used, MAX_COMBINATIONS);
  }

  freelog(LOG_DEBUG, "there are now %d combination which use %d tiles",
	  count_valid_combinations(fields_used), fields_used);
}

/****************************************************************************
 Will create combinations which use (fields_to_use) fields from the
 combinations which use (fields_to_use-1) fields.
*****************************************************************************/
static void expand_cache3(struct city *pcity, int fields_to_use,
			  const struct tile_stats *const stats)
{
  int i;

  freelog(EXPAND_CACHE3_LOG_LEVEL,
	  "expand_cache3(fields_to_use=%d) results[%d] "
	  "has %d valid combinations",
	  fields_to_use, fields_to_use - 1,
	  count_valid_combinations(fields_to_use - 1));

  for (i = 0; i < MAX_COMBINATIONS; i++) {
    cache3.results[fields_to_use].combinations[i].is_valid = FALSE;
  }

  for (i = 0; i < MAX_COMBINATIONS; i++) {
    struct combination *current =
	&cache3.results[fields_to_use - 1].combinations[i];

    if (!current->is_valid) {
      continue;
    }

    my_city_map_iterate(pcity, x, y) {
      struct combination new_pc;

      if (current->worker_positions[x][y] != C_TILE_EMPTY) {
	continue;
      }

      memcpy(&new_pc, current, sizeof(struct combination));
      assert(stats->tiles[x][y].is_valid);
      new_pc.production2[FOOD] += stats->tiles[x][y].stats[FOOD];
      new_pc.production2[SHIELD] += stats->tiles[x][y].stats[SHIELD];
      new_pc.production2[TRADE] += stats->tiles[x][y].stats[TRADE];

      new_pc.worker_positions[x][y] = C_TILE_WORKER;
      new_pc.worker = fields_to_use;
      add_combination(fields_to_use, &new_pc);
    } my_city_map_iterate_end;
  }

  freelog(EXPAND_CACHE3_LOG_LEVEL,
	  "expand_cache3(fields_to_use=%d): %d valid combinations",
	  fields_to_use, count_valid_combinations(fields_to_use));

  if (SHOW_EXPAND_CACHE3_RESULT) {
    for (i = 0; i < MAX_COMBINATIONS; i++) {
      struct combination *current =
	  &cache3.results[fields_to_use].combinations[i];

      if (!current->is_valid) {
	continue;
      }

      print_combination(pcity, current);
    }
  }
}

/****************************************************************************
 Expand the secondary_stats and city_status fields of cache2 if this
 is necessary. For this the function tries to estimate the upper limit
 of trade and luxury. It will also invalidate cache2.
*****************************************************************************/
static void ensure_invalid_cache2(struct city *pcity, int total_tile_trade)
{
  bool change_size = FALSE;
  int i, luxury, total_trade = total_tile_trade;

  for (i = 0; i < NUM_TRADEROUTES; i++) {
    struct city *pc2 = find_city_by_id(pcity->trade[i]);

    if (pc2) {
      int bonus = (total_tile_trade + pc2->tile_trade + 4) / 8;

      /* Double if on different continents. */
      if (map_get_continent(pcity->x, pcity->y) !=
	  map_get_continent(pc2->x, pc2->y)) {bonus *= 2;
      }

      if (pcity->owner == pc2->owner) {
	bonus /= 2;
      }
      total_trade += bonus;
    }
  }

  /*
   * Estimate an upper limit for the luxury. We assume that the player
   * has set the luxury rate to 100%. There are two extremal cases: all
   * citizen are entertainers (yielding a luxury of "(pcity->size * 2
   * * get_city_tax_bonus(pcity))/100" = A) or all citizen are
   * working on tiles and the resulting trade is converted to luxury
   * (yielding a luxury of "(total_trade * get_city_tax_bonus(pcity))
   * / 100" = B) . We can't use MAX(A, B) since there may be cases in
   * between them which are better than these two exremal cases. So we
   * use A+B as upper limit.
   */
  luxury =
      ((pcity->size * 2 + total_trade) * get_city_tax_bonus(pcity)) / 100;

  /* +1 because we want to index from 0 to pcity->size inclusive */
  if (pcity->size + 1 > cache2.allocated_size) {
    cache2.allocated_size = pcity->size + 1;
    change_size = TRUE;
  }

  if (total_trade + 1 > cache2.allocated_trade) {
    cache2.allocated_trade = total_trade + 1;
    change_size = TRUE;
  }

  if (luxury + 1 > cache2.allocated_luxury) {
    cache2.allocated_luxury = luxury + 1;
    change_size = TRUE;
  }

  if (change_size) {
    freelog(LOG_DEBUG,
	    "CMA: expanding cache2 to size=%d, trade=%d, luxury=%d",
	    cache2.allocated_size, cache2.allocated_trade,
	    cache2.allocated_luxury);
    if (cache2.secondary_stats) {
      free(cache2.secondary_stats);
      cache2.secondary_stats = NULL;
    }
    cache2.secondary_stats =
	fc_malloc(cache2.allocated_trade * cache2.allocated_size *
		  NUM_SPECIALISTS_ROLES * sizeof(struct secondary_stat));

    if (cache2.city_status) {
      free(cache2.city_status);
      cache2.city_status = NULL;
    }
    cache2.city_status =
	fc_malloc(cache2.allocated_luxury * cache2.allocated_size *
		  sizeof(struct city_status));
  }

  /* Make cache2 invalid */
  memset(cache2.secondary_stats, 0,
	 cache2.allocated_trade * cache2.allocated_size *
	 NUM_SPECIALISTS_ROLES * sizeof(struct secondary_stat));
  memset(cache2.city_status, 0,
	 cache2.allocated_luxury * cache2.allocated_size *
	 sizeof(struct city_status));
}
/****************************************************************************
 Setup. Adds the root combination (the combination which doesn't use
 any worker but the production of the city center). Incrementaly calls
 expand_cache3.
*****************************************************************************/
static void build_cache3(struct city *pcity)
{
  struct combination root_combination;
  int i, j, total_tile_trade;
  struct tile_stats tile_stats;
  bool is_celebrating = base_city_celebrating(pcity);

  if (cache3.pcity != pcity) {
    cache3.pcity = NULL;
  } else {
    cache3.hits++;
    return;
  }

  cache3.pcity = pcity;
  cache3.misses++;

  /* Make cache3 invalid */
  for (i = 0; i < MAX_FIELDS_USED + 1; i++) {
    for (j = 0; j < MAX_COMBINATIONS; j++) {
      cache3.results[i].combinations[j].is_valid = FALSE;
    }
  }

  /*
   * Construct root combination. Update
   * cache3.fields_available_total. Fill tile_stats.
   */
  root_combination.worker = 0;
  root_combination.production2[FOOD] =
      base_city_get_food_tile(2, 2, pcity, is_celebrating);
  root_combination.production2[SHIELD] =
      base_city_get_shields_tile(2, 2, pcity, is_celebrating);
  root_combination.production2[TRADE] =
      base_city_get_trade_tile(2, 2, pcity, is_celebrating);

  total_tile_trade = root_combination.production2[TRADE];

  cache3.fields_available_total = 0;

  memset(&tile_stats, 0, sizeof(tile_stats));

  my_city_map_iterate(pcity, x, y) {
    tile_stats.tiles[x][y].is_valid = TRUE;
    tile_stats.tiles[x][y].stats[FOOD] =
	base_city_get_food_tile(x, y, pcity, is_celebrating);
    tile_stats.tiles[x][y].stats[SHIELD] =
	base_city_get_shields_tile(x, y, pcity, is_celebrating);
    tile_stats.tiles[x][y].stats[TRADE] =
	base_city_get_trade_tile(x, y, pcity, is_celebrating);

    if (can_field_be_used_for_worker(pcity, x, y)) {
      cache3.fields_available_total++;
      root_combination.worker_positions[x][y] = C_TILE_EMPTY;
      total_tile_trade += tile_stats.tiles[x][y].stats[TRADE];
    } else {
      root_combination.worker_positions[x][y] = C_TILE_UNAVAILABLE;
    }
  } my_city_map_iterate_end;

  /* Add root combination. */
  root_combination.is_valid = TRUE;
  add_combination(0, &root_combination);

  for (i = 1; i <= MIN(cache3.fields_available_total, pcity->size); i++) {
    expand_cache3(pcity, i, &tile_stats);
  }

  ensure_invalid_cache2(pcity, total_tile_trade);
}

/****************************************************************************
 Creates all realisations of the given combination. Finds the best one.
*****************************************************************************/
static void find_best_specialist_arrangement(struct city *pcity, const struct cma_parameter
					     *const parameter, struct combination
					     *base_combination, struct cma_result
					     *best_result,
					     int *best_major_fitness,
					     int *best_minor_fitness)
{
  int worker = base_combination->worker;
  int specialists = pcity->size - worker;
  int scientists, taxmen;

  if (!base_combination->cache1) {

    /* setup cache1 */

    int i, items;

    if (can_use_specialist(pcity, SP_SCIENTIST)) {
      base_combination->max_scientists = specialists;
    } else {
      base_combination->max_scientists = 0;
    }

    if (can_use_specialist(pcity, SP_TAXMAN)) {
      base_combination->max_taxmen = specialists;
    } else {
      base_combination->max_taxmen = 0;
    }
    items = (base_combination->max_scientists + 1) *
	(base_combination->max_taxmen + 1);
    base_combination->cache1 =
	fc_malloc(sizeof(struct cma_result) * items);
    for (i = 0; i < items; i++) {
      base_combination->cache1[i].found_a_valid = FALSE;
    }
  }

  best_result->found_a_valid = FALSE;

  for (scientists = 0;
       scientists <= base_combination->max_scientists; scientists++) {
    for (taxmen = 0;
	 taxmen <= base_combination->max_scientists - scientists; taxmen++) {
      int major_fitness, minor_fitness;
      struct cma_result result;

      freelog(FIND_BEST_SPECIALIST_ARRANGEMENT_LOG_LEVEL,
	      "  optimize_people: using (W/E/S/T) %d/%d/%d/%d",
	      worker, pcity->size - (worker + scientists + taxmen),
	      scientists, taxmen);

      fill_out_result(pcity, &result, base_combination, scientists,
		      taxmen);

      freelog(FIND_BEST_SPECIALIST_ARRANGEMENT_LOG_LEVEL,
	      "  optimize_people: got extra=(tax=%d, luxury=%d, "
	      "science=%d)",
	      result.surplus[GOLD] - parameter->minimal_surplus[GOLD],
	      result.surplus[LUXURY] -
	      parameter->minimal_surplus[LUXURY],
	      result.surplus[SCIENCE] -
	      parameter->minimal_surplus[SCIENCE]);

      if (!is_valid_result(parameter, &result)) {
	freelog(FIND_BEST_SPECIALIST_ARRANGEMENT_LOG_LEVEL,
		"  optimize_people: doesn't have enough surplus or disorder");
	continue;
      }

      calc_fitness(pcity, parameter, &result, &major_fitness,
		   &minor_fitness);

      freelog(FIND_BEST_SPECIALIST_ARRANGEMENT_LOG_LEVEL,
	      "  optimize_people: fitness=(%d,%d)", major_fitness,
	      minor_fitness);

      result.found_a_valid = TRUE;
      if (!best_result->found_a_valid
	  || ((major_fitness > *best_major_fitness)
	      || (major_fitness == *best_major_fitness
		  && minor_fitness > *best_minor_fitness))) {
	memcpy(best_result, &result, sizeof(struct cma_result));
	*best_major_fitness = major_fitness;
	*best_minor_fitness = minor_fitness;
      }
    }				/* for taxmen */
  }				/* for scientists */
}

/****************************************************************************
 The top level optimization method. It finds the realisation with
 the best fitness.
*****************************************************************************/
static void optimize_final(struct city *pcity,
			   const struct cma_parameter *const parameter,
			   struct cma_result *best_result)
{
  int fields_used, i;
  int results_used = 0, not_enough_primary = 0, not_enough_secondary = 0;
  /* Just for the compiler. Guarded by best_result->found_a_valid */
  int best_major_fitness = 0, best_minor_fitness = 0;

  build_cache3(pcity);

  best_result->found_a_valid = FALSE;

  /* Loop over all combinations */
  for (fields_used = 0;
       fields_used <= MIN(cache3.fields_available_total, pcity->size);
       fields_used++) {
    freelog(OPTIMIZE_FINAL_LOG_LEVEL,
	    "there are %d combinations which use %d fields",
	    count_valid_combinations(fields_used), fields_used);
    for (i = 0; i < MAX_COMBINATIONS; i++) {
      struct combination *current =
	  &cache3.results[fields_used].combinations[i];
      int stat, major_fitness, minor_fitness;
      struct cma_result result;

      if (!current->is_valid) {
	continue;
      }

      freelog(OPTIMIZE_FINAL_LOG_LEVEL2, "  trying combination %d", i);

      /* this will set the all_entertainer result */
      fill_out_result(pcity, &result, current, 0, 0);

      /*
       * Check. The actual production can be bigger because of city
       * improvements such a Factory.
       */
      for (stat = 0; stat < NUM_PRIMARY_STATS; stat++) {
	if (result.production[stat] < current->production2[stat]) {
	  freelog(LOG_NORMAL, "expected:");
	  print_combination(pcity, current);
	  freelog(LOG_NORMAL, "got:");
	  print_result(pcity, &result);
	  assert(0);
	}
      }

      /*
       * the secondary stats aren't calculated yet but we want to use
       * is_valid_result()
       */
      result.surplus[GOLD] = parameter->minimal_surplus[GOLD];
      result.surplus[LUXURY] = parameter->minimal_surplus[LUXURY];
      result.surplus[SCIENCE] = parameter->minimal_surplus[SCIENCE];

      if (!is_valid_result(parameter, &result)) {
	not_enough_primary++;
	freelog(OPTIMIZE_FINAL_LOG_LEVEL2, "    not enough primary");
	continue;
      }

      find_best_specialist_arrangement(pcity, parameter, current, &result,
				       &major_fitness, &minor_fitness);
      if (!result.found_a_valid) {
	freelog(OPTIMIZE_FINAL_LOG_LEVEL2, "    not enough secondary");
	not_enough_secondary++;
	continue;
      }

      freelog(OPTIMIZE_FINAL_LOG_LEVEL2, "    is ok");
      results_used++;

      if (!best_result->found_a_valid
	  || ((major_fitness > best_major_fitness)
	      || (major_fitness == best_major_fitness
		  && minor_fitness > best_minor_fitness))) {
	freelog(OPTIMIZE_FINAL_LOG_LEVEL2, "    is new best result");
	memcpy(best_result, &result, sizeof(struct cma_result));
	best_major_fitness = major_fitness;
	best_minor_fitness = minor_fitness;
      } else {
	freelog(OPTIMIZE_FINAL_LOG_LEVEL2,
		"    isn't better than the best result");
      }
    }
  }

  freelog(OPTIMIZE_FINAL_LOG_LEVEL,
	  "%d combinations don't have the required minimal primary surplus",
	  not_enough_primary);

  freelog(OPTIMIZE_FINAL_LOG_LEVEL,
	  "%d combinations don't have the required minimal secondary surplus",
	  not_enough_secondary);

  freelog(OPTIMIZE_FINAL_LOG_LEVEL, "%d combinations did remain",
	  results_used);
}

/****************************************************************************
 The given city has changed. handle_city ensures that either the city
 follows the set CMA goal or that the CMA detaches itself from the
 city.
*****************************************************************************/
static void handle_city(struct city *pcity)
{
  struct cma_result result;
  bool handled;
  int i, city_id = pcity->id;

  freelog(HANDLE_CITY_LOG_LEVEL,
	  "handle_city(city='%s'(%d) pos=(%d,%d) owner=%s)", pcity->name,
	  pcity->id, pcity->x, pcity->y, city_owner(pcity)->name);

  freelog(HANDLE_CITY_LOG_LEVEL2, "START handle city='%s'(%d)",
	  pcity->name, pcity->id);

  handled = FALSE;
  for (i = 0; i < 5; i++) {
    struct cma_parameter parameter;

    freelog(HANDLE_CITY_LOG_LEVEL2, "  try %d", i);

    if (!check_city(city_id, &parameter)) {
      handled = TRUE;	
      break;
    }

    pcity = find_city_by_id(city_id);

    cma_query_result(pcity, &parameter, &result);
    if (!result.found_a_valid) {
      freelog(HANDLE_CITY_LOG_LEVEL2, "  no valid found result");

      cma_release_city(pcity);

      create_event(pcity->x, pcity->y, E_CITY_CMA_RELEASE,
		   _("CMA: The agent can't fulfill the requirements "
		     "for %s. Passing back control."), pcity->name);
      handled = TRUE;
      break;
    } else {
      if (!apply_result_on_server(pcity, &result)) {
	freelog(HANDLE_CITY_LOG_LEVEL2, "  doesn't cleanly apply");
	if (check_city(city_id, NULL) && i == 0) {
	  create_event(pcity->x, pcity->y, E_NOEVENT,
		       _("CMA: %s has changed and the calculated "
			 "result can't be applied. Will retry."),
		       pcity->name);
	}
      } else {
	freelog(HANDLE_CITY_LOG_LEVEL2, "  ok");
	/* Everything ok */
	handled = TRUE;
	break;
      }
    }
  }

  pcity = find_city_by_id(city_id);

  if (!handled) {
    assert(pcity);
    freelog(HANDLE_CITY_LOG_LEVEL2, "  not handled");

    create_event(pcity->x, pcity->y, E_CITY_CMA_RELEASE,
		 _("CMA: %s has changed multiple times. This may be "
		   "an error in freeciv or bad luck. Please contact "
		   "<freeciv-dev@freeciv.org>. The CMA will detach "
		   "itself from the city now."), pcity->name);

    cma_release_city(pcity);

#if (IS_DEVEL_VERSION || IS_BETA_VERSION)
    freelog(LOG_ERROR, _("CMA: %s has changed multiple times. This may be "
			 "an error in freeciv or bad luck. Please contact "
			 "<freeciv-dev@freeciv.org>. The CMA will detach "
			 "itself from the city now."), pcity->name);
    assert(0);
    exit(EXIT_FAILURE);
#endif
  }

  freelog(HANDLE_CITY_LOG_LEVEL2, "END handle city=(%d)", city_id);
}

/****************************************************************************
 Callback for the agent interface.
*****************************************************************************/
static void city_changed(int city_id)
{
  struct city *pcity = find_city_by_id(city_id);

  if (pcity) {
    clear_caches(pcity);
    handle_city(pcity);
  }
}

/****************************************************************************
 Callback for the agent interface.
*****************************************************************************/
static void city_remove(int city_id)
{
  release_city(city_id);
}

/****************************************************************************
 Callback for the agent interface.
*****************************************************************************/
static void new_turn(void)
{
  report_stats();
}

/*************************** public interface *******************************/
/****************************************************************************
...
*****************************************************************************/
void cma_init(void)
{
  struct agent self;

  freelog(LOG_DEBUG, "sizeof(struct cma_result)=%d",
	  (unsigned int) sizeof(struct cma_result));
  freelog(LOG_DEBUG, "sizeof(struct cma_parameter)=%d",
	  (unsigned int) sizeof(struct cma_parameter));
  freelog(LOG_DEBUG, "sizeof(struct combination)=%d",
	  (unsigned int) sizeof(struct combination));
  freelog(LOG_DEBUG, "sizeof(cache2)=%d", (unsigned int) sizeof(cache2));
  freelog(LOG_DEBUG, "sizeof(cache3)=%d", (unsigned int) sizeof(cache3));

  /* reset cache counters */
  cache1.hits = 0;
  cache1.misses = 0;

  cache2.hits = 0;
  cache2.misses = 0;

  cache3.pcity = NULL;
  cache3.hits = 0;
  cache3.misses = 0;

  memset(&stats, 0, sizeof(stats));
  stats.wall_timer = new_timer(TIMER_USER, TIMER_ACTIVE);

  memset(&self, 0, sizeof(self));
  strcpy(self.name, "CMA");
  self.level = 1;
  self.city_callbacks[CB_CHANGE] = city_changed;
  self.city_callbacks[CB_NEW] = city_changed;
  self.city_callbacks[CB_REMOVE] = city_remove;
  self.turn_start_notify = new_turn;
  register_agent(&self);
}

/****************************************************************************
...
*****************************************************************************/
void cma_query_result(struct city *pcity,
		      const struct cma_parameter *const parameter,
		      struct cma_result *result)
{
  freelog(CMA_QUERY_RESULT_LOG_LEVEL, "cma_query_result(city='%s'(%d))",
	  pcity->name, pcity->id);

  start_timer(stats.wall_timer);
  optimize_final(pcity, parameter, result);
  stop_timer(stats.wall_timer);

  stats.queries++;
  freelog(CMA_QUERY_RESULT_LOG_LEVEL, "cma_query_result: return");
  if (DISABLE_CACHE3) {
    clear_caches(pcity);
  }
}

/****************************************************************************
...
*****************************************************************************/
bool cma_apply_result(struct city *pcity,
		     const struct cma_result *const result)
{
  assert(!cma_is_city_under_agent(pcity, NULL));
  return apply_result_on_server(pcity, result);
}

/****************************************************************************
...
*****************************************************************************/
void cma_put_city_under_agent(struct city *pcity,
			      const struct cma_parameter *const parameter)
{
  freelog(LOG_DEBUG, "cma_put_city_under_agent(city='%s'(%d))",
	  pcity->name, pcity->id);

  assert(city_owner(pcity) == game.player_ptr);

  cma_set_parameter(ATTR_CITY_CMA_PARAMETER, pcity->id, parameter);

  cause_a_city_changed_for_agent("CMA", pcity);

  freelog(LOG_DEBUG, "cma_put_city_under_agent: return");
}

/****************************************************************************
...
*****************************************************************************/
void cma_release_city(struct city *pcity)
{
  release_city(pcity->id);
}

/****************************************************************************
...
*****************************************************************************/
bool cma_is_city_under_agent(struct city *pcity,
			    struct cma_parameter *parameter)
{
  struct cma_parameter my_parameter;

  if (!cma_get_parameter(ATTR_CITY_CMA_PARAMETER, pcity->id, &my_parameter)) {
    return FALSE;
  }

  if (parameter) {
    memcpy(parameter, &my_parameter, sizeof(struct cma_parameter));
  }
  return TRUE;
}

/****************************************************************************
...
*****************************************************************************/
const char *const cma_get_stat_name(enum stat stat)
{
  switch (stat) {
  case FOOD:
    return _("Food");
  case SHIELD:
    return _("Shield");
  case TRADE:
    return _("Trade");
  case GOLD:
    return _("Gold");
  case LUXURY:
    return _("Luxury");
  case SCIENCE:
    return _("Science");
  default:
    assert(0);
    return "ERROR";
  }
}

/**************************************************************************
 Returns true if the two cma_parameters are equal.
**************************************************************************/
bool cma_are_parameter_equal(const struct cma_parameter *const p1,
			    const struct cma_parameter *const p2)
{
  int i;

  for (i = 0; i < NUM_STATS; i++) {
    if (p1->minimal_surplus[i] != p2->minimal_surplus[i]) {
      return FALSE;
    }
    if (p1->factor[i] != p2->factor[i]) {
      return FALSE;
    }
  }
  if (p1->require_happy != p2->require_happy) {
    return FALSE;
  }
  if (p1->factor_target != p2->factor_target) {
    return FALSE;
  }
  if (p1->happy_factor != p2->happy_factor) {
    return FALSE;
  }

  return TRUE;
}

/**************************************************************************
 ...
**************************************************************************/
void cma_copy_parameter(struct cma_parameter *dest,
			const struct cma_parameter *const src)
{
  memcpy(dest, src, sizeof(struct cma_parameter));
}

/**************************************************************************
 ...
**************************************************************************/
bool cma_get_parameter(enum attr_city attr, int city_id,
		       struct cma_parameter *parameter)
{
  size_t len;
  char buffer[SAVED_PARAMETER_SIZE];
  struct data_in din;
  int i, version;

  len = attr_city_get(attr, city_id, sizeof(buffer), buffer);
  if (len == 0) {
    return FALSE;
  }
  assert(len == SAVED_PARAMETER_SIZE);

  dio_input_init(&din, buffer, len);

  dio_get_uint8(&din, &version);
  assert(version == 2);

  for (i = 0; i < NUM_STATS; i++) {
    dio_get_sint16(&din, &parameter->minimal_surplus[i]);
    dio_get_sint16(&din, &parameter->factor[i]);
  }

  dio_get_sint16(&din, &parameter->happy_factor);
  dio_get_uint8(&din, (int *) &parameter->factor_target);
  dio_get_bool8(&din, &parameter->require_happy);

  return TRUE;
}

/**************************************************************************
 ...
**************************************************************************/
void cma_set_parameter(enum attr_city attr, int city_id,
		       const struct cma_parameter *parameter)
{
  char buffer[SAVED_PARAMETER_SIZE];
  struct data_out dout;
  int i;

  dio_output_init(&dout, buffer, sizeof(buffer));

  dio_put_uint8(&dout, 2);

  for (i = 0; i < NUM_STATS; i++) {
    dio_put_sint16(&dout, parameter->minimal_surplus[i]);
    dio_put_sint16(&dout, parameter->factor[i]);
  }

  dio_put_sint16(&dout, parameter->happy_factor);
  dio_put_uint8(&dout, (int) parameter->factor_target);
  dio_put_bool8(&dout, parameter->require_happy);

  assert(dio_output_used(&dout) == SAVED_PARAMETER_SIZE);

  attr_city_set(attr, city_id, SAVED_PARAMETER_SIZE, buffer);
}
