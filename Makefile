.PHONY: all model program run clean paper paper-clean

all: model program

model:
	$(MAKE) -C src/model

program:
	$(MAKE) -C src/program

run: all
	./src/model/build/sim src/program/build/program

clean:
	$(MAKE) -C src/model clean
	$(MAKE) -C src/program clean

paper:
	$(MAKE) -C docs/paper

paper-clean:
	$(MAKE) -C docs/paper clean
