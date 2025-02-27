import os.path
import random

from ecl.util.test import TestRun, path_exists
from libres_utils import ResTest


class RunTest(ResTest):
    def setUp(self):
        # Slightly weird - tests need existing file,
        # but it can be empty ....
        self.testConfig = f"/tmp/config-{random.randint(100000, 999999):06d}"
        with open(self.testConfig, "w"):
            pass

    def test_init(self):
        with self.assertRaises(IOError):
            TestRun("Does/notExist")

        tr = TestRun(self.testConfig)
        self.assertEqual(tr.config_file, os.path.split(self.testConfig)[1])
        self.assertEqual(tr.ert_version, "stable")

    def test_args(self):
        tr = TestRun(self.testConfig, args=["-v", "latest"])
        self.assertEqual(tr.ert_version, "latest")

    def test_cmd(self):
        tr = TestRun(self.testConfig)
        self.assertEqual(tr.ert_cmd, TestRun.default_ert_cmd)

        tr.ert_cmd = "/tmp/test"
        self.assertEqual("/tmp/test", tr.ert_cmd)

    def test_args2(self):
        tr = TestRun(self.testConfig, args=["arg1", "arg2", "-v", "latest"])
        self.assertEqual(tr.get_args(), ["arg1", "arg2"])
        self.assertEqual(tr.ert_version, "latest")

    def test_workflows(self):
        tr = TestRun(self.testConfig)
        self.assertEqual(tr.get_workflows(), [])

        tr.add_workflow("wf1")
        tr.add_workflow("wf2")
        self.assertEqual(tr.get_workflows(), ["wf1", "wf2"])

    def test_run_no_workflow(self):
        tr = TestRun(self.testConfig)
        with self.assertRaises(Exception):
            tr.run()

    def test_runpath(self):
        tr = TestRun(self.testConfig, "Name")
        self.assertEqual(TestRun.default_path_prefix, tr.path_prefix)

    def test_check(self):
        tr = TestRun(self.testConfig, "Name")
        tr.add_check(path_exists, "some/file")

        with self.assertRaises(Exception):
            tr.add_check(25, "arg")

        with self.assertRaises(Exception):
            # pylint: disable=undefined-variable
            tr.add_check(func_does_not_exist, "arg")  # noqa
