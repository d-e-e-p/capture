
#include <iostream>
#include <fstream>
#include <string>
#include "opencv2/stitching.hpp"
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

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

// magick api
#define MAGICKCORE_HDRI_ENABLE 1
#define MAGICKCORE_QUANTUM_DEPTH 16

#include "MagickCore/studio.h"
#include "MagickCore/exception.h"
#include "MagickCore/exception-private.h"
#include "MagickCore/image.h"
#include "MagickWand/MagickWand.h"
#include "MagickWand/magick-cli.h"
#include <cmath>


// for threading
#include <future>
#include <thread>
#include <chrono>

// for streams
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>

// for rolling mean
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>


#define ENABLE_LOG 1
#define LOG(msg) std::cout << msg << std::endl
#define ERR(msg) std::cerr << msg << std::endl

using namespace std;
using namespace cv;
using namespace cv::detail;
//using namespace boost::accumulators;


