/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'enkf_config_node.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_ENKF_CONFIG_NODE_H
#define ERT_ENKF_CONFIG_NODE_H

#include <ert/util/hash.h>
#include <ert/util/stringlist.h>

#include <ert/config/config_parser.hpp>

#include <ert/ecl/ecl_grid.hpp>

#include <ert/enkf/enkf_macros.hpp>
#include <ert/enkf/enkf_types.hpp>
#include <ert/enkf/field_trans.hpp>
#include <ert/enkf/gen_data_config.hpp>
#include <ert/enkf/summary_config.hpp>

typedef void(config_free_ftype)(void *);
typedef int(get_data_size_ftype)(const void *);
typedef void(config_fprintf_ftype)(const void *, enkf_var_type, FILE *);

typedef struct enkf_config_node_struct enkf_config_node_type;
typedef struct enkf_node_struct enkf_node_type;

bool enkf_config_node_has_vector(const enkf_config_node_type *node,
                                 enkf_fs_type *fs, int iens);
bool enkf_config_node_has_node(const enkf_config_node_type *node,
                               enkf_fs_type *fs, node_id_type node_id);
bool enkf_config_node_vector_storage(const enkf_config_node_type *config_node);

void enkf_config_node_update_min_std(enkf_config_node_type *config_node,
                                     const char *min_std_file);

enkf_config_node_type *
enkf_config_node_alloc_GEN_PARAM(const char *node_key, bool forward_init,
                                 gen_data_file_format_type input_format,
                                 gen_data_file_format_type output_format,
                                 const char *init_file_fmt,
                                 const char *ert_outfile_fmt);

enkf_config_node_type *
enkf_config_node_alloc_GEN_DATA_result(const char *key,
                                       gen_data_file_format_type input_format,
                                       const char *enkf_infile_fmt);

enkf_config_node_type *enkf_config_node_new_surface(const char *key,
                                                    bool forward_init);

void enkf_config_node_update_surface(enkf_config_node_type *config_node,
                                     const char *base_surface,
                                     const char *init_file_fmt,
                                     const char *output_file,
                                     const char *min_std_file);

void enkf_config_node_update_gen_kw(
    enkf_config_node_type *config_node,
    const char *
        enkf_outfile_fmt, /* The include file created by ERT for the forward model. */
    const char *template_file, const char *parameter_file,
    const char *min_std_file, const char *init_file_fmt);

extern "C" enkf_config_node_type *
enkf_config_node_alloc(enkf_var_type, ert_impl_type, bool, const char *,
                       const char *, const char *, const char *, void *);

extern "C" enkf_config_node_type *
enkf_config_node_alloc_summary(const char *key, load_fail_type load_fail);

extern "C" void enkf_config_node_update_parameter_field(
    enkf_config_node_type *config_node, const char *enkf_outfile_fmt,
    const char *init_file_fmt, const char *min_std_file, int truncation,
    double value_min, double value_max, const char *init_transform,
    const char *output_transform);

extern "C" enkf_config_node_type *
enkf_config_node_alloc_GEN_DATA_everest(const char *key,
                                        const char *result_file_fmt,
                                        const int_vector_type *report_steps);

extern "C" void enkf_config_node_update_general_field(
    enkf_config_node_type *config_node, const char *enkf_outfile_fmt,
    const char *enkf_infile_fmt, const char *init_file_fmt,
    const char *min_std_file, int truncation, double value_min,
    double value_max, const char *init_transform, const char *input_transform,
    const char *output_transform);

enkf_config_node_type *enkf_config_node_new_gen_kw(const char *key,
                                                   const char *tag_fmt,
                                                   bool forward_init);
extern "C" enkf_config_node_type *
enkf_config_node_alloc_field(const char *key, ecl_grid_type *ecl_grid,
                             field_trans_table_type *trans_table,
                             bool forward_init);
int enkf_config_node_get_data_size(const enkf_config_node_type *node,
                                   int report_step);
char *enkf_config_node_alloc_infile(const enkf_config_node_type *, int);
char *enkf_config_node_alloc_outfile(const enkf_config_node_type *, int);
int enkf_config_node_get_num_obs(const enkf_config_node_type *config_node);
int enkf_config_node_load_obs(const enkf_config_node_type *config_node,
                              enkf_obs_type *enkf_obs, const char *key_index,
                              int obs_count, time_t *sim_time, double *y,
                              double *std);
extern "C" const stringlist_type *
enkf_config_node_get_obs_keys(const enkf_config_node_type *);
void enkf_config_node_add_obs_key(enkf_config_node_type *, const char *);
void enkf_config_node_clear_obs_keys(enkf_config_node_type *config_node);
extern "C" void enkf_config_node_free(enkf_config_node_type *);
bool enkf_config_node_include_type(const enkf_config_node_type *, int);
bool enkf_config_node_include_type(const enkf_config_node_type *, int);
extern "C" ert_impl_type
enkf_config_node_get_impl_type(const enkf_config_node_type *);
extern "C" enkf_var_type
enkf_config_node_get_var_type(const enkf_config_node_type *);
extern "C" void *enkf_config_node_get_ref(const enkf_config_node_type *);
extern "C" const char *enkf_config_node_get_key(const enkf_config_node_type *);
const char *
enkf_config_node_get_FIELD_fill_file(enkf_config_node_type *config_node,
                                     const path_fmt_type *runpath_fmt);
extern "C" const char *
enkf_config_node_get_min_std_file(const enkf_config_node_type *config_node);
extern "C" const char *
enkf_config_node_get_enkf_outfile(const enkf_config_node_type *conifg_node);
extern "C" const char *
enkf_config_node_get_enkf_infile(const enkf_config_node_type *config_node);
extern "C" const char *
enkf_config_node_get_init_file_fmt(const enkf_config_node_type *config_node);
char *enkf_config_node_alloc_initfile(const enkf_config_node_type *node,
                                      const char *path, int iens);

void enkf_config_node_set_internalize(enkf_config_node_type *node,
                                      int report_step);
bool enkf_config_node_internalize(const enkf_config_node_type *node,
                                  int report_step);

void enkf_config_node_fprintf_config(const enkf_config_node_type *config_node,
                                     FILE *stream);
enkf_config_node_type *
enkf_config_node_container_iget(const enkf_config_node_type *node, int index);
extern "C" int
enkf_config_node_container_size(const enkf_config_node_type *node);

extern "C" enkf_config_node_type *
enkf_config_node_new_container(const char *key);
extern "C" void
enkf_config_node_update_container(enkf_config_node_type *config_node,
                                  const enkf_config_node_type *child_node);
extern "C" PY_USED const char *
enkf_config_node_iget_container_key(const enkf_config_node_type *config_node,
                                    int index);
/*
    The enkf_node_free() function declaration is in the enkf_config_node.h header,
    because the enkf_config_node needs to know how to free the min_std node.
  */
extern "C" void enkf_node_free(enkf_node_type *enkf_node);
const enkf_node_type *
enkf_config_node_get_min_std(const enkf_config_node_type *config_node);

extern "C" bool
enkf_config_node_use_forward_init(const enkf_config_node_type *config_node);

void enkf_config_node_add_GEN_DATA_config_schema(config_parser_type *config);
void enkf_config_node_add_GEN_PARAM_config_schema(config_parser_type *config);
enkf_config_node_type *enkf_config_node_alloc_GEN_PARAM_from_config(
    const config_content_node_type *config_node);
enkf_config_node_type *enkf_config_node_alloc_GEN_DATA_from_config(
    const config_content_node_type *node);

extern "C" PY_USED enkf_config_node_type *enkf_config_node_alloc_GEN_PARAM_full(
    const char *node_key, bool forward_init,
    gen_data_file_format_type input_format,
    gen_data_file_format_type output_format, const char *init_file_fmt,
    const char *ecl_file, const char *min_std_file, const char *template_file,
    const char *data_key);

extern "C" PY_USED enkf_config_node_type *enkf_config_node_alloc_GEN_DATA_full(
    const char *node_key, const char *result_file,
    gen_data_file_format_type input_format, const int_vector_type *report_steps,
    const char *ecl_file, const char *init_file_fmt, const char *template_file,
    const char *data_key);

extern "C" PY_USED enkf_config_node_type *enkf_config_node_alloc_GEN_KW_full(
    const char *node_key, bool forward_init, const char *gen_kw_format,
    const char *template_file, const char *enkf_outfile,
    const char *parameter_file, const char *min_std_file,
    const char *init_file_fmt);

extern "C" PY_USED enkf_config_node_type *enkf_config_node_alloc_SURFACE_full(
    const char *node_key, bool forward_init, const char *output_file,
    const char *base_surface, const char *min_std_file,
    const char *init_file_fmt);

UTIL_IS_INSTANCE_HEADER(enkf_config_node);
UTIL_SAFE_CAST_HEADER(enkf_config_node);
VOID_FREE_HEADER(enkf_config_node);
#endif
