/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'block_obs.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_BLOCK_OBS_H
#define ERT_BLOCK_OBS_H

#include <ert/sched/history.hpp>

#include <ert/config/conf.hpp>

#include <ert/ecl/ecl_grid.h>
#include <ert/ecl/ecl_sum.h>

#include <ert/enkf/active_list.hpp>
#include <ert/enkf/enkf_macros.hpp>
#include <ert/enkf/field.hpp>
#include <ert/enkf/field_config.hpp>
#include <ert/enkf/obs_data.hpp>

typedef struct block_obs_struct block_obs_type;

typedef enum { SOURCE_FIELD = 10, SOURCE_SUMMARY = 12 } block_obs_source_type;

block_obs_type *block_obs_alloc_complete(
    const char *obs_label, block_obs_source_type source_type,
    const stringlist_type *summary_keys, const void *data_config,
    const ecl_grid_type *grid, int size, const int *i, const int *j,
    const int *k, const double *obs_value, const double *obs_std);

extern "C" block_obs_type *block_obs_alloc(const char *obs_key,
                                           const void *data_config,
                                           const ecl_grid_type *grid);

extern "C" void block_obs_free(block_obs_type *block_obs);

extern "C" PY_USED double block_obs_iget_depth(const block_obs_type *block_obs,
                                               int index);
extern "C" PY_USED int block_obs_iget_i(const block_obs_type *, int index);
extern "C" PY_USED int block_obs_iget_j(const block_obs_type *, int index);
extern "C" PY_USED int block_obs_iget_k(const block_obs_type *, int index);
extern "C" int block_obs_get_size(const block_obs_type *);
extern "C" double block_obs_iget_value(const block_obs_type *block_obs,
                                       int index);
extern "C" double block_obs_iget_std(const block_obs_type *block_obs,
                                     int index);
extern "C" double block_obs_iget_data(const block_obs_type *block_obs,
                                      const void *state, int iobs,
                                      node_id_type node_id);
extern "C" double block_obs_iget_std_scaling(const block_obs_type *block_obs,
                                             int index);
extern "C" PY_USED void
block_obs_update_std_scale(block_obs_type *block_obs, double scale_factor,
                           const ActiveList *active_list);
extern "C" void block_obs_append_field_obs(block_obs_type *block_obs, int i,
                                           int j, int k, double value,
                                           double std);
extern "C" void block_obs_append_summary_obs(block_obs_type *block_obs, int i,
                                             int j, int k, const char *sum_key,
                                             double value, double std);

VOID_FREE_HEADER(block_obs);
VOID_GET_OBS_HEADER(block_obs);
UTIL_IS_INSTANCE_HEADER(block_obs);
VOID_MEASURE_HEADER(block_obs);
VOID_USER_GET_OBS_HEADER(block_obs);
VOID_CHI2_HEADER(block_obs);
VOID_UPDATE_STD_SCALE_HEADER(block_obs);

#endif
