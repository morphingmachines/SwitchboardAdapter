#!/usr/bin/env python3

# Copyright (c) 2024 Zero ASIC Corporation
# This code is licensed under Apache License 2.0 (see LICENSE for details)

import time
from pathlib import Path

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


def main():
    reset = [dict(name="reset", delay=0)]
    clock  = [dict(name="clock")]

    interfaces = {
        "io_in": dict(type="sb", dw=256, uri = "in_port.q", direction="input"),
        "io_out": dict(type="sb", dw=256, uri = "out_port.q", direction="output"),
    }

    rtl_dir = "switchboard.Minimal.None/chisel_gen_rtl"
    topModule_name = "Minimal"
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
    delete_queues(["in_port.q", "out_port.q"])

    # start client and chip
    # this order yields a smaller waveform file
    client = binary_run(THIS_DIR / "client")
    dut.simulate()
    #print(dut.intf_defs)

    # wait for client and chip to complete
    retcode = client.wait()
    assert retcode == 0


if __name__ == "__main__":
    main()
