/*
   Copyright (C) 2013  Equinor ASA, Norway.

   The file 'config_root_path.c' is part of ERT - Ensemble based Reservoir Tool.

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

#include <ert/util/test_util.hpp>
#include <ert/util/util.hpp>

#include <ert/config/config_parser.hpp>

int main(int argc, char **argv) {
    char *cwd = util_alloc_cwd();

    {
        config_root_path_type *root_path = config_root_path_alloc(NULL);

        if (!test_check_string_equal(config_root_path_get_abs_path(root_path),
                                     cwd))
            test_error_exit("abs:path:%s   expeceted:%s \n",
                            config_root_path_get_abs_path(root_path), cwd);

        if (!test_check_string_equal(config_root_path_get_input_path(root_path),
                                     NULL))
            test_error_exit("input:path:%s   expeceted:%s \n",
                            config_root_path_get_input_path(root_path), NULL);

        if (!test_check_string_equal(config_root_path_get_rel_path(root_path),
                                     NULL))
            test_error_exit("rel:path:%s   expeceted:%s \n",
                            config_root_path_get_rel_path(root_path), NULL);

        config_root_path_free(root_path);
    }

    {
        config_root_path_type *root_path =
            config_root_path_alloc("/does/not/exist");
        if (root_path != NULL)
            test_error_exit(
                "Created root_path instance for not-existing input \n");
    }

    {
        const char *input_path = argv[1];
        char *cwd = util_alloc_cwd();
        char *rel_path = util_alloc_rel_path(cwd, input_path);

        config_root_path_type *root_path1 = config_root_path_alloc(input_path);
        config_root_path_type *root_path2 = config_root_path_alloc(rel_path);

        if (!test_check_string_equal(config_root_path_get_rel_path(root_path1),
                                     config_root_path_get_rel_path(root_path2)))
            test_error_exit("Rel: %s != %s \n",
                            config_root_path_get_rel_path(root_path1),
                            config_root_path_get_rel_path(root_path2));

        if (!test_check_string_equal(config_root_path_get_abs_path(root_path1),
                                     config_root_path_get_abs_path(root_path2)))
            test_error_exit("Abs: %s != %s \n",
                            config_root_path_get_abs_path(root_path1),
                            config_root_path_get_abs_path(root_path2));

        config_root_path_free(root_path1);
        config_root_path_free(root_path2);
    }

    exit(0);
}
