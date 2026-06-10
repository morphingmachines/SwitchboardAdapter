cmake_minimum_required(VERSION 3.14)
project(Minimal CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Resolve switchboard include path at configure time
execute_process(
    COMMAND switchboard --path
    OUTPUT_VARIABLE SBDIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE SBDIR_RESULT
)
if(NOT SBDIR_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to run 'switchboard --path'. Is switchboard installed?")
endif()

# client executable (match Makefile: only client.cc)
add_executable(client
    client.cc
)

set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(client PRIVATE
    "${SBDIR}/cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
)

# ---------------------------------------------------------------------------
# Verilator targets
# ---------------------------------------------------------------------------

option(TRACE "Enable FST waveform tracing" OFF)

# Shared commands used by both verilator targets.
set(BUILD_PY "${CMAKE_CURRENT_SOURCE_DIR}/build.py" $<$<BOOL:${TRACE}>:--trace>)

# verilator: incremental — skips unchanged RTL (fast=True in build.py).
add_custom_target(verilator
    COMMAND ${BUILD_PY} 2>&1
    DEPENDS client
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Verilating RTL and running simulation"
)

# verilator-rebuild: full recompile (fast=False). Use when DPI sources change.
add_custom_target(verilator-rebuild
    COMMAND ${BUILD_PY} --rebuild 2>&1
    DEPENDS client
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Verilating RTL and running simulation (force rebuild)"
)

# clean-extra: remove artifacts not tracked by CMake's own clean.
add_custom_target(clean-extra
    COMMAND ${CMAKE_COMMAND} -E rm -f queue-* *.q *.vcd *.fst *.fst.hier *.o *.vpi *.vvp client
    COMMAND ${CMAKE_COMMAND} -E rm -rf rtl_build obj_dir
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Removing simulation artifacts"
)
