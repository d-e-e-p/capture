/*
 * capture frames
 *  (c) 2020 Tensorfield Ag
 */

// modified by tensorfield ag feb 2020

// sv system
#include "sv/sv.h"
#include "common_cpp/fps_measurer.hpp"
#include "common_cpp/image_processor.hpp"
#include "common_cpp/camera_configurator.hpp"
#include "common_cpp/common.hpp"


#include <atomic>
#include <condition_variable>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <linux/limits.h>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <opencv2/highgui.hpp>

#include <getopt.h>

// global vars -- yikes
// TODO: hold this in a map instead, eg opt[gain] = 20
int  opt_minutes = 0;
int  opt_frames = 1000;
int  opt_gain = 20;
int  opt_exposure = 1000; // max = 8256

// -1 value for min/max sweep => use sensor min/max
bool opt_vary_gain = false;
int  opt_min_gain = -1; 
int  opt_max_gain = -1;

bool opt_vary_exposure = false;
int  opt_min_exposure = -1; 
int  opt_max_exposure = -1;

bool opt_nodisplay = false;
bool opt_verbose = false;


void PrintHelp() {
    std::cout <<
              "--minutes <n>:       set num of minutes to capture\n"
              "--frames <n>:        set number of frames to capture\n"
              "--gain <n>:          set gain\n"
              "--exposure <n>:      set exposure\n"
              "--vary_gain:         sweep gain from min to max\n"
              "--min_gain:          min gain for sweep\n"
              "--max_gain:          max gain for sweep\n"
              "--vary_exposure:     sweep exposure from min to max\n"
              "--min_exposure:      min gain for sweep\n"
              "--max_exposure:      max gain for sweep\n"
              "--nodisplay:         no jpeg or display--just raw image dump\n"
              "--verbose:           verbose\n"
              "--help:              show help\n";
    exit(1);
}


void ProcessArgs(int argc, char** argv);
void SaveFrame(void *data, uint32_t length, std::string name, std::string folder);
std::string getCurrentControlValue(IControl *control);
void setGain (ICamera *camera, uint32_t value);
void setExposure (ICamera *camera, uint32_t value);
std::string GetCurrentWorkingDir();
std::string GetDateStamp();

/*
 * main routine:
 *    1. parse params
 *    2. setup cameras
 *    3. capture raw image
 *    4. convert to jpeg and display
 *
 */

int main(int argc, char **argv) {

    ProcessArgs(argc, argv);

    ICameraList cameras = sv::GetAllCameras();
    if (cameras.size() == 0) {
        std::cout << "No cameras detected! Exiting..." << std::endl;
        return 0;
    }

    // on multi-camera system just select first camera
    ICamera *camera = cameras[0];
    IControl *control ;

    control = camera->GetControl(SV_V4L2_IMAGEFORMAT);
   
    // set default gain and exposure
    setGain(camera,opt_gain);
    setExposure(camera,opt_exposure);
    camera->GetControl(SV_V4L2_BLACK_LEVEL)->Set(0);

    // assume defaults
    //common::SelectPixelFormat(control); 
    //common::SelectFrameSize(camera->GetControl(SV_V4L2_FRAMESIZE), camera->GetControl(SV_V4L2_FRAMEINTERVAL));
    //common::SelectFrameSize(camera->GetControl(SV_V4L2_FRAMESIZE), camera->GetControl(SV_V4L2_FRAMEINTERVAL));

    // dump settings
    IControlList controls = camera->GetControlList();
    for (IControl *control : controls) {
        std::string name  = std::string(control->GetName());
        std::string value = getCurrentControlValue(control);
        std::string id    = std::to_string(control->GetID());
        if (opt_verbose) {
            std::cout << "Control: " << name << " = " << value << " : id = " << id << std::endl;
        } else {
            std::cout << "Control: " << name << " = " << value <<  std::endl;
        }
    }


    if (!camera->StartStream()) {
        std::cout << "Failed to start stream!" << std::endl;
        return 0;
    }

    int frameCount = opt_frames;
    if (opt_minutes > 0) {
        // calculate frames instead of actually counting minutes
        // assume fps
        int estimated_fps = 20;
        frameCount = opt_minutes * 60 * estimated_fps;
        std::cout << "estimating to capture " << frameCount << " frames at " <<  estimated_fps << "fps " << std::endl;
    }
    const std::string dateStamp  = GetDateStamp();
    //const std::string currentWorkingDir = GetCurrentWorkingDir();
    const std::string folderBase = "/home/deep/build/snappy/data/images/" + dateStamp + "/";
    const std::string folderRaw  = folderBase + "raw/";
    const std::string folderJpeg = folderBase + "jpeg/";

    mkdir(folderBase.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    std::cout << "writing to folder : " << folderRaw << std::endl;
    mkdir(folderRaw.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

    std::cout << "writing to folder : " << folderJpeg << std::endl;
    mkdir(folderJpeg.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);


    std::string command = "/usr/local/bin/v4l2-ctl --verbose --all --list-ctrls-menus > " + folderBase + "camera.settings";
    std::system( (const char *) command.c_str());


    int frameSaved = 0;
    int maxIntegerWidth = std::to_string(frameCount).length() + 1;
    common::ImageProcessor processor;
    IProcessedImage processedImage = sv::AllocateProcessedImage(camera->GetImageInfo());
    cv::UMat mImage;
    cv::namedWindow("capture", cv::WINDOW_OPENGL | cv::WINDOW_AUTOSIZE);


    common::FpsMeasurer fpsMeasurer;

    for (int i = 0; i < frameCount; i++) {

        IImage image = camera->GetImage();

        if (image.data == nullptr) {
            std::cout << "Unable to save frame, invalid image data" << std::endl;
            continue;
        }

        std::string num = std::to_string(i);
        num.insert(num.begin(), maxIntegerWidth - num.length(), '0');
        fpsMeasurer.FrameReceived();

        if (opt_vary_gain) {
            setGain(camera, opt_gain + i);
        }
        if (opt_vary_exposure) {
            setExposure(camera, opt_exposure + i * 25);
        }


        std::string basename     = "frame" + num ;
        std::string fpsText      = " fps = "  + std::to_string(fpsMeasurer.GetFps());
        std::string gainText     = " gain = " + std::to_string(camera->GetControl(SV_V4L2_GAIN)->Get());
        std::string exposureText = " exp = "  + std::to_string(camera->GetControl(SV_V4L2_EXPOSURE)->Get());
        std::string text = basename + fpsText + gainText + exposureText;

        void *data;
        uint32_t length;
        if (! opt_nodisplay) {
            sv::ProcessImage(image, processedImage, SV_ALGORITHM_AUTODETECT);
            data = processedImage.data;
            length = processedImage.length;
            processor.AllocateMat(processedImage, mImage);
            processor.DebayerImage(mImage,processedImage.pixelFormat);
            processor.DrawText(mImage,text);
        } else {
            data = image.data;
            length = image.length;
            if (data == nullptr) {
                std::cout << "data null" << std::endl;
            }
            //std::cout << "length" << image.length << std::endl;
        }


        std::string filenameRaw  = basename + ".raw";
        std::string filenameJpeg = basename + ".jpeg";

        SaveFrame(data, length, filenameRaw, folderRaw);
        if (! opt_nodisplay) {
            processor.SaveImageAsJpeg(mImage, folderJpeg + filenameJpeg);
            cv::imshow("capture", mImage);
            cv::waitKey(1);
        }


        if (opt_verbose) {
            std::cout << ". " << text << std::endl << std::flush;
        }
        frameSaved += 1;

        camera->ReturnImage(image);
    }

    camera->StopStream();

    std::string fpsText      = " fps = "  + std::to_string(fpsMeasurer.GetFps());
    std::cout << "\nSaved " << frameSaved << " frames to " << folderRaw << " folder with" << fpsText << std::endl;

    sv::DeallocateProcessedImage(processedImage);

    return 0;
}


// set global vars (yikes)
void ProcessArgs(int argc, char** argv) {
    const char* const short_opts = "-:";
    enum Option {
        OptMinutes,
        OptFrames,
        OptGain,
        OptExposure,
        OptVaryGain,
        OptMinGain,
        OptMaxGain,
        OptVaryExposure,
        OptMinExposure,
        OptMaxExposure,
        OptNoDisplay,
        OptVerbose
    };
    static struct option long_opts[] = {
        {"minutes",       required_argument,  0,    OptMinutes },
        {"frames",        required_argument,  0,    OptFrames },
        {"gain",          required_argument,  0,    OptGain },
        {"exposure",      required_argument,  0,    OptExposure },
        {"vary_gain",     no_argument,        0,    OptVaryGain },
        {"min_gain",      required_argument,  0,    OptMinGain },
        {"max_gain",      required_argument,  0,    OptMaxGain },
        {"vary_exposure", no_argument,        0,    OptVaryExposure },
        {"min_exposure",  required_argument,  0,    OptMinExposure },
        {"max_exposure",  required_argument,  0,    OptMaxExposure },
        {"nodisplay",     no_argument,        0,    OptNoDisplay },
        {"verbose",       no_argument,        0,    OptVerbose },
        {NULL,            0,               NULL,    0}
    };

    while (true) {
        const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);
        //printf("opt ='%d' \n", opt);

        if (-1 == opt)
            break;

        switch (opt) {
        case OptMinutes:
            opt_minutes = std::stoi(optarg);
            std::cout << "Minutes set to: " << opt_minutes << std::endl;
            break;
        case OptFrames:
            opt_frames = std::stoi(optarg);
            std::cout << "Frames set to: " << opt_frames << std::endl;
            break;
        case OptGain:
            opt_gain = std::stoi(optarg);
            std::cout << "Gain set to: " << opt_gain << std::endl;
            break;
        case OptExposure:
            opt_exposure = std::stoi(optarg);
            std::cout << "Exposure set to: " << opt_exposure << std::endl;
            break;
        case OptVaryGain:
            opt_vary_gain = true;
            std::cout << "sweep gain from min to max" << std::endl;
            break;
        case OptMinGain:
            opt_min_gain = std::stoi(optarg);
            std::cout << "Min gain set to: " << opt_min_gain << std::endl;
            break;
        case OptMaxGain:
            opt_max_gain = std::stoi(optarg);
            std::cout << "Max gain set to: " << opt_max_gain << std::endl;
            break;
        case OptVaryExposure:
            opt_vary_exposure = true;
            std::cout << "sweep exposure from min to max" << std::endl;
            break;
        case OptMinExposure:
            opt_min_exposure = std::stoi(optarg);
            std::cout << "Min exposure set to: " << opt_min_exposure << std::endl;
            break;
        case OptMaxExposure:
            opt_max_exposure = std::stoi(optarg);
            std::cout << "Max exposure set to: " << opt_max_exposure << std::endl;
            break;
        case OptVerbose:
            opt_verbose = true;
            std::cout << "verbose mode" << std::endl;
            break;
        case OptNoDisplay:
            opt_nodisplay = true;
            std::cout << "no display or jpeg files" << std::endl;
            break;
        default:
            PrintHelp();
            break;
        }
    }
}

// sort of from http://www.cplusplus.com/forum/beginner/60194/

std::string GetDateStamp() {
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    printf ("Time %s", asctime(timeinfo));

    int year    = timeinfo->tm_year+1900;
    int month   = timeinfo->tm_mon+1;
    int day     = timeinfo->tm_mday;
    int hour    = timeinfo->tm_hour;
    int minute  = timeinfo->tm_min;

    std::stringstream iss;
    iss << std::setfill('0');
    iss << std::setw(4) << year;
    iss << std::setw(2) << month;
    iss << std::setw(2) << day;
    iss << std::setw(2) << hour;
    iss << std::setw(2) << minute;

    return iss.str(); //return the string present in iss.

}


std::string GetCurrentWorkingDir()
{
    char buff[PATH_MAX];
    if (getcwd(buff, PATH_MAX) == NULL) {
        return "";
    }

    std::string current_working_dir(buff);
    return current_working_dir;
}

// control values can either be an integer value or list of values
std::string getCurrentControlValue(IControl *control) {
    if (control->IsMenu()) {
        return (control->GetMenuEntries()[control->Get()].name);
    } else {
        return std::to_string(control->Get());
    }
}


// modulo gain if asked value greater than max !
void setGain (ICamera *camera, uint32_t value) {

    int32_t minValue = camera->GetControl(SV_V4L2_GAIN)->GetMinValue();
    int32_t maxValue = camera->GetControl(SV_V4L2_GAIN)->GetMaxValue();
    int32_t defValue = camera->GetControl(SV_V4L2_GAIN)->GetDefaultValue();

    if (opt_min_gain > 0) {
        minValue = opt_min_gain;
    }
    if (opt_max_gain > 0) {
        maxValue = opt_max_gain;
    }

    if (value < minValue) {
        value = minValue;
    } else if (value > maxValue) {
        value = value % maxValue;
    }

    if (opt_verbose) {
        if (value < minValue) {
            std::cout << value << "  less than min value of Gain = " << minValue << std::endl;
        } else if (value > maxValue) {
            std::cout << value << "  greater than max value of Gain = " << maxValue << std::endl;
        }
        std::cout << " setting Gain = " << value << " default = " << defValue << std::endl;
    }

    camera->GetControl(SV_V4L2_GAIN)->Set(value);

}

// modulo exposure if asked value greater than max !
void setExposure (ICamera *camera, uint32_t value) {

    int32_t minValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetMinValue();
    int32_t maxValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetMaxValue();
    int32_t defValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetDefaultValue();

    if (opt_min_exposure > 0) {
        minValue = opt_min_exposure;
    }
    if (opt_max_exposure > 0) {
        maxValue = opt_max_exposure;
    }

    if (value < minValue) {
        value = minValue;
    } else if (value > maxValue) {
        value = value % maxValue;
    }

    if (opt_verbose) {
        if (value < minValue) {
            std::cout << value << "  less than min value of Exposure = " << minValue << std::endl;
        } else if (value > maxValue) {
            std::cout << value << "  greater than max value of Exposure = " << maxValue << std::endl;
        }
        std::cout << " setting Exposure = " << value << " default = " << defValue << std::endl;
    }


    camera->GetControl(SV_V4L2_EXPOSURE)->Set(value);

}

void SaveFrame(void *data, uint32_t length, std::string name, std::string folder)
{

    std::string file = folder + name;
    remove(file.c_str());
    int fd = open (file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        std::cout << "Unable to save frame, cannot open file " << file << std::endl;
        return;
    }

    int writenBytes = write(fd, data, length);
    if (writenBytes < 0) {
        std::cout << "Error writing to file " << file << std::endl;
    } else if ( (uint32_t) writenBytes != length) {
        std::cout << "Warning: " << writenBytes << " out of " << length << " were written to file " << file << std::endl;
    }

    close(fd);
}
