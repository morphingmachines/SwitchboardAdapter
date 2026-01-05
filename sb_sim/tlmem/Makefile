# Copyright (c) 2024 Zero ASIC Corporation
# This code is licensed under Apache License 2.0 (see LICENSE for details)

EXDIR := $(abspath ..)
SBDIR := $(shell switchboard --path)


.PHONY: verilator
verilator: client
	./build.py --tool verilator

.PHONY: icarus
icarus: client
	./build.py --tool icarus

LDFLAGS += -L$(RISCV)/lib -lfesvr
CFLAGS += -I$(RISCV)/include

client: client.cc $(SBDIR)/cpp/switchboard.hpp \
		../include/tilelinklib.h ../include/tilelinklib.hpp  
	g++ -std=c++17 -I$(SBDIR)/cpp -I../include $(CFLAGS) $< -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f client
	rm -f queue-* *.q
	rm -f *.vcd *.fst *.fst.hier
	rm -f *.o *.vpi *.vvp
	rm -rf build
	rm -rf obj_dir
