#
# makefile for capture
# (c) tensorfield ag 2020
# 
#
#
CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
OPENCV_FLAGS=-I/usr/local/include/opencv4 -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
CFLAGS=-I../libsv/include -I../libsv/examples  -std=c++11 -pthread -Wl,-rpath=/usr/local/lib -L/usr/local/lib -lexiv2 -lstdc++fs
CSOURCES= ../libsv/examples/common_cpp/image_processor.cpp
PROFILE=-g -pg

#../bin/capture: capture.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS) $(PROFILE)


../bin/postprocess: postprocess.cpp 
	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS) $(OPENCV_FLAGS)
