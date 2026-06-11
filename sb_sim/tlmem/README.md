# tlmem example

Demonstrates TileLink write transactions and ELF loading into a Chisel-generated `TLMem` DUT — a memory device with a single client port and no manager port.

## What happens

```
client.cc                        TLMem DUT
  ClientTLAgent ──A──▶  io_client_0_a ──▶ [memory array]
                ◀──D──  io_client_0_d ◀──
```

1. `send_thread` issues 32 `PutFullData` (A-channel) writes — size=2 (4 bytes), addresses `0`, `4`, ..., `124`.
2. `recv_thread` receives 32 `AccessAck` (D-channel) responses concurrently; asserts `denied == 0`.
3. Both threads join.
4. `ClientTLMemIfc` wraps the agent as a FESVR `memif_t`; `load_elf` loads `vecAdd.elf` into the DUT's memory array via TileLink `PutFullData` bursts.
5. On success prints `PASS!`.

## File overview

| File | Role |
|------|------|
| `client.cc` | C++ testbench — concurrent send/recv threads, then ELF load via `ClientTLMemIfc` |
| `build.py` | Orchestrator — builds Verilator sim, cleans queues, runs client + DUT |
| `rtl_build/testbench.sv` | Auto-generated SV wrapper connecting TL SB ports to `TLMem` |
| `settings.py` | RTL path constants (`CHISEL_GEN_RTL_DIR`, `TOP_MODULE`, `N_CLIENTS`, `N_MANAGERS`) |
| `CMakeLists.txt` | CMake-based build targets |
| `vecAdd.elf` | RISC-V ELF binary loaded into TLMem memory during the test |

## Queue topology

| Queue file | Direction | Description |
|------------|-----------|-------------|
| `client_0_a.q` | client → DUT | A-channel requests (writes/reads) |
| `client_0_d.q` | DUT → client | D-channel responses (acks) |

All queues use 416-bit switchboard packets (TileLink bundle packed width).

## Building and running

Requires the `RISCV` environment variable pointing to the RISC-V toolchain root (used for `fesvr` headers and library).

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

- `QUEUE_TO_SB_SIM` — drives `io_client_0_a` from `client_0_a.q`
- `SB_TO_QUEUE_SIM` — captures `io_client_0_d` into `client_0_d.q`

Queue URIs can be overridden at runtime via plusargs (`+io_client_0_a=<path>`, `+io_client_0_d=<path>`).

## IDE setup (VS Code)

```sh
cp .vscode/settings.json.template .vscode/settings.json
```

Edit `.vscode/settings.json` and update two fields:

| Field | Value |
|-------|-------|
| `python.defaultInterpreterPath` | Path to Python in your conda env, e.g. `~/miniconda3/envs/Switchboard/bin/python` |
| `python.analysis.extraPaths` | Path to the switchboard repo root (the dir containing the `switchboard/` package), e.g. `/home/you/switchboard` |

`settings.json` is gitignored — changes stay local.

## Dependencies

- [switchboard](https://github.com/zeroasiccorp/switchboard) installed and on `PATH`
- Verilator
- C++17-capable compiler
- `RISCV` env var — RISC-V toolchain root (provides `fesvr` headers and `libfesvr`)
- `sb_sim/include/tilelinklib.hpp` and `memifc.hpp` (in this repo, resolved via `../include`)
- Chisel-generated RTL under `switchboard.TLMem.None/chisel_gen_rtl/`
