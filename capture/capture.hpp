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

// plog
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>

//exif
#include <exiv2/exiv2.hpp>

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
#include <opencv2/highgui.hpp>

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



//#define LOG(msg) std::cout << msg << std::endl
//#define ERR(msg) std::cerr << msg << std::endl
