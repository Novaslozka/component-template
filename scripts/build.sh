#!/bin/bash

set -e

# class is either server or client
CLASS="$1"

# type is the build type/platform (Linux, Debug, STM32 etc.)
TYPE="$2"

# build flags is everything else, this is passed directly to CMake
BUILD_FLAGS="${@:3}"
echo "$BUILD_FLAGS"

mkdir -p build/$CLASS-$TYPE
cd build/$CLASS-$TYPE

cmake -DCMAKE_BUILD_TYPE=$TYPE -DCMAKE_BUILD_CLASS=$CLASS -G "CodeBlocks - Unix Makefiles" $BUILD_FLAGS ../..

make -j4
