# tlloopback example

Demonstrates end-to-end TileLink traffic over switchboard queues, with a Chisel-generated `TLLoopback` DUT that forwards A-channel messages from a client port to a manager port and routes D-channel responses back.

## What happens

```
client.cc                          TLLoopback DUT
  ClientTLAgent ──A──▶  io_client_0_a ──▶ io_manager_0_a ──A──▶ ManagerTLAgent
                ◀──D──  io_client_0_d ◀── io_manager_0_d ◀──D──
```

1. `client_thread` sends 2× `PutFullData` (A-channel) to address `0xDEAD0`, size=4, mask=`0xFF`.
2. DUT forwards each A-channel packet to the manager port.
3. `manager_thread` receives both `PutFullData` packets and asserts `corrupt == 0`.
4. `manager_thread` sends 1× `AccessAck` (D-channel) back through the DUT.
5. `client_thread` receives `AccessAck`, asserts `denied == 0`.
6. Both threads join. On success prints `PASS!`.

Sample output:

```text
TL-A[op=PutFullData, param=0, size=4, source=0, corrupt=0, addr=0xdead0, mask=0xff, data=[00 01 02 03 04 05 06 07]]
TL-A[op=PutFullData, param=0, size=4, source=0, corrupt=0, addr=0xdead0, mask=0xff, data=[00 01 02 03 04 05 06 07]]
TL-D[op=AccessAck, ...]
PASS!
```

## File overview

| File | Role |
|------|------|
| `client.cc` | C++ testbench — drives `ClientTLAgent` and `ManagerTLAgent` from two threads |
| `build.py` | Orchestrator — builds Verilator sim, cleans queues, runs client + DUT |
| `rtl_build/testbench.sv` | Auto-generated SV wrapper connecting TL SB ports to `TLLoopback` |
| `settings.py` | RTL path constants (`CHISEL_GEN_RTL_DIR`, `TOP_MODULE`, `N_CLIENTS`, `N_MANAGERS`) |
| `CMakeLists.txt` | CMake-based build targets |

## Queue topology

| Queue file | Direction | Description |
|------------|-----------|-------------|
| `client_0_a.q` | client → DUT | A-channel requests from `ClientTLAgent` |
| `client_0_d.q` | DUT → client | D-channel responses to `ClientTLAgent` |
| `manager_0_a.q` | DUT → manager | A-channel requests forwarded to `ManagerTLAgent` |
| `manager_0_d.q` | manager → DUT | D-channel responses from `ManagerTLAgent` |

All queues use 416-bit switchboard packets (TileLink bundle packed width).

## How the SV side works

`testbench.sv` uses switchboard macros (`QUEUE_TO_SB_SIM`, `SB_TO_QUEUE_SIM`) from `switchboard.vh` — each reads or writes its respective queue file via DPI, direction determined by the macro used.

> For details on these macros and the SV simulation flow, refer to the [Switchboard documentation](https://github.com/zeroasiccorp/switchboard) and its README.

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

## Adapting this example for your own TL-only DUT

This example can be reused as-is for any DUT whose only IOs are TileLink client/manager ports (i.e. no custom non-TL signals beyond `clock` and `reset`).

### Chisel layer

DUTs must extend `SwitchboardTLAdapter` ([src/main/scala/SwitchboardTLAdapter.scala](../../src/main/scala/SwitchboardTLAdapter.scala)). [`TLLoopback`](../../src/main/scala/example/TLLoopback.scala) is the reference:

```scala
// Extend SwitchboardTLAdapter to wrap your DUT's TL-only interface.
// This makes each TL port accessible as a Switchboard queue during simulation.
class YourDUT(implicit p: Parameters) extends SwitchboardTLAdapter {

  // Step 1: Declare TL ports as ordered Seq.
  //   - Index i → managers(i) / clients(i) in the diplomacy graph
  //   - Index i → manager_i_{a,d}.q / client_i_{a,d}.q as SB queues
  //   - Param constraints: SBConst (src/main/scala/Bundles.scala)
  override val nManagerParams = Seq(TLManagerPortParams(base = 0x0, size = 0x10000, beatBytes = 8))
  override val nClientParams  = Seq(TLClientPortParams(idBits = 4))

  // Step 2: Wire TL nodes in the diplomacy graph.
  //   Replace with your DUT's internal TL connectivity.
  (0 until nManagerParams.length).foreach(i => managers(i) := clients(i))

  lazy val module = new YourDUTImp(this)
}

// Step 3: Add RTL logic in the Imp class.
//   SwitchboardTLAdapterImp already wires io.client(i).{a,d} / io.manager(i).{a,d}
//   to the TL diplomacy ports. 
class YourDUTImp(outer: YourDUT) extends SwitchboardTLAdapterImp(outer) {
  // Place you extra RTL logic here.
}
```

`nClientParams` and `nManagerParams` are ordered collections. Each element's index `i` determines the SB queue names for that port:

| Port | A-channel queue | D-channel queue |
|------|----------------|----------------|
| `nClientParams(i)` | `client_i_a.q` | `client_i_d.q` |
| `nManagerParams(i)` | `manager_i_a.q` | `manager_i_d.q` |

`_a` = A-channel (requests), `_d` = D-channel (responses). `nClientParams.length` and `nManagerParams.length` also drive the diplomacy graph and `io` Vec sizes in `SwitchboardTLAdapterImp`. After generating RTL, these must match `N_CLIENTS` / `N_MANAGERS` in `settings.py`.

### Simulation layer

**Step 1 — point at your RTL**

Edit `settings.py`:

```python
CHISEL_GEN_RTL_DIR = "<your-generator>.<YourModule>.None/chisel_gen_rtl"
TOP_MODULE = CHISEL_GEN_RTL_DIR.split(".")[-2]   # or set explicitly
N_CLIENTS  = <len(nClientParams) from your Chisel module>
N_MANAGERS = <len(nManagerParams) from your Chisel module>
```

`build.py` reads these at startup. The SV wrapper (`testbench.sv`) and all queue files are regenerated automatically.

**Step 2 — update `client.cc`**

Replace the `client_thread` / `manager_thread` bodies with stimulus and checks for your DUT:

| Agent | Queue prefix | Use when DUT has |
|-------|-------------|-----------------|
| `ClientTLAgent client("client_N")` | `client_N_a.q`, `client_N_d.q` | a client port `N` |
| `ManagerTLAgent manager("manager_N")` | `manager_N_a.q`, `manager_N_d.q` | a manager port `N` |

Instantiate one agent per port (matching `N_CLIENTS` / `N_MANAGERS`), wire each into its own thread, and call `join()` on all threads before printing `PASS!`.

**Step 3 — build and run**

```sh
cmake -B build
cmake --build build --target verilator
./build/client &
python3 build.py   # or: cmake --build build --target run
```

No changes needed to `build.py`, `CMakeLists.txt`, or the SV wrapper.

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
- `sb_sim/include/tilelinklib.hpp` (in this repo, resolved via `../include`)
- Chisel-generated RTL under `switchboard.TLLoopback.None/chisel_gen_rtl/`
