import fileinput
import logging
import os
import shutil
from datetime import datetime
from pathlib import Path

import pytest
from ecl.summary import EclSum
from ecl.util.util import BoolVector

from ert_shared.libres_facade import LibresFacade
from res.enkf import ResConfig, EnKFMain


@pytest.fixture
def copy_data(tmp_path, source_root):
    os.chdir(tmp_path)
    shutil.copytree(
        os.path.join(source_root, "test-data", "local/snake_oil"), "test_data"
    )
    os.chdir("test_data")


def run_simulator(time_step_count, start_date):
    """@rtype: EclSum"""
    ecl_sum = EclSum.writer("SNAKE_OIL_FIELD", start_date, 10, 10, 10)

    ecl_sum.addVariable("FOPR", unit="SM3/DAY")
    ecl_sum.addVariable("FOPRH", unit="SM3/DAY")

    ecl_sum.addVariable("WOPR", wgname="OP1", unit="SM3/DAY")
    ecl_sum.addVariable("WOPRH", wgname="OP1", unit="SM3/DAY")

    mini_step_count = 10
    for report_step in range(time_step_count):
        for mini_step in range(mini_step_count):
            t_step = ecl_sum.addTStep(
                report_step + 1, sim_days=report_step * mini_step_count + mini_step
            )
            t_step["FOPR"] = 1
            t_step["WOPR:OP1"] = 2
            t_step["FOPRH"] = 3
            t_step["WOPRH:OP1"] = 4

    return ecl_sum


def test_load_inconsistent_time_map_summary(copy_data, caplog):
    """
    Checking that we dont util_abort, we fail the forward model instead
    """
    cwd = os.getcwd()

    # Get rid of GEN_DATA as we are only interested in SUMMARY
    with fileinput.input("snake_oil.ert", inplace=True) as fin:
        for line in fin:
            if line.startswith("GEN_DATA"):
                continue
            print(line, end="")

    res_config = ResConfig("snake_oil.ert")
    ert = EnKFMain(res_config)
    facade = LibresFacade(ert)
    realisation_number = 0
    assert (
        facade.get_current_fs().getStateMap()[realisation_number].name
        == "STATE_HAS_DATA"
    )  # Check prior state

    # Create a result that is incompatible with the refcase
    run_path = Path("storage") / "snake_oil" / "runpath" / "realization-0" / "iter-0"
    os.chdir(run_path)
    ecl_sum = run_simulator(1, datetime(2000, 1, 1))
    ecl_sum.fwrite()
    os.chdir(cwd)

    realizations = BoolVector(
        default_value=False, initial_size=facade.get_ensemble_size()
    )
    realizations[realisation_number] = True
    with caplog.at_level(logging.ERROR):
        loaded = facade.load_from_forward_model("default_0", realizations, 0)
    assert (
        "Realization: 0, load failure: 2 inconsistencies in time_map, first: "
        "Time mismatch for step: 0, response time: 2000-01-01, reference case: "
        "2010-01-01, last: Time mismatch for step: 1, response time: 2000-01-10, "
        "reference case: 2010-01-10"
    ) in caplog.messages
    assert loaded == 0
    assert (
        facade.get_current_fs().getStateMap()[realisation_number].name
        == "STATE_LOAD_FAILURE"
    )  # Check that status is as expected


def test_load_forward_model(copy_data):
    """
    Checking that we are able to load from forward model
    """
    # Get rid of GEN_DATA it causes a failure to load from forward model
    with fileinput.input("snake_oil.ert", inplace=True) as fin:
        for line in fin:
            if line.startswith("GEN_DATA"):
                continue
            print(line, end="")

    res_config = ResConfig("snake_oil.ert")
    ert = EnKFMain(res_config)
    facade = LibresFacade(ert)
    realisation_number = 0

    realizations = BoolVector(
        default_value=False, initial_size=facade.get_ensemble_size()
    )
    realizations[realisation_number] = True
    loaded = facade.load_from_forward_model("default_0", realizations, 0)
    assert loaded == 1
    assert (
        facade.get_current_fs().getStateMap()[realisation_number].name
        == "STATE_HAS_DATA"
    )  # Check that status is as expected
