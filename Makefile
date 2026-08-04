.PHONY: all model program run run-emu run-native integration experiments demo hls-host hls-synth vivado-bd vivado-impl vivado-util vivado-bitstream onboard-export-cases onboard-deploy metrics clean paper paper-clean

all: model program

model:
	$(MAKE) -C src/model

program:
	$(MAKE) -C src/program

run: all
	./src/model/build/sim src/program/build/program

run-emu:
	$(MAKE) -C src/program run-emu

run-native:
	$(MAKE) -C src/program run-native

# INT-1: run the program<->model co-simulation over TLM (mock accelerator).
integration:
	$(MAKE) -C src/program integration

# INT-2: run OFF/ON experiments, collect CSV, and generate tables/plots.
experiments:
	bash scripts/run_experiments.sh
	python3 scripts/plot_results.py

# Reproducible one-shot demo (Avance II video): RISC-V emulation + SystemC co-simulation.
demo:
	bash scripts/demo.sh

# FPGA-1 (#62): functional check of the HLS kernel on the host, WITHOUT Vitis.
# Compiles the same source csynth will consume against the software reference and
# the 21 golden cases. Does NOT replace csynth/cosim (timing, AXI, RTL).
#
# Shared by hls-host and onboard-export-cases (#67) -- both need the kernel plus
# every algorithm/case source, just with a different entry point on top.
HLS_COMMON_SOURCES := \
	src/hls/dfs_accel.cpp \
	src/program/cases/datasets.cpp \
	src/program/harness/accelerator.cpp \
	src/program/algorithms/dfs_algorithm.cpp \
	src/program/algorithms/number_of_islands/number_of_islands.cpp \
	src/program/algorithms/unique_paths_iii/unique_paths_iii.cpp \
	src/program/algorithms/word_search_ii/word_search_ii.cpp \
	src/program/algorithms/longest_increasing_path/longest_increasing_path.cpp \
	src/program/algorithms/pacific_atlantic/pacific_atlantic.cpp

HLS_TB_SOURCES := src/hls/tb/dfs_accel_tb.cpp $(HLS_COMMON_SOURCES)
HLS_EXPORT_SOURCES := src/hls/tools/export_cases.cpp $(HLS_COMMON_SOURCES)

HLS_CXXFLAGS ?= -O2

hls-host:
	mkdir -p src/hls/build
	$(CXX) -std=c++17 $(HLS_CXXFLAGS) -Wall -Wextra -Werror \
	    -Wno-unknown-pragmas -Wno-unused-label \
	    -Isrc/hls -Isrc/program $(HLS_TB_SOURCES) -o src/hls/build/hls_host
	./src/hls/build/hls_host

# FPGA-6 (#67): exports the 21 golden cases to src/onboard/cases.json, packed
# exactly as dfs_accel() expects (see kernel_io.h) -- the fixture the
# on-board validator checks results against (validate_cynq.cpp; PYNQ's
# driver.py/validate.ipynb use the same fixture but are currently blocked,
# see src/onboard/README.md). Plain host build, no Vitis/Vivado needed.
onboard-export-cases:
	mkdir -p src/hls/build src/onboard
	$(CXX) -std=c++17 $(HLS_CXXFLAGS) -Wall -Wextra -Werror \
	    -Wno-unknown-pragmas -Wno-unused-label \
	    -Isrc/hls -Isrc/program $(HLS_EXPORT_SOURCES) -o src/hls/build/export_cases
	./src/hls/build/export_cases src/onboard/cases.json

# FPGA-2/2b/2c (#63/#90/#95): csim + csynth + cosim + export_design, real Vitis
# HLS run. Requires vitis_hls on PATH (source Vitis's settings64.sh first).
# Cheap CI-only check without a license: `tclsh src/hls/scripts/validate_run_hls.tcl`.
hls-synth:
	@command -v vitis_hls >/dev/null 2>&1 || { \
	    echo "error: vitis_hls not found on PATH -- source Vitis's settings64.sh first"; \
	    exit 1; \
	}
	vitis_hls -f src/hls/scripts/run_hls.tcl

# FPGA-3 (#64): builds the PS<->PL block design against the real dfs_accel IP
# exported by `make hls-synth`. Requires vivado on PATH and a real component.xml
# at src/hls/scripts/dfs_accel_prj/solution1/impl/ip/ (i.e. run hls-synth first).
# Cheap CI-only check without a license: `tclsh src/vivado/scripts/validate_build_bd.tcl`.
vivado-bd:
	@command -v vivado >/dev/null 2>&1 || { \
	    echo "error: vivado not found on PATH -- source Vivado's settings64.sh first"; \
	    exit 1; \
	}
	vivado -mode batch -source src/vivado/scripts/build_bd.tcl

# FPGA-5 (#66): synthesis + implementation (through route_design) of the
# dfs_system project from `make vivado-bd`, then timing closure report and
# achievable Fmax. Does not write a bitstream (#67) or capture utilization
# (#65) -- see run_impl.tcl's header for why. Requires vivado on PATH and
# the project from `make vivado-bd` to already exist.
# Cheap CI-only check without a license: `tclsh src/vivado/scripts/validate_run_impl.tcl`.
vivado-impl:
	@command -v vivado >/dev/null 2>&1 || { \
	    echo "error: vivado not found on PATH -- source Vivado's settings64.sh first"; \
	    exit 1; \
	}
	vivado -mode batch -source src/vivado/scripts/run_impl.tcl

# FPGA-4 (#65): resource utilization off the impl_1 run already routed by
# `make vivado-impl` (or `make vivado-bitstream`, which also leaves impl_1
# routed) -- no re-synth, just a report. Requires vivado on PATH.
vivado-util:
	@command -v vivado >/dev/null 2>&1 || { \
	    echo "error: vivado not found on PATH -- source Vivado's settings64.sh first"; \
	    exit 1; \
	}
	vivado -mode batch -source src/vivado/scripts/report_utilization.tcl

# FPGA-6 (#67): generates the on-board bitstream (.bit + .hwh) for on-board
# bring-up (loaded via CYNQ in practice -- see src/onboard/README.md; PYNQ
# was the original plan but is blocked on this board), fixed at 200 MHz -- see
# build_bitstream.tcl's header for why that's not the 204 MHz reported as
# Fmax by #66. Requires vivado on PATH and the project from `make vivado-bd`
# to already exist.
# Cheap CI-only check without a license: `tclsh src/vivado/scripts/validate_build_bitstream.tcl`.
vivado-bitstream:
	@command -v vivado >/dev/null 2>&1 || { \
	    echo "error: vivado not found on PATH -- source Vivado's settings64.sh first"; \
	    exit 1; \
	}
	vivado -mode batch -source src/vivado/scripts/build_bitstream.tcl

# FPGA-6 (#67): copies the on-board deliverables to the KV260 over SSH --
# the bitstream (from `make vivado-bitstream`, gitignored/local-only), the
# case fixture (from `make onboard-export-cases`), the working CYNQ validator
# (validate_cynq.cpp), and the PYNQ driver/notebook (currently blocked,
# kept for when/if it becomes usable -- see src/onboard/README.md).
#
# KV260_HOST has NO default on purpose: this targets a shared lab board
# where host/user/auth can change across sessions (see src/onboard/README.md)
# -- a wrong silent default risks scp-ing to the wrong machine. Set up an
# SSH config alias (e.g. `Host kria` with ProxyJump, if the board sits
# behind a jump host) and pass that alias here, e.g.:
#   make onboard-deploy KV260_HOST=kria
KV260_HOST ?=
KV260_DIR  ?= dfs_accel

ONBOARD_DEPLOY_FILES := \
	src/vivado/dfs_system/exports/dfs_system.bit \
	src/vivado/dfs_system/exports/dfs_system.hwh \
	src/onboard/cases.json \
	src/onboard/driver.py \
	src/onboard/validate.ipynb \
	src/onboard/validate_cynq.cpp

onboard-deploy:
	@if [ -z "$(KV260_HOST)" ]; then \
	    echo "error: set KV260_HOST=<ssh-alias-or-user@host> (e.g. make onboard-deploy KV260_HOST=kria)"; \
	    exit 1; \
	fi
	@for f in $(ONBOARD_DEPLOY_FILES); do \
	    if [ ! -f "$$f" ]; then \
	        echo "error: missing $$f -- run 'make vivado-bitstream' and/or 'make onboard-export-cases' first"; \
	        exit 1; \
	    fi; \
	done
	ssh $(KV260_HOST) 'mkdir -p $(KV260_DIR)'
	scp $(ONBOARD_DEPLOY_FILES) $(KV260_HOST):$(KV260_DIR)/

# FPGA-7 (#68): capture whatever HLS/Vivado reports exist right now into
# results/hw_metrics.csv. No tool dependency beyond awk/bash -- safe to run
# any time, degrades to NA for stages not run yet.
metrics:
	./scripts/extract_hw_metrics.sh

clean:
	$(MAKE) -C src/model clean
	$(MAKE) -C src/program clean

paper:
	$(MAKE) -C docs/paper

paper-clean:
	$(MAKE) -C docs/paper clean
