/*
 * capture raw frames from framos camera over csi
 * 
 *  (c) 2020 Tensorfield Ag
 * 
 */

//
#include "ThreadPool.h"
#include <indicators/progress_bar.hpp>

//opencv
#include <opencv2/highgui.hpp>
#include "opencv2/opencv_modules.hpp"
#include <opencv2/core/utility.hpp>
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching/detail/autocalib.hpp"
#include "opencv2/stitching/detail/blenders.hpp"
#include "opencv2/stitching/detail/timelapsers.hpp"
#include "opencv2/stitching/detail/camera.hpp"
#include "opencv2/stitching/detail/exposure_compensate.hpp"
#include "opencv2/stitching/detail/matchers.hpp"
#include "opencv2/stitching/detail/motion_estimators.hpp"
#include "opencv2/stitching/detail/seam_finders.hpp"
#include "opencv2/stitching/detail/warpers.hpp"
#include "opencv2/stitching/warpers.hpp"

#ifdef HAVE_OPENCV_XFEATURES2D
#include "opencv2/xfeatures2d/nonfree.hpp"
#endif

// plog
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>

//exif
#include <exiv2/exiv2.hpp>
#include "ExifTool.h"

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
#include <cmath>

#include <getopt.h>

#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>

// magick api
#define MAGICKCORE_HDRI_ENABLE 1
#define MAGICKCORE_QUANTUM_DEPTH 16

//#include <Magick++.h>
#include "MagickCore/studio.h"
#include "MagickCore/exception.h"
#include "MagickCore/exception-private.h"
#include "MagickCore/image.h"
#include "MagickWand/MagickWand.h"
#include "MagickWand/magick-cli.h"


// boost
#include "boost/program_options/parsers.hpp"



// for threading
#include <future>
#include <thread>
#include <chrono>

#include <regex>

// use plog instead
//#define LOG(msg) std::cout << msg << std::endl
//#define ERR(msg) std::cerr << msg << std::endl
//
// v4l2
#include <linux/videodev2.h>

using namespace std;
using namespace cv;


// for window size
#include <sys/ioctl.h>
#include <unistd.h>
