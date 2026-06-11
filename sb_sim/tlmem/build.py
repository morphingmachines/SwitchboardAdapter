#!/usr/bin/env python3

# Copyright (c) 2024 Zero ASIC Corporation
# This code is licensed under Apache License 2.0 (see LICENSE for details)

import os
import argparse
import sys
from pathlib import Path

from switchboard import SbDut, delete_queues, binary_run

PROJ_DIR = Path(__file__).resolve().parent.parent.parent
THIS_DIR = Path(__file__).resolve().parent

HDL_EXTS = {".v", ".sv", ".vh", ".svh"}


def chisel_generated_sources_filelist(topModule_name):
    """Writes HDL-only absolute-path filelist for verilator -f.
    Returns (filelist_path, non_hdl_files) where non_hdl_files are C/C++ sources."""
    src_dir = PROJ_DIR / "generated_sv_dir" / topModule_name
    abs_filelist = THIS_DIR / "build" / "abs_filelist.f"
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
            if Path(name).suffix in HDL_EXTS:
                out.write(str(abs_path) + "\n")
            else:
                non_hdl.append(str(abs_path))
    return str(abs_filelist), non_hdl


def make_interfaces(n_clients=1, n_managers=0):
    interfaces = {}
    for i in range(n_clients):
        interfaces[f"io_client_{i}_a"] = dict(type="sb", dw=416, uri=f"client_{i}_a.q", direction="input")
        interfaces[f"io_client_{i}_d"] = dict(type="sb", dw=416, uri=f"client_{i}_d.q", direction="output")
    for i in range(n_managers):
        interfaces[f"io_manager_{i}_a"] = dict(type="sb", dw=416, uri=f"manager_{i}_a.q", direction="output")
        interfaces[f"io_manager_{i}_d"] = dict(type="sb", dw=416, uri=f"manager_{i}_d.q", direction="input")
    return interfaces


def main(rtl_dir, topModule_name, n_clients=1, n_managers=0, trace=False, rebuild=False):
    reset = [dict(name="reset", delay=0)]
    clock = [dict(name="clock")]

    interfaces = make_interfaces(n_clients, n_managers)

    os.environ.setdefault("CXX", "ccache g++")
    os.environ.setdefault("CC", "ccache gcc")

    n_threads = max(1, min(32, (os.cpu_count() or 8) - 4))

    dut = SbDut(
        topModule_name,
        autowrap=True,
        cmdline=True,
        trace=trace,
        trace_type="fst",
        interfaces=interfaces,
        resets=reset,
        clocks=clock,
        builddir="rtl_build",
        threads=n_threads,
    )

    dut.add(
        "tool",
        "verilator",
        "task",
        "compile",
        "warningoff",
        ["WIDTHEXPAND", "CASEINCOMPLETE", "WIDTHTRUNC", "TIMESCALEMOD"],
    )

    if trace:
        dut.add(
            "tool",
            "verilator",
            "task",
            "compile",
            "option",
            ["--trace-underscore"],
        )

    dut.add(
        "tool",
        "verilator",
        "task",
        "compile",
        "option",
        [
            "-CFLAGS",
            "-O1",
            "-CFLAGS",
            "-mcmodel=large",
            "--output-split",
            "20000",
            "--output-split-cfuncs",
            "20000",
        ],
    )

    dut.add("tool", "verilator", "task", "compile", "var", "mode", "cc")

    abs_filelist, non_hdl_srcs = chisel_generated_sources_filelist(rtl_dir)
    dut.add("tool", "verilator", "task", "compile", "option", ["-f", abs_filelist])

    for src in non_hdl_srcs:
        dut.input(src)

    dut.build(fast=not rebuild)

    delete_queues([v["uri"] for v in interfaces.values() if "uri" in v])

    # start client and chip
    # this order yields a smaller waveform file
    client = binary_run(THIS_DIR / "client")

    dut.simulate()

    retcode = client.wait()
    assert retcode == 0


if __name__ == "__main__":
    from settings import CHISEL_GEN_RTL_DIR, TOP_MODULE, N_CLIENTS, N_MANAGERS

    parser = argparse.ArgumentParser(description="Build and simulate with TestDriver")
    parser.add_argument(
        "--trace", action="store_true", help="Enable FST waveform tracing"
    )
    parser.add_argument(
        "--rebuild",
        action="store_true",
        help="Force full rebuild (skip incremental cache)",
    )
    args, remaining = parser.parse_known_args()
    sys.argv = [sys.argv[0]] + remaining

    main(
        CHISEL_GEN_RTL_DIR,
        TOP_MODULE,
        N_CLIENTS,
        N_MANAGERS,
        trace=args.trace,
        rebuild=args.rebuild,
    )
