cmake_minimum_required(VERSION 3.18)

# (0) choose an appropriate name for the server
project(driver-templ-server)

set(EXECUTABLE ${PROJECT_NAME})
file(GLOB_RECURSE SOURCES "src/*.c")
add_executable(${EXECUTABLE} ${SOURCES})

# (1) enable SALLY CSP support, if needed
#     you will have to add libcsp as submodule in ../lib!
#set(SERVICE_CSP ON)

# (2) ../lib is common to both server and client
add_subdirectory("../lib" "../build/lib")

# (3) create the platform-specific configuration for release and debug
if(CMAKE_BUILD_TYPE STREQUAL "Linux" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  # (3.1) add platform-specific backends as sources
  target_sources(${EXECUTABLE} PRIVATE
    "../lib/sally-backends/fs/posix_backend.c"
    "../lib/sally-backends/os/posix_backend.c"
    "../lib/sally-backends/com/posix_tty_backend.c"
  )

  # (3.2) set the definitions that can be used in code
  target_compile_definitions(${EXECUTABLE}
    PRIVATE FS_BACKEND=backend_fs_posix
    PRIVATE OS_BACKEND=backend_os_posix
    PRIVATE COM_BACKEND=backend_posix_tty
  )

  # (3.3) link the libraries that are needed for backends 
  target_link_libraries(${EXECUTABLE}
    pthread
  )
else()
  message(FATAL_ERROR "${CMAKE_BUILD_TYPE} is not a known build type")
endif()

set_property(CACHE CMAKE_BUILD_TYPE PROPERTY
  STRINGS ${ALLOWED_BUILD_TYPES}
)
message("Building for platform: ${CMAKE_BUILD_TYPE}")

target_include_directories(${EXECUTABLE}
    PRIVATE "src"
    PRIVATE "include"
)

target_link_libraries(${EXECUTABLE}
  srv-abs
)
