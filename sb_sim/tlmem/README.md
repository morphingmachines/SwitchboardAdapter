# tlmem

TileLink write transactions and RISC-V ELF loading into `TLMem` — a memory DUT with 1 client port and 0 manager ports.

Generate RTL before running: `make lazyrtl TARGET=Mem` (from repo root). Requires `$RISCV` env var (RISC-V toolchain root with fesvr).

## What happens

```
client.cc                        TLMem DUT
  ClientTLAgent ──A──▶  io_client_0_a ──▶ [memory array]
                ◀──D──  io_client_0_d ◀──
```

1. `send_thread` — 32× `PutFullData` writes (size=2, 4 bytes), addresses `0`, `4`, ..., `124`.
2. `recv_thread` — receives 32 `AccessAck` concurrently; asserts `denied == 0`.
3. `ClientTLMemIfc` wraps the agent as fesvr `memif_t`; `load_elf` loads `vecAdd.elf` into the DUT.
4. Prints `PASS!`.

## Files

| File | Role |
|------|------|
| `client.cc` | Concurrent send/recv threads, then ELF load via `ClientTLMemIfc` |
| `build.py` | Builds Verilator sim, cleans queues, runs client + DUT |
| `settings.py` | `CHISEL_GEN_RTL_DIR`, `TOP_MODULE`, `N_CLIENTS`, `N_MANAGERS` |
| `CMakeLists.txt` | Build targets |
| `vecAdd.elf` | RISC-V ELF loaded into TLMem during the test |

For [build targets](../README.md#cmake-build-targets), [SV wrapper](../README.md#sv-wrapper), and [IDE setup](../README.md#ide-setup-vs-code) — see [sb_sim/README.md](../README.md).

## Queue topology

| Queue | Direction | Channel |
|-------|-----------|---------|
| `client_0_a.q` | client → DUT | A-channel requests |
| `client_0_d.q` | DUT → client | D-channel responses |

## Dependencies

- [switchboard](https://github.com/zeroasiccorp/switchboard) on `PATH`
- Verilator, C++17 compiler
- `$RISCV` — fesvr headers + `libfesvr`
- [`tilelinklib.hpp`](../include/tilelinklib.hpp), [`memifc.hpp`](../include/memifc.hpp)
- RTL under `switchboard.TLMem.None/chisel_gen_rtl/`
