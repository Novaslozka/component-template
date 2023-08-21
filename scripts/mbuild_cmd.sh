#!/bin/bash

meson -Dbuild_class=client -Dbuild_autotest=false -Dbuild_cmdline=true build
cd build
ninja
