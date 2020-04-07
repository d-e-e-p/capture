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
#include "Imagedata.hpp"

// global vars -- but in a struct so that makes it ok :-)
struct Options {
    int  minutes = 0;
    int  frames = 1000;
    int  gain = 50;
    int  expo = 750; // max = 8256
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

    bool alljpg = false;
    bool nosave = false;
    bool nojpg = false;
    bool noexif = false;
    bool display = false;
    bool interactive = false;
    bool verbose = false;

    string comment = "";

} opt;

struct Globals {
    string header  = "Tensorfield Ag (c) 2020";
    string command;
    // gain+exposure table
    list < cv::Point > gaex_table;
    list < cv::Point >::iterator gaex_table_ptr;

    //
    string datestamp;
    string mainWindowName;

    int imagewidth  = 768;
    int imageheight = 544;
    bool toggle_f = false;

} g;

struct FilePaths {

    //dng header
    fs::path headerfile = "/home/deep/build/snappy/bin/dng_header.bin";

    //misc file locations
    fs::path folder_data ; // relative to exe ../bin/capture => ../data/images
    fs::path folder_base ; // datestamp dir under folder_data
    fs::path folder_raw ;
    fs::path folder_dng ;
    fs::path folder_jpg ;

    fs::path file_current_dng = folder_data / "current.dng";
    fs::path file_current_jpg = folder_data / "current.jpg";
    fs::path file_next_jpg    = folder_data / "next.jpg";

    fs::path file_log ;

} fp;

// using in callback in loop
struct Loop {
    int num_of_dummy_pics = 0;
    int num_frames = 0; // index for frame count
    int frameSaved = 0;
    int maxIntegerWidth;
    common::FpsMeasurer fpsMeasurer;
    int sweep_index = 0;
    int sweep_total = 0;
    int duration_between_saves_ms = 0;

    chrono::system_clock::time_point syst_time_prog = chrono::system_clock::now();
    chrono::system_clock::time_point syst_time_last  = syst_time_prog;
    chrono::system_clock::time_point syst_time_this  = syst_time_last;
    // use std::chrono::high_resolution_clock::time_point 
    chrono::steady_clock::time_point stdy_time_last = chrono::steady_clock::now();
    chrono::steady_clock::time_point stdy_time_this = stdy_time_last;

} lp;


// stuff to shutdown...including camera
struct shutDown {
    vector<thread> threads;
    future <void> job;
} sd;


// init Imagedata class
Imagedata::ClassInit Imagedata::readDngHeaderData(fp.headerfile);


// protos...i prefer having them here over instead of way over in .hpp
void PrintHelp() ;
void setupDirs() ;
void setupCamera() ;
void runCaptureLoop(ICamera *camera) ;
void generateGainExposureTable() ;
void saveAndDisplayJpgDirect(ICamera *camera, IProcessedImage processedImage, string text, string basename) ;
string exec(string cmd);
void exifPrint(const Exiv2::ExifData exifData) ;
void processArgs(int argc, char** argv) ;
string getDateTimeOriginal() ;
string getDateStamp() ;
string getCurrentControlValue(IControl *control) ;
void setMinMaxGain (ICamera *camera) ;
void setMinMaxExposure (ICamera *camera) ;
void setGain (int value) ;
void setExposure (int value) ;
void takeDummyPicsToMakeSettingsStick(void) ;
void varyGain (void) ;
void varyExposure (void) ;
int varyBoth (void) ;
void saveRaw(Imagedata* image) ;
void saveDng(Imagedata* image) ;
void saveJpg(Imagedata* image);
void cleanUp (void) ;
void setupLoop(void);
void endLoop(void);
int endLoopCalback(void *ptr, int size, struct v4l2_buffer buf);
void syncGainExpo(void);
void callbackButton(int state, void *font);


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
    dev_name = (char *) "/dev/video0";

    setupDirs();
    processArgs(argc, argv);
    setupLoop();
    open_device(); 
     init_device(); 
        setupCamera();
        start_capturing();

         frame_count = opt.frames;
         mainloop();

       stop_capturing(); 
     uninit_device(); 
    close_device();
    endLoop();
    cleanUp();

    return 0;
}


void PrintHelp() {
    LOGI <<
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
       "--nojpg:             no jpeg or display--just raw image dump\n"
       "--alljpg:            capture jpg for every image\n"
       "--noexif:            no exif metadata in jpeg\n"
       "--comment:           specify a comment to remember the run\n"
       "--display:           display window\n"
       "--interactive        display window and only save on keypress\n"
       "--verbose:           verbose\n"
       "--help:              show help\n";
    exit(1);
}

// count = BUFSIZ is also a problem...
fs::path getExePath() {
  char result[ BUFSIZ ];
  int count = readlink( "/proc/self/exe", result, BUFSIZ );
  if (count == -1 || count == sizeof(result)) {
    perror("readlink error on /proc/self/exe: ");
    exit(-1);
  }
  result[count] = '\0';
  return string( result );
}

void setupDirs() {

    // first figure out path to exe and base use that to determine where to 
    // put the images
    fs::path file_exe = getExePath();
    // snappy/bin/capture -> snappy/data
    fp.folder_data = file_exe.parent_path().parent_path().string() + "/data/images";
    if (! fs::exists(fp.folder_data)) {
        cout << "creating data dir : " << fp.folder_data << "\n";
    }
    fp.file_current_dng = fp.folder_data / "current.dng";
    fp.file_current_jpg = fp.folder_data / "current.jpg";
    fp.file_next_jpg    = fp.folder_data / "next.jpg";

    // filename stuff...
    g.datestamp  = getDateStamp();
    fp.folder_base = fp.folder_data / g.datestamp;
    fs::create_directories(fp.folder_base);

    fp.file_log = fp.folder_base / "run.log";
    if (fs::exists(fp.file_log)) {
        fs::remove(fp.file_log);
    }
    g.mainWindowName = fp.folder_base.string();

    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    static plog::RollingFileAppender<plog::TxtFormatter, plog::NativeEOLConverter<> > fileAppender(fp.file_log.string().c_str());

    // verbose mode? capture everything in the log
    // oops, can't do this yet because args haven't been processed...
    //plog::Severity maxSeverity = opt.verbose ? plog::verbose : plog::info;
    plog::init(plog::info, &fileAppender).addAppender(&consoleAppender);

    LOGI << g.header ;
    LOGI << opt.comment ;

    fp.folder_raw = fp.folder_base / "raw"; fs::create_directories(fp.folder_raw);
    fp.folder_dng = fp.folder_base / "dng"; fs::create_directories(fp.folder_dng);
    fp.folder_jpg = fp.folder_base / "jpg"; fs::create_directories(fp.folder_jpg);

    LOGI << "results under:" << fp.folder_base.string() ;
    LOGV << "   raw under : "  << fp.folder_raw.string();
    LOGV << "   dng under : "  << fp.folder_dng.string();
    LOGV << "   jpg under : "  << fp.folder_jpg.string();

}

void setupCamera() {

    // set default gain and exposure
    setGain(opt.gain);
    setExposure(opt.expo);
    set_fps(200);

    fs::path file_camera_settings = fp.folder_base / "camera.settings";
    string command = "/usr/local/bin/v4l2-ctl --verbose --all > " + file_camera_settings.string();
    //system( (const char *) command.c_str());
    LOGI << command;
    LOGI << exec(command);

    if (opt.display) {
        int trackbar;
        namedWindow(g.mainWindowName, CV_WINDOW_NORMAL);
        resizeWindow(g.mainWindowName, g.imagewidth, g.imageheight);
        createButton("save image",callbackButton,NULL,QT_PUSH_BUTTON,1);
        createTrackbar( "track1", g.mainWindowName, &trackbar, 255,  NULL);
    }


    //TODO
    //camera->GetControl(SV_V4L2_BLACK_LEVEL)->Set(0);
    //camera->GetControl(SV_V4L2_FRAMESIZE)->Set(0);

    // set min/max levels
    //setMinMaxGain(camera);
    //setMinMaxExposure(camera);

    // assume defaults
    //common::SelectPixelFormat(control); 
    //common::SelectFrameSize(camera->GetControl(SV_V4L2_FRAMESIZE), camera->GetControl(SV_V4L2_FRAMEINTERVAL));
    //common::SelectFrameSize(camera->GetControl(SV_V4L2_FRAMESIZE), camera->GetControl(SV_V4L2_FRAMEINTERVAL));

    // dump settings
    /*
    IControlList controls = camera->GetControlList();
    for (IControl *control : controls) {
        string name  = string(control->GetName());
        string value = getCurrentControlValue(control);
        string id    = to_string(control->GetID());
        if (opt.verbose) {
            LOGV << "Control: " << name << " = " << value << " : id = " << id ;
        } else {
            LOGI << "Control: " << name << " = " << value ;
        }
    }


    if (!camera->StartStream()) {
        LOGE << "Failed to start stream!" ;
        exit(-1);
    }

    return camera;
    */
    return;

}

// src : https://gist.github.com/royshil/1449e22993e98414e9eb#file-simplestcolorbalance-cpp
//
/// perform the Simplest Color Balancing algorithm
void SimplestCB(Mat& in, Mat& out, float percent) {
    assert(in.channels() == 3);
    assert(percent > 0 && percent < 100);

    float half_percent = percent / 200.0f;

    vector<Mat> tmpsplit; split(in,tmpsplit);
    for(int i=0;i<3;i++) {
        //find the low and high precentile values (based on the input percentile)
        Mat flat; tmpsplit[i].reshape(1,1).copyTo(flat);
        cv::sort(flat,flat,CV_SORT_EVERY_ROW + CV_SORT_ASCENDING);
        int lowval = flat.at<uchar>(cvFloor(((float)flat.cols) * half_percent));
        int highval = flat.at<uchar>(cvCeil(((float)flat.cols) * (1.0 - half_percent)));
        //cout << lowval << " " << highval << endl;

        //saturate below the low percentile and above the high percentile
        tmpsplit[i].setTo(lowval,tmpsplit[i] < lowval);
        tmpsplit[i].setTo(highval,tmpsplit[i] > highval);

        //scale the channel
        normalize(tmpsplit[i],tmpsplit[i],0,255,NORM_MINMAX);
    }
    merge(tmpsplit,out);
}


void performHotkeyActions(Imagedata* image, int key) {

    LOGI << "key = " << key;
    switch (key) {
        case 'f' : 
            g.toggle_f = ! g.toggle_f;
            LOGI << "resize window " << g.toggle_f;
            if (g.toggle_f) {
                resizeWindow(g.mainWindowName, 2 * g.imagewidth, 2 * g.imageheight);
            } else {
                resizeWindow(g.mainWindowName, 1 * g.imagewidth, 1 * g.imageheight);
            }
            break;
        case 's' : 
            LOGI << "saving";
            saveRaw(image) ;
            saveDng(image) ;
            saveJpg(image) ;
            break;
        case 'q' : 
            LOGI << "quitting.";
            stop_capturing();
            uninit_device();
            close_device();
            endLoop();
            cleanUp();
            exit(0);
            break;
        default :
            LOGI << "unknown key " << key;
    }

}

void showImage(Imagedata* image) {
    Mat image_16bit(image->height, image->width, CV_16UC1);
    memcpy(image_16bit.data, image->data, image->size);
    const int CONVERSION_SCALE = 1.0;
    Mat image_bayer1(image->height, image->width, CV_16UC3);
    demosaicing(image_16bit, image_bayer1, COLOR_BayerRG2RGB);

    Mat image_colorcorr(image->height, image->width, CV_16UC3);
    //cv::xphoto::WhiteBalancer::balanceWhite(image_bayer1,image_colorcorr);
    //SimplestCB(image_bayer1,image_colorcorr,50);

    double min, max;
    //minMaxLoc(image_colorcorr, &min, &max);
    //LOGV << "image_colorcorr (black,white) = " << Point(min,max);
    imshow(g.mainWindowName,image_bayer1);
	image->createAnnoText();
    displayOverlay(g.mainWindowName, image->text_north, 0);
    displayStatusBar(g.mainWindowName, image->text_east, 0);

    int key = cv::waitKey(1);
    if (key >= 0) {
        performHotkeyActions(image, key);
    }
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void process_image(void *ptr, int size, struct v4l2_buffer buf) {
    //if (out_buf)
    //   fwrite(p, size, 1, stdout);
    endLoopCalback(ptr,size, buf);
    //fflush(stderr);
    //fprintf(stderr, ".");
    //fflush(stdout);
}


void setupLoop(void) {
    // inits for loop
    //
    // estimate lp.maxIntegerWidth to format basename
    int frameCount = opt.frames;
    if (opt.minutes > 0) {
        // calculate frames only for finding width of filenames field for printf
        if (opt.fps == 0) {
            int estimated_fps = 100;
            frameCount = opt.minutes * 60 * estimated_fps;
            LOGV << "estimating to capture " << frameCount << " frames at " <<  estimated_fps << "fps " ;
        } else {
            frameCount = opt.minutes * 60 * opt.fps;
        }
    }
    lp.maxIntegerWidth = to_string(frameCount).length() + 1;



    // time stuff
    if (opt.fps > 0) {
        lp.duration_between_saves_ms = round(1000.0 / (float) opt.fps);
    }

    //vector<thread> s.threads;  // keep track of threads to close in shutdown
    //stuct
    sd.job = async(launch::async, []{ });

} 


void startLoopCallback( void ) {

    if (lp.num_of_dummy_pics > 0) {
        // skip advancing on this round
        lp.num_of_dummy_pics--;
        return;
    }

    // was gain or exposure externally changed?
    syncGainExpo();

    if (opt.vary_both) {
        lp.sweep_index = varyBoth();
    } else {
        if (opt.vary_gain)
            varyGain();
        if (opt.vary_expo)
            varyExposure();
    }
    return;
}

int endLoopCalback( void *ptr, int size, struct v4l2_buffer buf ) {
    LOGV << "end loop with size = " << size;
    if (lp.num_of_dummy_pics > 0) {
        // skip advancing on this round
        return(0);
    }

   // actually store the image
   Imagedata *image = new Imagedata;
   image->size = size;
   image->height = 544;
   image->width = image->size / ( image->height * image->bpp / CHAR_BIT);
   // TODO: do we really need memcpy?
   image->data = new char [image->size];
   memcpy(image->data,ptr,image->size);

   if (image->data == nullptr) {
       LOGE << "Unable to save frame, invalid image data" ;
       return(-1);
   }

   //string num = to_string(lp.num_frames++);
   //num.insert(num.begin(), lp.maxIntegerWidth - num.length(), '0');
   //string basename     = "frame" + num ;

   lp.fpsMeasurer.FrameReceived();

   lp.syst_time_this  = chrono::system_clock::now();
   lp.stdy_time_this =  chrono::steady_clock::now();
   // need another duration_cast<duration<double>> ?
   // or duration_cast<milliseconds>
   chrono::duration<double, std::milli> delta_time = lp.stdy_time_this - lp.stdy_time_last;
   float delta_ms = delta_time.count();
   long timestamp_last = lp.stdy_time_last.time_since_epoch().count();
   long timestamp_this = lp.stdy_time_this.time_since_epoch().count();
   //LOGV << " dt = " << timestamp_this << " - " << timestamp_last << " = " << delta_ms << "ms" ;
   long syst_timestamp_this = lp.syst_time_this.time_since_epoch().count();


   char buff[BUFSIZ];
   if (opt.vary_both or opt.vary_gain or opt.vary_expo) {
       snprintf(buff, sizeof(buff), "img%0*d_G%03dE%04d",lp.maxIntegerWidth, lp.num_frames++, opt.gain, opt.expo);
   } else {
       snprintf(buff, sizeof(buff), "img%0*d", lp.maxIntegerWidth, lp.num_frames++);
   }
   string basename = buff;

   // update rest image attributes
    image->src = "camera";
    image->dst = fp.folder_raw / (basename + ".raw");
    image->basename = basename;
    image->datestamp= g.datestamp;
    image->system_clock_ns=syst_timestamp_this;
    image->steady_clock_ns=timestamp_this;
    image->gain=opt.gain;
    image->expo=opt.expo;
    image->sweep_index=lp.sweep_index;
    image->sweep_total=lp.sweep_total;
    image->fps  = lp.fpsMeasurer.GetFps();
    image->delta_ms=delta_ms;
    image->comment=opt.comment;
    image->command=g.command;

    //TODO also save from
    // buf =   {index = 0, type = 1, bytesused = 835584, flags = 8193, field = 1, timestamp = {tv_sec = 255241, tv_usec = 840596}, timecode = {type = 0, flags = 0, frames = 0 '\000', seconds = 0 '\000', minutes = 0 '\000', hours = 0 '\000', userbits = "\000\000\000"}, sequence = 0, memory = 1, m = {offset = 0, userptr = 0, planes = 0x0, fd = 0}, length = 835584, reserved2 = 0, reserved = 0}


    swap( lp.stdy_time_last, lp.stdy_time_this ) ;

   // if display is set then show the image
   if ( opt.display ) {
       showImage(image);
   }

   // ok now to save raw/dng
   if (! opt.nosave) {
       saveRaw(image);
       saveDng(image);
   } 

   if (! opt.nojpg) {
       saveJpg(image);
    }


   // all done with processing 
   delete image;


   // ready to return...but first check if we should exist or wait instead

   // exit on minutes of wall clock time?
   // TODO: handle case when both opt.minutes and opt.frames is set
   // TODO: not really working because frame_count is ignored during main
   // loop
   lp.syst_time_this = chrono::system_clock::now();
   if (opt.minutes > 0) {
       chrono::duration<double, milli> elapsed_time = lp.syst_time_this - lp.syst_time_prog;
       int duration_min = round(chrono::duration_cast<chrono::minutes> 
               (elapsed_time).count());
       if (duration_min > opt.minutes) {
           // exit
           //frame_count = lp.num_frames;
       }
   }

   // ratelimit if opt.fps is set
   //
   //
   //
   if (opt.fps > 0) {
       int sleep_duration_ms = lp.duration_between_saves_ms - delta_ms;
       if (sleep_duration_ms > 0) {
           this_thread::sleep_for(chrono::milliseconds(sleep_duration_ms));
       }
   }

   return(0);

} // end of loop

void endLoop(void) {
    // cleanup
    LOGI << "Saved " << lp.num_frames << " frames with fps = " << lp.fpsMeasurer.GetFps() ;
    LOGI << "     raw dir: " << fp.folder_raw.string() ;
    LOGI << "session log : " << fp.file_log.string() << endl;
}


void saveDng(Imagedata* image) {

    fs::path file_dng = fp.folder_dng / (image->basename + ".dng");
	image->writeDng(file_dng);
}

void saveRaw(Imagedata* image) {

    fs::path file_raw = fp.folder_raw / (image->basename + ".raw");
	image->writeRaw(file_raw);
}

// different from others because it might be threaded out so needs a 
// private copy of imagedata
void saveJpg(Imagedata* image) {

    fs::path file_dng = fp.folder_dng / (image->basename + ".dng");
    fs::path file_jpg = fp.folder_jpg / (image->basename + ".jpg");

    image->createAnnoText();
    image->src = file_dng;
    image->dst = file_jpg;

    // race condition possible ... dng should be written before jpg
    // TODO: store in image if dng is written
    if (! fs::exists(file_dng)) {
        image->writeDng(file_dng);
    }

    // make a shallow copy for jpg and return...this could take a while
    Imagedata imgobj = *image;
    imgobj.data = nullptr; // TODO: constructor in class should handle this
    imgobj.writeJpg(file_jpg);

    // warning image object destroyed before writeJpg could complete...

}

void generateGainExposureTable() {

    for (int value_gain = opt.min_gain; value_gain < opt.max_gain; value_gain += opt.step_gain) {
        for (int value_expo = opt.min_expo; value_expo < opt.max_expo; value_expo += opt.step_expo) {
            g.gaex_table.push_back(cv::Point(value_gain, value_expo ));
            LOGV << " Vary table " << lp.sweep_total++ << ": (gain,expo) = " << cv::Point(value_gain, value_expo) ;
        }
    }

    LOGI << "Generated gain+exposure table ";
    LOGI << "    gain range =     " << cv::Point(opt.min_gain, opt.max_gain) << " in steps of " << opt.step_gain ;
    LOGI << "    exposure range = " << cv::Point(opt.min_expo, opt.max_expo) << " in steps of " << opt.step_expo ;
    LOGI << "    total values in sweep  =  " << g.gaex_table.size() ;
    lp.sweep_total = g.gaex_table.size();

    // record the number somewhere...

    if (false) {
        for (auto it : g.gaex_table) {
            LOGV << it << "\n";
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
    const string datestamp  = getDateStamp();

    cv::UMat mImage;
    uint32_t length = processedimage->length;
    processor.AllocateMat(processedImage, mImage);
    processor.DebayerImage(mImage,processedimage->pixelFormat);
    processor.DrawText(mImage,text);
    sv::DeallocateProcessedImage(processedImage);

    string filenameJpg = basename + ".jpg";
    vector<uchar> jpgbuf;
    Exiv2::ExifData exifData;

    // convert from 16 to 8 bit
    if (mimage->depth() != CV_8U) {
        constexpr auto CONVERSION_SCALE_16_TO_8 = 0.00390625;
        mimage->convertTo(mImage, CV_8U, CONVERSION_SCALE_16_TO_8);

    }
    cv::imencode(".jpg",mImage, jpgbuf);
    if (opt.noexif) {
        saveRaw(&jpgbuf[0], jpgbuf.size(), filenameJpg);
    } else {
        Exiv2::Image::AutoPtr eImage = Exiv2::ImageFactory::open(&jpgbuf[0],jpgbuf.size());
        exifData["Exif.image->ProcessingSoftware"] = "capture.cpp";
        exifData["Exif.image->SubfileType"] = 1;
        exifData["Exif.image->ImageDescription"] = "snappy carrots";
        exifData["Exif.image->Model"] = "tensorfield ag";
        exifData["Exif.image->AnalogBalance"] = opt.gain;
        exifData["Exif.image->ExposureTime"] = opt.expo;
        exifData["Exif.image->DateTimeOriginal"] = datestamp;
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


// https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-within-c-using-po
string exec(string cmd) {


    array<char, 128> buffer;
    string result;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    // remove blank lines from result
    // https://stackoverflow.com/questions/22395333/removing-empty-lines-from-a-string-c
    result.erase(unique(result.begin(), result.end(),
                  [] (char a, char b) { return a == '\n' && b == '\n'; }),
              result.end());
    return result;
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
        OptAllJpg,
        OptNoSave,
        OptNoJpg,
        OptNoExif,
        OptDisplay,
        OptInteractive,
        OptVerbose,
        OptComment
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
        {"alljpg",        no_argument,        0,    OptAllJpg },
        {"nosave",        no_argument,        0,    OptNoSave },
        {"nojpg",         no_argument,        0,    OptNoJpg },
        {"noexif",        no_argument,        0,    OptNoExif },
        {"display",       no_argument,        0,    OptDisplay },
        {"interactive",   no_argument,        0,    OptInteractive },
        {"verbose",       no_argument,        0,    OptVerbose },
        {"comment",       required_argument,  0,    OptComment },
        {NULL,            0,               NULL,    0}
    };

    // echo params out to record in run log
    // http://www.cplusplus.com/forum/beginner/211100/
    stringstream ss;
    copy( argv, argv+argc, ostream_iterator<const char*>( ss, " " ) ) ;
    g.command = ss.str();
    LOGI << g.command;

    while (true) {
        const auto options = getopt_long(argc, argv, short_opts, long_opts, nullptr);
        //printf("options ='%d' \n", options);

        if (options == -1)
            break;

        switch (options) {
        case OptMinutes:
            opt.minutes = stoi(optarg);
            LOGI << "Option: Minutes set to: " << opt.minutes ;
            break;
        case OptFrames:
            opt.frames = stoi(optarg);
            LOGI << "Option: Frames set to: " << opt.frames ;
            break;
        case OptGain:
            opt.gain = stoi(optarg);
            LOGI << "Option: Gain set to: " << opt.gain ;
            break;
        case OptExposure:
            opt.expo = stoi(optarg);
            LOGI << "Option: Exposure set to: " << opt.expo ;
            break;
        case OptFps:
            opt.fps = stoi(optarg);
            LOGI << "Option: fps set to: " << opt.fps ;
            break;
        case OptVaryGain:
            opt.vary_gain = true;
            LOGI << "Option: sweep gain from min to max" ;
            break;
        case OptMinGain:
            opt.min_gain = stoi(optarg);
            LOGI << "Option: Min gain set to: " << opt.min_gain ;
            break;
        case OptMaxGain:
            opt.max_gain = stoi(optarg);
            LOGI << "Option: Max gain set to: " << opt.max_gain ;
            break;
        case OptVaryExposure:
            opt.vary_expo = true;
            LOGI << "Option: sweep exposure from min to max" ;
            break;
        case OptMinExposure:
            opt.min_expo = stoi(optarg);
            LOGI << "Option: Min exposure set to: " << opt.min_expo ;
            break;
        case OptMaxExposure:
            opt.max_expo = stoi(optarg);
            LOGI << "Option: Max exposure set to: " << opt.max_expo ;
            break;
        case OptVaryBoth:
            opt.vary_both = true;
            LOGI << "Option: sweep both gain and exposure from min to max" ;
            break;
        case OptAllJpg:
            opt.alljpg = true;
            LOGI << "Option: save all jpeg files" ;
            break;
        case OptNoSave:
            opt.nosave = true;
            LOGI << "Option: no saving raw or jpeg files" ;
            break;
        case OptNoExif:
            opt.noexif = true;
            LOGI << "Option: no exif metadata in jpeg files" ;
            break;
        case OptNoJpg:
            opt.nojpg = true;
            LOGI << "Option: no jpeg files" ;
            break;
        case OptComment:
            opt.comment = string(optarg);
            LOGI << "Option: comment: " << opt.comment ;
            break;
        case OptDisplay:
            opt.display = true;
            LOGI << "Option: show live view" ;
            break;
        case OptInteractive:
            opt.interactive = true;
            LOGI << "Option: show live view and turn off saving" ;
            opt.display = true;
            opt.nosave = true;
            opt.nojpg = true;
            opt.frames = 100000;
            break;
        case OptVerbose:
            opt.verbose = true;
            LOGI << "Option: verbose mode" ;
            // turn on verbose reporting
            plog::get()->setMaxSeverity(plog::verbose);
            break;
        default:
            PrintHelp();
            break;
        }
    }

    LOGI << "Option processing complete";
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

void syncGainExpo(void) {
    int new_value;

    new_value = get_gain();
    if (new_value != opt.gain) {
        LOGI << " Gain changed from " << opt.gain << " to " << new_value ;
        opt.gain = new_value;
    }

    new_value = get_expo();
    if (new_value != opt.expo) {
        LOGI << " Exposure changed from " << opt.expo << " to " << new_value ;
        opt.expo = new_value;
    }



}

// modulo gain if asked value greater than max !
void setGain (int value) {
    LOGV << " setting gain to: " << value ;

    struct v4l2_query_ext_ctrl qctrl = query_gain();
    int minValue = qctrl.minimum;
    int maxValue = qctrl.maximum;
    int defValue = qctrl.default_value;

    if (opt.min_gain < 0) {
        opt.min_gain = minValue;
    }
    if (opt.max_gain < 0) {
        opt.max_gain = maxValue;
    }

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

    int old_value = get_gain();
    if (old_value != value) {
        set_gain(value);
        opt.gain = value;
        LOGV << " Gain changed from " << old_value << " to " << value ;
    }

    if (opt.verbose) {
        if (value < minValue) {
            LOGV << value << "  less than min value of Gain = " << minValue ;
        } else if (value > maxValue) {
            LOGV << value << "  greater than max value of Gain = " << maxValue ;
        }
    }


}

// modulo exposure if asked value greater than max !
void setExposure (int value) {
    LOGV << " setting expo to: " << value ;

    struct v4l2_query_ext_ctrl qctrl = query_expo();
    int minValue = qctrl.minimum;
    int maxValue = qctrl.maximum;
    int defValue = qctrl.default_value;

    // override to make it easier to track numbers...instead of min of 38
    minValue = 50;

    if (opt.min_expo < 0) {
        opt.min_expo = minValue;
    }
    if (opt.max_expo < 0) {
        opt.max_expo = maxValue;
    }

    if (opt.min_expo > 0) {
        minValue = opt.min_expo;
    }
    // and less than max possible?
    if (opt.max_expo > 0) {
        maxValue = opt.max_expo;
    }

    if (value < minValue) {
        value = minValue;
    } else if (value > maxValue) {
        value = value % maxValue;
    }

    int old_value = get_expo();
    if (old_value != value) {
        set_expo(value);
        opt.expo = value;
        LOGV << " Exposure changed from " << old_value << " to " << value ;
    }

    if (opt.verbose) {
        if (value < minValue) {
            LOGV << value << "  less than min value of Exposure = " << minValue ;
        } else if (value > maxValue) {
            LOGV << value << "  greater than max value of Exposure = " << maxValue ;
        }
    }

}

// without this there are a couple of frames with wrong setting
void takeDummyPicsToMakeSettingsStick() {
    lp.num_of_dummy_pics = 10;
}

void varyGain () {
    opt.gain += opt.step_gain;
    if (opt.gain > opt.max_gain) {
        opt.gain = opt.min_gain;
    }
    setGain(opt.gain);
    takeDummyPicsToMakeSettingsStick();

}

void varyExposure () {
    opt.expo += opt.step_expo;
    if (opt.expo > opt.max_expo) {
        opt.expo = opt.min_expo;
    }
    setExposure(opt.expo);
    takeDummyPicsToMakeSettingsStick();

}

// return index number of current iteration
int varyBoth (void) {

    // table empty? make.
    if (g.gaex_table.empty()) {
        generateGainExposureTable();
        g.gaex_table_ptr = g.gaex_table.begin();
    }
    cv::Point pt = *g.gaex_table_ptr;
    opt.gain = pt.x;
    opt.expo = pt.y;

    setGain(opt.gain);
    setExposure(opt.expo);
    takeDummyPicsToMakeSettingsStick();

    // circulate pointer
    if ( g.gaex_table_ptr == g.gaex_table.end() ) {
        g.gaex_table_ptr = g.gaex_table.begin();
    } else {
        g.gaex_table_ptr++;
    }

    return distance(g.gaex_table.begin(),g.gaex_table_ptr);
}


void cleanUp () {
    // clean up...
    LOGV << " final cleanup.." << endl << flush;;
    destroyAllWindows();
    MagickCoreTerminus();

    sd.job.wait();
    for (auto& th : sd.threads) {
        th.join();
    }
}

void callbackButton(int state, void *font) {
    LOGV << "state = " << state;
}

