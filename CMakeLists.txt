cmake_minimum_required(VERSION 3.18)

# (0) choose an appropriate name for the driver
set(DRIVER_NAME driver-templ)

# (1) enable SALLY CSP support, if needed
#     you will have to add libcsp as submodule in ../lib!
#set(SERVICE_CSP ON)

# common config that doesn't need to be changed
project(${DRIVER_NAME}-${CMAKE_BUILD_CLASS})
set(EXECUTABLE ${PROJECT_NAME})
file(GLOB_RECURSE SOURCES "${CMAKE_BUILD_CLASS}/src/*.c")
add_executable(${EXECUTABLE} ${SOURCES})
add_subdirectory("lib" "build/lib")

# (2) create the platform-specific configuration for release and debug
if(CMAKE_BUILD_TYPE STREQUAL "Linux" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  # (2.1) add platform-specific backends as sources
  target_sources(${EXECUTABLE} PRIVATE
    "lib/sally-backends/fs/posix_backend.c"
    "lib/sally-backends/os/posix_backend.c"
    "lib/sally-backends/com/posix_tty_backend.c"
  )

  # (2.2) set the definitions that can be used in code
  target_compile_definitions(${EXECUTABLE}
    PRIVATE FS_BACKEND=backend_fs_posix
    PRIVATE OS_BACKEND=backend_os_posix
    PRIVATE COM_BACKEND=backend_posix_tty
  )

  # (2.3) link the libraries that are needed for backends 
  target_link_libraries(${EXECUTABLE}
    pthread
  )
else()
  message(FATAL_ERROR "${CMAKE_BUILD_TYPE} is not a known build type")
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
  STRINGS ${ALLOWED_BUILD_TYPES}
)
message("Build class: ${CMAKE_BUILD_CLASS}")
message("Build type:  ${CMAKE_BUILD_TYPE}")

target_include_directories(${EXECUTABLE}
  PRIVATE "${CMAKE_BUILD_CLASS}/src"
  PRIVATE "${CMAKE_BUILD_CLASS}/include"
)

target_link_libraries(${EXECUTABLE}
  srv-abs
)
