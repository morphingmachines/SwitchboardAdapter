# Switchboard TileLink Adapter

A Chisel project that exposes TileLink diplomacy ports as [Switchboard](https://github.com/zeroasiccorp/switchboard) queues, so C++ testbenches can drive RTL simulations over typed TileLink A/D channels.

Each TileLink transaction is packed into a 416-bit Switchboard payload — wide enough to carry a full `TLBundleA` or `TLBundleD` (opcode, param, size, source, address, mask, data, corrupt/denied) in a single beat.

## Repository layout

```
SwitchboardTLAdapter/
├── src/main/scala/
│   ├── Bundles.scala              # SBConst, SBIO, pack/unpack for TLBundleA/D
│   ├── SwitchboardTLAdapter.scala # Abstract LazyModule base class
│   ├── Top.scala                  # RTL entry points (rtlMain, lazyrtlMain)
│   └── example/
│       ├── Minimal.scala          # Raw SB packet loopback (no TL)
│       ├── TLLoopback.scala       # 1 client + 1 manager, passthrough
│       └── TLMem.scala            # 1 client + 2× TLRAM banks via xbar
├── sb_sim/
│   ├── include/                   # C++ TileLink agent library (tilelinklib, memifc)
│   ├── minimal/                   # Simulation example: raw packet exchange
│   ├── tlloopback/                # Simulation example: TL loopback
│   └── tlmem/                     # Simulation example: TL memory + ELF load
├── sim_build/
│   ├── sim_build.py                # Co-sim run module: run_cosim() builds/reuses + runs (Python)
│   ├── tests/                      # pytest suite + hand-written loopback fixture
│   └── README.md                   # Usage, n_clients/n_managers, dependency versions
├── doc/
│   └── dependencies.md            # Verilator + Python package install guide
├── build.sc                       # Mill build definition
└── Makefile                       # RTL generation targets
```

This project uses [playground](https://github.com/morphingmachines/playground.git) as a library. Both repos must be siblings in the same directory:

```
workspace/
├── playground/
└── SwitchboardTLAdapter/
```

## Clone

```bash
git clone https://github.com/morphingmachines/SwitchboardAdapter.git
```

## Generating RTL

Uses `lazyrtl` for diplomacy-based modules and `rtl` for plain Chisel:

```bash
make lazyrtl TARGET=Loopback   # → generated_sv_dir/switchboard.TLLoopback.None/
make lazyrtl TARGET=Mem        # → generated_sv_dir/switchboard.TLMem.None/
make rtl TARGET=Minimal        # → generated_sv_dir/switchboard.Minimal.None/
```

Output Verilog and a filelist are written to `./generated_sv_dir/<module>/`. A GraphML file visualizing the TileLink diplomacy graph is also generated — open it with [yEd](https://www.yworks.com/products/yed).

## Scala console

```bash
make console
```

Load a design interactively:

```bash
scala> :load inConsole.scala
```

`inConsole.scala` loads the `TLMem` module. Query its bundle parameters:

```
scala> dut.ram0.node.in(0)._2.bundle
val res1: freechips.rocketchip.tilelink.TLBundleParameters =
  TLBundleParameters(16,32,4,1,2,List(),List(),List(),false)
```

## Simulation

Install [dependencies](./doc/dependencies.md) first, then generate the RTL for the target module.

```bash
cd sb_sim/<example>           # minimal | tlloopback | tlmem
cmake -B build
cmake --build build --target verilator          # build if needed + run
cmake --build build --target verilator-rebuild  # force full recompile + run
cmake --build build --target clean-extra        # remove simulation artifacts
```

Enable FST waveform tracing:

```bash
cmake -B build -DTRACE=ON
cmake --build build --target verilator
```

`tlmem` additionally requires a RISC-V toolchain with `fesvr`:

```bash
export RISCV=/path/to/riscv-toolchain
cd sb_sim/tlmem && cmake -B build && cmake --build build --target verilator
```

See [sb_sim/README.md](./sb_sim/README.md) for details on the C++ TileLink agent library (`tilelinklib`, `memifc`), the 416-bit pack format, and how to adapt these examples for your own DUT.

## Building your own DUT's simulation

The `minimal`/`tlloopback`/`tlmem` examples' `build.py` are thin CLIs around
one call to `sim_build.run_cosim()` ([sim_build/README.md](./sim_build/README.md)).
It owns everything any project autowrapping its RTL with SbDut needs:
Verilator flags, interface wiring, cached-build reuse, starting the simulator
before the client, and failing loudly if either side dies. If your own project
drives its DUT the same way, call `run_cosim()` instead of writing this
plumbing again.

## Chisel resources

- [Chisel Book](https://github.com/schoeberl/chisel-book)
- [Chisel Documentation](https://www.chisel-lang.org/chisel3/)
- [Chisel API](https://www.chisel-lang.org/api/latest/)
