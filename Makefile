 .PHONY: all model program run run-emu run-native integration experiments demo hls-host hls-synth vivado-bd vivado-impl clean paper paper-clean
 
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
 HLS_TB_SOURCES := \
 	src/hls/dfs_accel.cpp \
 	src/hls/tb/dfs_accel_tb.cpp \
 	src/program/cases/datasets.cpp \
 	src/program/harness/accelerator.cpp \
 	src/program/algorithms/dfs_algorithm.cpp \
 	src/program/algorithms/number_of_islands/number_of_islands.cpp \
 	src/program/algorithms/unique_paths_iii/unique_paths_iii.cpp \
 	src/program/algorithms/word_search_ii/word_search_ii.cpp \
 	src/program/algorithms/longest_increasing_path/longest_increasing_path.cpp \
 	src/program/algorithms/pacific_atlantic/pacific_atlantic.cpp
 
 HLS_CXXFLAGS ?= -O2
 
 hls-host:
 	mkdir -p src/hls/build
 	$(CXX) -std=c++17 $(HLS_CXXFLAGS) -Wall -Wextra -Werror \
 	    -Wno-unknown-pragmas -Wno-unused-label \
 	    -Isrc/hls -Isrc/program $(HLS_TB_SOURCES) -o src/hls/build/hls_host
 	./src/hls/build/hls_host
 
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
 
 # FPGA-4 (#65): Captures real, post-route resource utilization (LUT/FF/BRAM/DSP)
 # for `dfs_system`, replacing Vitis HLS's pre-Vivado estimates (already
 # recorded from #90) with actual numbers from the implemented design.
 # Run `make vivado-bd` then `make vivado-impl` first; this target only
 # extracts the existing post-route utilization report. Requires vivado on PATH.
 vivado-util:
 	@command -v vivado >/dev/null 2>&1 || { \
 	    echo "error: vivado not found on PATH -- source Vivado's settings64.sh first"; \
 	    exit 1; \
 	}
 	vivado -mode batch -source src/vivado/scripts/report_utilization.tcl
 
 clean:
 	$(MAKE) -C src/model clean
 	$(MAKE) -C src/program clean
 
 paper:
 	$(MAKE) -C docs/paper
 
 paper-clean:
 	$(MAKE) -C docs/paper clean
 
.PHONY: all model program run run-emu run-native integration experiments demo hls-host hls-synth vivado-bd vivado-impl clean paper paper-clean

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
HLS_TB_SOURCES := \
	src/hls/dfs_accel.cpp \
	src/hls/tb/dfs_accel_tb.cpp \
	src/program/cases/datasets.cpp \
	src/program/harness/accelerator.cpp \
	src/program/algorithms/dfs_algorithm.cpp \
	src/program/algorithms/number_of_islands/number_of_islands.cpp \
	src/program/algorithms/unique_paths_iii/unique_paths_iii.cpp \
	src/program/algorithms/word_search_ii/word_search_ii.cpp \
	src/program/algorithms/longest_increasing_path/longest_increasing_path.cpp \
	src/program/algorithms/pacific_atlantic/pacific_atlantic.cpp

HLS_CXXFLAGS ?= -O2

hls-host:
	mkdir -p src/hls/build
	$(CXX) -std=c++17 $(HLS_CXXFLAGS) -Wall -Wextra -Werror \
	    -Wno-unknown-pragmas -Wno-unused-label \
	    -Isrc/hls -Isrc/program $(HLS_TB_SOURCES) -o src/hls/build/hls_host
	./src/hls/build/hls_host

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

clean:
	$(MAKE) -C src/model clean
	$(MAKE) -C src/program clean

paper:
	$(MAKE) -C docs/paper

paper-clean:
	$(MAKE) -C docs/paper clean
