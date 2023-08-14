# Component Driver Template
This repository contains the template project to be used for implementation of drivers for various flight hardware procured from 3rd parties, or internally developed - reaction wheels, gyroscopes, or full subsystems.

Rationale is to create a unified driver structure that enables various levels of hardware- and software-in-the-loop testing.

For further details, see [the Doxygen main page](doc/doxygen_mainpage.md).

## How to use

This template should be forked to whichever group it is most applicable.
After forking, make sure to correct the `.gitmodules` paths, or initialising the submodules will fail.

## Documentation

To generate the documentation for this repository, go to the [doc/generated](doc/generated) folder and run `doxygen`

The documentation will be accessible through the .html files that were generated.
