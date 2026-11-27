#!/usr/bin/env python3

# Copyright (c) 2024 Zero ASIC Corporation
# This code is licensed under Apache License 2.0 (see LICENSE for details)

import argparse
import sys
from pathlib import Path

PROJ_DIR = Path(__file__).resolve().parent.parent.parent
THIS_DIR = Path(__file__).resolve().parent

_sim_build_path = str(PROJ_DIR / "sim_build")
sys.path.insert(0, _sim_build_path)
try:
    import sim_build
finally:
    sys.path.remove(_sim_build_path)

from settings import CHISEL_GEN_RTL_DIR, N_CLIENTS, N_MANAGERS, TOP_MODULE  # noqa: E402

INTERFACES = sim_build.make_interfaces(N_CLIENTS, N_MANAGERS)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build and simulate with the client")
    parser.add_argument("--trace", action="store_true", help="Enable FST waveform tracing")
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
    args = parser.parse_args()

    sim_build.run_cosim(
        THIS_DIR,
        PROJ_DIR / "generated_sv_dir" / CHISEL_GEN_RTL_DIR,
        TOP_MODULE,
        INTERFACES,
        [THIS_DIR / "client"],
        trace=args.trace,
        rebuild=args.rebuild,
        dev_mode=args.debug,
    )
