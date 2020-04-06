/*
 * capture raw frames from framos camera over csi
 * 
 *  (c) 2020 Tensorfield Ag
 * 
 */

//
using namespace std;

// sv system
#include "sv/sv.h"
#include "common_cpp/fps_measurer.hpp"
#include "common_cpp/image_processor.hpp"
#include "common_cpp/camera_configurator.hpp"
#include "common_cpp/common.hpp"

//opencv
#include <opencv2/highgui.hpp>
using namespace cv;

// plog
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>

//exif
#include <exiv2/exiv2.hpp>

//std
#include <algorithm>
#include <array>
#include <atomic>
#include <ios>
#include <condition_variable>
#include <ctime>
#include <experimental/filesystem>
namespace fs=std::experimental::filesystem;
#include <iomanip>
#include <linux/limits.h>
#include <memory>
#include <mutex>
#include <string>
#include<iterator>

#include <getopt.h>

#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>

// for threading
#include <future>
#include <thread>
#include <chrono>

// use plog instead
//#define LOG(msg) std::cout << msg << std::endl
//#define ERR(msg) std::cerr << msg << std::endl
//
// v4l2
//
//
//
#include <linux/videodev2.h>

enum io_method {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct buffer {
    void   *start;
    size_t  length;
};

extern char            *dev_name;
extern enum io_method   io;
extern int              fd;
extern struct buffer          *buffers;
extern unsigned int     n_buffers;
extern int              out_buf;
extern int              force_format;
extern int              frame_count;


extern  void errno_exit(const char *s);
extern  void process_image(const void *ptr, int size);
extern  void mainloop(void);
extern  void stop_capturing(void);
extern  void start_capturing(void);
extern  void uninit_device(void);
extern  void init_read(unsigned int buffer_size);
extern  void init_mmap(void);
extern  void init_userp(unsigned int buffer_size);
extern  void init_device(void);
extern  void close_device(void);
extern  void open_device(void);
extern  void usage(FILE *fp, int argc, char **argv);

long get_gain(void) ;
long get_expo(void) ;
float get_fps(void) ;
int set_gain(long value) ;
int set_expo(long value) ;
int set_fps(float value) ;
struct v4l2_query_ext_ctrl query_gain(void) ;
struct v4l2_query_ext_ctrl query_expo(void) ;


