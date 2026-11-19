#!/usr/bin/env python3
"""Extract cycle timing (clock, reset, A/D handshake counts, latency, throughput)
from a Verilator VCD of the sb_sim testbench.

Usage: vcd_timing.py <file.vcd>
"""
import sys, re
path = sys.argv[1]
want = {"clk","rstvec","io_client_0_a_valid","io_client_0_a_ready","io_client_0_a_last",
        "io_client_0_d_valid","io_client_0_d_ready","io_client_0_d_last"}
ids = {}; scope=[]; timescale=None
f = open(path)
for line in f:
    t = line.split()
    if not t: continue
    if t[0]=="$timescale": timescale=" ".join(t[1:-1]) if len(t)>2 else next(f).strip()
    elif t[0]=="$scope": scope.append(t[2])
    elif t[0]=="$upscope": scope.pop()
    elif t[0]=="$var" and scope==["TOP","testbench"] and t[4] in want: ids[t[3]]=t[4]
    elif t[0]=="$enddefinitions": break
val = {n:0 for n in want}; now=0
rises=[]; rst_fall=None; afire=[]; dfire=[]; alast=[]; dlast=[]
def on_rise():
    if val["rstvec"]: return
    if val["io_client_0_a_valid"] and val["io_client_0_a_ready"]:
        afire.append(now); (alast.append(now) if val["io_client_0_a_last"] else None)
    if val["io_client_0_d_valid"] and val["io_client_0_d_ready"]:
        dfire.append(now); (dlast.append(now) if val["io_client_0_d_last"] else None)
for line in f:
    if line[0]=="#":
        now=int(line[1:]); continue
    c=line[0]
    if c in "01xz":
        k=line[1:].strip()
        if k in ids:
            n=ids[k]; v=1 if c=="1" else 0
            if n=="clk" and v==1 and val["clk"]==0:
                rises.append(now); on_rise()   # sample pre-edge values
            if n=="rstvec" and v==0 and val["rstvec"]==1: rst_fall=now
            val[n]=v
    elif c=="b":
        v,k=line[1:].split()
        if k in ids: val[ids[k]]=int(v.replace("x","0").replace("z","0"),2)
end=now
period = rises[1]-rises[0] if len(rises)>1 else None
cyc=lambda ps: ps/period
print(f"timescale           : {timescale}")
print(f"clock period        : {period} ({1e6/period:.1f} MHz if 1ps)" if period else "no clock")
print(f"sim end time        : {end}  = {cyc(end):.0f} cycles")
print(f"reset deasserted at : {rst_fall}" + (f" (cycle {cyc(rst_fall):.0f})" if rst_fall is not None else ""))
print(f"A-channel beats     : {len(afire)}  (msgs w/ last: {len(alast)})")
print(f"D-channel beats     : {len(dfire)}  (msgs w/ last: {len(dlast)})")
if afire: print(f"first A fire        : cycle {cyc(afire[0]):.0f}, last A fire: cycle {cyc(afire[-1]):.0f}")
if dfire: print(f"first D fire        : cycle {cyc(dfire[0]):.0f}, last D fire: cycle {cyc(dfire[-1]):.0f}")
if afire and dfire:
    print(f"active window       : {cyc(dfire[-1]-afire[0]):.0f} cycles (first A -> last D)")
    # pair A-last with D-last in order (in-order assumption)
    lat=[cyc(d-a) for a,d in zip(alast,dlast)]
    if lat:
        print(f"req->resp latency   : min {min(lat):.0f}, max {max(lat):.0f}, avg {sum(lat)/len(lat):.1f} cycles  (n={len(lat)}, assumes in-order)")
if afire and len(afire)==len(dfire):
    lat=[cyc(d-a) for a,d in zip(afire,dfire)]
    from collections import Counter
    print(f"A->D beat latency   : min {min(lat):.0f}, max {max(lat):.0f}, avg {sum(lat)/len(lat):.2f} cycles (1:1 in-order pairing)")
    print(f"latency histogram   : {sorted(Counter(int(x) for x in lat).items())}")
    gaps=[cyc(b-a) for a,b in zip(afire,afire[1:])]
    print(f"A issue interval    : min {min(gaps):.0f}, avg {sum(gaps)/len(gaps):.2f} cycles -> throughput {len(afire)/cyc(dfire[-1]-afire[0]+period):.3f} req/cycle")
