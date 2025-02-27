/*
   Copyright (C) 2013  Equinor ASA, Norway.

   The file 'enkf_model_config.c' is part of ERT - Ensemble based Reservoir Tool.

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
#include <stdlib.h>

#include <ert/util/test_util.h>

#include <ert/enkf/model_config.hpp>

void test_create() {
    model_config_type *model_config = model_config_alloc_empty();
    test_assert_true(model_config_is_instance(model_config));
    model_config_free(model_config);
}

void test_runpath() {
    model_config_type *model_config = model_config_alloc_empty();
    model_config_add_runpath(model_config, "KEY", "RunPath%d");
    model_config_add_runpath(model_config, "KEY2", "2-RunPath%d");
    test_assert_true(model_config_select_runpath(model_config, "KEY"));
    test_assert_false(model_config_select_runpath(model_config, "KEYX"));
    test_assert_string_equal("RunPath%d",
                             model_config_get_runpath_as_char(model_config));

    model_config_set_runpath(model_config, "PATH%d");
    test_assert_string_equal("PATH%d",
                             model_config_get_runpath_as_char(model_config));
    test_assert_true(model_config_select_runpath(model_config, "KEY2"));
    test_assert_string_equal("2-RunPath%d",
                             model_config_get_runpath_as_char(model_config));
    test_assert_true(model_config_select_runpath(model_config, "KEY"));
    test_assert_string_equal("PATH%d",
                             model_config_get_runpath_as_char(model_config));

    test_assert_false(model_config_runpath_requires_iter(model_config));
    model_config_set_runpath(model_config, "iens%d/iter%d");
    test_assert_true(model_config_runpath_requires_iter(model_config));

    model_config_free(model_config);
}

void test_data_root() {
    model_config_type *model_config = model_config_alloc_empty();
    test_assert_false(model_config_data_root_is_set(model_config));
    model_config_set_data_root(model_config, "root");
    test_assert_true(model_config_data_root_is_set(model_config));
    model_config_free(model_config);
}

void test_export_file() {
    model_config_type *model_config = model_config_alloc_empty();

    model_config_free(model_config);
}

int main(int argc, char **argv) {
    test_create();
    test_runpath();
    test_data_root();
    test_export_file();
    exit(0);
}
