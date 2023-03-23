# This Makefile acts as a launcher for the various build / program scripts.
# The actual build is done using CMake!

.PHONY: default all

default:
	@echo "Please specify a target."

clean:
	./scripts/clean.sh

dbg:
	./scripts/build.sh Debug

all:
	./scripts/build.sh Linux
