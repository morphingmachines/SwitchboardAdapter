"""Tests for run_cosim, run for real against the Loopback fixture RTL.

Needs Verilator and switchboard installed. The first test does a cold build,
so expect a few minutes for the whole file.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import sim_build  # noqa: E402

FIXTURES = Path(__file__).resolve().parent / "fixtures"
RTL_DIR = FIXTURES / "loopback_rtl"
INTERFACES = {
    "io_in": dict(type="sb", dw=sim_build.SB_DATA_WIDTH, uri="in_port.q", direction="input"),
    "io_out": dict(type="sb", dw=sim_build.SB_DATA_WIDTH, uri="out_port.q", direction="output"),
}


@pytest.fixture(scope="module")
def project_dir(tmp_path_factory):
    """One build cache shared by every test in this file, so later tests
    exercise reuse and rebuild against the earlier builds."""
    return tmp_path_factory.mktemp("project")


@pytest.fixture
def run(project_dir, tmp_path, monkeypatch):
    """run(mode, **kw) -> runs the fixture client in `mode` from a fresh cwd."""
    monkeypatch.chdir(tmp_path)

    def _run(mode, **kw):
        client = [sys.executable, FIXTURES / "loopback_client.py", mode]
        sim_build.run_cosim(project_dir, RTL_DIR, "Loopback", INTERFACES, client, **kw)

    return _run


def test_cold_build_then_pass(run, project_dir, capfd):
    run("pass")
    out = capfd.readouterr().out
    assert "rtl build: building (no cached build)" in out
    assert "PASS!" in out
    assert (project_dir / "rtl_build" / "build-stamp.json").exists()


def test_second_run_reuses_cache(run, capfd):
    run("pass")
    out = capfd.readouterr().out
    assert "rtl build: reusing" in out
    assert "PASS!" in out


def test_nothing_lands_in_cwd_but_queues(run, tmp_path):
    run("pass")
    assert not (tmp_path / "rtl_build").exists()
    assert not (tmp_path / "build").exists()


def test_trace_change_rebuilds_and_writes_waveform(run, tmp_path, capfd):
    stale = tmp_path / "testbench.fst"
    stale.write_text("stale")
    run("pass", trace=True)
    out = capfd.readouterr().out
    assert "rtl build: building (trace changed (False -> True))" in out
    assert f"waveform: {stale}" in out
    assert stale.read_bytes() != b"stale"


def test_regenerated_rtl_rebuilds(run, capfd):
    filelist = RTL_DIR / "filelist.f"
    filelist.touch()
    run("pass", trace=True)
    assert "rtl build: building (RTL regenerated" in capfd.readouterr().out


def test_failing_client_raises(run):
    with pytest.raises(sim_build.CosimError) as e:
        run("fail", trace=True)
    assert e.value.stage == "client"
    assert e.value.returncode == 1


def test_dead_simulator_raises_instead_of_hanging(run):
    with pytest.raises(sim_build.CosimError) as e:
        run("kill", trace=True)
    assert e.value.stage == "sim"
    assert e.value.returncode != 0
