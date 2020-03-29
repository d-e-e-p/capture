#
# makefile for capture
# (c) tensorfield ag 2020
# 
#
#
CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
OPENCV_FLAGS=-I../opencv/include -L../opencv/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
CFLAGS=-I../libsv/include -I../libsv/examples  -L../libsv/lib -std=c++11 -pthread -Wl,-rpath=/usr/local/lib -L/usr/local/lib -lexiv2 -lstdc++fs -fpermissive
CSOURCES= ../libsv/examples/common_cpp/image_processor.cpp
PROFILE=-g -pg

#../bin/capture: capture.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS) $(PROFILE)
#
#

all: ../bin/capture
	echo done

capture.o: capture.cpp capture.hpp
	$(CC) -g -c -o $@ $< $(CFLAGS)  $(OPENCV_FLAGS)

capture-helper.o: capture-helper.c
	$(CC) -g -c -o $@ $< $(CFLAGS) 

../bin/capture: capture.o capture-helper.o 
	$(CC) -o $@ $^ $(CFLAGS) -lv4l2

