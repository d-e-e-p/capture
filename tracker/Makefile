#
# makefile for capture
# (c) tensorfield ag 2020
# 
#
#
CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
OPENCV_FLAGS=-I/usr/local/include/opencv4 -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs -lopencv_stitching -lopencv_features2d
CFLAGS=-I../libsv/include -I../libsv/examples  -std=c++11 -pthread -Wl,-rpath=/usr/local/lib -L/usr/local/lib -lexiv2 -lstdc++fs
CSOURCES= 
PROFILE=-g -pg

#../bin/capture: capture.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS) $(PROFILE)


#../bin/test_map: test_map.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS) $(OPENCV_FLAGS)

../bin/tracker: tracker.cpp tracker.hpp
	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS) $(OPENCV_FLAGS)
