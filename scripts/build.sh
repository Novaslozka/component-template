#!/bin/bash

set -e

if [ "$#" -eq 0 ]; then
  BUILD_FLAGS=""
else
  BUILD_FLAGS="$@"
fi

echo "$BUILD_FLAGS"
mkdir -p build/server
cd build/server

cmake -G "CodeBlocks - Unix Makefiles" $BUILD_FLAGS ../../server

make -j4
