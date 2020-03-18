/*
 * capture frames
 *  (c) 2020 Tensorfield Ag
 */

// modified by tensorfield ag feb 2020
//
using namespace std;

// sv system
#include "sv/sv.h"
#include "common_cpp/fps_measurer.hpp"
#include "common_cpp/image_processor.hpp"
#include "common_cpp/camera_configurator.hpp"
#include "common_cpp/common.hpp"

//exif
#include <exiv2/exiv2.hpp>

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

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/tee.hpp>

#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>

// for threading
#include <future>
#include <thread>
#include <chrono>


typedef boost::iostreams::tee_device<ostream, ofstream> TeeDevice;
typedef boost::iostreams::stream<TeeDevice> TeeStream;


// global vars -- yikes
// TODO: hold this in a map instead, eg opt[gain] = 20
int  opt_minutes = 0;
int  opt_frames = 1000;
int  opt_gain = 10;
int  opt_exposure = 500; // max = 8256

// -1 value for min/max sweep => use sensor min/max
bool opt_vary_gain = false;
int  opt_min_gain = -1; 
int  opt_max_gain = -1;
int  opt_step_gain = 10;

bool opt_vary_exposure = false;
int  opt_min_exposure = -1; 
int  opt_max_exposure = -1;
int  opt_step_exposure = 50;

bool opt_vary_both = false;

bool opt_nosave = false;
bool opt_nodisplay = false;
bool opt_noexif = false;
bool opt_verbose = false;

string opt_header  = "Tensorfield Ag (c) 2020\n";
string opt_comment = "\n";

// gain+exposure table
list < cv::Point > g_gaex_table;
list < cv::Point >::iterator g_gaex_table_ptr;

//dng header
char* g_dng_headerdata = NULL;
int   g_dng_headerlength = 0;

void PrintHelp() {
    cout <<
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
              "--vary_both:         sweep both gain and exposure from min to max\n"
              "--nosave:            no saving any images--just display\n"
              "--nodisplay:         no jpeg or display--just raw image dump\n"
              "--noexif:            no exif metadata in jpeg\n"
              "--comment:           specify a comment to remember the run\n"
              "--verbose:           verbose\n"
              "--help:              show help\n";
    exit(1);
}



// *****************************************************************************

void ProcessArgs(int argc, char** argv);
void SaveFrame(void *data, uint32_t length, string filename);
void saveAndDisplayJpgDirect(ICamera *camera, IProcessedImage processedImage, string text, string folderJpg, string basename,  string dateStamp);
void saveJpgExternal(void *data, uint32_t length, string name, string folderDng, string folderJpg, string basename,  string dateStamp);
string getCurrentControlValue(IControl *control);
void setGain (ICamera *camera, uint32_t value);
void setExposure (ICamera *camera, uint32_t value);
void setMinMaxGain (ICamera *camera);
void setMinMaxExposure (ICamera *camera);
void varyGain (ICamera *camera);
void varyExposure (ICamera *camera);
void varyBoth (ICamera *camera);
string GetCurrentWorkingDir();
string GetDateStamp();
string GetDateTimeOriginal();
void exifPrint(const Exiv2::ExifData exifData);


int SaveFrameHeader(void *data, uint32_t length, string filename) {

    // read in header data once
    if (g_dng_headerdata == nullptr) {
        FILE *fp = nullptr; 
        fs::path headerfile = "/home/deep/build/snappy/bin/dng_header.bin";
        if (opt_verbose) {
            cout << "reading in " << headerfile ;
        }
        g_dng_headerlength = fs::file_size(headerfile);
        fp = fopen(headerfile.c_str(), "rb");
        g_dng_headerdata = (char*) malloc (sizeof(char) * g_dng_headerlength);
        fread(g_dng_headerdata, sizeof(char), g_dng_headerlength, fp);
        fclose(fp);
    }

    remove(filename.c_str());
    ofstream outfile (filename,ofstream::binary);
    outfile.write (reinterpret_cast<const char*>(g_dng_headerdata), g_dng_headerlength);
    outfile.write (reinterpret_cast<const char*>(data), length);
    outfile.close();

    /*
    int fd = open (file.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    write(fd, g_dng_headerdata, sizeof(g_dng_headerdata));
    write(fd, data, length);
    close(fd);
    */

}

void generateGainExposureTable() {


    for (int value_gain = opt_min_gain; value_gain < opt_max_gain; value_gain += opt_step_gain) {
        for (int value_exposure = opt_min_exposure; value_exposure < opt_max_exposure; value_exposure += opt_step_exposure) {
            g_gaex_table.push_back(cv::Point(value_gain, value_exposure ));
            //cout << "Vary table (gain,exposure) =  " << cv::Point(value_gain, value_exposure) << endl;
        }
    }

    cout << "Generated gain+exposure table:" << endl;
    cout << "    gain range =     " << cv::Point(opt_min_gain, opt_max_gain) << " in steps of " << opt_step_gain << endl;
    cout << "    exposure range = " << cv::Point(opt_min_exposure, opt_max_exposure) << " in steps of " << opt_step_exposure << endl;
    cout << "    total values in sweep  =  " << g_gaex_table.size() << endl;

    if (false) {
        for (auto it : g_gaex_table) {
            cout << it << "\n";
        }
    }


}


/*
 * main routine:
 *    1. parse params
 *    2. setup cameras
 *    3. capture raw image
 *    4. convert to jpeg and display
 *
 */

int main(int argc, char **argv) {


    // filename stuff...
    // TODO: move into function
    const string dateStamp  = GetDateStamp();
    const string folderBase = "/home/deep/build/snappy/data/images/" + dateStamp + "/";
    fs::create_directories(folderBase);

    // see https://stackoverflow.com/questions/19641190/c-duplicate-stdout-to-file-by-redirecting-cout
    string fileLog = folderBase + "run.log";
    remove(fileLog.c_str());
    ofstream logFile;
    logFile.open(fileLog);

    ostream tmp(cout.rdbuf()); // <----
    TeeDevice outputDevice(tmp, logFile); // <----
    TeeStream logger(outputDevice);

    cout.rdbuf(logger.rdbuf());
    cout << opt_header << endl;
    cout << opt_comment << endl;

    ProcessArgs(argc, argv);

    cout << "saving results under " << folderBase << endl;

    const string folderRaw  = folderBase  + "raw/";
    const string folderJpg  = folderBase  + "jpg/";
    const string folderDng  =  folderBase + "dng/";
    fs::create_directories(folderRaw);
    fs::create_directories(folderJpg);
    fs::create_directories(folderDng);

    if (opt_verbose) {
        cout << "writing raw to folder : " << folderRaw << endl;
        cout << "writing jpg to folder : " << folderJpg << endl;
        cout << "writing dng to folder : " << folderDng << endl;
    }


    string command = "/usr/local/bin/v4l2-ctl --verbose --all > " + folderBase + "camera.settings";
    system( (const char *) command.c_str());


    ICameraList cameras = sv::GetAllCameras();
    if (cameras.size() == 0) {
        cout << "No cameras detected! Exiting..." << endl;
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
    camera->GetControl(SV_V4L2_FRAMESIZE)->Set(0);

    // set min/max levels
    setMinMaxGain(camera);
    setMinMaxExposure(camera);

    // assume defaults
    //common::SelectPixelFormat(control); 
    //common::SelectFrameSize(camera->GetControl(SV_V4L2_FRAMESIZE), camera->GetControl(SV_V4L2_FRAMEINTERVAL));
    //common::SelectFrameSize(camera->GetControl(SV_V4L2_FRAMESIZE), camera->GetControl(SV_V4L2_FRAMEINTERVAL));

    // dump settings
    IControlList controls = camera->GetControlList();
    for (IControl *control : controls) {
        string name  = string(control->GetName());
        string value = getCurrentControlValue(control);
        string id    = to_string(control->GetID());
        if (opt_verbose) {
            cout << "Control: " << name << " = " << value << " : id = " << id << endl;
        } else {
            cout << "Control: " << name << " = " << value <<  endl;
        }
    }


    if (!camera->StartStream()) {
        cout << "Failed to start stream!" << endl;
        return 0;
    }

    int frameCount = opt_frames;
    if (opt_minutes > 0) {
        // calculate frames only for finding width of filenames
        int estimated_fps = 100;
        frameCount = opt_minutes * 60 * estimated_fps;
        //cout << "estimating to capture " << frameCount << " frames at " <<  estimated_fps << "fps " << endl;
    }
    // TODO: use image.date to set filename and exif date information

    // inits for loop
    int num_frames = 0; // index for frame count
    int frameSaved = 0;
    int maxIntegerWidth = to_string(frameCount).length() + 1;
    common::FpsMeasurer fpsMeasurer;
    clock_t start_prog = clock();
    clock_t start_loop = start_prog - CLOCKS_PER_SEC * 60; // trigger first save
    double duration;

    vector<thread> threads;  // keep track of threads to close
    future <void> future = std::async(std::launch::async, []{ });

    while (true) {

        // exit on minutes of wall clock time?
        if (opt_minutes > 0) {
            duration = ( clock() - start_prog ) / (double) CLOCKS_PER_SEC;
            if (duration > opt_minutes) {
                break;
            }
        } else if (num_frames > opt_frames) {
            break;
        }

        IImage image = camera->GetImage();
        if (image.data == nullptr) {
            cerr << "Unable to save frame, invalid image data" << endl;
            break;
        }
        if (false) {
            cout << " image.id          : " << image.id           << endl;
            cout << " image.length      : " << image.length       << endl;
            cout << " image.width       : " << image.width        << endl;
            cout << " image.height      : " << image.height       << endl;
            cout << " image.pixelFormat : " << image.pixelFormat  << endl;
            cout << " image.stride      : " << image.stride       << endl;
            cout << " image.timestamp.s : " << image.timestamp.s << "." << image.timestamp.us  << endl;
        }

        //string num = to_string(num_frames++);
        //num.insert(num.begin(), maxIntegerWidth - num.length(), '0');
        //string basename     = "frame" + num ;

        fpsMeasurer.FrameReceived();

        if (opt_vary_both) {
            varyBoth(camera);
        } else {
            if (opt_vary_gain)
                varyGain(camera);
            if (opt_vary_exposure)
                varyExposure(camera);
        }

        // c++ does have reasonable printf until c++20 sigh.
        char buff[BUFSIZ];
        snprintf(buff, sizeof(buff), "imgG%03dE%04dF%0*d", opt_gain, opt_exposure , maxIntegerWidth, num_frames++);
        string basename = buff;

        string fpsText      = " fps = "  + to_string(fpsMeasurer.GetFps());
        string gainText     = " gain = " + to_string(camera->GetControl(SV_V4L2_GAIN)->Get());
        string exposureText = " exp = "  + to_string(camera->GetControl(SV_V4L2_EXPOSURE)->Get());
        string text = basename + fpsText + gainText + exposureText;
        if (opt_verbose) {
            cout << "\t" << text << endl << flush;
        }

        if (image.data == nullptr) {
            cerr << "ERR: image.data null" << endl;
            break;
        }
        if (! opt_nosave) {
            string filenameRaw  = basename + ".raw";
            SaveFrame(image.data, image.length, folderRaw + filenameRaw);
        }
        //saveJpgExternal( image.data, image.length, text, folderDng, folderJpg,  basename, dateStamp);

        //
        // TODO: make this vary based on fps...
        // save about once per sec
        //
        /*
        duration = ( clock() - start_loop ) / (double) CLOCKS_PER_SEC;
        int duration_between_saves_in_sec = 0.5;
        if (duration > duration_between_saves_in_sec) {
                start_loop = clock();
                threads.push_back(thread(saveJpgExternal, image.data, image.length, text, folderDng, folderJpg,  basename, dateStamp));
                //IProcessedImage processedImage = sv::AllocateProcessedImage(camera->GetImageInfo());
                //sv::ProcessImage(image, processedImage, SV_ALGORITHM_AUTODETECT);
                //threads.push_back(thread(SaveAndDisplayJpg, camera, processedImage, text, folderJpg,  basename, dateStamp));
         }
         */

        // Use wait_for() with zero time to check thread status.
        std::chrono::nanoseconds ns(1);
        auto status = future.wait_for(ns);
        if (status == std::future_status::ready) {
            future = std::async(std::launch::async, saveJpgExternal, image.data, image.length, text, folderDng, folderJpg,  basename, dateStamp);
        } 


        camera->ReturnImage(image);
    }

    // cleanup
    string fpsText      = " fps = "  + to_string(fpsMeasurer.GetFps());
    cout << "\nSaved " << num_frames << " frames to " << folderRaw << " folder with" << fpsText << endl;
    cout << "session log :" << fileLog << endl;

    camera->StopStream();
    // close any threads that may be running
    future.wait();
    for (auto& th : threads) {
        th.join();
    }
    logger.close();

    return 0;
}

// run in separate thread
// TODO: setup workers to handle this in background without wasting time every
// loop allocating and releasing all this overhead memory
void saveAndDisplayJpgDirect(ICamera *camera, IProcessedImage processedImage, string text, string folderJpg, string basename,  string dateStamp) {
    common::ImageProcessor processor;

    cv::UMat mImage;
    uint32_t length = processedImage.length;
    processor.AllocateMat(processedImage, mImage);
    processor.DebayerImage(mImage,processedImage.pixelFormat);
    processor.DrawText(mImage,text);
    sv::DeallocateProcessedImage(processedImage);

    string filenameJpg = basename + ".jpg";
    vector<uchar> jpgbuf;
    Exiv2::ExifData exifData;

    // convert from 16 to 8 bit
    if (mImage.depth() != CV_8U) {
        constexpr auto CONVERSION_SCALE_16_TO_8 = 0.00390625;
        mImage.convertTo(mImage, CV_8U, CONVERSION_SCALE_16_TO_8);

    }
    cv::imencode(".jpg",mImage, jpgbuf);
    if (opt_noexif) {
        SaveFrame(&jpgbuf[0], jpgbuf.size(), folderJpg + filenameJpg);
    } else {
        Exiv2::Image::AutoPtr eImage = Exiv2::ImageFactory::open(&jpgbuf[0],jpgbuf.size());
        exifData["Exif.Image.ProcessingSoftware"] = "capture.cpp";
        exifData["Exif.Image.SubfileType"] = 1;
        exifData["Exif.Image.ImageDescription"] = "snappy carrots";
        exifData["Exif.Image.Model"] = "tensorfield ag";
        exifData["Exif.Image.AnalogBalance"] = opt_gain;
        exifData["Exif.Image.ExposureTime"] = opt_exposure;
        exifData["Exif.Image.DateTimeOriginal"] = dateStamp;
        eImage->setExifData(exifData);
        eImage->writeMetadata();
        if (opt_verbose) {
            Exiv2::ExifData &ed2 = eImage->exifData();
            exifPrint(ed2);
        }

        if (! opt_nosave) {
            string outJpg = folderJpg + filenameJpg;
            Exiv2::FileIo file(outJpg);
            file.open("wb");
            file.write(eImage->io()); 
            file.close();

            const string curJpg = "/home/deep/build/snappy/capture/current.jpg";
            ifstream src(outJpg, ios::binary);
            ofstream dst(curJpg, ios::binary);
            dst << src.rdbuf();
        }

    }

    //cv::Mat dst = cv::imdecode(jpgbuf,1);
    //cv::imshow("dst", dst);
    if (opt_verbose) {
        cout << ". " << folderJpg << filenameJpg << endl << flush;
    }
    cv::namedWindow("capture", cv::WINDOW_OPENGL | cv::WINDOW_AUTOSIZE);
    cv::imshow("capture", mImage);
    cv::waitKey(1);
}


void saveJpgExternal(void *data, uint32_t length, string text, string folderDng, string folderJpg, string basename,  string dateStamp) {

    string filenameDng = basename + ".dng";
    SaveFrameHeader(data, length, folderDng + filenameDng);
    if (opt_verbose) {
        cout << "written to file " << folderDng << filenameDng << endl;
    }

    string filenameJpg = basename + ".jpg";
    //string programName = "rawtherapee-cli -Y -o /home/deep/build/snappy/capture/next.jpg -q -p /home/deep/build/snappy/bin/wb.pp4 -c " + folderDng + filenameDng;
    string programName = "rawtherapee-cli -Y -o /home/deep/build/snappy/capture/next.jpg -q -c " + folderDng + filenameDng;
    programName += "; convert -pointsize 20 -fill yellow -draw \'text 10,50 \" " + text + " \" \'   " + "/home/deep/build/snappy/capture/next.jpg" + " " + folderJpg + filenameJpg ;
    if (opt_verbose) {
        cout << "running cmd=" << programName << endl;
    }
    //cout << "running cmd: " << programName << endl;
    system( (const char *) programName.c_str());
    fs::copy(folderJpg + filenameJpg,"/home/deep/build/snappy/capture/current.jpg", fs::copy_options::overwrite_existing);

}



void exifPrint(const Exiv2::ExifData exifData) {
    Exiv2::ExifData::const_iterator i = exifData.begin();
    for (; i != exifData.end(); ++i) {
        cout << setw(35) << setfill(' ') << left
                  << i->key() << " "
                  << "0x" << setw(4) << setfill('0') << right
                  << hex << i->tag() << " "
                  << setw(9) << setfill(' ') << left
                  << i->typeName() << " "
                  << dec << setw(3)
                  << setfill(' ') << right
                  << i->count() << "  "
                  << dec << i->value()
                  << "\n";
    }
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
        OptVaryBoth,
        OptNoSave,
        OptNoDisplay,
        OptNoExif,
        OptComment,
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
        {"vary_both",     no_argument,        0,    OptVaryBoth },
        {"nosave",        no_argument,        0,    OptNoSave },
        {"nodisplay",     no_argument,        0,    OptNoDisplay },
        {"noexif",        no_argument,        0,    OptNoExif },
        {"verbose",       no_argument,        0,    OptVerbose },
        {"comment",       required_argument,  0,    OptComment },
        {NULL,            0,               NULL,    0}
    };

    // echo params out to record in run log
    // http://www.cplusplus.com/forum/beginner/211100/
    copy( argv, argv+argc, ostream_iterator<const char*>( cout, " " ) ) ;
    cout << endl << endl;

    while (true) {
        const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);
        //printf("opt ='%d' \n", opt);

        if (-1 == opt)
            break;

        switch (opt) {
        case OptMinutes:
            opt_minutes = stoi(optarg);
            cout << "Option: Minutes set to: " << opt_minutes << endl;
            break;
        case OptFrames:
            opt_frames = stoi(optarg);
            cout << "Option: Frames set to: " << opt_frames << endl;
            break;
        case OptGain:
            opt_gain = stoi(optarg);
            cout << "Option: Gain set to: " << opt_gain << endl;
            break;
        case OptExposure:
            opt_exposure = stoi(optarg);
            cout << "Option: Exposure set to: " << opt_exposure << endl;
            break;
        case OptVaryGain:
            opt_vary_gain = true;
            cout << "Option: sweep gain from min to max" << endl;
            break;
        case OptMinGain:
            opt_min_gain = stoi(optarg);
            cout << "Option: Min gain set to: " << opt_min_gain << endl;
            break;
        case OptMaxGain:
            opt_max_gain = stoi(optarg);
            cout << "Option: Max gain set to: " << opt_max_gain << endl;
            break;
        case OptVaryExposure:
            opt_vary_exposure = true;
            cout << "Option: sweep exposure from min to max" << endl;
            break;
        case OptMinExposure:
            opt_min_exposure = stoi(optarg);
            cout << "Option: Min exposure set to: " << opt_min_exposure << endl;
            break;
        case OptMaxExposure:
            opt_max_exposure = stoi(optarg);
            cout << "Option: Max exposure set to: " << opt_max_exposure << endl;
            break;
        case OptVaryBoth:
            opt_vary_both = true;
            cout << "Option: sweep both gain and exposure from min to max" << endl;
            break;
        case OptNoSave:
            opt_nosave = true;
            cout << "Option: no saving raw or jpeg files" << endl;
            break;
        case OptNoExif:
            opt_noexif = true;
            cout << "Option: no exif metadata in jpeg files" << endl;
            break;
        case OptNoDisplay:
            opt_nodisplay = true;
            cout << "Option: no display or jpeg files" << endl;
            break;
        case OptComment:
            opt_comment = string(optarg);
            cout << "Option: comment: " << opt_comment << endl;
            break;
        case OptVerbose:
            opt_verbose = true;
            cout << "Option: verbose mode" << endl;
            break;
        default:
            PrintHelp();
            break;
        }
    }

    cout << endl;
}

string GetDateTimeOriginal() {
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    return asctime(timeinfo);
}


// sort of from http://www.cplusplus.com/forum/beginner/60194/

string GetDateStamp() {
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

    stringstream iss;
    iss << setfill('0');
    iss << setw(4) << year;
    iss << setw(2) << month;
    iss << setw(2) << day;
    iss << setw(2) << hour;
    iss << setw(2) << minute;

    return iss.str(); //return the string present in iss.

}


string GetCurrentWorkingDir()
{
    char buff[PATH_MAX];
    if (getcwd(buff, PATH_MAX) == NULL) {
        return "";
    }

    string current_working_dir(buff);
    return current_working_dir;
}

// control values can either be an integer value or list of values
string getCurrentControlValue(IControl *control) {
    if (control->IsMenu()) {
        return (control->GetMenuEntries()[control->Get()].name);
    } else {
        return to_string(control->Get());
    }
}

void setMinMaxGain (ICamera *camera) {

    int32_t minValue = camera->GetControl(SV_V4L2_GAIN)->GetMinValue();
    int32_t maxValue = camera->GetControl(SV_V4L2_GAIN)->GetMaxValue();

    if (opt_min_gain < 0) {
        opt_min_gain = minValue;
    }
    if (opt_max_gain < 0) {
        opt_max_gain = maxValue;
    }

}

void setMinMaxExposure (ICamera *camera) {

    int32_t minValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetMinValue();
    int32_t maxValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetMaxValue();

    // override to make it easier to track numbers...instead of min of 38
    minValue = 50;

    if (opt_min_exposure < 0) {
        opt_min_exposure = minValue;
    }
    if (opt_max_exposure < 0) {
        opt_max_exposure = maxValue;
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
            cout << value << "  less than min value of Gain = " << minValue << endl;
        } else if (value > maxValue) {
            cout << value << "  greater than max value of Gain = " << maxValue << endl;
        }
        cout << " setting Gain = " << value << " default = " << defValue << endl;
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
            cout << value << "  less than min value of Exposure = " << minValue << endl;
        } else if (value > maxValue) {
            cout << value << "  greater than max value of Exposure = " << maxValue << endl;
        }
        cout << " setting Exposure = " << value << " default = " << defValue << endl;
    }


    camera->GetControl(SV_V4L2_EXPOSURE)->Set(value);

}

void varyGain (ICamera *camera) {
    opt_gain += opt_step_gain;
    if (opt_gain > opt_max_gain) {
        opt_gain = opt_min_gain;
    }
    setGain(camera, opt_gain);

}

void varyExposure (ICamera *camera) {
    opt_exposure += opt_step_exposure;
    if (opt_exposure > opt_max_exposure) {
        opt_exposure = opt_min_exposure;
    }
    setExposure(camera, opt_exposure);

}

void varyBoth (ICamera *camera) {

    // table empty? make.
    if (g_gaex_table.empty()) {
        generateGainExposureTable();
        g_gaex_table_ptr = g_gaex_table.begin();
    }
    cv::Point pt = *g_gaex_table_ptr;
    opt_gain = pt.x;
    opt_exposure = pt.y;

    setGain(camera, opt_gain);
    setExposure(camera, opt_exposure);

    // circulate pointer
    if ( g_gaex_table_ptr == g_gaex_table.end() ) {
        g_gaex_table_ptr = g_gaex_table.begin();
    } else {
        g_gaex_table_ptr++;
    }

}


void SaveFrame(void *data, uint32_t length, string filename) {

    remove(filename.c_str());
    int fd = open (filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        cout << "Unable to save frame, cannot open file " << filename << endl;
        return;
    }

    int writenBytes = write(fd, data, length);
    if (writenBytes < 0) {
        cout << "Error writing to file " << filename << endl;
    } else if ( (uint32_t) writenBytes != length) {
        cout << "Warning: " << writenBytes << " out of " << length << " were written to file " << filename << endl;
    }

    close(fd);
}

