#!/bin/bash

set -e

CWD=`pwd`
PROGRAM=$1
ROOT=$CWD/..

xcc -target=XCORE-200-EXPLORER -g $CWD/$1.c $ROOT/platform/lf_xmos_support.c -I$ROOT -I$ROOT/platform -D__xmos__ -DLF_TARGET_EMBEDDED -DNUMBER_OF_WORKERS=4


xsim a.xe