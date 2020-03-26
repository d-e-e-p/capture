/*
 * capture raw frames from framos camera over csi
 * 
 *  (c) 2020 Tensorfield Ag
 * 
 *  take pics and save them 
 * 
 * 
 */


#include "capture.hpp"

// for logging
typedef boost::iostreams::tee_device<ostream, ofstream> TeeDevice;
typedef boost::iostreams::stream<TeeDevice> TeeStream;

// global vars -- but in a struct so that makes it ok :-)
struct Options {
    int  minutes = 0;
    int  frames = 1000;
    int  gain = 10;
    int  expo = 500; // max = 8256
    int  fps = 0;

    // -1 value for min/max sweep => use sensor min/max
    bool vary_gain = false;
    int  min_gain = -1;
    int  max_gain = -1;
    int  step_gain = 10;

    bool vary_expo = false;
    int  min_expo = -1;
    int  max_expo = -1;
    int  step_expo = 50;

    bool vary_both = false;

    bool create_jpeg_for_all = false;
    bool nosave = false;
    bool nodisplay = false;
    bool noexif = false;
    bool verbose = false;


    string header  = "Tensorfield Ag (c) 2020";
    string comment = "";

} opt;

struct Globals {
    // gain+exposure table
    list < cv::Point > gaex_table;
    list < cv::Point >::iterator gaex_table_ptr;

    //dng header
    char* dng_headerdata = NULL;
    int   dng_headerlength = 0;
    int   dng_attribute_start = 5310;

    //
    string dateStamp;

} g;

struct FilePaths {

    //dng header
    fs::path headerfile = "/home/deep/build/snappy/bin/dng_header.bin";

    //misc file locations
    fs::path folder_data = "/home/deep/build/snappy/data/images/";
    fs::path folder_base ;
    fs::path folder_raw ;
    fs::path folder_dng ;
    fs::path folder_jpg ;
    fs::path file_log ;
    fs::path file_raw ;
    fs::path file_dng ;
    fs::path file_jpg ;

    fs::path file_current_jpg = folder_data / "current.jpg";
    fs::path file_next_jpg    = folder_data / "next.jpg";

} p;

// stuff to shutdown...including camera
struct shutDown {
    TeeStream* logger;
    vector<thread> threads;
    future <void> job;
} s;


// protos...i prefer having them here over in .hpp
void PrintHelp() ;
void setupDirs() ;
ICamera* setupCamera() ;
void runCaptureLoop(ICamera *camera) ;
string constructImageInfoTag(IImage image, string basename) ;
void readDngHeaderData() ;
int saveFrameWithHeader(void *data, uint32_t datalength, string filename, string imageinfo) ;
void generateGainExposureTable() ;
void saveAndDisplayJpgDirect(ICamera *camera, IProcessedImage processedImage, string text, string basename) ;
void saveDng(IImage image, string basename) ;
void saveJpgExternal(string text, string basename ) ;
void exifPrint(const Exiv2::ExifData exifData) ;
void processArgs(int argc, char** argv) ;
string getDateTimeOriginal() ;
string getDateStamp() ;
string getCurrentControlValue(IControl *control) ;
void setMinMaxGain (ICamera *camera) ;
void setMinMaxExposure (ICamera *camera) ;
void setGain (ICamera *camera, uint32_t value) ;
void setExposure (ICamera *camera, uint32_t value) ;
void takeDummyPicsToMakeSettingsStick(ICamera *camera) ;
void varyGain (ICamera *camera) ;
void varyExposure (ICamera *camera) ;
void varyBoth (ICamera *camera) ;
void saveFrame(void *data, uint32_t length, string filename) ;
void cleanUp (ICamera *camera) ;

/*
 * main routine:
 *    1. parse params
 *    2. setup cameras
 *    3. loop capture raw image
 *    4.    convert some to jpeg inside loop
 *    5. end
 *
 */

int main(int argc, char **argv) {
    ICamera *camera;

    setupDirs();
    processArgs(argc, argv);
    camera = setupCamera();
    runCaptureLoop(camera);
    cleanUp(camera);

    return 0;
}


void PrintHelp() {
    cout <<
       "--minutes <n>:       set num of minutes to capture\n"
       "--frames <n>:        set number of frames to capture\n"
       "--gain <n>:          set gain\n"
       "--exposure <n>:      set exposure\n"
       "--fps <n>:           set rate limit on frames to capture per sec\n"
       "--vary_gain:         sweep gain from min to max\n"
       "--min_gain:          min gain for sweep\n"
       "--max_gain:          max gain for sweep\n"
       "--vary_expo:         sweep exposure from min to max\n"
       "--min_expo:          min gain for sweep\n"
       "--max_expo:          max gain for sweep\n"
       "--vary_both:         sweep both gain and exposure from min to max\n"
       "--nosave:            no saving any images--just display\n"
       "--nodisplay:         no jpeg or display--just raw image dump\n"
       "--noexif:            no exif metadata in jpeg\n"
       "--comment:           specify a comment to remember the run\n"
       "--verbose:           verbose\n"
       "--help:              show help\n";
    exit(1);
}


void setupDirs() {
    // filename stuff...
    g.dateStamp  = getDateStamp();
    p.folder_base = p.folder_data / g.dateStamp;
    fs::create_directories(p.folder_base);

    // see https://stackoverflow.com/questions/19641190/c-duplicate-stdout-to-file-by-redirecting-cout
    p.file_log = p.folder_base / "run.log";
    fs::remove(p.file_log);
    ofstream logFile;
    logFile.open(p.file_log);

    ostream tmp(cout.rdbuf()); // <----
    TeeDevice outputDevice(tmp, logFile); // <----
    TeeStream logger(outputDevice);
    s.logger = &logger;

    //cout.rdbuf(logger.rdbuf());
    cout << opt.header << endl;
    cout << opt.comment << endl << flush;

    p.folder_raw  = p.folder_base  / "raw";
    p.folder_dng  = p.folder_base  / "dng";
    p.folder_jpg  = p.folder_base  / "jpg";
    fs::create_directories(p.folder_raw);
    fs::create_directories(p.folder_dng);
    fs::create_directories(p.folder_jpg);

    cout << "saving results under " << p.folder_base << endl;
    if (opt.verbose) {
        cout << " raw under folder : " << p.folder_raw << endl;
        cout << " dng under folder : " << p.folder_dng << endl;
        cout << " jpg under folder : " << p.folder_jpg << endl;
    }
}

ICamera* setupCamera() {

    fs::path file_camera_settings = p.folder_base / "camera.settings";
    string command = "/usr/local/bin/v4l2-ctl --verbose --all > " + file_camera_settings.string();
    system( (const char *) command.c_str());

    ICameraList cameras = sv::GetAllCameras();
    if (cameras.size() == 0) {
        cerr << "No cameras detected! Exiting..." << endl;
        return 0;
    }

    // on multi-camera system just select first camera
    ICamera *camera = cameras[0];
    IControl *control ;

    control = camera->GetControl(SV_V4L2_IMAGEFORMAT);

    // set default gain and exposure
    setGain(camera,opt.gain);
    setExposure(camera,opt.expo);
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
        if (opt.verbose) {
            cout << "Control: " << name << " = " << value << " : id = " << id << endl;
        } else {
            cout << "Control: " << name << " = " << value <<  endl;
        }
    }


    if (!camera->StartStream()) {
        cerr << "Failed to start stream!" << endl;
        return 0;
    }

    return camera;

}

void runCaptureLoop(ICamera *camera) {
    // inits for loop
    int num_frames = 0; // index for frame count
    int frameSaved = 0;
    int frameCount = opt.frames;
    if (opt.minutes > 0) {
        // calculate frames only for finding width of filenames field for printf
        if (opt.fps == 0) {
            int estimated_fps = 100;
            frameCount = opt.minutes * 60 * estimated_fps;
            //cout << "estimating to capture " << frameCount << " frames at " <<  estimated_fps << "fps " << endl;
        } else {
            frameCount = opt.minutes * 60 * opt.fps;
        }
    }
    int maxIntegerWidth = to_string(frameCount).length() + 1;
    common::FpsMeasurer fpsMeasurer;

    // time stuff
    auto now  = chrono::system_clock::now();
    auto start_prog = now;
    auto start_loop = now;
    int duration_between_saves_ms;
    if (opt.fps > 0) {
        duration_between_saves_ms = round(1000.0 / (float) opt.fps);
    }  

    //vector<thread> s.threads;  // keep track of threads to close in shutdown
    //stuct
    s.job = async(launch::async, []{ });

    while (true) {

        now = chrono::system_clock::now();
        // exit on minutes of wall clock time?
        if (opt.minutes > 0) {
            int duration_min = round(chrono::duration_cast<chrono::minutes>(now -   start_prog).count());
            if (duration_min > opt.minutes) {
                break;
            }
        } else if (num_frames > opt.frames) {
            break;
        }

        // ratelimit if opt.fps is set
        if (opt.fps > 0) {
            int duration_ms = round(chrono::duration_cast<chrono::minutes>(now - start_loop).count());
            if (duration_ms < duration_between_saves_ms) {
                int sleep_duration_ms = duration_between_saves_ms - duration_ms;
                this_thread::sleep_for(chrono::milliseconds(sleep_duration_ms));
            }
            start_loop  = chrono::system_clock::now();
        } 

        if (opt.vary_both) {
            varyBoth(camera);
        } else {
            if (opt.vary_gain)
                varyGain(camera);
            if (opt.vary_expo)
                varyExposure(camera);
        }

        // actually get the image
        IImage image = camera->GetImage();

        if (image.data == nullptr) {
            cerr << "Unable to save frame, invalid image data" << endl;
            break;
        }

        //string num = to_string(num_frames++);
        //num.insert(num.begin(), maxIntegerWidth - num.length(), '0');
        //string basename     = "frame" + num ;

        fpsMeasurer.FrameReceived();



        // is it possible that opt.gain is not the same as control?
        if (opt.gain != camera->GetControl(SV_V4L2_GAIN)->Get()) {
            cerr << "mismatch! opt.gain = " << opt.gain << " while control = " << camera->GetControl(SV_V4L2_GAIN)->Get() << endl;
        }
        if (opt.expo != camera->GetControl(SV_V4L2_EXPOSURE)->Get()) {
            cerr << "mismatch! opt.gain = " << opt.gain << " while control = " << camera->GetControl(SV_V4L2_EXPOSURE)->Get() << endl;
        }

        // c++ does have reasonable printf until c++20 sigh.
        // if opt/gain are being changed, used long name...
        char buff[BUFSIZ];
        if (opt.vary_both or opt.vary_gain or opt.vary_expo) {
            snprintf(buff, sizeof(buff), "img%0*d_G%03dE%04d",maxIntegerWidth, num_frames++, opt.gain, opt.expo);
        } else {
            snprintf(buff, sizeof(buff), "img%0*d", maxIntegerWidth, num_frames++);
        }
        string basename = buff;

        stringstream ss;
        ss << basename;
        ss << " fps = "  << fpsMeasurer.GetFps();
        ss << " gain = " << opt.gain;
        ss << " exp = "  << opt.expo;
        string text = ss.str();    
        if (opt.verbose) {
            cout << "\t" << text << endl << flush;
        }

        if (image.data == nullptr) {
            cerr << "ERR: image.data null" << endl;
            break;
        }
        if (! opt.nosave) {
            fs::path file_raw = p.folder_raw / (basename + ".raw");
            saveFrame(image.data, image.length, file_raw.string());
        }


        saveDng(image, basename);

        if (opt.create_jpeg_for_all) {
            saveJpgExternal( text, basename ) ;
        } else {
            // Use wait_for() with zero time to check thread status.
            chrono::nanoseconds ns(1);
            auto status = s.job.wait_for(ns);
            if (status == future_status::ready) {
                s.job = async(launch::async, saveJpgExternal, text, basename ) ;
            } 
         }


        camera->ReturnImage(image);
    }
    // cleanup
    cout << "\nSaved " << num_frames << " frames to " << p.folder_raw << " folder with fps = " << fpsMeasurer.GetFps() << endl << flush;
    cout << "session log :" << p.file_log << endl;
}

// to go into exif of jpeg eventually...
string constructImageInfoTag(IImage image, string basename) {
     if (false) {
         cout << " image.id          : " << image.id           << endl;
         cout << " image.length      : " << image.length       << endl;
         cout << " image.width       : " << image.width        << endl;
         cout << " image.height      : " << image.height       << endl;
         cout << " image.pixelFormat : " << image.pixelFormat  << endl;
         cout << " image.stride      : " << image.stride       << endl;
         cout << " image.timestamp.s : " << image.timestamp.s << "." << image.timestamp.us  << endl;
     }

     stringstream ss;
     ss << "header="                    << opt.header          << ":";
     ss << "frame="                     << basename            << ":";
     ss << "gain="                      << opt.gain            << ":";
     ss << "expo="                      << opt.expo            << ":";
     ss << "image.id="                  << image.id            << ":";
     ss << "image.length="              << image.length        << ":";
     ss << "image.width="               << image.width         << ":";
     ss << "image.height="              << image.height        << ":";
     ss << "image.pixelFormat="         << image.pixelFormat   << ":";
     ss << "image.stride="              << image.stride        << ":";
     ss << "image.timestamp.s="         << image.timestamp.s   << ":";
     ss << "image.timestamp.us="        << image.timestamp.us  << ":";
     ss << "comment="                   << opt.comment         << ":";

     return ss.str();
}

void readDngHeaderData() {
    g.dng_headerlength = fs::file_size(p.headerfile);
    cout << " dng header : " << p.headerfile << " size = " << g.dng_headerlength << endl << flush;
    g.dng_headerdata = new char [g.dng_headerlength];
    ifstream fhead (p.headerfile, ios::in|ios::binary);
    fhead.read (g.dng_headerdata, g.dng_headerlength);
    fhead.close();
}

int saveFrameWithHeader(void *data, uint32_t datalength, string filename, string imageinfo) {

    // read in header data once if it doesn't exist
    if (g.dng_headerdata == nullptr) {
        readDngHeaderData();
    }

    //modify header with image attributes
    // TODO seek for <?xpacket end="w"?> once and store that position
    // instead of using hardcoded header start
    vector<char> mod_headerdata(g.dng_headerdata,g.dng_headerdata + g.dng_headerlength);
    for (int i = 0; i < imageinfo.size(); i++) {
        mod_headerdata.at(i+g.dng_attribute_start) = imageinfo[i];
    }

    fs::remove(filename);
    ofstream outfile (filename,ofstream::binary);
    outfile.write (reinterpret_cast<const char*>(&mod_headerdata[0]), g.dng_headerlength);
    outfile.write (reinterpret_cast<const char*>(data), datalength);
    outfile.close();

}

void generateGainExposureTable() {


    for (int value_gain = opt.min_gain; value_gain < opt.max_gain; value_gain += opt.step_gain) {
        for (int value_expo = opt.min_expo; value_expo < opt.max_expo; value_expo += opt.step_expo) {
            g.gaex_table.push_back(cv::Point(value_gain, value_expo ));
            //cout << "Vary table (gain,exposure) =  " << cv::Point(value_gain, value_expo) << endl;
        }
    }

    cout << "Generated gain+exposure table:" << endl;
    cout << "    gain range =     " << cv::Point(opt.min_gain, opt.max_gain) << " in steps of " << opt.step_gain << endl;
    cout << "    exposure range = " << cv::Point(opt.min_expo, opt.max_expo) << " in steps of " << opt.step_expo << endl;
    cout << "    total values in sweep  =  " << g.gaex_table.size() << endl;

    if (false) {
        for (auto it : g.gaex_table) {
            cout << it << "\n";
        }
    }


}

/*

// run in separate thread
// TODO: setup workers to handle this in background without wasting time every
// loop allocating and releasing all this overhead memory
void saveAndDisplayJpgDirect(ICamera *camera, IProcessedImage processedImage, string text, string basename) {
    common::ImageProcessor processor;
    // TODO: use image attribute date instead
    const string dateStamp  = getDateStamp();

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
    if (opt.noexif) {
        saveFrame(&jpgbuf[0], jpgbuf.size(), filenameJpg);
    } else {
        Exiv2::Image::AutoPtr eImage = Exiv2::ImageFactory::open(&jpgbuf[0],jpgbuf.size());
        exifData["Exif.Image.ProcessingSoftware"] = "capture.cpp";
        exifData["Exif.Image.SubfileType"] = 1;
        exifData["Exif.Image.ImageDescription"] = "snappy carrots";
        exifData["Exif.Image.Model"] = "tensorfield ag";
        exifData["Exif.Image.AnalogBalance"] = opt.gain;
        exifData["Exif.Image.ExposureTime"] = opt.expo;
        exifData["Exif.Image.DateTimeOriginal"] = dateStamp;
        eImage->setExifData(exifData);
        eImage->writeMetadata();
        if (opt.verbose) {
            Exiv2::ExifData &ed2 = eImage->exifData();
            exifPrint(ed2);
        }

        if (! opt.nosave) {
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
    if (opt.verbose) {
        cout << ". " << folderJpg << filenameJpg << endl << flush;
    }
    cv::namedWindow("capture", cv::WINDOW_OPENGL | cv::WINDOW_AUTOSIZE);
    cv::imshow("capture", mImage);
    cv::waitKey(1);
}
*/

void saveDng(IImage image, string basename) {

    string imageinfo = constructImageInfoTag(image, basename);

    fs::path file_dng = p.folder_dng / (basename + ".dng");
    saveFrameWithHeader(image.data, image.length, file_dng, imageinfo);
    if (opt.verbose) {
        cout << "dng output: " << file_dng << endl;
        cout << "  tags = " << imageinfo << endl;
    }
}


void saveJpgExternal(string text, string basename ) {

    // long names => smaller font
    int pointsize = 20;
    if (opt.vary_both or opt.vary_gain or opt.vary_expo) {
        pointsize = 15;
    }

    fs::path file_dng = p.folder_dng / (basename + ".dng");
    fs::path file_jpg = p.folder_jpg / (basename + ".jpg");
    //string cmd = "rawtherapee-cli -Y -o /home/deep/build/snappy/capture/next.jpg -q -p /home/deep/build/snappy/bin/wb.pp4 -c " + folderDng + filenameDng;
    string cmd = "rawtherapee-cli -Y -o " +  p.file_next_jpg.string() + " -q -c " +  file_dng.string();
    cmd += "; magick " +  p.file_next_jpg.string() + " -clahe 25x25%+128+2 -pointsize " + to_string(pointsize) + " -font Inconsolata -fill yellow -gravity East -draw \'translate 20,230 rotate 90 text 0,0 \" " + text + " \" \'   " + file_jpg.string() ;
    if (opt.verbose) {
        cout << "running cmd=" << cmd << endl;
    }
    //cout << "running cmd: " << cmd << endl;
    system( (const char *) cmd.c_str());
    fs::copy(file_jpg, p.file_current_jpg, fs::copy_options::overwrite_existing);

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




void processArgs(int argc, char** argv) {
    const char* const short_opts = "-:";
    enum Option {
        OptMinutes,
        OptFrames,
        OptGain,
        OptExposure,
        OptFps,
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
        {"fps",           required_argument,  0,    OptFps },
        {"vary_gain",     no_argument,        0,    OptVaryGain },
        {"min_gain",      required_argument,  0,    OptMinGain },
        {"max_gain",      required_argument,  0,    OptMaxGain },
        {"vary_expo",     no_argument,        0,    OptVaryExposure },
        {"vary_expo",     no_argument,        0,    OptVaryExposure },
        {"min_expo",      required_argument,  0,    OptMinExposure },
        {"max_expo",      required_argument,  0,    OptMaxExposure },
        {"vary_exposure", no_argument,        0,    OptVaryExposure },
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
        const auto options = getopt_long(argc, argv, short_opts, long_opts, nullptr);
        //printf("options ='%d' \n", options);

        if (options == -1)
            break;

        switch (options) {
        case OptMinutes:
            opt.minutes = stoi(optarg);
            cout << "Option: Minutes set to: " << opt.minutes << endl;
            break;
        case OptFrames:
            opt.frames = stoi(optarg);
            cout << "Option: Frames set to: " << opt.frames << endl;
            break;
        case OptGain:
            opt.gain = stoi(optarg);
            cout << "Option: Gain set to: " << opt.gain << endl;
            break;
        case OptExposure:
            opt.expo = stoi(optarg);
            cout << "Option: Exposure set to: " << opt.expo << endl;
            break;
        case OptFps:
            opt.fps = stoi(optarg);
            cout << "Option: fps set to: " << opt.fps << endl;
            break;
        case OptVaryGain:
            opt.vary_gain = true;
            cout << "Option: sweep gain from min to max" << endl;
            break;
        case OptMinGain:
            opt.min_gain = stoi(optarg);
            cout << "Option: Min gain set to: " << opt.min_gain << endl;
            break;
        case OptMaxGain:
            opt.max_gain = stoi(optarg);
            cout << "Option: Max gain set to: " << opt.max_gain << endl;
            break;
        case OptVaryExposure:
            opt.vary_expo = true;
            cout << "Option: sweep exposure from min to max" << endl;
            break;
        case OptMinExposure:
            opt.min_expo = stoi(optarg);
            cout << "Option: Min exposure set to: " << opt.min_expo << endl;
            break;
        case OptMaxExposure:
            opt.max_expo = stoi(optarg);
            cout << "Option: Max exposure set to: " << opt.max_expo << endl;
            break;
        case OptVaryBoth:
            opt.vary_both = true;
            cout << "Option: sweep both gain and exposure from min to max" << endl;
            break;
        case OptNoSave:
            opt.nosave = true;
            cout << "Option: no saving raw or jpeg files" << endl;
            break;
        case OptNoExif:
            opt.noexif = true;
            cout << "Option: no exif metadata in jpeg files" << endl;
            break;
        case OptNoDisplay:
            opt.nodisplay = true;
            cout << "Option: no display or jpeg files" << endl;
            break;
        case OptComment:
            opt.comment = string(optarg);
            cout << "Option: comment: " << opt.comment << endl;
            break;
        case OptVerbose:
            opt.verbose = true;
            cout << "Option: verbose mode" << endl;
            break;
        default:
            PrintHelp();
            break;
        }
    }

    cout << endl;
}

string getDateTimeOriginal() {
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    return asctime(timeinfo);
}


// sort of from http://www.cplusplus.com/forum/beginner/60194/

string getDateStamp() {
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

    if (opt.min_gain < 0) {
        opt.min_gain = minValue;
    }
    if (opt.max_gain < 0) {
        opt.max_gain = maxValue;
    }

}

void setMinMaxExposure (ICamera *camera) {

    int32_t minValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetMinValue();
    int32_t maxValue = camera->GetControl(SV_V4L2_EXPOSURE)->GetMaxValue();

    // override to make it easier to track numbers...instead of min of 38
    minValue = 50;

    if (opt.min_expo < 0) {
        opt.min_expo = minValue;
    }
    if (opt.max_expo < 0) {
        opt.max_expo = maxValue;
    }

}


// modulo gain if asked value greater than max !
void setGain (ICamera *camera, uint32_t value) {

    int32_t minValue = camera->GetControl(SV_V4L2_GAIN)->GetMinValue();
    int32_t maxValue = camera->GetControl(SV_V4L2_GAIN)->GetMaxValue();
    int32_t defValue = camera->GetControl(SV_V4L2_GAIN)->GetDefaultValue();

    if (opt.min_gain > 0) {
        minValue = opt.min_gain;
    }
    if (opt.max_gain > 0) {
        maxValue = opt.max_gain;
    }

    if (value < minValue) {
        value = minValue;
    } else if (value > maxValue) {
        value = value % maxValue;
    }

    if (opt.verbose) {
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

    if (opt.min_expo > 0) {
        minValue = opt.min_expo;
    }
    if (opt.max_expo > 0) {
        maxValue = opt.max_expo;
    }

    if (value < minValue) {
        value = minValue;
    } else if (value > maxValue) {
        value = value % maxValue;
    }

    if (opt.verbose) {
        if (value < minValue) {
            cout << value << "  less than min value of Exposure = " << minValue << endl;
        } else if (value > maxValue) {
            cout << value << "  greater than max value of Exposure = " << maxValue << endl;
        }
        cout << " setting Exposure = " << value << " default = " << defValue << endl;
    }


    camera->GetControl(SV_V4L2_EXPOSURE)->Set(value);

}

// without this there are a couple of frames with wrong setting
void takeDummyPicsToMakeSettingsStick(ICamera *camera) {
    int num_of_dummy_pics = 10;
    IImage image;
    for (int i = 0; i < num_of_dummy_pics; i++) {
        image = camera->GetImage();
        camera->ReturnImage(image);
    }
}

void varyGain (ICamera *camera) {
    opt.gain += opt.step_gain;
    if (opt.gain > opt.max_gain) {
        opt.gain = opt.min_gain;
    }
    setGain(camera, opt.gain);
    takeDummyPicsToMakeSettingsStick(camera);

}

void varyExposure (ICamera *camera) {
    opt.expo += opt.step_expo;
    if (opt.expo > opt.max_expo) {
        opt.expo = opt.min_expo;
    }
    setExposure(camera, opt.expo);
    takeDummyPicsToMakeSettingsStick(camera);

}

// TODO: return index number of current iteration
void varyBoth (ICamera *camera) {

    // table empty? make.
    if (g.gaex_table.empty()) {
        generateGainExposureTable();
        g.gaex_table_ptr = g.gaex_table.begin();
    }
    cv::Point pt = *g.gaex_table_ptr;
    opt.gain = pt.x;
    opt.expo = pt.y;

    setGain(camera, opt.gain);
    setExposure(camera, opt.expo);
    takeDummyPicsToMakeSettingsStick(camera);

    // circulate pointer
    if ( g.gaex_table_ptr == g.gaex_table.end() ) {
        g.gaex_table_ptr = g.gaex_table.begin();
    } else {
        g.gaex_table_ptr++;
    }

}


void saveFrame(void *data, uint32_t length, string filename) {

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

void cleanUp (ICamera *camera) {
    // clean up...
    if (opt.verbose) {
        cout << " final cleanup.." << endl << flush;;
    }
    camera->StopStream();
    s.job.wait();
    for (auto& th : s.threads) {
        th.join();
    }
    cout << " final cleanup going to close logger.." << endl << flush;;
    s.logger->close();
    cout << " final cleanup done     close logger.." << endl << flush;;
}

