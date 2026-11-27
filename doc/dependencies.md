Verilator
=================
* Follow this [link](https://verilator.org/guide/latest/install.html) for detailed instructions.
* Install Verilator using **Run-in-Place from `VERILATOR_ROOT`** installation option.

```sh
git clone https://github.com/verilator/verilator 
cd verilator
git tag                     # See what versions exit
#git checkout stable        # Use most recent release
#git checkout v{version}    # Switch to specified release version

autoconf # create ./configure script

export VERILATOR_ROOT=`pwd`
./configure
make -j$(nproc)
```
Add `$VERILATOR_ROOT/bin` to `PATH` environment variable.

Python packages
===============
Use the system Python 3.10 (`/usr/bin/python3`) with packages installed into
your user site-packages (`~/.local`) via pip. `sim_build` is tested with these
versions:

```sh
python3 -m pip install --user \
    switchboard-hw==0.3.4 \
    siliconcompiler==0.38.1 \
    umi==0.4.15 \
    pytest               # only needed to run sim_build/tests
```

Check the install:

```sh
python3 -c "import switchboard, siliconcompiler; print(switchboard.__file__)"
```

[Switchboard](https://github.com/zeroasiccorp/switchboard) source (optional)
====================================
Only needed to read or debug switchboard itself; the pip package above is enough
to build and run the simulations. See the
[switchboard examples](https://github.com/zeroasiccorp/switchboard/tree/main/examples/umiram#readme).

```sh
git clone https://github.com/zeroasiccorp/switchboard.git
cd switchboard
git submodule update --init
```

<!---
Gtkwave to view waveforms
-->
