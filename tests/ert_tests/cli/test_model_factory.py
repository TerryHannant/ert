from argparse import Namespace

from ert_utils import ErtTest

from ert.ensemble_evaluator.activerange import ActiveRange
from ert_shared.cli import model_factory
from ert_shared.libres_facade import LibresFacade
from ert_shared.models.ensemble_experiment import EnsembleExperiment
from ert_shared.models.ensemble_smoother import EnsembleSmoother
from ert_shared.models.multiple_data_assimilation import MultipleDataAssimilation
from ert_shared.models.iterated_ensemble_smoother import IteratedEnsembleSmoother
from ert_shared.models.single_test_run import SingleTestRun
from res.test import ErtTestContext


class ModelFactoryTest(ErtTest):
    def test_custom_target_case_name(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_custom_target_case_name", config_file) as work_area:
            ert = work_area.getErt()
            facade = LibresFacade(ert)
            custom_name = "test"
            args = Namespace(target_case=custom_name)
            res = model_factory._target_case_name(
                ert, args, facade.get_current_case_name()
            )
            self.assertEqual(custom_name, res)

    def test_default_target_case_name(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_default_target_case_name", config_file) as work_area:
            ert = work_area.getErt()
            facade = LibresFacade(ert)

            args = Namespace(target_case=None)
            res = model_factory._target_case_name(
                ert, args, facade.get_current_case_name()
            )
            self.assertEqual("default_smoother_update", res)

    def test_default_target_case_name_format_mode(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext(
            "test_default_target_case_name_format_mode", config_file
        ) as work_area:
            ert = work_area.getErt()
            facade = LibresFacade(ert)

            args = Namespace(target_case=None)
            res = model_factory._target_case_name(
                ert, args, facade.get_current_case_name(), format_mode=True
            )
            self.assertEqual("default_%d", res)

    def test_default_realizations(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_default_realizations", config_file) as work_area:
            ert = work_area.getErt()

            args = Namespace(realizations=None)
            ensemble_size = ert.getEnsembleSize()
            res = model_factory._realizations(args, ensemble_size)
            self.assertEqual([True] * 100, res)

    def test_init_iteration_number(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_init_iteration_number", config_file) as work_area:
            ert = work_area.getErt()
            args = Namespace(iter_num=10, realizations=None)
            model = model_factory._setup_ensemble_experiment(
                ert, args, ert.getEnsembleSize()
            )
            run_context = model.create_context()
            self.assertEqual(model._simulation_arguments["iter_num"], 10)
            self.assertEqual(run_context.get_iter(), 10)

    def test_custom_realizations(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_custom_realizations", config_file) as work_area:
            ert = work_area.getErt()

            args = Namespace(realizations="0-4,7,8")
            ensemble_size = ert.getEnsembleSize()
            res = model_factory._realizations(args, ensemble_size)
            self.assertEqual(
                ActiveRange(rangestring="0-4,7,8", length=ensemble_size).mask, res
            )

    def test_setup_single_test_run(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_single_test_run", config_file) as work_area:
            ert = work_area.getErt()

            model = model_factory._setup_single_test_run(ert)
            self.assertTrue(isinstance(model, SingleTestRun))
            self.assertEqual(1, len(model._simulation_arguments.keys()))
            self.assertTrue("active_realizations" in model._simulation_arguments)
            model.create_context()

    def test_setup_ensemble_experiment(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_single_test_run", config_file) as work_area:
            ert = work_area.getErt()

            model = model_factory._setup_single_test_run(ert)
            self.assertTrue(isinstance(model, EnsembleExperiment))
            self.assertEqual(1, len(model._simulation_arguments.keys()))
            self.assertTrue("active_realizations" in model._simulation_arguments)
            model.create_context()

    def test_setup_ensemble_smoother(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_single_test_run", config_file) as work_area:
            ert = work_area.getErt()
            facade = LibresFacade(ert)
            args = Namespace(realizations="0-4,7,8", target_case="test_case")

            model = model_factory._setup_ensemble_smoother(
                ert,
                args,
                facade.get_ensemble_size(),
                facade.get_current_case_name(),
            )
            self.assertTrue(isinstance(model, EnsembleSmoother))
            self.assertEqual(3, len(model._simulation_arguments.keys()))
            self.assertTrue("active_realizations" in model._simulation_arguments)
            self.assertTrue("target_case" in model._simulation_arguments)
            self.assertTrue("analysis_module" in model._simulation_arguments)
            model.create_context()

    def test_setup_multiple_data_assimilation(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext("test_single_test_run", config_file) as work_area:
            ert = work_area.getErt()
            facade = LibresFacade(ert)
            args = Namespace(
                realizations="0-4,7,8",
                weights="6,4,2",
                target_case="test_case_%d",
                start_iteration="0",
            )

            model = model_factory._setup_multiple_data_assimilation(
                ert,
                args,
                facade.get_ensemble_size(),
                facade.get_current_case_name(),
            )
            self.assertTrue(isinstance(model, MultipleDataAssimilation))
            self.assertEqual(5, len(model._simulation_arguments.keys()))
            self.assertTrue("active_realizations" in model._simulation_arguments)
            self.assertTrue("target_case" in model._simulation_arguments)
            self.assertTrue("analysis_module" in model._simulation_arguments)
            self.assertTrue("weights" in model._simulation_arguments)
            self.assertTrue("start_iteration" in model._simulation_arguments)
            model.create_context(0)

    def test_setup_iterative_ensemble_smoother(self):
        config_file = self.createTestPath("local/poly_example/poly.ert")
        with ErtTestContext(
            "_setup_iterative_ensemble_smoother", config_file
        ) as work_area:
            ert = work_area.getErt()
            facade = LibresFacade(ert)
            args = Namespace(
                realizations="0-4,7,8",
                target_case="test_case_%d",
                num_iterations="10",
            )

            model = model_factory._setup_iterative_ensemble_smoother(
                ert,
                args,
                facade.get_ensemble_size(),
                facade.get_current_case_name(),
            )
            self.assertTrue(isinstance(model, IteratedEnsembleSmoother))
            self.assertEqual(4, len(model._simulation_arguments.keys()))
            self.assertTrue("active_realizations" in model._simulation_arguments)
            self.assertTrue("target_case" in model._simulation_arguments)
            self.assertTrue("analysis_module" in model._simulation_arguments)
            self.assertTrue("num_iterations" in model._simulation_arguments)
            self.assertTrue(facade.get_number_of_iterations() == 10)
            model.create_context(0)
