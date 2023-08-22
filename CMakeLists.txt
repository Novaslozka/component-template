cmake_minimum_required(VERSION 3.18)

# (0) choose an appropriate name for the driver
set(DRIVER_NAME driver-templ)

# (1) enable SALLY CSP support, if needed
#     you will have to make sure that libcsp is available!
#set(SERVICE_CSP ON)

# (2) create the platform-specific configuration for release and debug
if(CMAKE_BUILD_PLATFORM STREQUAL "Linux")
  # (2.1) set platform-specific backend sources
  set(BACKEND_SOURCES
    "lib/sally-backends/fs/posix_backend.c"
    "lib/sally-backends/os/posix_backend.c"
    "lib/sally-backends/com/posix_tty_backend.c"
    "lib/sally-backends/com/memcom_backend.c"
  )

  # (2.2) set the definitions that can be used in code
  set(BACKEND_DEFINITIONS
    PRIVATE FS_BACKEND=backend_fs_posix
    PRIVATE OS_BACKEND=backend_os_posix
    PRIVATE COM_BACKEND=backend_posix_tty
  )

  # (2.3) set the libraries that are needed for backends 
  set(BACKEND_LIBS pthread)

  # (2.4) enable building autotest binaries for this platform
  set(BUILD_AUTOTEST ON)

  # (2.5) disable building commandline commands for this platform
  set(BUILD_CMDLINE OFF)
else()
  message(FATAL_ERROR "${CMAKE_BUILD_PLATFORM} is not a known build type")
endif()

# check the build class
if(NOT((CMAKE_BUILD_CLASS STREQUAL "client") OR (CMAKE_BUILD_CLASS STREQUAL "server") OR (CMAKE_BUILD_CLASS STREQUAL "combined")))
  message(FATAL_ERROR "${CMAKE_BUILD_CLASS} is not a known build class (must be \"server\" or \"client\" or \"combined\")")
endif()

# set paths to the different build classes
# the path is different from class on purpose to avoid confusion
if(CMAKE_BUILD_CLASS STREQUAL "client")
  set(CMAKE_BUILD_CLASS_PATH "cli")
endif()

if(CMAKE_BUILD_CLASS STREQUAL "server")
  set(CMAKE_BUILD_CLASS_PATH "srv")
endif()

if(CMAKE_BUILD_CLASS STREQUAL "combined")
  set(CMAKE_BUILD_CLASS_PATH "comb")
endif()

# user config is done, create the project
project(${DRIVER_NAME}-${CMAKE_BUILD_CLASS})

# set name for the static library and autotest executable
set(STATIC_LIB ${PROJECT_NAME})
if(${BUILD_AUTOTEST})
  # SALLY should be provided by the higher-level user (e.g. VCOM or OBSW),
  # unless this is autotest, where there is no higher-level user
  add_subdirectory("lib/service-abstraction")
  set(AUTOTEST_EXE ${PROJECT_NAME}-bin)
endif()

# set sources for library and executable
file(GLOB_RECURSE LIB_SOURCES "${CMAKE_BUILD_CLASS_PATH}/src/*.c")
if(${BUILD_AUTOTEST})
  file(GLOB_RECURSE BIN_SOURCES
    "autotest/${CMAKE_BUILD_CLASS_PATH}/*.c"
    "autotest/munit/munit.c")
endif()

# if client commands are to be built, add them to sources
if(${BUILD_CMDLINE})
  file(GLOB_RECURSE CMD_SOURCES "cli/cmd/*.c")
  list(APPEND LIB_SOURCES ${CMD_SOURCES})
endif()

# add lpldgen if needed
if(${ENABLE_LPLDGEN})
  file(GLOB_RECURSE LPLDGEN_SOURCES "common/lpldgen/src/*.c")
  list(APPEND LIB_SOURCES ${LPLDGEN_SOURCES})
endif()

# add both targets
add_library(${STATIC_LIB} STATIC ${LIB_SOURCES})
if(${BUILD_AUTOTEST})
  add_executable(${AUTOTEST_EXE} ${BIN_SOURCES})
  target_include_directories(${AUTOTEST_EXE} PRIVATE "autotest/munit")
endif()

# set common libraries
set(LIBS_COMMON srv-abs)

# set user-defined properties for static library 
target_sources(${STATIC_LIB} PRIVATE ${BACKEND_SOURCES})
target_compile_definitions(${STATIC_LIB} PRIVATE ${BACKEND_DEFINITIONS})
target_link_libraries(${STATIC_LIB} ${BACKEND_LIBS})

# set user-defined properties for executable
if(${BUILD_AUTOTEST})
  target_sources(${AUTOTEST_EXE} PRIVATE ${BACKEND_SOURCES})
  target_compile_definitions(${AUTOTEST_EXE} PRIVATE ${BACKEND_DEFINITIONS})
  target_link_libraries(${AUTOTEST_EXE} ${BACKEND_LIBS})
endif()

# let the user know what is being built
message("Driver:          ${DRIVER_NAME}")
message("Build class:     ${CMAKE_BUILD_CLASS}")
message("Build platform:  ${CMAKE_BUILD_PLATFORM}")
message("Autotest:        ${BUILD_AUTOTEST}")

# set include dirs
set(INCLUDE_DIRS 
  PRIVATE "${CMAKE_BUILD_CLASS_PATH}/src"
  PRIVATE "${CMAKE_BUILD_CLASS_PATH}/include"
  PRIVATE "common/include"
)

# add lpldgen if needed
if(${ENABLE_LPLDGEN})
  configure_file(
    "common/lpldgen/include/conf_payloads.h.in"
    "common/include/conf_payloads.h"
  )
  list(APPEND INCLUDE_DIRS
    PRIVATE "common/lpldgen/include"
    PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/common/include"
  )
endif()

# link the common libs to the static lib
target_include_directories(${STATIC_LIB} ${INCLUDE_DIRS})
target_link_libraries(${STATIC_LIB} ${LIBS_COMMON})

# link the common libs and static lib to executable
if(${BUILD_AUTOTEST})
  target_include_directories(${AUTOTEST_EXE} ${INCLUDE_DIRS})
  target_link_libraries(${AUTOTEST_EXE} ${LIBS_COMMON} ${STATIC_LIB})
endif()

# if client commands are to be built, add the commands headers and link libcommands
if(${BUILD_CMDLINE})
  target_include_directories(${STATIC_LIB} PUBLIC "cli/include")
  target_link_libraries(${STATIC_LIB} commands)
endif()

# for combined build, link both client and server
# this assumes both server and client libraries are already built!
if(${CMAKE_BUILD_CLASS} STREQUAL "combined")
  set(SERVER_LIB_HEADER "srv/include")
  set(CLIENT_LIB_HEADER "cli/include")
  set(SERVER_LIB ${CMAKE_CURRENT_BINARY_DIR}/../server-${CMAKE_BUILD_PLATFORM}/lib${DRIVER_NAME}-server.a)
  set(CLIENT_LIB ${CMAKE_CURRENT_BINARY_DIR}/../client-${CMAKE_BUILD_PLATFORM}/lib${DRIVER_NAME}-client.a)

  target_include_directories(${STATIC_LIB} PRIVATE ${SERVER_LIB_HEADER} ${CLIENT_LIB_HEADER})
  target_link_libraries(${STATIC_LIB} ${SERVER_LIB} ${CLIENT_LIB})

  if(${BUILD_AUTOTEST})
    target_include_directories(${AUTOTEST_EXE} PRIVATE ${SERVER_LIB_HEADER} ${CLIENT_LIB_HEADER})
    target_link_libraries(${AUTOTEST_EXE} ${SERVER_LIB} ${CLIENT_LIB})
  endif()
endif()
