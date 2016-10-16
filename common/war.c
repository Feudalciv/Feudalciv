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

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* utility */
#include "log.h"
#include "mem.h"

/* common */
#include "game.h"
#include "player.h"

/* common/scriptcore */
#include "luascript_types.h"

#include "war.h"

/**************************************************************************
  Initialize the war cache
**************************************************************************/
void war_cache_init()
{
  wars = war_list_new();
}

/**************************************************************************
  Free the war cache
**************************************************************************/
void war_cache_free()
{
  if (wars) {
    war_list_iterate(wars, pwar) {
      free((char*)pwar->casus_belli);
      player_list_clear(pwar->defenders);
      player_list_destroy(pwar->defenders);
      player_list_clear(pwar->aggressors);
      player_list_destroy(pwar->aggressors);
      free(pwar);
    } war_list_iterate_end;
    war_list_destroy(wars);
    wars = NULL;
  }
}

/***************************************************************
  Save the wars into the savegame.
***************************************************************/
void wars_save(struct section_file *file, const char *section)
{
  int war_count = 0;

  war_list_iterate(wars, pwar) {
    int defender_nums[player_list_size(pwar->defenders)], aggressor_nums[player_list_size(pwar->aggressors)];
    int defender_count, aggressor_count;

    secfile_insert_int(file, player_number(pwar->aggressor), "%s.war%d.aggressor", section, war_count);
    secfile_insert_int(file, player_number(pwar->defender), "%s.war%d.defender", section, war_count);

    defender_count = 0;
    player_list_iterate(pwar->defenders, pplayer) {
      defender_nums[defender_count] = player_number(pplayer);
      defender_count++;
    } player_list_iterate_end;

    aggressor_count = 0;
    player_list_iterate(pwar->aggressors, pplayer) {
      aggressor_nums[aggressor_count] = player_number(pplayer);
      aggressor_count++;
    } player_list_iterate_end;

    secfile_insert_int_vec(file, aggressor_nums, aggressor_count,  "%s.war%d.aggressors", section, war_count);
    secfile_insert_int_vec(file, defender_nums, defender_count, "%s.war%d.defenders", section, war_count);

    war_count++;
  } war_list_iterate_end;

  secfile_insert_int(file, war_count, "%s.count", section);
}

/***************************************************************
  Load the wars from the savegame.
***************************************************************/
void wars_load(struct section_file *file, const char *section)
{
  int war_count, i, j;
  struct war *pwar;
  int defender_num, aggressor_num;
  struct player *defender, *aggressor, *tmp;
  struct player_list *defenders, *aggressors;
  int *defender_nums, *aggressor_nums;
  size_t num_defenders, num_aggressors;

  war_count = secfile_lookup_int_default(file, 0, "%s.count", section);

  for (i = 0; i < war_count; i++) {
    if (!secfile_lookup_int(file, &aggressor_num, "%s.war%d.aggressor", section, i)) {
      log_verbose("[War %4d] Missing war aggressor.", i);
      continue;
    }
    aggressor = player_by_number(aggressor_num);
    if (aggressor == NULL) {
      log_verbose("[War %4d] Aggressor number %d is invalid", i, aggressor_num);
      continue;
    }

    if (!secfile_lookup_int(file, &defender_num, "%s.war%d.defender", section, i)) {
      log_verbose("[War %4d] Missing war defender.", i);
      continue;
    }
    defender = player_by_number(defender_num);
    if (defender == NULL) {
      log_verbose("[War %4d] Defender number %d is invalid", i, defender_num);
      continue;
    }

    defender_nums = secfile_lookup_int_vec(file, &num_defenders, "%s.war%d.defenders", section, i);
    if (defender_nums == NULL) {
      log_verbose("[war %4d] Defender list is invalid or missing", i);
      continue;
    }
    defenders = player_list_new();
    for (j = 0; j < num_defenders; j++) {
      tmp = player_by_number(defender_nums[j]);
      if (tmp == NULL) {
        log_verbose("[war %4d] Player number %d is invalid", i, defender_nums[j]);
        continue;
      }
      player_list_append(defenders, tmp);
    }

    aggressor_nums = secfile_lookup_int_vec(file, &num_aggressors, "%s.war%d.aggressors", section, i);
    if (aggressor_nums == NULL) {
      log_verbose("[war %4d] Aggressor list is invalid or missing", i);
      continue;
    }
    aggressors = player_list_new();
    for (j = 0; j < num_aggressors; j++) {
      tmp = player_by_number(aggressor_nums[j]);
      if (tmp == NULL) {
        log_verbose("[war %4d] Player number %d is invalid", i, aggressor_nums[j]);
        continue;
      }
      player_list_append(aggressors, tmp);
    }

    pwar = fc_malloc(sizeof(*pwar));
    pwar->defender = defender;
    pwar->aggressor = aggressor;
    pwar->defenders = defenders;
    pwar->aggressors = aggressors;

    player_list_iterate(defenders, pplayer) {
      war_list_append(pplayer->current_wars, pwar);
    } player_list_iterate_end;

    player_list_iterate(aggressors, pplayer) {
      war_list_append(pplayer->current_wars, pwar);
    } player_list_iterate_end;

    war_list_append(wars, pwar);
  }
}
