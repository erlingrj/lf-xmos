#!/bin/bash

set -e

CWD=`pwd`
PROGRAM=$1
ROOT=$CWD/..

xcc -target=XCORE-200-EXPLORER -g $CWD/$1.c $ROOT/platform/lf_xmos_support.c $ROOT/platform/swlock.c $ROOT/platform/swlock_asm.S -I$ROOT -I$ROOT/platform


xsim a.xe