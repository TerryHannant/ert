/*
   Copyright (C) 2012  Equinor ASA, Norway.

   The file 'config_content_node.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_CONFIG_CONTENT_NODE_H
#define ERT_CONFIG_CONTENT_NODE_H

#include <ert/util/hash.hpp>

#include <ert/config/config_path_elm.hpp>
#include <ert/config/config_schema_item.hpp>

typedef struct config_content_node_struct config_content_node_type;

extern "C" config_item_types
config_content_node_iget_type(const config_content_node_type *node, int index);
config_content_node_type *
config_content_node_alloc(const config_schema_item_type *schema,
                          const config_path_elm_type *cwd);
void config_content_node_add_value(config_content_node_type *node,
                                   const char *value);
void config_content_node_set(config_content_node_type *node,
                             const stringlist_type *token_list);
char *
config_content_node_alloc_joined_string(const config_content_node_type *node,
                                        const char *sep);
void config_content_node_free(config_content_node_type *node);
void config_content_node_free__(void *arg);
extern "C" const char *
config_content_node_get_full_string(const config_content_node_type *node,
                                    const char *sep);
extern "C" const char *
config_content_node_iget(const config_content_node_type *node, int index);
extern "C" bool
config_content_node_iget_as_bool(const config_content_node_type *node,
                                 int index);
extern "C" int
config_content_node_iget_as_int(const config_content_node_type *node,
                                int index);
extern "C" double
config_content_node_iget_as_double(const config_content_node_type *node,
                                   int index);
extern "C" const char *
config_content_node_iget_as_path(config_content_node_type *node, int index);
extern "C" const char *
config_content_node_iget_as_abspath(config_content_node_type *node, int index);
extern "C" const char *
config_content_node_iget_as_relpath(config_content_node_type *node, int index);
const char *
config_content_node_iget_as_executable(config_content_node_type *node,
                                       int index);
extern "C" time_t
config_content_node_iget_as_isodate(const config_content_node_type *node,
                                    int index);
const stringlist_type *
config_content_node_get_stringlist(const config_content_node_type *node);
const char *config_content_node_safe_iget(const config_content_node_type *node,
                                          int index);
extern "C" int
config_content_node_get_size(const config_content_node_type *node);
const char *config_content_node_get_kw(const config_content_node_type *node);
void config_content_node_assert_key_value(const config_content_node_type *node);
void config_content_node_init_opt_hash(const config_content_node_type *node,
                                       hash_type *opt_hash, int elm_offset);

#endif
