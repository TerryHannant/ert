#  Copyright (C) 2017  Equinor ASA, Norway.
#
#  The file 'test_site_config.py' is part of ERT - Ensemble based Reservoir Tool.
#
#  ERT is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  ERT is distributed in the hope that it will be useful, but WITHOUT ANY
#  WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.
#
#  See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
#  for more details.

import os

from ecl.util.test import TestAreaContext
from libres_utils import ResTest, tmpdir
from pytest import MonkeyPatch

from res.enkf import ConfigKeys, ResConfig, SiteConfig


class SiteConfigTest(ResTest):
    def setUp(self):
        self.case_directory = self.createTestPath("local/simple_config/")
        self.snake_case_directory = self.createTestPath("local/snake_oil/")

        self.monkeypatch = MonkeyPatch()

    def tearDown(self):
        self.monkeypatch.undo()

    def test_invalid_user_config(self):
        with TestAreaContext("void land"):
            with self.assertRaises(IOError):
                SiteConfig("this/is/not/a/file")

    def test_init(self):
        with TestAreaContext("site_config_init_test") as work_area:
            work_area.copy_directory(self.case_directory)
            config_file = "simple_config/minimum_config"
            site_config = SiteConfig(user_config_file=config_file)
            self.assertIsNotNone(site_config)

    def test_constructors(self):
        with TestAreaContext("site_config_constructor_test") as work_area:
            work_area.copy_directory(self.snake_case_directory)
            config_file = "snake_oil/snake_oil.ert"

            ERT_SITE_CONFIG = SiteConfig.getLocation()
            ERT_SHARE_PATH = os.path.dirname(ERT_SITE_CONFIG)
            snake_config_dict = {
                ConfigKeys.INSTALL_JOB: [
                    {
                        ConfigKeys.NAME: "SNAKE_OIL_SIMULATOR",
                        ConfigKeys.PATH: os.getcwd()
                        + "/snake_oil/jobs/SNAKE_OIL_SIMULATOR",
                    },
                    {
                        ConfigKeys.NAME: "SNAKE_OIL_NPV",
                        ConfigKeys.PATH: os.getcwd() + "/snake_oil/jobs/SNAKE_OIL_NPV",
                    },
                    {
                        ConfigKeys.NAME: "SNAKE_OIL_DIFF",
                        ConfigKeys.PATH: os.getcwd() + "/snake_oil/jobs/SNAKE_OIL_DIFF",
                    },
                ],
                ConfigKeys.INSTALL_JOB_DIRECTORY: [
                    ERT_SHARE_PATH + "/forward-models/res",
                    ERT_SHARE_PATH + "/forward-models/shell",
                    ERT_SHARE_PATH + "/forward-models/templating",
                    ERT_SHARE_PATH + "/forward-models/old_style",
                ],
                ConfigKeys.SETENV: [
                    {ConfigKeys.NAME: "SILLY_VAR", ConfigKeys.VALUE: "silly-value"},
                    {
                        ConfigKeys.NAME: "OPTIONAL_VAR",
                        ConfigKeys.VALUE: "optional-value",
                    },
                ],
                ConfigKeys.LICENSE_PATH: "some/random/path",
                ConfigKeys.UMASK: 18,
            }

            site_config_user_file = SiteConfig(user_config_file=config_file)
            site_config_dict = SiteConfig(config_dict=snake_config_dict)
            self.assertEqual(site_config_dict, site_config_user_file)

            with self.assertRaises(ValueError):
                SiteConfig(user_config_file=config_file, config_dict=snake_config_dict)

    @tmpdir()
    def test_site_config_hook_workflow(self):
        site_config_filename = "test_site_config"
        test_config_filename = "test_config"
        site_config_content = """
QUEUE_SYSTEM LOCAL
LOAD_WORKFLOW_JOB ECHO_WORKFLOW_JOB
LOAD_WORKFLOW ECHO_WORKFLOW
HOOK_WORKFLOW ECHO_WORKFLOW PRE_SIMULATION
"""

        with open(site_config_filename, "w") as fh:
            fh.write(site_config_content)

        with open(test_config_filename, "w") as fh:
            fh.write("NUM_REALIZATIONS 1\n")

        with open("ECHO_WORKFLOW_JOB", "w") as fh:
            fh.write(
                """INTERNAL False
EXECUTABLE echo
MIN_ARG 1
"""
            )

        with open("ECHO_WORKFLOW", "w") as fh:
            fh.write("ECHO_WORKFLOW_JOB hello")

        self.monkeypatch.setenv("ERT_SITE_CONFIG", site_config_filename)

        res_config = ResConfig(user_config_file=test_config_filename)
        self.assertTrue(len(res_config.hook_manager) == 1)
        self.assertEqual(
            res_config.hook_manager[0].getWorkflow().src_file,
            os.path.join(os.getcwd(), "ECHO_WORKFLOW"),
        )
