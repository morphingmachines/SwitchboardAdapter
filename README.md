Switchboard Tilelink Adapter
============================
 Switchboard TileLink Adapter — a Chisel project that exposes TileLink master/target ports as Switchboard interfaces so C/C++ testbenches can drive RTL simulations via the Switchboard library
 This project uses [playground](https://github.com/morphingmachines/playground.git) as a library. `playground` and `SwitchboardAdapter` (this repo) directories should be at the same level, as shown below.  
```
  workspace
  |-- playground
  |-- SwitchboardAdapter
```
Make sure that you have a working [playground](https://github.com/morphingmachines/playground.git) project before proceeding further. Do not rename/modify `playground` directory structure.

## Clone the repository
Clone this repository into the same directory that contains `playground`.
```bash
$ git clone https://github.com/morphingmachines/SwitchboardAdapter.git
```
### Generating RTL
```bash
$ make rtl TARGET=Point2Point # other allowed targets {RegNode}
```
The output verilog files are generated in the ./generated_sv_dir directory. This also generates a graphml file that visualizes the diplomacy graph of different components in the system. To view graphml file, use yEd.

## Scala console
```bash
$ make console
```

You load a design into the console for interacting running, as shown below

```bash
scala> :load inConsole.scala
```

`inConsole.scala` will load `TLMem` module in the console. We can query module parameter once it is loaded, as shown below.

```
scala> dut.ram.node.in(0)._2.bundle
val res1: freechips.rocketchip.tilelink.TLBundleParameters = TLBundleParameters(16,32,4,1,2,List(),List(),List(),false)
```
## Simulation
To run simulations, you need to install the following [dependencies](./doc/dependencies.md)

We use [Switchboard](https://github.com/zeroasiccorp/switchboard) to provide stimulus to the accelerator module. All the stimulus generation

After generating the RTL, follow the below steps to run the simulation.
```sh
$ conda activate Switchboard
$ cd sb_sim/tlmem # "sb_sim/tlloopback" "sb_sim/regNode" "sb_sim/minimal"
$ make 
```
## Chisel Learning Resources

- [Chisel Book](https://github.com/schoeberl/chisel-book)
- [Chisel Documentation](https://www.chisel-lang.org/chisel3/)
- [Chisel API](https://www.chisel-lang.org/api/latest/)




