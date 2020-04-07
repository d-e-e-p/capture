#
# makefile for capture
# (c) tensorfield ag 2020
# 
#
#

CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
#OPENCV_FLAGS=-I/usr/local/include/opencv4 -I/usr/local/include -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
#OPENCV_FLAGS=$(shell pkg-config opencv4 --libs --cflags)
OPENCV_FLAGS=-I/usr/local/include/opencv4 -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs -lopencv_stitching -lopencv_features2d
CFLAGS= -I../common -I../libsv/include -I../libsv/examples  -std=c++11 -pthread -Wl,-rpath=/usr/local/lib -L/usr/local/lib -lexiv2 -lstdc++fs -lboost_system -lboost_filesystem -lboost_program_options
MAGICK_FLAGS=-fopenmp -Wno-write-strings -I/usr/local/include/ImageMagick-7 -L/usr/local/lib -lMagick++-7.Q16HDRI -lMagickWand-7.Q16HDRI -lMagickCore-7.Q16HDRI
CSOURCES= ../libsv/examples/common_cpp/image_processor.cpp
EXIFOBJ = ../common/exif/ExifTool.o ../common/exif/ExifToolPipe.o ../common/exif/TagInfo.o

PROFILE=-g -pg


#../bin/capture: capture.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS) $(PROFILE)
#
#

all: ../bin/capture
	echo done

capture.o: capture.cpp capture.hpp
	$(CC) -g -c -o $@ $< $(CFLAGS)  $(OPENCV_FLAGS) $(MAGICK_FLAGS)

capture-helper.o: capture-helper.c
	$(CC) -g -c -o $@ $< $(CFLAGS) 

../bin/capture: capture.o capture-helper.o 
	$(CC) -o $@ $^  $(EXIFOBJ) $(CFLAGS) $(OPENCV_FLAGS) $(MAGICK_FLAGS)

watch:
	while true; do \
		make all; \
		inotifywait -e close_write capture.cpp capture.hpp capture-helper.c; \
	done

