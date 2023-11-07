#!/bin/bash

prefix="/opt/vzlu"
meson --prefix "$prefix" \
 --libdir lib \
 --pkg-config-path "$prefix/lib/pkgconfig" \
 -Dbuild_class=client \
 -Dbuild_autotest=false \
 -Dbuild_cmdline=plugin build
cd build
ninja