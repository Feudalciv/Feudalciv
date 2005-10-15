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
#ifndef FC__EFFECTS_H
#define FC__EFFECTS_H

#include "shared.h"		/* bool */

#include "connection.h"
#include "fc_types.h"
#include "requirements.h"
#include "tech.h"
#include "terrain.h"

/* Type of effects. (Used in effect.type field)
 * These must correspond to effect_type_names[] in effects.c. */
enum effect_type {
  EFT_TECH_PARASITE,
  EFT_AIRLIFT,
  EFT_ANY_GOVERNMENT,
  EFT_CAPITAL_CITY,
  EFT_ENABLE_NUKE,
  EFT_ENABLE_SPACE,
  EFT_SPECIALIST_OUTPUT,
  EFT_OUTPUT_BONUS,
  EFT_OUTPUT_BONUS_2,
  EFT_OUTPUT_ADD_TILE,
  EFT_OUTPUT_INC_TILE,
  EFT_OUTPUT_PER_TILE,
  EFT_OUTPUT_WASTE_PCT,
  EFT_FORCE_CONTENT,
  /* TODO: EFT_FORCE_CONTENT_PCT, */
  EFT_GIVE_IMM_TECH,
  EFT_GROWTH_FOOD,
  EFT_HAVE_EMBASSIES,
  EFT_MAKE_CONTENT,
  EFT_MAKE_CONTENT_MIL,
  EFT_MAKE_CONTENT_MIL_PER,
  /* TODO: EFT_MAKE_CONTENT_PCT, */
  EFT_MAKE_HAPPY,
  EFT_NO_ANARCHY,
  EFT_NO_SINK_DEEP,
  EFT_NUKE_PROOF,
  /* TODO: EFT_POLLU_ADJ, */
  /* TODO: EFT_POLLU_PCT, */
  /* TODO: EFT_POLLU_POP_ADJ, */
  EFT_POLLU_POP_PCT,
  /* TODO: EFT_POLLU_PROD_ADJ, */
  EFT_POLLU_PROD_PCT,
  /* TODO: EFT_PROD_PCT, */
  EFT_PROD_TO_GOLD,
  EFT_REVEAL_CITIES,
  EFT_REVEAL_MAP,
  /* TODO: EFT_INCITE_DIST_ADJ, */
  EFT_INCITE_COST_PCT,
  EFT_SIZE_ADJ,
  EFT_SIZE_UNLIMIT,
  EFT_SS_STRUCTURAL,
  EFT_SS_COMPONENT,
  EFT_SS_MODULE,
  EFT_SPY_RESISTANT,
  EFT_SEA_MOVE,
  EFT_UNIT_NO_LOSE_POP,
  EFT_UNIT_RECOVER,
  EFT_UPGRADE_UNIT,
  EFT_UPKEEP_FREE,
  EFT_NO_UNHAPPY,
  EFT_LAND_VETERAN,
  EFT_SEA_VETERAN,
  EFT_AIR_VETERAN,
  EFT_LAND_VET_COMBAT,
  /* TODO: EFT_SEA_VET_COMBAT, */
  /* TODO: EFT_AIR_VET_COMBAT, */
  EFT_LAND_REGEN,
  EFT_SEA_REGEN,
  EFT_AIR_REGEN,
  EFT_CITY_VISION_RADIUS_SQ,
  EFT_UNIT_VISION_RADIUS_SQ,
  EFT_LAND_DEFEND,
  EFT_SEA_DEFEND,
  EFT_AIR_DEFEND,
  EFT_MISSILE_DEFEND,
  EFT_NO_INCITE,
  EFT_GAIN_AI_LOVE,
  EFT_SLOW_DOWN_TIMELINE,
  EFT_CIVIL_WAR_CHANCE,
  EFT_EMPIRE_SIZE_MOD,
  EFT_EMPIRE_SIZE_STEP,
  EFT_MAX_RATES,
  EFT_MARTIAL_LAW_EACH,
  EFT_MARTIAL_LAW_MAX,
  EFT_RAPTURE_GROW,
  EFT_UNBRIBABLE_UNITS,
  EFT_VETERAN_DIPLOMATS,
  EFT_REVOLUTION_WHEN_UNHAPPY,
  EFT_HAS_SENATE,
  EFT_INSPIRE_PARTISANS,
  EFT_HAPPINESS_TO_GOLD,
  EFT_FANATICS, /* stupid special case, we hatess it */
  EFT_NO_DIPLOMACY,
  EFT_TRADE_REVENUE_BONUS,
  EFT_LAST	/* keep this last */
};

/* lookups */
enum effect_type effect_type_from_str(const char *str);
const char *effect_type_name(enum effect_type effect_type);

/* An effect is provided by a source.  If the source is present, and the
 * other conditions (described below) are met, the effect will be active.
 * Note the difference between effect and effect_type. */
struct effect {
  enum effect_type type;

  /* The "value" of the effect.  The meaning of this varies between
   * effects.  When get_xxx_bonus() is called the value of all applicable
   * effects will be summed up. */
  int value;

  /* An effect can have multiple requirements.  The effect will only be
   * active if all of these requirement are met. */
  struct requirement_list *reqs;

  /* An effect can have multiple negated requirements.  The effect will
   * only be active if none of these requirements are met. */
  struct requirement_list *nreqs;
};

/* An effect_list is a list of effects. */
#define SPECLIST_TAG effect
#define SPECLIST_TYPE struct effect
#include "speclist.h"
#define effect_list_iterate(effect_list, peffect) \
  TYPED_LIST_ITERATE(struct effect, effect_list, peffect)
#define effect_list_iterate_end LIST_ITERATE_END

struct effect *effect_new(enum effect_type type, int value);
void effect_req_append(struct effect *peffect, bool neg,
		       struct requirement *preq);

void get_effect_req_text(struct effect *peffect, char *buf, size_t buf_len);

/* ruleset cache creation and communication functions */
struct packet_ruleset_effect;
struct packet_ruleset_effect_req;

void ruleset_cache_init(void);
void ruleset_cache_free(void);
void recv_ruleset_effect(struct packet_ruleset_effect *packet);
void recv_ruleset_effect_req(struct packet_ruleset_effect_req *packet);
void send_ruleset_cache(struct conn_list *dest);

bool is_effect_useful(const struct player *target_player,
		      const struct city *target_pcity,
		      const struct impr_type *target_building,
		      const struct tile *target_tile,
		      const struct unit_type *target_unittype,
		      const struct output_type *target_output,
		      const struct specialist *target_specialist,
		      Impr_type_id source, const struct effect *effect);

bool is_building_replaced(const struct city *pcity, Impr_type_id building);

/* functions to know the bonuses a certain effect is granting */
int get_world_bonus(enum effect_type effect_type);
int get_player_bonus(const struct player *plr, enum effect_type effect_type);
int get_city_bonus(const struct city *pcity, enum effect_type effect_type);
int get_city_specialist_output_bonus(const struct city *pcity,
				     const struct specialist *pspecialist,
				     const struct output_type *poutput,
				     enum effect_type effect_type);
int get_city_tile_output_bonus(const struct city *pcity,
			       const struct tile *ptile,
			       const struct output_type *poutput,
			       enum effect_type effect_type);
int get_building_bonus(const struct city *pcity, Impr_type_id building,
		       enum effect_type effect_type);
int get_unittype_bonus(const struct player *pplayer,
		       const struct tile *ptile, /* pcity is implied */
		       const struct unit_type *punittype,
		       enum effect_type effect_type);
int get_unit_bonus(const struct unit *punit, enum effect_type effect_type);

/* miscellaneous auxiliary effects functions */
struct effect_list *get_req_source_effects(struct req_source *psource);
bool is_effect_disabled(const struct player *target_player,
		        const struct city *target_city,
		        const struct impr_type *target_building,
		        const struct tile *target_tile,
			const struct unit_type *target_unittype,
			const struct output_type *target_output,
			const struct specialist *target_specialist,
		        const struct effect *peffect);

int get_player_bonus_effects(struct effect_list *plist,
    const struct player *pplayer, enum effect_type effect_type);
int get_city_bonus_effects(struct effect_list *plist,
			   const struct city *pcity,
			   const struct output_type *poutput,
			   enum effect_type effect_type);

bool building_has_effect(Impr_type_id building,
			 enum effect_type effect_type);
int get_current_construction_bonus(const struct city *pcity,
				   enum effect_type effect_type);

Impr_type_id ai_find_source_building(struct player *pplayer,
				     enum effect_type effect_type);

#endif  /* FC__EFFECTS_H */

