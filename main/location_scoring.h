#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

/* Location parsing and ranking can be checked natively without a board. */

#define LOCATION_SCORING_STR_LEN 64

typedef struct {
  char name[LOCATION_SCORING_STR_LEN];
  char state[LOCATION_SCORING_STR_LEN];
  char country[8];
  bool country_is_alpha2;
  char state_name[LOCATION_SCORING_STR_LEN];
} location_query_parts_t;

/* Trims and splits a "Name, State, Country" query on commas; resolves a
 * 2-letter US state abbreviation to its full name in state_name. */
void location_scoring_parse_query(const char *raw_query, location_query_parts_t *out_parts);

/* Keep the qualifier in the provider request before its result limit applies. */
void location_scoring_format_search_name(
  const location_query_parts_t *parts, char *output, size_t output_size
);

/* Picks the highest-scoring entry from an Open-Meteo /v1/search "results"
 * array. Qualifiers must already have been sent to the provider; scores rank
 * that returned set rather than enforcing a second geographic filter.
 * Ties keep provider order. The returned entry is borrowed from results.
 * Returns NULL if results is empty/not an array. */
cJSON *location_scoring_select_best_result(cJSON *results, const location_query_parts_t *query_parts);

/* Joins the nonempty name, administrative area and country with ", ". */
void location_scoring_format_label(
  const char *name,
  const char *admin1,
  const char *country_code,
  char *output,
  size_t output_size
);

/* Read the final country token from an already resolved label, not a user query. */
bool location_scoring_label_has_country(const char *label, const char *country_code);
