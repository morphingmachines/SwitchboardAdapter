# sim_build

Shared Verilator/SbDut build logic for any project that autowraps its RTL
with [SbDut](https://github.com/zeroasiccorp/switchboard) and drives it over
this adapter's TileLink-over-Switchboard queues. Pairs with
[sb_sim](../sb_sim/README.md), the C++ side of the same consumer contract
(`ClientTLAgent`/`ManagerTLAgent`).

`sim_build.py` is a plain Python module, not a CMake target. Consume it the
same way you'd consume any Python helper library: point `sys.path` at this
directory and import it.

Tested with Python 3.10, Verilator 5.050, `siliconcompiler` 0.38.1,
`switchboard-hw` 0.3.4, and `umi` 0.4.15.

## What it does

- Assembles Verilator compile flags (warnings-off list, `-O3`/`-O0`,
  `-mcmodel=large`, `--output-split`, thread counts) for production and
  dev-mode builds
- Builds the SbDut `interfaces` dict for your design's TileLink client/manager
  ports
- Writes an absolute-path HDL filelist for `verilator -f`
- Works around an SbDut bug where `build(fast=True)` looks for the compiled
  binary in the wrong directory when `autowrap=True`
- Enables FST waveform tracing without double-passing `--trace` (SbDut's own
  `trace=True` already appends `--trace-fst` for `trace_type="fst"`; passing
  `--trace` yourself on top of that conflicts with it)

## Usage

```python
import sys
from pathlib import Path

_sim_build_path = str(Path(__file__).resolve().parent / "path/to/SwitchboardAdapter/sim_build")
sys.path.insert(0, _sim_build_path)
try:
    import sim_build
finally:
    sys.path.remove(_sim_build_path)

from siliconcompiler import Design

design = Design(top_module_name)
design.set_topmodule(top_module_name, fileset="verilator")

abs_filelist, non_hdl_srcs = sim_build.chisel_generated_sources_filelist(
    generated_sv_dir, top_module_name, build_dir
)

interfaces = sim_build.make_interfaces(n_clients, n_managers)
dut = sim_build.make_dut(design, interfaces, trace)

n_build_threads, n_sim_threads = sim_build.thread_counts()
sim_build.configure_verilator(
    dut, n_build_threads, n_sim_threads, trace, dev_mode, abs_filelist, non_hdl_srcs
)
sim_build.build_or_reuse(dut, Path(sim_build.BUILD_DIR).resolve(), rebuild)

dut.remove_queues_on_exit()
dut.simulate()
```

Your own `build.py` keeps whatever CLI parsing and testbench-binary
invocation are specific to your project; `sim_build` covers everything that
is the same for any consumer.

`BUILD_DIR` (`"rtl_build"`) is relative, so by default the cached Verilator
build lands in whatever directory `build.py` is run from. To keep one cached
build no matter the cwd, anchor it yourself and pass the same path to both
calls:

```python
build_dir = Path(__file__).resolve().parent / sim_build.BUILD_DIR
dut = sim_build.make_dut(design, interfaces, trace, builddir=build_dir)
...
sim_build.build_or_reuse(dut, build_dir, rebuild)
```

`make_dut` takes a plain SbDut `interfaces` dict, not `n_clients`/`n_managers`
directly -- `make_interfaces()` is the TileLink-specific helper that builds
one. A design that doesn't speak TileLink (e.g. one exposing raw Switchboard
ports) builds its own interfaces dict and passes that to `make_dut` instead.

## `n_clients` / `n_managers`

These describe your design's TileLink ports from the RTL's point of view --
same terms `sb_sim`'s `ClientTLAgent`/`ManagerTLAgent` use:

- **`n_clients`**: number of ports where your RTL is the TileLink *manager*
  -- it receives A-channel requests and sends D-channel responses. Each one
  becomes an `io_client_N_a` (input to the RTL) / `io_client_N_d` (output
  from the RTL) pair of Switchboard queues. Drive each from the host side
  with one `ClientTLAgent`.
- **`n_managers`**: number of ports where your RTL is the TileLink *client*
  -- it issues A-channel requests and expects D-channel responses. Each one
  becomes an `io_manager_N_a` (output from the RTL) / `io_manager_N_d` (input
  to the RTL) pair. Serve each from the host side with one `ManagerTLAgent`.

A design with, say, two independent TileLink links where the RTL always
receives requests would pass `n_clients=2, n_managers=0`. A design where the
RTL also initiates requests on a separate link (e.g. reporting completion to
the host) adds one to `n_managers` for that link.
