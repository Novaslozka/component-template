# Component Driver Template
This repository contains the template project to be used for implementation of drivers for various flight hardware procured from 3rd parties, or internally developed - reaction wheels, gyroscopes, or full subsystems.

Rationale is to create a unified driver structure that enables various levels of hardware- and software-in-the-loop testing.

For further details, see [the Doxygen main page](doc/doxygen_mainpage.md).

## How to use

Usually, the easiest/fastest option is to fork this repository. When the target repository is to be hosted somewhere other than GitLab.com, then the workflow is as follows:

```
$ git clone git@gitlab.com:vzlu/sw/component-template.git <your-component-name>
$ git remote rename origin template
$ git remote add origin <path-to-your-origin>
$ git push -u origin --all
$ git push -u origin --tags
```

After this, you can use this template for your SW driver implementation. Usually this will involve:

1. Updating the configuration file (meson.build or CMakeLists.txt) to fit your target platform, backends and source files. The numbered steps in the file will help you get started, however, knowledge of the build system is assumed.
2. Adding source files for client, server, autotests, commands etc.
3. Building and testing your client/server or deploying command plugins.

## Build

There are two options how to build the template and its tests: Meson build (preferred) and CMake (legacy). Building by Meson is done by executing the build script `./scripts/mbuild.sh`. When building with Meson, the following dependencies must be available as shared library:

* lpldgen: https://gitlab.com/vzlu/sw/lpldgen
* libcommands: https://gitlab.com/vzlu/sw/libcommands
* libcsp (VZLU fork): https://gitlab.com/vzlu/sw/libcsp
* libconsole: https://gitlab.com/vzlu/sw/lib-console

To install them as shared library into the default location (e.g., `/usr/local/lib/x86_64-linux-gnu/` on Debian-based Linux), clone the repository and then run the following:

```
meson build
cd build && ninja install
```

Building by CMake can be done by targets in the provided Makefile (e.g., `make test`), but it should be noted CMake support is for legacy projects only and its use is strongly discouraged for new development.

## Documentation

To generate the documentation for this repository, go to the [doc/generated](doc/generated) folder and run `doxygen`

The documentation will be accessible through the .html files that were generated.
