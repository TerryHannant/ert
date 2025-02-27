/*
   Copyright (C) 2012  Equinor ASA, Norway.

   The file 'config_content_item.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_CONFIG_CONTENT_ITEM_H
#define ERT_CONFIG_CONTENT_ITEM_H

#include <ert/util/hash.hpp>
#include <ert/util/stringlist.hpp>
#include <ert/util/type_macros.hpp>

#include <ert/config/config_content_node.hpp>
#include <ert/config/config_error.hpp>
#include <ert/config/config_path_elm.hpp>
#include <ert/config/config_schema_item.hpp>

typedef struct config_content_item_struct config_content_item_type;

extern "C" int
config_content_item_get_size(const config_content_item_type *item);
config_content_node_type *
config_content_item_get_last_node(const config_content_item_type *item);
extern "C" config_content_node_type *
config_content_item_iget_node(const config_content_item_type *item, int index);
const stringlist_type *
config_content_item_iget_stringlist_ref(const config_content_item_type *item,
                                        int occurence);
hash_type *config_content_item_alloc_hash(const config_content_item_type *item,
                                          bool copy);
const char *config_content_item_iget(const config_content_item_type *item,
                                     int occurence, int index);
bool config_content_item_iget_as_bool(const config_content_item_type *item,
                                      int occurence, int index);
int config_content_item_iget_as_int(const config_content_item_type *item,
                                    int occurence, int index);
double config_content_item_iget_as_double(const config_content_item_type *item,
                                          int occurence, int index);
void config_content_item_clear(config_content_item_type *item);
extern "C" void config_content_item_free(config_content_item_type *item);
void config_content_item_free__(void *arg);
extern "C" config_content_item_type *
config_content_item_alloc(const config_schema_item_type *schema,
                          const config_path_elm_type *path_elm);
config_content_node_type *
config_content_item_alloc_node(const config_content_item_type *item,
                               const config_path_elm_type *path_elm);
const config_schema_item_type *
config_content_item_get_schema(const config_content_item_type *item);
const config_path_elm_type *
config_content_item_get_path_elm(const config_content_item_type *item);

UTIL_IS_INSTANCE_HEADER(config_content_item);

#endif
