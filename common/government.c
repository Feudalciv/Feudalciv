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
#include <config.h>
#endif

#include <assert.h>

#include "game.h"
#include "log.h"
#include "mem.h"
#include "player.h"
#include "shared.h"
#include "support.h"
#include "tech.h"

#include "government.h"

struct government *governments = NULL;

static const char *flag_names[] = {
  "Build_Veteran_Diplomats", "Revolution_When_Unhappy", "Has_Senate",
  "Unbribable", "Inspires_Partisans", "Rapture_City_Growth",
  "Fanatic_Troops", "No_Unhappy_Citizens", "Convert_Tithes_To_Money",
  "Reduced_Research"
};

/***************************************************************
  Convert flag names to enum; case insensitive;
  returns G_LAST_FLAG if can't match.
***************************************************************/
enum government_flag_id government_flag_from_str(const char *s)
{
  enum government_flag_id i;

  assert(ARRAY_SIZE(flag_names) == G_LAST_FLAG);
  
  for(i=G_FIRST_FLAG; i<G_LAST_FLAG; i++) {
    if (mystrcasecmp(flag_names[i], s)==0) {
      return i;
    }
  }
  return G_LAST_FLAG;
}

/****************************************************************************
  Returns TRUE iff the given government has the given flag.
****************************************************************************/
bool government_has_flag(const struct government *gov,
			 enum government_flag_id flag)
{
  assert(flag>=G_FIRST_FLAG && flag<G_LAST_FLAG);
  return TEST_BIT(gov->flags, flag);
}

/****************************************************************************
  Does a linear search of the governments to find the one that matches the
  given (translated) name.  Returns NULL if none match.
****************************************************************************/
struct government *find_government_by_name(const char *name)
{
  government_iterate(gov) {
    if (mystrcasecmp(gov->name, name) == 0) {
      return gov;
    }
  } government_iterate_end;

  return NULL;
}

/****************************************************************************
  Does a linear search of the governments to find the one that matches the
  given original (untranslated) name.  Returns NULL if none match.
****************************************************************************/
struct government *find_government_by_name_orig(const char *name)
{
  government_iterate(gov) {
    if (mystrcasecmp(gov->name_orig, name) == 0) {
      return gov;
    }
  } government_iterate_end;

  return NULL;
}

/****************************************************************************
  Return the government with the given ID.
****************************************************************************/
struct government *get_government(int gov)
{
  assert(game.government_count > 0 && gov >= 0
	 && gov < game.government_count);
  assert(governments[gov].index == gov);
  return &governments[gov];
}

/****************************************************************************
  Return this player's government.
****************************************************************************/
struct government *get_gov_pplayer(const struct player *pplayer)
{
  assert(pplayer != NULL);
  return get_government(pplayer->government);
}

/****************************************************************************
  Return the government of the player who owns the city.
****************************************************************************/
struct government *get_gov_pcity(const struct city *pcity)
{
  assert(pcity != NULL);
  return get_gov_pplayer(city_owner(pcity));
}


/***************************************************************
...
***************************************************************/
const char *get_ruler_title(int gov, bool male, int nation)
{
  struct government *g = get_government(gov);
  struct ruler_title *best_match = NULL;
  int i;

  for(i=0; i<g->num_ruler_titles; i++) {
    struct ruler_title *title = &g->ruler_titles[i];
    if (title->nation == DEFAULT_TITLE && !best_match) {
      best_match = title;
    } else if (title->nation == nation) {
      best_match = title;
      break;
    }
  }

  if (best_match) {
    return male ? best_match->male_title : best_match->female_title;
  } else {
    freelog(LOG_ERROR,
	    "get_ruler_title: found no title for government %d (%s) nation %d",
	    gov, g->name, nation);
    return male ? "Mr." : "Ms.";
  }
}

/***************************************************************
...
***************************************************************/
int get_government_max_rate(int type)
{
  if(type == G_MAGIC)
    return 100;
  if(type >= 0 && type < game.government_count)
    return governments[type].max_rate;
  return 50;
}

/***************************************************************
Added for civil war probability computation - Kris Bubendorfer
***************************************************************/
int get_government_civil_war_prob(int type)
{
  if(type >= 0 && type < game.government_count)
    return governments[type].civil_war;
  return 0;
}

/***************************************************************
...
***************************************************************/
const char *get_government_name(int type)
{
  if(type >= 0 && type < game.government_count)
    return governments[type].name;
  return "";
}

/***************************************************************
  Can change to government if appropriate tech exists, and one of:
   - no required tech (required is A_NONE)
   - player has required tech
   - we have an appropriate wonder
***************************************************************/
bool can_change_to_government(struct player *pplayer, int government)
{
  struct government *gov = &governments[government];

  if (government < 0 || government >= game.government_count) {
    assert(0);
    return FALSE;
  }

  if (get_player_bonus(pplayer, EFT_ANY_GOVERNMENT) > 0) {
    /* Note, this may allow govs that are on someone else's "tech tree". */
    return TRUE;
  }

  return are_reqs_active(TARGET_PLAYER, pplayer, NULL, B_LAST, NULL,
			 gov->req, MAX_NUM_REQS);
}

/***************************************************************
...
***************************************************************/
void set_ruler_title(struct government *gov, int nation,
                     const char *male, const char *female)
{
  struct ruler_title *title;

  gov->num_ruler_titles++;
  gov->ruler_titles =
    fc_realloc(gov->ruler_titles,
      gov->num_ruler_titles*sizeof(struct ruler_title));
  title = &(gov->ruler_titles[gov->num_ruler_titles-1]);

  title->nation = nation;

  sz_strlcpy(title->male_title_orig, male);
  title->male_title = title->male_title_orig;

  sz_strlcpy(title->female_title_orig, female);
  title->female_title = title->female_title_orig;
}

/***************************************************************
 Allocate space for the given number of governments.
***************************************************************/
void governments_alloc(int num)
{
  int index;

  governments = fc_calloc(num, sizeof(struct government));
  game.government_count = num;

  for (index = 0; index < num; index++) {
    governments[index].index = index;
  }
}

/***************************************************************
 De-allocate resources associated with the given government.
***************************************************************/
static void government_free(struct government *gov)
{
  free(gov->ruler_titles);
  gov->ruler_titles = NULL;

  free(gov->helptext);
  gov->helptext = NULL;
}

/***************************************************************
 De-allocate the currently allocated governments.
***************************************************************/
void governments_free(void)
{
  government_iterate(gov) {
    government_free(gov);
  } government_iterate_end;
  free(governments);
  governments = NULL;
  game.government_count = 0;
}
