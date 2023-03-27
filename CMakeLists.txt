cmake_minimum_required(VERSION 3.18)

# (0) choose an appropriate name for the driver
set(DRIVER_NAME driver-templ)

# (1) enable SALLY CSP support, if needed
#     you will have to add libcsp as submodule in ../lib!
#set(SERVICE_CSP ON)

# (2) create the platform-specific configuration for release and debug
if(CMAKE_BUILD_TYPE STREQUAL "Linux" OR CMAKE_BUILD_TYPE STREQUAL "Debug")
  # (2.1) set platform-specific backend sources
  set(BACKEND_SOURCES
    "lib/sally-backends/fs/posix_backend.c"
    "lib/sally-backends/os/posix_backend.c"
    "lib/sally-backends/com/posix_tty_backend.c"
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
else()
  message(FATAL_ERROR "${CMAKE_BUILD_TYPE} is not a known build type")
endif()

# check the build class
if(NOT((CMAKE_BUILD_CLASS STREQUAL "client") OR (CMAKE_BUILD_CLASS STREQUAL "server")))
  message(FATAL_ERROR "${CMAKE_BUILD_CLASS} is not a known build class (must be \"server\" or \"client\")")
endif()

# user config is done, create the project
project(${DRIVER_NAME}-${CMAKE_BUILD_CLASS})

# set name for the static library and autotest executable
set(STATIC_LIB ${PROJECT_NAME})
if(${BUILD_AUTOTEST})
  set(AUTOTEST_EXE ${PROJECT_NAME}-bin)
endif()

# set sources for library and executable
file(GLOB_RECURSE LIB_SOURCES "${CMAKE_BUILD_CLASS}/src/${CMAKE_BUILD_CLASS}.c")
if(${BUILD_AUTOTEST})
  file(GLOB_RECURSE BIN_SOURCES "autotest/${CMAKE_BUILD_CLASS}/${CMAKE_BUILD_CLASS}.c")
endif()

# add both targets
add_library(${STATIC_LIB} STATIC ${LIB_SOURCES})
if(${BUILD_AUTOTEST})
  add_executable(${AUTOTEST_EXE} ${BIN_SOURCES})
endif()

# set common libraries
add_subdirectory("lib" "build/lib")
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
message("Build class: ${CMAKE_BUILD_CLASS}")
message("Build type:  ${CMAKE_BUILD_TYPE}")
message("Autotest:    ${BUILD_AUTOTEST}")

# set include dirs
set(INCLUDE_DIRS 
  PRIVATE "${CMAKE_BUILD_CLASS}/src"
  PRIVATE "${CMAKE_BUILD_CLASS}/include"
)

# link the common libs to the static lib
target_include_directories(${STATIC_LIB} ${INCLUDE_DIRS})
target_link_libraries(${STATIC_LIB} ${LIBS_COMMON})

# link the common libs and static lib to executable
if(${BUILD_AUTOTEST})
  target_include_directories(${AUTOTEST_EXE} ${INCLUDE_DIRS})
  target_link_libraries(${AUTOTEST_EXE} ${LIBS_COMMON} ${STATIC_LIB})
endif()

# for client, we also have to link the server library, as it contains packet definitions
if(${CMAKE_BUILD_CLASS} STREQUAL "client")
  set(SERVER_LIB_HEADER "server/include")
  set(SERVER_LIB ${CMAKE_CURRENT_BINARY_DIR}/../server-${CMAKE_BUILD_TYPE}/lib${DRIVER_NAME}-server.a)
  target_include_directories(${STATIC_LIB} PRIVATE ${SERVER_LIB_HEADER})
  target_link_libraries(${STATIC_LIB} ${SERVER_LIB})

  if(${BUILD_AUTOTEST})
    target_include_directories(${AUTOTEST_EXE} PRIVATE ${SERVER_LIB_HEADER})
    target_link_libraries(${AUTOTEST_EXE} ${SERVER_LIB})
  endif()
endif()
