/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'ensemble_config.h' is part of ERT - Ensemble based Reservoir Tool.

   ERT is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ERT is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
   for more details.
*/

#ifndef ERT_ENSEMBLE_CONFIG_H
#define ERT_ENSEMBLE_CONFIG_H
#include <stdbool.h>

#include <string>
#include <vector>

#include <ert/util/hash.hpp>
#include <ert/util/stringlist.hpp>

#include <ert/ecl/ecl_grid.hpp>
#include <ert/ecl/ecl_sum.hpp>

#include <ert/config/config_content.hpp>
#include <ert/config/config_parser.hpp>

#include <ert/enkf/enkf_config_node.hpp>
#include <ert/enkf/enkf_types.hpp>
#include <ert/enkf/summary_config.hpp>
#include <ert/enkf/summary_key_matcher.hpp>

typedef struct ensemble_config_struct ensemble_config_type;

void ensemble_config_set_refcase(ensemble_config_type *ensemble_config,
                                 const ecl_sum_type *refcase);
void ensemble_config_set_gen_kw_format(ensemble_config_type *ensemble_config,
                                       const char *gen_kw_format_string);
enkf_config_node_type *
ensemble_config_add_container(ensemble_config_type *ensemble_config,
                              const char *key);
enkf_config_node_type *
ensemble_config_add_surface(ensemble_config_type *ensemble_config,
                            const char *key, bool forward_init);

extern "C" void ensemble_config_add_node(ensemble_config_type *ensemble_config,
                                         enkf_config_node_type *node);
enkf_config_node_type *
ensemble_config_add_gen_data(ensemble_config_type *config, const char *key,
                             bool dynamic, bool forward_init);
extern "C" enkf_config_node_type *
ensemble_config_add_summary(ensemble_config_type *ensemble_config,
                            const char *key, load_fail_type load_fail);
enkf_config_node_type *
ensemble_config_add_summary_observation(ensemble_config_type *ensemble_config,
                                        const char *key,
                                        load_fail_type load_fail);
extern "C" enkf_config_node_type *
ensemble_config_add_gen_kw(ensemble_config_type *config, const char *key,
                           bool forward_init);
extern "C" enkf_config_node_type *
ensemble_config_add_field(ensemble_config_type *config, const char *key,
                          ecl_grid_type *ecl_grid, bool forward_init);
int ensemble_config_get_observations(const ensemble_config_type *config,
                                     enkf_obs_type *enkf_obs,
                                     const char *user_key, int obs_count,
                                     time_t *obs_time, double *y, double *std);
void ensemble_config_clear_obs_keys(ensemble_config_type *ensemble_config);
void ensemble_config_add_obs_key(ensemble_config_type *, const char *,
                                 const char *);
const enkf_config_node_type *
ensemble_config_user_get_node(const ensemble_config_type *, const char *,
                              char **);
extern "C" void ensemble_config_free(ensemble_config_type *);
extern "C" bool ensemble_config_has_key(const ensemble_config_type *,
                                        const char *);
bool ensemble_config_has_impl_type(const ensemble_config_type *config,
                                   const ert_impl_type impl_type);
bool ensemble_config_have_forward_init(
    const ensemble_config_type *ensemble_config);
bool ensemble_config_require_summary(const ensemble_config_type *config);
void ensemble_config_add_config_items(config_parser_type *);

void ensemble_config_init_GEN_PARAM(ensemble_config_type *ensemble_config,
                                    const config_content_type *config);

extern "C" field_trans_table_type *
ensemble_config_get_trans_table(const ensemble_config_type *ensemble_config);
extern "C" enkf_config_node_type *
ensemble_config_get_node(const ensemble_config_type *, const char *);
enkf_config_node_type *ensemble_config_get_or_create_summary_node(
    ensemble_config_type *ensemble_config, const char *key);
extern "C" stringlist_type *
ensemble_config_alloc_keylist(const ensemble_config_type *);
std::vector<std::string>
ensemble_config_keylist_from_var_type(const ensemble_config_type *,
                                      int var_mask);
extern "C" stringlist_type *
ensemble_config_alloc_keylist_from_impl_type(const ensemble_config_type *,
                                             ert_impl_type);
ensemble_config_type *ensemble_config_alloc_load(const char *, ecl_grid_type *,
                                                 const ecl_sum_type *);
extern "C" ensemble_config_type *
ensemble_config_alloc(const config_content_type *, ecl_grid_type *,
                      const ecl_sum_type *);
extern "C" PY_USED ensemble_config_type *
ensemble_config_alloc_full(const char *gen_kw_format_string);
extern "C" void ensemble_config_init_SUMMARY_full(ensemble_config_type *,
                                                  const char *,
                                                  const ecl_sum_type *);

extern "C" const summary_key_matcher_type *
ensemble_config_get_summary_key_matcher(
    const ensemble_config_type *ensemble_config);
extern "C" int
ensemble_config_get_size(const ensemble_config_type *ensemble_config);
std::pair<fw_load_status, std::string>
ensemble_config_forward_init(const ensemble_config_type *ens_config,
                             const run_arg_type *run_arg);

UTIL_IS_INSTANCE_HEADER(ensemble_config);
UTIL_SAFE_CAST_HEADER(ensemble_config);

#endif
