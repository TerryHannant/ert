/*
   Copyright (C) 2011  Equinor ASA, Norway.

   The file 'subst_list.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_SUBST_H
#define ERT_SUBST_H

#include <stdbool.h>
#include <stdio.h>

#include <ert/util/buffer.hpp>
#include <ert/util/type_macros.hpp>

#include <ert/res_util/subst_func.hpp>

typedef struct subst_list_struct subst_list_type;
bool subst_list_update_buffer(const subst_list_type *subst_list,
                              buffer_type *buffer);
void subst_list_insert_func(subst_list_type *subst_list, const char *func_name,
                            const char *local_func_name);
void subst_list_fprintf(const subst_list_type *, FILE *stream);
void subst_list_set_parent(subst_list_type *subst_list,
                           const subst_list_type *parent);
extern "C" subst_list_type *subst_list_alloc(const void *input_arg);
subst_list_type *subst_list_alloc_deep_copy(const subst_list_type *);
extern "C" void subst_list_free(subst_list_type *);
void subst_list_clear(subst_list_type *subst_list);
extern "C" void subst_list_append_copy(subst_list_type *, const char *,
                                       const char *, const char *doc_string);
void subst_list_append_owned_ref(subst_list_type *, const char *, const char *,
                                 const char *doc_string);
void subst_list_prepend_copy(subst_list_type *, const char *, const char *,
                             const char *doc_string);
void subst_list_prepend_ref(subst_list_type *, const char *, const char *,
                            const char *doc_string);
void subst_list_prepend_owned_ref(subst_list_type *, const char *, const char *,
                                  const char *doc_string);

bool subst_list_filter_file(const subst_list_type *, const char *,
                            const char *);
bool subst_list_update_string(const subst_list_type *, char **);
char *subst_list_alloc_filtered_string(const subst_list_type *, const char *);
extern "C" int subst_list_get_size(const subst_list_type *);
extern "C" const char *subst_list_get_value(const subst_list_type *subst_list,
                                            const char *key);
const char *subst_list_iget_value(const subst_list_type *subst_list, int index);
extern "C" const char *subst_list_iget_key(const subst_list_type *subst_list,
                                           int index);
extern "C" const char *
subst_list_get_doc_string(const subst_list_type *subst_list, const char *key);
extern "C" bool subst_list_has_key(const subst_list_type *subst_list,
                                   const char *key);
void subst_list_add_from_string(subst_list_type *subst_list,
                                const char *arg_string, bool append);

UTIL_IS_INSTANCE_HEADER(subst_list);
#endif
