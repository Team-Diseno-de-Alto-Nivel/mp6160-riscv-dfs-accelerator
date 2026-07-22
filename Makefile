.PHONY: all model program run run-emu run-native clean paper paper-clean

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

clean:
	$(MAKE) -C src/model clean
	$(MAKE) -C src/program clean

paper:
	$(MAKE) -C docs/paper

paper-clean:
	$(MAKE) -C docs/paper clean
