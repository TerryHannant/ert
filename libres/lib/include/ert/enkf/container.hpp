/*
   Copyright (C) 2012  Equinor ASA, Norway.

   The file 'container.h' is part of ERT - Ensemble based Reservoir Tool.

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

#ifndef ERT_CONTAINER_H
#define ERT_CONTAINER_H

#include <ert/enkf/enkf_macros.hpp>
#include <ert/util/type_macros.h>

typedef struct container_struct container_type;

void container_add_node(container_type *container, void *child_node);
const void *container_iget_node(const container_type *container, int index);
int container_get_size(const container_type *container);
void container_assert_size(const container_type *container);

VOID_ALLOC_HEADER(container);
VOID_FREE_HEADER(container);
UTIL_IS_INSTANCE_HEADER(container);
UTIL_SAFE_CAST_HEADER_CONST(container);

#endif
