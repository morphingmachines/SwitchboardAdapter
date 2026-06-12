# tlloopback

TileLink A/D loopback via Switchboard queues. `TLLoopback` DUT forwards A-channel from its client port through to its manager port, and D-channel back.

Generate RTL before running: `make lazyrtl TARGET=Loopback` (from repo root).

## What happens

```
client.cc                          TLLoopback DUT
  ClientTLAgent ──A──▶  io_client_0_a ──▶ io_manager_0_a ──A──▶ ManagerTLAgent
                ◀──D──  io_client_0_d ◀── io_manager_0_d ◀──D──
```

1. `client_thread` sends 2× `PutFullData` → addr `0xDEAD0`, size=4, mask=`0xFF`.
2. DUT forwards each packet to `manager_thread`; asserts `corrupt == 0`.
3. `manager_thread` sends 1× `AccessAck` back through the DUT.
4. `client_thread` asserts `denied == 0`. Prints `PASS!`.

## Files

| File | Role |
|------|------|
| `client.cc` | Drives `ClientTLAgent` + `ManagerTLAgent` from two threads |
| `build.py` | Builds Verilator sim, cleans queues, runs client + DUT |
| `settings.py` | `CHISEL_GEN_RTL_DIR`, `TOP_MODULE`, `N_CLIENTS`, `N_MANAGERS` |
| `CMakeLists.txt` | Build targets |

For [build targets](../README.md#cmake-build-targets), [SV wrapper](../README.md#sv-wrapper), and [IDE setup](../README.md#ide-setup-vs-code) — see [sb_sim/README.md](../README.md).

## Queue topology

| Queue | Direction | Channel |
|-------|-----------|---------|
| `client_0_a.q` | client → DUT | A-channel requests |
| `client_0_d.q` | DUT → client | D-channel responses |
| `manager_0_a.q` | DUT → manager | A-channel forwarded |
| `manager_0_d.q` | manager → DUT | D-channel responses |

All queues use 416-bit switchboard packets.

## Adapting for your DUT

Any DUT with only TL client/manager ports (no custom IOs beyond `clock`/`reset`) can reuse this example.

### Chisel

Extend [`SwitchboardTLAdapter`](../../src/main/scala/SwitchboardTLAdapter.scala). Use [`TLLoopback.scala`](../../src/main/scala/example/TLLoopback.scala) as reference:

```scala
class YourDUT(implicit p: Parameters) extends SwitchboardTLAdapter {
  override val nManagerParams = Seq(TLManagerPortParams(base = 0x0, size = 0x10000, beatBytes = 8))
  override val nClientParams  = Seq(TLClientPortParams(idBits = 4))
  (0 until nManagerParams.length).foreach(i => managers(i) := clients(i))
  lazy val module = new YourDUTImp(this)
}
class YourDUTImp(outer: YourDUT) extends SwitchboardTLAdapterImp(outer) {
  // extra RTL here
}
```

Port index `i` maps to queue names `client_i_{a,d}.q` / `manager_i_{a,d}.q`. `N_CLIENTS` / `N_MANAGERS` in `settings.py` must match `nClientParams.length` / `nManagerParams.length`.

### Simulation

1. `settings.py` — set `CHISEL_GEN_RTL_DIR`, `TOP_MODULE`, `N_CLIENTS`, `N_MANAGERS`.
2. `client.cc` — one `ClientTLAgent("client_N")` / `ManagerTLAgent("manager_N")` per port; each in its own thread.
3. Build — `cmake -B build && cmake --build build --target verilator`.

## Dependencies

- [switchboard](https://github.com/zeroasiccorp/switchboard) on `PATH`
- Verilator, C++17 compiler
- [`tilelinklib.hpp`](../include/tilelinklib.hpp)
- RTL under `switchboard.TLLoopback.None/chisel_gen_rtl/`
