#!/bin/bash

meson -Dbuild_class=client -Dbuild_autotest=false -Dbuild_cmdline=plugin -Ddefault_library=static build
cd build
ninja
