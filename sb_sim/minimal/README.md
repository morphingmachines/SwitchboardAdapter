# minimal example

Demonstrates C++ ↔ RTL communication over switchboard queues, with a Chisel-generated `Minimal` DUT that increments every byte of each incoming 256-bit packet and sends it back.

## What happens

1. `client` spawns two threads concurrently:
   - **send thread** — sends 32 packets over `in_port.q`; each packet has `data[i] = i & 0xff`
   - **recv thread** — receives 32 packets from `out_port.q`; asserts `data[i] == (i & 0xff) + 1`
2. Both threads join — all 32 pairs exchanged, queues fully drained.
3. Client exits, simulation terminates. On success prints `PASS!`.

Sample output (32 TX/RX pairs, interleaved across stderr/stdout):

```text
TX packet: dest: beefcafe, last: 1, data: {00, 01, 02, ..., 1f}
RX packet: dest: beefcafe, last: 1, data: {01, 02, 03, ..., 20}
...
PASS!
```

## File overview

| File | Role |
|------|------|
| `client.cc` | C++ test client — sends/receives packets via `SBTX`/`SBRX` |
| `build.py` | Orchestrator — builds Verilator sim, cleans queues, runs client + DUT |
| `rtl_build/testbench.sv` | Auto-generated SV wrapper connecting `io_in`/`io_out` SB ports to `Minimal` |
| `settings.py` | RTL path constants (`CHISEL_GEN_RTL_DIR`, `TOP_MODULE`) |
| `CMakeLists.txt` | CMake-based build targets |

## Building and running

```sh
cmake -B build
cmake --build build --target verilator          # incremental (fast=True)
cmake --build build --target verilator-rebuild  # force full recompile
cmake --build build --target clean-extra        # remove sim artifacts
```

Enable FST waveform tracing:

```sh
cmake -B build -DTRACE=ON
cmake --build build --target verilator
```

## How the SV side works

`testbench.sv` uses switchboard macros from `switchboard.vh`:

- `QUEUE_TO_SB_SIM` — receives packets from `in_port.q` into `io_in` wires
- `SB_TO_QUEUE_SIM` — transmits packets from `io_out` wires into `out_port.q`

Queue URIs can be overridden at runtime via plusargs (`+io_in=<path>`, `+io_out=<path>`).

## IDE setup (VS Code)

Copy the settings template and fill in your local paths. This enables Pylance to resolve the `switchboard` package, giving accurate go-to-definition and autocomplete for switchboard symbols.

```sh
cp .vscode/settings.json.template .vscode/settings.json
```

Then edit `.vscode/settings.json` and update two fields:

| Field | Value |
|-------|-------|
| `python.defaultInterpreterPath` | Path to Python in your conda env, e.g. `~/miniconda3/envs/Switchboard/bin/python` |
| `python.analysis.extraPaths` | Path to the switchboard repo root (the dir containing the `switchboard/` package), e.g. `/home/you/switchboard` |

`settings.json` is gitignored — changes stay local.

## Dependencies

- [switchboard](https://github.com/zeroasiccorp/switchboard) installed and on `PATH`
- Verilator (default) or Icarus Verilog
- C++17-capable compiler
- Chisel-generated RTL under `switchboard.Minimal.None/chisel_gen_rtl/`
