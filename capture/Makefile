#
# makefile for capture
# (c) tensorfield ag 2020
# 
#
#
CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
OPENCV_FLAGS=-I../opencv/include -L../opencv/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
CFLAGS=-I../libsv/include -I../libsv/examples  -L../libsv/lib -lsv -std=c++11 -pthread
CSOURCES= ../libsv/examples/common_cpp/image_processor.cpp

../bin/capture: capture.cpp 
	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS)
