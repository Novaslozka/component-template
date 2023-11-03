#!/bin/bash

meson -Denable_csp=true -Dbuild_class=client -Dbuild_autotest=false -Dbuild_cmdline=plugin build
cd build
ninja
