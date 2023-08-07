cmake_minimum_required(VERSION 3.18)

project(driver-libs)

# put any additional libraries here as CMake subdirectories
# sally-backends and service-abstraction are excluded intentionally!
# these are only built in some configurations (like autotest)
# in others, there is a higher-level user (flight SW, VCOM etc.) providing them
