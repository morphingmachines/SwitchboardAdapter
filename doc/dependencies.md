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

[Conda env](https://conda.io/projects/conda/en/latest/user-guide/getting-started.html)
==============================
- Install `switchboard` in a conda env to keep things tidy. Follow below steps to install conda.
```sh
mkdir -p ~/miniconda3
wget https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh -O ~/miniconda3/miniconda.sh
bash ~/miniconda3/minconda.sh -b -u -p ~/miniconda3
rm -rf ~/miniconda3/miniconda.sh
```

- Create a conda environment for `switchboard`
```sh
conda create --name switchboard
```

[Switchboard](https://github.com/zeroasiccorp/switchboard)
====================================
Follow this [link](https://github.com/zeroasiccorp/switchboard/tree/main/examples/umiram#readme) for detailed instructions.

```sh
git clone https://github.com/zeroasiccorp/switchboard.git
cd switchboard
git submodule update --init
```

- Install all require python packages within `switchboard` conda environment
```sh
conda activate switchboard
conda install pip
pip install --upgrade pip
pip install -e .
pip install -r examples/requirements.txt
```

<!---
Gtkwave to view waveforms
-->
