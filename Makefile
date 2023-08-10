# This Makefile acts as a launcher for the various build / program scripts.
# The actual build is done using CMake!

.PHONY: default all

default:
	@echo "Please specify a target."

clean:
	./scripts/clean.sh

dbg:
	./scripts/build.sh server Linux -DCMAKE_BUILD_TYPE=Debug
	./scripts/build.sh client Linux -DCMAKE_BUILD_TYPE=Debug

all:
# build server first, we need the library for the client autotesting
	./scripts/build.sh server Linux
	./scripts/build.sh client Linux

test:
# build server and client first, we need the libraries for the autotesting
	./scripts/build.sh server Linux
	./scripts/build.sh client Linux
	./scripts/build.sh combined Linux
# now launch the test - ran with --no-fork option
# so that munit doesn't try to execute tests in child processes
	./build/combined-Linux/driver-templ-combined-bin --no-fork

dbg-test:
# build server and client, we need the libraries for the autotesting
	./scripts/build.sh server Linux -DCMAKE_BUILD_TYPE=Debug
	./scripts/build.sh client Linux -DCMAKE_BUILD_TYPE=Debug
	./scripts/build.sh combined Linux -DCMAKE_BUILD_TYPE=Debug
	gdb --args ./build/combined-Linux/driver-templ-combined-bin --no-fork
