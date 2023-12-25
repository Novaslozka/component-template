#!/bin/bash

prefix="/opt/vzlu"
meson setup --prefix "$prefix" \
 --libdir lib \
 --pkg-config-path "$prefix/lib/pkgconfig" \
 -Dbuild_class=client \
 -Dbuild_autotest=false \
 -Dbuild_cmdline=plugin build
cd build
ninja