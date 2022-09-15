
cmake_minimum_required(VERSION 3.24)
project(multithread LANGUAGES C)
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
   $(LF_GEN_SRCS)
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