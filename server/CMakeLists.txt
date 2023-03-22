cmake_minimum_required(VERSION 3.18)

# (1) choose an appropriate name for the server
project(driver-templ-server)

set(EXECUTABLE ${PROJECT_NAME})
file(GLOB_RECURSE SOURCES "src/*.c")
add_executable(${EXECUTABLE} ${SOURCES})
#set(CMAKE_BUILD_TYPE Debug)

# (2) enable SALLY CSP support, if needed
#     you will have to add libcsp as submodule in ../lib!
#set(SERVICE_CSP ON)

# (3) ../lib is common to both server and client
add_subdirectory("../lib" "../build/lib")

# (4) add platform-specific backends as sources
target_sources(${EXECUTABLE} PRIVATE
  "../lib/sally-backends/fs/posix_backend.c"
  "../lib/sally-backends/os/posix_backend.c"
  "../lib/sally-backends/com/posix_tty_backend.c"
)

# (5) now set the definitions that can be used in code
target_compile_definitions(${EXECUTABLE}
  PRIVATE FS_BACKEND=backend_fs_posix
  PRIVATE OS_BACKEND=backend_os_posix
  PRIVATE COM_BACKEND=backend_posix_tty
)

# (6) link the libraries that are needed for backends 
target_link_libraries(${EXECUTABLE}
  pthread
)

target_include_directories(${EXECUTABLE}
    PRIVATE "src"
    PRIVATE "include"
)

target_link_libraries(${EXECUTABLE}
  srv-abs
)
