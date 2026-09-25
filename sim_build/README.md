# sim_build

The **co-sim run** module for any project that autowraps its RTL with
[SbDut](https://github.com/zeroasiccorp/switchboard) and drives it from a
host-side client over this adapter's Switchboard queues. Pairs with
[sb_sim](../sb_sim/README.md), the C++ side of the same consumer contract
(`ClientTLAgent`/`ManagerTLAgent`).

`sim_build.py` is a plain Python module, not a CMake target. Point `sys.path`
at this directory and import it.

Tested with Python 3.10, Verilator 5.050, `siliconcompiler` 0.38.1,
`switchboard-hw` 0.3.4, and `umi` 0.4.15.

## Usage

One call builds the simulation (or reuses a cached build) and runs it against
your client:

```python
import sys
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
_sim_build_path = str(THIS_DIR / "path/to/SwitchboardAdapter/sim_build")
sys.path.insert(0, _sim_build_path)
try:
    import sim_build
finally:
    sys.path.remove(_sim_build_path)

sim_build.run_cosim(
    THIS_DIR,                                  # project_dir: owns the build cache
    Path("/abs/path/to/generated_sv_dir/<config>/chisel_gen_rtl"),  # rtl_dir
    "MyTop",                                   # top module
    sim_build.make_interfaces(n_clients=1, n_managers=0),
    [THIS_DIR / "TestDriver", "arg1"],         # client argv
    trace=False, rebuild=False, dev_mode=False,
)
```

Your own `build.py` keeps only its CLI parsing and what's specific to your
project (RTL location, ports, client arguments).

## What `run_cosim` does

- Writes an absolute-path HDL filelist from `rtl_dir/filelist.f` into
  `project_dir/build/abs_filelist.f`.
- Assembles Verilator flags: warnings-off list, `-O3`/`-O0`, `-mcmodel=large`,
  `--output-split`, thread counts; `dev_mode=True` enables X-propagation and
  assertions.
- Builds into `project_dir/rtl_build/`, or reuses the cached build when it
  matches (see below). The cache never depends on the current directory.
- Starts the simulator, **then** the client. The order matters: the simulator
  recreates every queue file, so a client started first would hang on a
  deleted queue.
- Waits for the client, watching the simulator. Once the client exits the
  simulator is stopped.
- With `trace=True`, deletes any old `./testbench.fst`, and prints
  `waveform: <path>` after the run.

Queue files (`*.q`) and the waveform land in the current directory.

### Reusing the cached build

`project_dir/rtl_build/build-stamp.json` records what the cached build was built
from. The build is reused only if `top`, `rtl_dir`, `trace` and `dev_mode` all
match and `filelist.f` hasn't changed since (Chisel rewrites it on every
generation). Otherwise it is rebuilt, and the reason is printed:

```
rtl build: building (trace changed (False -> True))
rtl build: reusing /…/rtl_build/AutowrapDesign/job0/compile/0/outputs/testbench.vexe
```

Hand-editing an RTL file without regenerating isn't detected. Pass
`rebuild=True` for that.

### Errors

`run_cosim` returns `None` when the client exits 0. Otherwise it raises
`sim_build.CosimError`, whose `stage` says which part failed and whose
`returncode` is that process's exit code:

| `stage` | When |
|---|---|
| `"build"` | Verilator build failed (`returncode` is `None`) |
| `"client"` | client exited non-zero |
| `"sim"` | simulator exited before the client finished, e.g. `$fatal` or a crash |

### Concurrent runs

Runs from **different** directories can share one `project_dir` safely: a
lock on `project_dir/build/.build.lock` serialises the build step, and
`waiting for build lock…` is printed while one run waits. Two runs from the
**same** directory aren't supported, because they'd share queue files.

## `make_interfaces(n_clients, n_managers)`

Builds the SbDut `interfaces` dict for a TileLink design. The counts describe
your design's TileLink ports from the RTL's point of view, in the same terms
`sb_sim`'s `ClientTLAgent`/`ManagerTLAgent` use:

- **`n_clients`**: number of ports where your RTL is the TileLink *manager*
  -- it receives A-channel requests and sends D-channel responses. Each one
  becomes an `io_client_N_a` (input to the RTL) / `io_client_N_d` (output
  from the RTL) pair of Switchboard queues. Drive each from the host side
  with one `ClientTLAgent`.
- **`n_managers`**: number of ports where your RTL is the TileLink *client*
  -- it issues A-channel requests and expects D-channel responses. Each one
  becomes an `io_manager_N_a` (output from the RTL) / `io_manager_N_d` (input
  to the RTL) pair. Serve each from the host side with one `ManagerTLAgent`.

A design that doesn't speak TileLink (e.g. one exposing raw Switchboard ports)
passes its own interfaces dict instead; see `../sb_sim/minimal/build.py`.

`SB_DATA_WIDTH` (416) is the packed TileLink-over-Switchboard payload width; it
must match `Bundles.scala`'s `DataWidth`.

## Tests

```sh
python3 -m pytest tests
```

The tests run `run_cosim` for real against a small hand-written loopback design
in `tests/fixtures/`: no Chisel needed, just Verilator and switchboard. They
cover a cold build, cache reuse, rebuild when trace changes or the RTL is
regenerated, a failing client, and a simulator that dies mid-run.
