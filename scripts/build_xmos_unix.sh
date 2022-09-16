#!/usr/bin/env bash
echo Running build_xmos_unix.sh

# Exit on first error
set -e

# Project root is one up from the bin directory.
PROJECT_ROOT=$LF_BIN_DIRECTORY/..
APP_NAME=${LF_SOURCE_GEN_DIRECTORY##*/}  

# FIXME: This is a hack to rename all #include "platform.h" int #include "lf_platform.h"
for f in $(find $LF_SOURCE_GEN_DIRECTORY -path $LF_SOURCE_GEN_DIRECTORY/build -prune -o -type f -print)
do
    sed -i 's/platform.h"/lf_platform.h"/g' $f
done

# FIXME: This script currently only works for threaded runtime

# Copy platform into /core
cp $PROJECT_ROOT/cmake/xs2a.cmake $LF_SOURCE_GEN_DIRECTORY/xs2a.cmake
cp $PROJECT_ROOT/platform/lf_xmos_support.c $LF_SOURCE_GEN_DIRECTORY/core/platform/
cp $PROJECT_ROOT/platform/lf_xmos_support.h $LF_SOURCE_GEN_DIRECTORY/core/platform/
cp $PROJECT_ROOT/platform/lf_platform.h $LF_SOURCE_GEN_DIRECTORY/core/
cp $PROJECT_ROOT/platform/reactor.c $LF_SOURCE_GEN_DIRECTORY/core/
cp $PROJECT_ROOT/platform/reactor_threaded.c $LF_SOURCE_GEN_DIRECTORY/core/threaded
#cp $PROJECT_ROOT/platform/reactor_common.c $LF_SOURCE_GEN_DIRECTORY/core/
rm $LF_SOURCE_GEN_DIRECTORY/core/platform.h

# Copy platform into /include/core
# TODO: Why are there two generated core dirs
cp $PROJECT_ROOT/platform/lf_xmos_support.c $LF_SOURCE_GEN_DIRECTORY/include/core/platform/
cp $PROJECT_ROOT/platform/lf_xmos_support.h $LF_SOURCE_GEN_DIRECTORY/include/core/platform/
cp $PROJECT_ROOT/platform/lf_platform.h $LF_SOURCE_GEN_DIRECTORY/include/core/
cp $PROJECT_ROOT/platform/reactor.c $LF_SOURCE_GEN_DIRECTORY/include/core/
cp $PROJECT_ROOT/platform/reactor_threaded.c $LF_SOURCE_GEN_DIRECTORY/include/core/threaded
#cp $PROJECT_ROOT/platform/reactor_common.c $LF_SOURCE_GEN_DIRECTORY/include/core/
rm $LF_SOURCE_GEN_DIRECTORY/include/core/platform.h

# Doing some hacking to get info from old cmake
echo $PROJECT

# Backup old CMake file
mv $LF_SOURCE_GEN_DIRECTORY/CMakeLists.txt $LF_SOURCE_GEN_DIRECTORY/CMakeLists_org.txt

# Create CMake file
printf '
cmake_minimum_required(VERSION 3.24)
project(%s LANGUAGES C)

# Include compiler, linker++ definitions provided by XMOS
include(xs2a.cmake)

file(GLOB LF_GEN_SRCS *.c )

set(APP_SRCS
   ${LF_GEN_SRCS}
   core/platform/lf_xmos_support.c
   lib/schedule.c
   lib/tag.c
   lib/time.c
   lib/util.c
   core/mixed_radix.c
   core/threaded/scheduler_NP.c
   core/utils/semaphore.c
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
   __xmos__
   LF_TARGET_EMBEDDED
   NUMBER_OF_WORKERS=2
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