# Convention: "<package>.<Module>.<Config>/chisel_gen_rtl" — TOP_MODULE is the middle component.
CHISEL_GEN_RTL_DIR = "switchboard.Minimal.None/chisel_gen_rtl"
TOP_MODULE = CHISEL_GEN_RTL_DIR.split(".")[-2]
