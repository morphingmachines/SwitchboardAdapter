project = switchboard

TARGET ?= "Loopback"

# Toolchains and tools
MILL = ./../playground/mill

-include ../playground/Makefile.include

# Targets

rtl:check-firtool ## Generates Verilog code from Chisel sources (output to ./generated_sv_dir)
	$(MILL) $(project).runMain $(project).rtlMain $(TARGET)

lazyrtl:check-firtool ## Generates Verilog code from Chisel sources (output to ./generated_sv_dir)
	$(MILL) $(project).runMain $(project).lazyrtlMain $(TARGET)

check: test
.PHONY: test
test:check-firtool ## Run Chisel tests
	$(MILL) $(project).test.testOnly $(project).serdes.TestSerDesLoopBack
	@echo "If using WriteVcdAnnotation in your tests, the VCD files are generated in ./test_run_dir/testname directories."
