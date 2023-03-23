#!/bin/bash

set -e

PLATFORM="$1"
BUILD_FLAGS="${@:2}"

echo "$BUILD_FLAGS"
mkdir -p build/server-$PLATFORM
cd build/server-$PLATFORM

cmake -DCMAKE_BUILD_TYPE=$PLATFORM -G "CodeBlocks - Unix Makefiles" $BUILD_FLAGS ../../server

make -j4
