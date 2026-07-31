.PHONY: all model program run run-emu run-native integration experiments demo hls-host clean paper paper-clean

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

clean:
	$(MAKE) -C src/model clean
	$(MAKE) -C src/program clean

paper:
	$(MAKE) -C docs/paper

paper-clean:
	$(MAKE) -C docs/paper clean
