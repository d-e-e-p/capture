#
# makefile for postprocess
# (c) tensorfield ag 2020
# 
#
#
CC=g++
WARNING_FLAGS=-pedantic
CFLAGS= -I../../librealsense/third-party/  -ftree-vectorize -pthread -fPIE -std=gnu++11 -lstdc++fs -rdynamic \
	-Wl,-rpath,/usr/local/lib: /usr/local/lib/librealsense2.so.2.34.0  \
	/usr/local/lib/libglfw3.a -lGL -lGLU -lrt -lm -ldl -lX11
OPENCV_FLAGS=-I/usr/local/include/opencv4 -I/usr/local/include -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs

PROFILE=-g -pg

all: ../bin/rgbd_capture
	echo done

../bin/rgbd_capture: rgbd_capture.cpp
	$(CC) -o $@ $< $(OPENCV_FLAGS) $(CFLAGS) $(PROFILE)

watch:
	while true; do \
		make all; \
		inotifywait -e close_write,move_self rgbd_capture.cpp \
	done


