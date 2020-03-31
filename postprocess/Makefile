#
# makefile for capture
# (c) tensorfield ag 2020
# 
#
#
CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
OPENCV_FLAGS=-I/usr/local/include/opencv4 -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
CFLAGS= -Iexif -I../libsv/include -I../libsv/examples  -std=c++11 -pthread -Wl,-rpath=/usr/local/lib -L/usr/local/lib -lexiv2 -lstdc++fs -lboost_system -lboost_filesystem -lboost_program_options
MAGICK_FLAGS=-fopenmp -Wno-write-strings -I/usr/local/include/ImageMagick-7 -L/usr/local/lib -lMagick++-7.Q16HDRI -lMagickWand-7.Q16HDRI -lMagickCore-7.Q16HDRI
CSOURCES= ../libsv/examples/common_cpp/image_processor.cpp
EXIFOBJ = exif/ExifTool.o exif/ExifToolPipe.o exif/TagInfo.o

PROFILE=-g -pg

#../bin/capture: capture.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS) $(PROFILE)


../bin/postprocess: postprocess.cpp 
	$(CC) -o $@ $< $(CFLAGS) $(OPENCV_FLAGS) $(EXIFOBJ) $(MAGICK_FLAGS) $(PROFILE)
