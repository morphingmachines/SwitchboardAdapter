"""Shared Verilator/SbDut build logic for projects that consume SwitchboardAdapter.

Any project that autowraps its RTL with SbDut (see sb_sim/ for the C++ side of
that same consumer contract -- ClientTLAgent/ManagerTLAgent) hits the same
Verilator flag assembly, SbDut interface wiring, and a documented SbDut bug
workaround. This module is that shared, tested build logic; each consumer
keeps its own build.py for CLI parsing and how it invokes its own testbench
binary, which differ per project.

See README.md in this directory for usage.
"""

import os
from pathlib import Path

from siliconcompiler import Design
from switchboard import SbDut

HDL_EXTS = {".v", ".sv", ".vh", ".svh"}
# Must match Bundles.scala's DataWidth -- the packed TileLink-over-Switchboard
# payload width. Duplicated here because Python can't import a Chisel constant.
SB_DATA_WIDTH = 416
BUILD_DIR = "rtl_build"
SPLIT_SIZE = 20000
OS_CORES_RESERVED = 4
MAX_BUILD_THREADS = 32
MAX_SIM_THREADS = 8
VERILATOR_WARNINGS_OFF = [
    "WIDTHEXPAND",
    "CASEINCOMPLETE",
    "WIDTHTRUNC",
    "TIMESCALEMOD",
    "PINMISSING",
]
VERILATOR_CFLAGS_COMMON = [
    "-CFLAGS",
    "-march=native",
    "-CFLAGS",
    "-mcmodel=large",
]
VERILATOR_CFLAGS_PROD = VERILATOR_CFLAGS_COMMON + [
    "-CFLAGS",
    "-O3",
    "--output-split",
    str(SPLIT_SIZE),
    "--output-split-cfuncs",
    str(SPLIT_SIZE),
]
VERILATOR_CFLAGS_DEV = VERILATOR_CFLAGS_COMMON + [
    "-CFLAGS",
    "-O0",
]


def vc_add(dut: SbDut, key: str, value) -> None:
    dut.add("tool", "verilator", "task", "compile", key, value)


def chisel_generated_sources_filelist(generated_sv_dir: Path, top_module_name: str,
                                      out_dir: Path):
    """Writes HDL-only absolute-path filelist for verilator -f.

    generated_sv_dir is the consumer's own generated-RTL directory; out_dir is
    the consumer's own build/ directory (abs_filelist.f is written there).
    Returns (filelist_path, non_hdl_files) where non_hdl_files are C/C++ sources.
    """
    src_dir = generated_sv_dir / top_module_name
    abs_filelist = out_dir / "abs_filelist.f"
    abs_filelist.parent.mkdir(parents=True, exist_ok=True)
    non_hdl = []
    with open(src_dir / "filelist.f") as f:
        lines = f.readlines()
    with open(abs_filelist, "w") as out:
        for line in lines:
            name = line.strip()
            if not name:
                continue
            abs_path = src_dir / name
            if abs_path.suffix in HDL_EXTS:
                out.write(str(abs_path) + "\n")
            else:
                non_hdl.append(str(abs_path))
    return str(abs_filelist), non_hdl


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


def find_existing_sim(build_dir: Path):
    """Return path to an already-compiled verilator binary, or None.

    SbDut.build(fast=True) has a bug when autowrap=True: its fast path calls
    find_sim() before set_design(AutowrapDesign), so it searches the original
    design's build directory (which never contains the binary) instead of the
    AutowrapDesign subdirectory.  We work around this by locating the binary
    ourselves via a glob and returning it directly.
    """
    hits = list(build_dir.glob("**/*.vexe"))
    return str(hits[0]) if hits else None


def configure_verilator(
    dut: SbDut,
    n_build_threads: int,
    n_sim_threads: int,
    trace: bool,
    dev_mode: bool,
    abs_filelist: str,
    non_hdl_srcs: list,
) -> None:
    vc_add(dut, "warningoff", VERILATOR_WARNINGS_OFF)
    vc_add(dut, "option", ["--threads", str(n_sim_threads)])
    vc_add(dut, "option", ["--threads-dpi", "all"])

    if not dev_mode:
        vc_add(dut, "option", VERILATOR_CFLAGS_PROD)
        vc_add(dut, "option", ["-O3"])
        vc_add(dut, "option", ["--x-assign", "fast"])
        vc_add(dut, "option", ["--noassert"])
    else:
        vc_add(dut, "option", VERILATOR_CFLAGS_DEV)
        vc_add(dut, "option", ["-O0"])

    if trace:
        # SbDut's own trace=True already makes siliconcompiler's verilator-compile
        # task append --trace-fst (for trace_type="fst"); don't also pass --trace
        # here, it conflicts with the auto-injected flag.
        vc_add(dut, "option", ["--trace-underscore"])
        vc_add(dut, "option", ["-DSB_TRACE"])
        if dut.trace_type == "fst":
            vc_add(dut, "option", ["-DSB_TRACE_FST"])

    vc_add(dut, "option", ["-f", abs_filelist])
    for src in non_hdl_srcs:
        vc_add(dut, "option", [src])

    dut.set("tool", "verilator", "task", "compile", "threads", n_build_threads)
    dut.set("tool", "verilator", "task", "compile", "var", "mode", "cc")


def build_or_reuse(dut: SbDut, build_dir: Path, rebuild: bool) -> None:
    existing_sim = None if rebuild else find_existing_sim(build_dir)
    if existing_sim is not None:
        # Skip compilation; patch build() so simulate()'s internal call also
        # returns the cached binary without rebuilding.
        dut.build = lambda *_, **_kw: existing_sim
    else:
        dut.build(fast=False)


def thread_counts():
    n_cpus = os.cpu_count() or 1
    n_build_threads = max(1, min(MAX_BUILD_THREADS, n_cpus - OS_CORES_RESERVED))
    n_sim_threads = max(1, min(MAX_SIM_THREADS, n_cpus // 2))
    return n_build_threads, n_sim_threads


def make_dut(design: Design, interfaces: dict, trace: bool, builddir=BUILD_DIR) -> SbDut:
    """interfaces is an SbDut interfaces dict -- build one with make_interfaces()
    for a TileLink client/manager design, or your own for anything else (e.g. a
    design exposing raw Switchboard ports instead of TileLink).

    builddir defaults to BUILD_DIR, which is relative and so resolves against
    the cwd. Pass an absolute path to keep one cached build regardless of where
    the consumer's build.py is invoked from (and pass the same path to
    build_or_reuse)."""
    return SbDut(
        design,
        autowrap=True,
        cmdline=True,
        trace=trace,
        trace_type="fst",
        interfaces=interfaces,
        resets=[dict(name="reset", delay=0)],
        clocks=[dict(name="clock")],
        builddir=str(builddir),
    )
