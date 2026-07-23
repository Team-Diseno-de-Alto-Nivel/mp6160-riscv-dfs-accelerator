.PHONY: all model program run run-emu run-native integration experiments clean paper paper-clean

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

clean:
	$(MAKE) -C src/model clean
	$(MAKE) -C src/program clean

paper:
	$(MAKE) -C docs/paper

paper-clean:
	$(MAKE) -C docs/paper clean
