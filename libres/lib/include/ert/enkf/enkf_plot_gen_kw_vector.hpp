/*
   Copyright (C) 2014  Equinor ASA, Norway.

   The file 'enkf_plot_gen_kw.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_ENKF_PLOT_GEN_KW_VECTOR_H
#define ERT_ENKF_PLOT_GEN_KW_VECTOR_H

#include <ert/util/stringlist.h>
#include <ert/util/type_macros.h>

#include <ert/enkf/enkf_config_node.hpp>

typedef struct enkf_plot_gen_kw_vector_struct enkf_plot_gen_kw_vector_type;

enkf_plot_gen_kw_vector_type *
enkf_plot_gen_kw_vector_alloc(const enkf_config_node_type *config_node,
                              int iens);
void enkf_plot_gen_kw_vector_free(enkf_plot_gen_kw_vector_type *vector);
extern "C" int
enkf_plot_gen_kw_vector_get_size(const enkf_plot_gen_kw_vector_type *vector);
void enkf_plot_gen_kw_vector_reset(enkf_plot_gen_kw_vector_type *vector);
void enkf_plot_gen_kw_vector_load(enkf_plot_gen_kw_vector_type *vector,
                                  enkf_fs_type *fs, bool transform_data,
                                  int report_step);
extern "C" double
enkf_plot_gen_kw_vector_iget(const enkf_plot_gen_kw_vector_type *vector,
                             int index);

UTIL_IS_INSTANCE_HEADER(enkf_plot_gen_kw_vector);

#endif
