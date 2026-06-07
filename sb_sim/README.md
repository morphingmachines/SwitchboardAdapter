# sb_sim

C++ host-side TileLink simulation library built on top of [Switchboard](https://github.com/zeroasiccorp/switchboard). Provides message structs, agent classes, and a memory interface for driving TileLink transactions from C++ test code against RTL simulations.

## Directory structure

```
sb_sim/
├── include/
│   ├── tilelinklib.h    # TileLink enums, packed message structs, TLBundleParams
│   ├── tilelinklib.hpp  # TLAgent, ClientTLAgent, ManagerTLAgent
│   └── memifc.hpp       # ClientTLMemIfc — fesvr chunked_memif_t over TileLink
├── minimal/             # Basic Switchboard packet exchange (no TileLink)
├── tlloopback/          # TileLink loopback: client + manager in one process
└── tlmem/              # TileLink memory interface + fesvr ELF loading
```

## Headers

### `tilelinklib.h`

Defines TileLink-A/D opcode enums (`TLAOpcode`, `TLDOpcode`), atomic operation enums, packed wire-format structs (`TLMessageA`, `TLMessageD`), and `TLBundleParams`.

### `tilelinklib.hpp`

- **`TLAgent`** — base class with message-building helpers (`put`, `putPartial`, `get`, `accessAck`, `accessAckData`) and mask alignment logic. Call `set_TLBundleParams()` before use.
- **`ClientTLAgent`** — master/source side. Sends on A channel (`send_a`), receives on D channel (`recv_d`).
- **`ManagerTLAgent`** — slave/sink side. Receives on A channel (`recv_a`), sends on D channel (`send_d`).

Both agents connect to Switchboard queues named `<uri>_a.q` and `<uri>_d.q`.

### `memifc.hpp`

**`ClientTLMemIfc`** implements fesvr's `chunked_memif_t` interface over a `ClientTLAgent`. Decomposes arbitrary-length reads/writes into naturally-aligned power-of-2 TileLink transactions. Intended for use with fesvr's `load_elf`.

## Examples

### `minimal/`

Basic Switchboard packet exchange with RTL. Sends a packet to RTL, which increments each byte and sends it back.

```sh
cd minimal && make
```

### `tlloopback/`

TileLink loopback test: a `ClientTLAgent` and `ManagerTLAgent` run in the same process. Exercises A-channel puts and D-channel acks without RTL.

```sh
cd tlloopback && make
```

### `tlmem/`

Demonstrates `ClientTLMemIfc` + fesvr: sends 32 write transactions, then loads a RISC-V ELF (`vecAdd.elf`) into the simulated memory over TileLink.

Requires a RISC-V toolchain with fesvr installed at `$RISCV`.

```sh
cd tlmem && make
```

## Dependencies

| Dependency | Used by |
|-----------|---------|
| [Switchboard](https://github.com/zeroasiccorp/switchboard) | all |
| Verilator or Icarus Verilog | `tlloopback`, `tlmem` |
| fesvr (`$RISCV`) | `tlmem` |
