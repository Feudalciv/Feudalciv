/****************************************************************************
 Freeciv - Copyright (C) 2004 - The Freeciv Team
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
****************************************************************************/

#ifdef HAVE_CONFIG_H
#include <fc_config.h>
#endif

/* common */
#include "fc_types.h"

#include "road.h"

static struct road_type roads[ROAD_LAST] =
  {
    { ROAD_ROAD, N_("Road"), ACTIVITY_ROAD, N_("Road"),
      S_ROAD },
    { ROAD_RAILROAD, N_("Railroad"), ACTIVITY_RAILROAD, N_("Railroad"),
      S_RAILROAD }
  };

/**************************************************************************
  Return the road id.
**************************************************************************/
Road_type_id road_number(const struct road_type *proad)
{
  fc_assert_ret_val(NULL != proad, -1);

  return proad->id;
}

/**************************************************************************
  Return the road index.

  Currently same as road_number(), paired with road_count()
  indicates use as an array index.
**************************************************************************/
Road_type_id road_index(const struct road_type *proad)
{
  fc_assert_ret_val(NULL != proad, -1);

  return proad - roads;
}

/**************************************************************************
  Return the number of road_types.
**************************************************************************/
Road_type_id road_count(void)
{
  return ROAD_LAST;
}

/****************************************************************************
  Return road type of given id.
****************************************************************************/
struct road_type *road_type_by_id(int id)
{
  fc_assert_ret_val(id >= 0 && id < ROAD_LAST, NULL);

  return &roads[id];
}

/****************************************************************************
  Return activity that is required in order to build given road type.
****************************************************************************/
enum unit_activity road_activity(struct road_type *road)
{
  return road->act;
}

/****************************************************************************
  Return road type that is built by give activity. Returns ROAD_LAST if
  activity is not road building activity at all.
****************************************************************************/
struct road_type *road_by_activity(enum unit_activity act)
{
  road_type_iterate(road) {
    if (road->act == act) {
      return road;
    }
  } road_type_iterate_end;

  return NULL;
}

/****************************************************************************
  Return tile special that represents this road type.
****************************************************************************/
enum tile_special_type road_special(struct road_type *road)
{
  return road->special;
}

/****************************************************************************
  Return road type represented by given special, or NULL if special does
  not represent road type at all.
****************************************************************************/
struct road_type *road_by_special(enum tile_special_type spe)
{
  road_type_iterate(road) {
    if (road->special == spe) {
      return road;
    }
  } road_type_iterate_end;

  return NULL;
}

/****************************************************************************
  Return translated name of this road type.
****************************************************************************/
const char *road_name_translation(struct road_type *road)
{
  return _(road->name);
}

/****************************************************************************
  Return untranslated name of this road type.
****************************************************************************/
const char *road_rule_name(struct road_type *road)
{
  return road->name;
}

/****************************************************************************
  Return verb describing building of this road type.
****************************************************************************/
const char *road_activity_text(struct road_type *road)
{
  return _(road->activity_text);
}
