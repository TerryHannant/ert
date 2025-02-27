/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'history.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_HISTORY_H
#define ERT_HISTORY_H

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include <ert/util/bool_vector.hpp>
#include <ert/util/double_vector.hpp>
#include <ert/util/type_macros.hpp>

#include <ert/ecl/ecl_sum.h>
#include <ert/tooling.hpp>

typedef enum {
    SCHEDULE = 0,
    /** ecl_sum_get_well_var( "WWCT" );  */
    REFCASE_SIMULATED = 1,
    /** ecl_sum_get_well_var( "WWCTH" ); */
    REFCASE_HISTORY = 2,
    HISTORY_SOURCE_INVALID = 10
} history_source_type;

typedef struct history_struct history_type;

history_source_type history_get_source_type(const char *string_source);

// Manipulators.
void history_free(history_type *);
history_type *history_alloc_from_refcase(const ecl_sum_type *refcase,
                                         bool use_h_keywords);
PY_USED const char *
history_get_source_string(history_source_type history_source);
bool history_init_ts(const history_type *history, const char *summary_key,
                     double_vector_type *value, bool_vector_type *valid);

// Accessors.
time_t history_get_start_time(const history_type *history);
int history_get_last_restart(const history_type *);
time_t history_get_time_t_from_restart_nr(const history_type *history,
                                          int restart_nr);
history_source_type history_get_source(const history_type *history);

UTIL_IS_INSTANCE_HEADER(history);

#endif
