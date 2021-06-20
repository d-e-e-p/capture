#
# makefile for postprocess
# (c) tensorfield ag 2020
# 
#
#
#
# don't remove intermediate files
.SECONDARY:
.PRECIOUS: ../bin/postprocess


CC=g++
WARNING_FLAGS=-Wall -Wextra -pedantic
OPENCV_FLAGS=-I/usr/local/include/opencv4 -I/usr/local/include -L/usr/local/lib -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_imgcodecs
CFLAGS= -I../common  -I../libsv/include -I../libsv/examples  -std=c++11 -lpthread -pthread -Wl,-rpath=/usr/local/lib -L/usr/local/lib -lexiv2 -lstdc++fs -lboost_system -lboost_filesystem -lboost_program_options 
MAGICK_FLAGS=-fopenmp -Wno-write-strings -I/usr/local/include/ImageMagick-7 -L/usr/local/lib -lMagick++-7.Q16HDRI -lMagickWand-7.Q16HDRI -lMagickCore-7.Q16HDRI
TIFF_FLAGS=-ltiff -ltiffxx
CSOURCES= ../libsv/examples/common_cpp/image_processor.cpp
EXIFOBJ = $(wildcard ../common/exif/*.o)

#PROFILE=-g -pg
PROFILE=

#../bin/capture: capture.cpp 
#	$(CC) -o $@ $< $(CSOURCES) $(CFLAGS)  $(OPENCV_FLAGS) $(PROFILE)

all: ../bin/postprocess
	echo done


../bin/postprocess: postprocess.cpp postprocess.hpp ../common/Imagedata.hpp
	$(CC) -o $@ $<  $(CFLAGS) $(OPENCV_FLAGS) $(EXIFOBJ) $(MAGICK_FLAGS)  $(PROFILE) $(TIFF_FLAGS)
	../bin/postprocess --overwrite --verbose --threads 1 --convert raw2dng /media/samsung/ML/2021-05-14-TopFlavor-Spence/202105141220/raw/

watch:
	while true; do \
		make all; \
		inotifywait -e close_write,move_self postprocess.cpp postprocess.hpp ../common/Imagedata.hpp; \
	done


