Switchboard TileLink Adapter
============================

A Chisel project that exposes TileLink master/target ports as [Switchboard](https://github.com/zeroasiccorp/switchboard) interfaces so C/C++ testbenches can drive RTL simulations.

This project uses [playground](https://github.com/morphingmachines/playground.git) as a library. `playground` and `SwitchboardTLAdapter` must be at the same level:

```
workspace/
├── playground/
└── SwitchboardTLAdapter/
```

Make sure you have a working [playground](https://github.com/morphingmachines/playground.git) project before proceeding. Do not rename or modify the `playground` directory.

## Clone

```bash
git clone https://github.com/morphingmachines/SwitchboardAdapter.git
```

## Generating RTL

```bash
make lazyrtl TARGET=Loopback   # other allowed targets: Mem
make rtl TARGET=Minimal
```

Output Verilog is written to `./generated_sv_dir/`. A GraphML file visualizing the diplomacy graph is also generated — open it with [yEd](https://www.yworks.com/products/yed).

## Scala console

```bash
make console
```

Load a design interactively:

```bash
scala> :load inConsole.scala
```

`inConsole.scala` loads the `TLMem` module. Query its parameters:

```
scala> dut.ram.node.in(0)._2.bundle
val res1: freechips.rocketchip.tilelink.TLBundleParameters = TLBundleParameters(16,32,4,1,2,List(),List(),List(),false)
```

## Simulation

Install [dependencies](./doc/dependencies.md) first, then generate the RTL.

```sh
conda activate switchboard
cd sb_sim/tlmem      # or sb_sim/tlloopback, sb_sim/minimal
make
```

See [sb_sim/README.md](./sb_sim/README.md) for details on the C++ TileLink simulation library, available examples, and build options.

## Chisel resources

- [Chisel Book](https://github.com/schoeberl/chisel-book)
- [Chisel Documentation](https://www.chisel-lang.org/chisel3/)
- [Chisel API](https://www.chisel-lang.org/api/latest/)
