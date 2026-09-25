"""Co-sim run: build an SbDut-autowrapped Verilator simulation (or reuse a
cached one) and run it against a host-side client process.

Any project that autowraps its RTL with SbDut and drives it from a host binary
(see sb_sim/ for the C++ side of that consumer contract --
ClientTLAgent/ManagerTLAgent) goes through the same sequence: filelist,
Verilator flags, SbDut interface wiring, cached-build reuse, simulator start,
client start, and waiting for both. run_cosim() owns that whole sequence,
including the ordering rules and the SbDut bug workaround, so callers pass only
what differs per project: the RTL, its ports, and the client command line.

Public interface: run_cosim, CosimError, make_interfaces, SB_DATA_WIDTH.
See README.md in this directory for usage.
"""

import fcntl
import json
import os
import subprocess
import time
from contextlib import contextmanager
from pathlib import Path

from siliconcompiler import Design
from switchboard import SbDut, binary_run

# Must match Bundles.scala's DataWidth -- the packed TileLink-over-Switchboard
# payload width. Duplicated here because Python can't import a Chisel constant.
SB_DATA_WIDTH = 416

_HDL_EXTS = {".v", ".sv", ".vh", ".svh"}
_BUILD_DIR = "rtl_build"
_OUT_DIR = "build"
_STAMP = "build-stamp.json"
_WAVEFORM = "testbench.fst"
_POLL_S = 0.1
_SIM_EXIT_GRACE_S = 1.0
_STOP_TIMEOUT_S = 10
_SPLIT_SIZE = 20000
_OS_CORES_RESERVED = 4
_MAX_BUILD_THREADS = 32
_MAX_SIM_THREADS = 8
_VERILATOR_WARNINGS_OFF = [
    "WIDTHEXPAND",
    "CASEINCOMPLETE",
    "WIDTHTRUNC",
    "TIMESCALEMOD",
    "PINMISSING",
]
_VERILATOR_CFLAGS_COMMON = [
    "-CFLAGS",
    "-march=native",
    "-CFLAGS",
    "-mcmodel=large",
]
_VERILATOR_CFLAGS_PROD = _VERILATOR_CFLAGS_COMMON + [
    "-CFLAGS",
    "-O3",
    "--output-split",
    str(_SPLIT_SIZE),
    "--output-split-cfuncs",
    str(_SPLIT_SIZE),
]
_VERILATOR_CFLAGS_DEV = _VERILATOR_CFLAGS_COMMON + [
    "-CFLAGS",
    "-O0",
]


class CosimError(RuntimeError):
    """A co-sim run failed.

    stage is "build", "sim" or "client" -- which part failed. returncode is
    that process's exit code (None for a build failure)."""

    def __init__(self, stage: str, returncode, message: str):
        super().__init__(message)
        self.stage = stage
        self.returncode = returncode


def make_interfaces(n_clients=1, n_managers=0):
    """Builds the SbDut interfaces dict for a design with n_clients TileLink
    client ports and n_managers TileLink manager ports.

    n_clients: number of ports where the RTL is the TileLink manager (it
    receives A-channel requests and sends D-channel responses). Each becomes
    an io_client_N_a (input to the RTL) / io_client_N_d (output from the RTL)
    pair of Switchboard queues, meant to be driven from the host side by one
    ClientTLAgent (sb_sim) per port.

    n_managers: number of ports where the RTL is the TileLink client (it
    issues A-channel requests and receives D-channel responses). Each becomes
    an io_manager_N_a (output from the RTL) / io_manager_N_d (input to the
    RTL) pair, meant to be served from the host side by one ManagerTLAgent
    (sb_sim) per port.
    """
    interfaces = {}
    for i in range(n_clients):
        interfaces[f"io_client_{i}_a"] = dict(
            type="sb", dw=SB_DATA_WIDTH, uri=f"client_{i}_a.q", direction="input"
        )
        interfaces[f"io_client_{i}_d"] = dict(
            type="sb", dw=SB_DATA_WIDTH, uri=f"client_{i}_d.q", direction="output"
        )
    for i in range(n_managers):
        interfaces[f"io_manager_{i}_a"] = dict(
            type="sb", dw=SB_DATA_WIDTH, uri=f"manager_{i}_a.q", direction="output"
        )
        interfaces[f"io_manager_{i}_d"] = dict(
            type="sb", dw=SB_DATA_WIDTH, uri=f"manager_{i}_d.q", direction="input"
        )
    return interfaces


def run_cosim(
    project_dir,
    rtl_dir,
    top: str,
    interfaces: dict,
    client: list,
    trace: bool = False,
    rebuild: bool = False,
    dev_mode: bool = False,
) -> None:
    """Build (or reuse) the simulation of `top`, then run it against `client`.

    project_dir: the calling project's own directory. The Verilator build is
      cached in project_dir/rtl_build/; the HDL filelist and the build lock live
      in project_dir/build/. Nothing depends on the current directory except
      the Switchboard queue files and the waveform, which land there.
    rtl_dir: directory holding the generated RTL and its filelist.f (paths in
      it are relative to rtl_dir).
    top: the RTL top module name.
    interfaces: SbDut interfaces dict -- make_interfaces() for a TileLink
      client/manager design, or your own (e.g. raw Switchboard ports).
    client: argv of the host-side client, e.g. [path/to/TestDriver, "x.json"].
      Started only after the simulator has created its queues.
    trace: dump an FST waveform to ./testbench.fst (an old one is deleted
      first); its path is printed after the run.
    rebuild: rebuild even if the cached build matches.
    dev_mode: X-propagation and assertions on (no --x-assign fast / --noassert).

    The cached build is reused only when it was built with the same trace,
    dev_mode, top and rtl_dir, from a filelist.f no newer than the last build;
    otherwise it is rebuilt and the reason printed. Hand-edits to an RTL file
    without regenerating filelist.f are not detected -- pass rebuild=True.

    Concurrent runs from different directories are safe: the build step is
    serialised by a lock. Two runs from the same directory are not (they share
    queue files).

    Returns None when the client exits 0. Raises CosimError when the build
    fails, the client exits non-zero, or the simulator exits before the client
    has finished. The simulator is stopped once the client is done.
    """
    project_dir = Path(project_dir).resolve()
    rtl_dir = Path(rtl_dir).resolve()
    build_dir = project_dir / _BUILD_DIR
    out_dir = project_dir / _OUT_DIR

    # Use ccache to accelerate repeated Verilator compiles (falls back to plain g++ if absent).
    os.environ.setdefault("CXX", "ccache g++")
    os.environ.setdefault("CC", "ccache gcc")

    stamp = {
        "top": top,
        "rtl_dir": str(rtl_dir),
        "filelist_mtime_ns": (rtl_dir / "filelist.f").stat().st_mtime_ns,
        "trace": trace,
        "dev_mode": dev_mode,
    }

    with _build_lock(out_dir / ".build.lock"):
        abs_filelist, non_hdl_srcs = _write_filelist(rtl_dir, out_dir)
        dut = _make_dut(top, interfaces, trace, build_dir)
        _configure_verilator(dut, trace, dev_mode, abs_filelist, non_hdl_srcs)
        _build_or_reuse(dut, build_dir, stamp, rebuild)

    dut.remove_queues_on_exit()

    # The sim inherits the cwd, so the waveform lands in ./testbench.fst. Delete
    # any stale one first so its presence afterwards means this run wrote it.
    waveform = Path(_WAVEFORM).resolve()
    if trace:
        waveform.unlink(missing_ok=True)

    # simulate() must run before the client starts: it deletes and recreates
    # every queue file. A client that opened a queue first would keep mapping
    # the deleted file and hang talking to a dead queue.
    sim = dut.simulate()
    client_proc = binary_run(client[0], args=[str(a) for a in client[1:]])
    _wait(sim, client_proc)

    if trace:
        if waveform.exists():
            print(f"waveform: {waveform}")
        else:
            print(f"warning: trace enabled but no waveform at {waveform}")


@contextmanager
def _build_lock(lock_path: Path):
    """Serialises filelist generation + Verilator build across concurrent runs
    sharing one project's build cache. The kernel drops the flock when the
    process exits, so a crash never leaves a stale lock."""
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with open(lock_path, "w") as f:
        try:
            fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print(f"waiting for build lock {lock_path}...", flush=True)
            fcntl.flock(f, fcntl.LOCK_EX)
        try:
            yield
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


def _write_filelist(rtl_dir: Path, out_dir: Path):
    """Writes an HDL-only absolute-path filelist for verilator -f.

    Returns (filelist_path, non_hdl_files) where non_hdl_files are C/C++ sources."""
    abs_filelist = out_dir / "abs_filelist.f"
    abs_filelist.parent.mkdir(parents=True, exist_ok=True)
    non_hdl = []
    with open(rtl_dir / "filelist.f") as f:
        lines = f.readlines()
    with open(abs_filelist, "w") as out:
        for line in lines:
            name = line.strip()
            if not name:
                continue
            abs_path = rtl_dir / name
            if abs_path.suffix in _HDL_EXTS:
                out.write(str(abs_path) + "\n")
            else:
                non_hdl.append(str(abs_path))
    return str(abs_filelist), non_hdl


def _make_dut(top: str, interfaces: dict, trace: bool, build_dir: Path) -> SbDut:
    # SbDut.build() requires design.has_fileset("verilator"); set_topmodule
    # creates it. The actual sources are passed via -f filelist to avoid long
    # command lines.
    design = Design(top)
    design.set_topmodule(top, fileset="verilator")
    return SbDut(
        design,
        autowrap=True,
        cmdline=False,
        trace=trace,
        trace_type="fst",
        interfaces=interfaces,
        resets=[dict(name="reset", delay=0)],
        clocks=[dict(name="clock")],
        builddir=str(build_dir),
    )


def _vc_add(dut: SbDut, key: str, value) -> None:
    dut.add("tool", "verilator", "task", "compile", key, value)


def _thread_counts():
    n_cpus = os.cpu_count() or 1
    n_build_threads = max(1, min(_MAX_BUILD_THREADS, n_cpus - _OS_CORES_RESERVED))
    n_sim_threads = max(1, min(_MAX_SIM_THREADS, n_cpus // 2))
    return n_build_threads, n_sim_threads


def _configure_verilator(
    dut: SbDut, trace: bool, dev_mode: bool, abs_filelist: str, non_hdl_srcs: list
) -> None:
    n_build_threads, n_sim_threads = _thread_counts()
    _vc_add(dut, "warningoff", _VERILATOR_WARNINGS_OFF)
    _vc_add(dut, "option", ["--threads", str(n_sim_threads)])
    _vc_add(dut, "option", ["--threads-dpi", "all"])

    if not dev_mode:
        _vc_add(dut, "option", _VERILATOR_CFLAGS_PROD)
        _vc_add(dut, "option", ["-O3"])
        _vc_add(dut, "option", ["--x-assign", "fast"])
        _vc_add(dut, "option", ["--noassert"])
    else:
        _vc_add(dut, "option", _VERILATOR_CFLAGS_DEV)
        _vc_add(dut, "option", ["-O0"])

    if trace:
        # SbDut's own trace=True already makes siliconcompiler's verilator-compile
        # task append --trace-fst (for trace_type="fst"); don't also pass --trace
        # here, it conflicts with the auto-injected flag.
        _vc_add(dut, "option", ["--trace-underscore"])
        _vc_add(dut, "option", ["-DSB_TRACE"])
        _vc_add(dut, "option", ["-DSB_TRACE_FST"])

    _vc_add(dut, "option", ["-f", abs_filelist])
    for src in non_hdl_srcs:
        _vc_add(dut, "option", [src])

    dut.set("tool", "verilator", "task", "compile", "threads", n_build_threads)
    dut.set("tool", "verilator", "task", "compile", "var", "mode", "cc")


def _find_sim(build_dir: Path):
    """Path to the compiled verilator binary under build_dir, or None.

    SbDut.build(fast=True) can't be used for this: with autowrap=True its fast
    path calls find_sim() before set_design(AutowrapDesign), so it searches the
    original design's build directory, which never contains the binary."""
    hits = list(build_dir.glob("**/*.vexe"))
    return str(hits[0]) if hits else None


def _stale_reason(build_dir: Path, stamp: dict):
    """Why the cached build can't be reused, or None if it can."""
    if _find_sim(build_dir) is None:
        return "no cached build"
    try:
        old = json.loads((build_dir / _STAMP).read_text())
    except (OSError, ValueError):
        return "no build stamp"
    for key in ("top", "rtl_dir", "trace", "dev_mode"):
        if old.get(key) != stamp[key]:
            return f"{key} changed ({old.get(key)} -> {stamp[key]})"
    if stamp["filelist_mtime_ns"] > old.get("filelist_mtime_ns", 0):
        return "RTL regenerated (filelist.f newer than build)"
    return None


def _build_or_reuse(dut: SbDut, build_dir: Path, stamp: dict, rebuild: bool) -> None:
    reason = "rebuild requested" if rebuild else _stale_reason(build_dir, stamp)
    if reason is None:
        sim = _find_sim(build_dir)
        print(f"rtl build: reusing {sim}", flush=True)
    else:
        print(f"rtl build: building ({reason})", flush=True)
        (build_dir / _STAMP).unlink(missing_ok=True)
        try:
            dut.build(fast=False)
        except Exception as e:
            raise CosimError("build", None, f"Verilator build failed: {e}") from e
        sim = _find_sim(build_dir)
        if sim is None:
            raise CosimError("build", None, f"build finished but no binary under {build_dir}")
        (build_dir / _STAMP).write_text(json.dumps(stamp, indent=2) + "\n")
    # simulate() calls build() again internally; hand it the binary instead.
    dut.build = lambda *_, **_kw: sim


def _stop(proc: subprocess.Popen) -> None:
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(_STOP_TIMEOUT_S)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()


def _wait(sim: subprocess.Popen, client: subprocess.Popen) -> None:
    """Waits for the client, watching the simulator. The simulator never exits
    on its own while the client still needs it, so a simulator exit first is a
    failure -- without this check the client would block on its queues forever."""
    while True:
        client_rc = client.poll()
        if client_rc is not None:
            _stop(sim)
            if client_rc != 0:
                raise CosimError("client", client_rc, f"client exited with code {client_rc}")
            return
        sim_rc = sim.poll()
        if sim_rc is not None:
            # The client may be finishing at the same moment; give it a moment.
            try:
                client_rc = client.wait(_SIM_EXIT_GRACE_S)
            except subprocess.TimeoutExpired:
                _stop(client)
                raise CosimError(
                    "sim", sim_rc, f"simulator exited with code {sim_rc} before the client finished"
                ) from None
            continue
        time.sleep(_POLL_S)
