#!/usr/bin/env python3

# Copyright (c) 2024 Zero ASIC Corporation
# This code is licensed under Apache License 2.0 (see LICENSE for details)

import time
from pathlib import Path
import argparse
import sys

from switchboard import SbDut, delete_queues, binary_run

PROJ_DIR = Path(__file__).resolve().parent.parent.parent
THIS_DIR = Path(__file__).resolve().parent


def chisel_generated_sources(topModule_name):
    """Reads chisel generated filelist.f and returns list of source files"""
    dir = str(PROJ_DIR) + "/generated_sv_dir/" + topModule_name
    filename = dir + "/filelist.f"
    hdl_sources = []
    with open(filename, "r") as f:
        lines = f.readlines()

    return list(map(lambda x: dir + "/" + x.strip("\n"), lines))

def make_interfaces(n_clients=1, n_managers=0):
    interfaces = {}
    for i in range(n_clients):
        interfaces[f"io_client_{i}_a"] = dict(type="sb", dw=416, uri = f"client_{i}_a.q", direction="input")
        interfaces[f"io_client_{i}_d"] = dict(type="sb", dw=416, uri = f"client_{i}_d.q", direction="output")
    for i in range(n_managers):
        interfaces[f"io_manager_{i}_a"] = dict(type="sb", dw=416, uri = f"manager_{i}_a.q", direction="output")
        interfaces[f"io_manager_{i}_d"] = dict(type="sb", dw=416, uri = f"manager_{i}_d.q", direction="input")
    return interfaces


def main(rtl_dir, topModule_name, n_clients=1, n_managers= 1):
    reset = [dict(name="reset", delay=0)]
    clock  = [dict(name="clock")]

    interfaces = make_interfaces(n_clients, n_managers) 

    # build the simulator
    dut = SbDut(
        topModule_name,
        autowrap=True,
        cmdline=True,
        interfaces=interfaces,
        resets=reset,
        clocks=clock,
    )

    for src_file in chisel_generated_sources(rtl_dir):
        dut.input(src_file)

    dut.add(
        "tool",
        "verilator",
        "task",
        "compile",
        "warningoff",
        ["WIDTHEXPAND", "CASEINCOMPLETE", "WIDTHTRUNC", "TIMESCALEMOD"],
    )

    dut.add(
        "tool",
        "verilator",
        "task",
        "compile",
        "var", 
        "mode", 
        "cc"
    )
    

    #dut.add("option", "mode", ["cc"])
    dut.build(fast=True)

    # clean up old queues if present
    queue_files = [u for u in map(lambda v: v.get("uri"), interfaces.values()) if u is not None]
    delete_queues(queue_files)

    # start client and chip
    # this order yields a smaller waveform file
    client = binary_run(THIS_DIR / "client")
    dut.simulate()
    #print(dut.intf_defs)

    # wait for client and chip to complete
    retcode = client.wait()
    assert retcode == 0


if __name__ == "__main__":
    from settings import CHISEL_GEN_RTL_DIR, TOP_MODULE, N_CLIENTS, N_MANAGERS
    main(CHISEL_GEN_RTL_DIR, TOP_MODULE, N_CLIENTS, N_MANAGERS)
