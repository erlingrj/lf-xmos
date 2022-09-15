#!/usr/bin/env bash
echo Running build_xmos_unix.sh

# Exit on first error
set -e

# Project root is one up from the bin directory.
PROJECT_ROOT=$LF_BIN_DIRECTORY/..
# FIXME: How do we get the name of the LF program?
APP_NAME=multithread
# Copy platform into /core
cp $PROJECT_ROOT/platform/lf_xmos_support.c $LF_SOURCE_GEN_DIRECTORY/core/platform/
cp $PROJECT_ROOT/platform/lf_xmos_support.h $LF_SOURCE_GEN_DIRECTORY/core/platform/
cp $PROJECT_ROOT/platform/platform.h $LF_SOURCE_GEN_DIRECTORY/core/
cp $PROJECT_ROOT/platform/reactor.c $LF_SOURCE_GEN_DIRECTORY/core/
cp $PROJECT_ROOT/platform/reactor_common.c $LF_SOURCE_GEN_DIRECTORY/core/

# Copy platform into /include/core
# TODO: Why are there two generated core dirs
cp $PROJECT_ROOT/platform/lf_xmos_support.c $LF_SOURCE_GEN_DIRECTORY/include/core/platform/
cp $PROJECT_ROOT/platform/lf_xmos_support.h $LF_SOURCE_GEN_DIRECTORY/include/core/platform/
cp $PROJECT_ROOT/platform/platform.h $LF_SOURCE_GEN_DIRECTORY/include/core/
cp $PROJECT_ROOT/platform/reactor.c $LF_SOURCE_GEN_DIRECTORY/include/core/
cp $PROJECT_ROOT/platform/reactor_common.c $LF_SOURCE_GEN_DIRECTORY/include/core/

# Create CMake file
printf '
cmake_minimum_required(VERSION 3.24)
project(%s LANGUAGES C)
## Specify your application sources and includes
if(DEFINED XMOS_TOOLS_PATH)
    set(CMAKE_C_COMPILER "${XMOS_TOOLS_PATH}/xcc")
    set(CMAKE_CXX_COMPILER  "${XMOS_TOOLS_PATH}/xcc")
    set(CMAKE_ASM_COMPILER  "${XMOS_TOOLS_PATH}/xcc")
    set(CMAKE_AR "${XMOS_TOOLS_PATH}/xmosar" CACHE FILEPATH "Archiver")
    set(CMAKE_C_COMPILER_AR "${XMOS_TOOLS_PATH}/xmosar")
    set(CMAKE_CXX_COMPILER_AR "${XMOS_TOOLS_PATH}/xmosar")
    set(CMAKE_ASM_COMPILER_AR "${XMOS_TOOLS_PATH}/xmosar")
else()
    set(CMAKE_C_COMPILER "xcc")
    set(CMAKE_CXX_COMPILER  "xcc")
    set(CMAKE_ASM_COMPILER  "xcc")
    set(CMAKE_AR "xmosar" CACHE FILEPATH "Archiver")
    set(CMAKE_C_COMPILER_AR "xmosar")
    set(CMAKE_CXX_COMPILER_AR "xmosar")
    set(CMAKE_ASM_COMPILER_AR "xmosar")
endif()


add_subdirectory(lib)

file(GLOB LF_GEN_SRCS *.c )

set(APP_SRCS
   ${LF_GEN_SRCS}
   core/platform/lf_xmos_support.c
   core/platform/swlock.c
   core/platform/swlock_asm.S
)

set(APP_INCLUDES
   core
   core/platform/
)

## Specify your compiler flags
set(APP_COMPILER_FLAGS
    -g
    -Wno-xcore-fptrgroup
    -mcmodel=large
)

## Specify any compile definitions
set(APP_COMPILE_DEFINITIONS
   DEBUG_PRINT_ENABLE=1
   LIBXCORE_XASSERT_IS_ASSERT
)

## Set your link options
set(APP_LINK_OPTIONS
   -target=XCORE-200-EXPLORER
   -report
)

## Create your targets
add_executable(my_app ${APP_SRCS})
target_include_directories(my_app PUBLIC ${APP_INCLUDES})
target_compile_definitions(my_app PRIVATE ${APP_COMPILE_DEFINITIONS})
target_compile_options(my_app PRIVATE ${APP_COMPILER_FLAGS})
target_link_libraries(my_app PUBLIC lib)
target_link_options(my_app PRIVATE ${APP_LINK_OPTIONS})

install(
    TARGETS my_app
    RUNTIME DESTINATION %s
)
' $APP_NAME $LF_BIN_DIRECTORY >  $LF_SOURCE_GEN_DIRECTORY/CMakeLists.txt

cd $LF_SOURCE_GEN_DIRECTORY

cmake -B build && cd build
make install
mv $LF_BIN_DIRECTORY/my_app.xe $LF_BIN_DIRECTORY/$APP_NAME.xe