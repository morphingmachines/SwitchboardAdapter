#!/usr/bin/env python3

# Copyright (c) 2024 Zero ASIC Corporation
# This code is licensed under Apache License 2.0 (see LICENSE for details)

import argparse
import os
import sys
from pathlib import Path

from siliconcompiler import Design
from switchboard import binary_run

PROJ_DIR = Path(__file__).resolve().parent.parent.parent
THIS_DIR = Path(__file__).resolve().parent

_sim_build_path = str(PROJ_DIR / "sim_build")
sys.path.insert(0, _sim_build_path)
try:
    import sim_build
finally:
    sys.path.remove(_sim_build_path)


def main(
    rtl_dir,
    topModule_name,
    n_clients=1,
    n_managers=0,
    trace=False,
    rebuild=False,
    debug=False,
):
    os.environ.setdefault("CXX", "ccache g++")
    os.environ.setdefault("CC", "ccache gcc")

    n_build_threads, n_sim_threads = sim_build.thread_counts()

    abs_filelist, non_hdl_srcs = sim_build.chisel_generated_sources_filelist(
        PROJ_DIR / "generated_sv_dir", rtl_dir, THIS_DIR / "build"
    )
    design = Design(topModule_name)
    design.set_topmodule(topModule_name, fileset="verilator")

    interfaces = sim_build.make_interfaces(n_clients, n_managers)
    dut = sim_build.make_dut(design, interfaces, trace)

    sim_build.configure_verilator(
        dut, n_build_threads, n_sim_threads, trace, debug, abs_filelist, non_hdl_srcs
    )
    sim_build.build_or_reuse(dut, Path(sim_build.BUILD_DIR).resolve(), rebuild)

    dut.remove_queues_on_exit()

    # start client and chip
    # this order yields a smaller waveform file
    client = binary_run(THIS_DIR / "client")

    dut.simulate()

    retcode = client.wait()
    if retcode != 0:
        raise RuntimeError(f"client exited with code {retcode}")


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
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Debug mode: enable X-propagation and assertions (disables --x-assign fast / --noassert)",
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
        debug=args.debug,
    )
