#!/bin/bash

set -e

if [ "$#" -eq 0 ]; then
  BUILD_FLAGS=""
else
  BUILD_FLAGS="$@"
fi

echo "$BUILD_FLAGS"
mkdir -p build
cd build

cmake -G "CodeBlocks - Unix Makefiles" $BUILD_FLAGS ..

make -j4
