#!/bin/bash

C_COMPILER=gcc
CPP_COMPILER=g++
WARNING_FLAGS="-Wall -Wextra -pedantic"
C_FLAGS="$WARNING_FLAGS -O2 -Wl,-rpath,\$ORIGIN -pthread"
CPP_FLAGS="$C_FLAGS -std=c++11"
OPENCV_FLAGS="-lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs"
INCLUDE="-I../../include -I../. -L../../lib -lsv"

BASEDIR=$(dirname "$0")
cd "$BASEDIR"
mkdir -p ../build
cd ../build

echo Building display_image cpp example...
$CPP_COMPILER ../../examples/display_image/display_image.cpp ../../examples/common_cpp/*.cpp -o display_image $CPP_FLAGS $OPENCV_FLAGS $INCLUDE
echo Building save_image cpp example...
$CPP_COMPILER ../../examples/save_image/save_image.cpp -o save_image $CPP_FLAGS $INCLUDE
echo Building acquire_image cpp example...
$CPP_COMPILER ../../examples/acquire_image/acquire_image.cpp -o acquire_image $CPP_FLAGS $INCLUDE
echo Building acquire_image c example...
$C_COMPILER ../../examples/acquire_image/acquire_image.c -o acquire_image_c $C_FLAGS $INCLUDE
echo Building save_image c example...
$C_COMPILER ../../examples/save_image/save_image.c -o save_image_c $C_FLAGS $INCLUDE
echo Building display_image c example...
$C_COMPILER ../../examples/display_image/display_image.c ../../examples/common_c/*.c -o display_image_c $C_FLAGS $OPENCV_FLAGS $INCLUDE
echo Building capture
$CPP_COMPILER ../../examples/capture/capture.cpp -o capture $CPP_FLAGS $INCLUDE
