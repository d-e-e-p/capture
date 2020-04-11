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

    // for gui
    int imagewidth  = 728;
    int imageheight = 544;
    bool toggle_f_windowsize = false;
    bool toggle_c_colorcorrect = true;

    Mat ccm;

} g;

struct FilePaths {

    //dng header
    fs::path headerfile   = "/home/deep/build/snappy/bin/dng_header.bin";
    fs::path dt_stylefile = "/home/deep/build/snappy/bin/darktable.xml";

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

// fps smoothing accumulator
namespace ba = boost::accumulators;
namespace bt = ba::tag;
typedef ba::accumulator_set < float, ba::stats <bt::rolling_mean > > MeanAccumulator;
MeanAccumulator fps_mean_accumulator(bt::rolling_window::window_size = 25);


// using in callback in loop
struct Loop {
    int num_of_dummy_pics = 0;
    int num_frames = 0; // index for frame count
    int frameSaved = 0;
    int maxIntegerWidth;
    int sweep_index = 0;
    int sweep_total = 0;
    int duration_between_saves_ms = 0;
    float fps = 0;

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
Imagedata::ClassInit Imagedata::readDngHeaderData(fp.headerfile, fp.dt_stylefile);



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
void buttonCallback(int state, void *font);
void setupCapture();
void stopCapture();


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
    setupCapture();

        frame_count = opt.frames;
        mainloop();

    stopCapture();
    return 0;
}

void setupCapture() { 
    setupLoop();
    open_device(); 
    init_device(); 
    setupCamera();
    start_capturing();
}

void stopCapture() { 
    stop_capturing(); 
    uninit_device(); 
    close_device();
    endLoop();
    cleanUp();
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

void tbCallbackGain(int value,void* userdata) {
    setGain(value);
} 

void tbCallbackExpo(int value,void* userdata) {
    setExposure(value);
} 

void buttonCallback(int state, void *font) {
    LOGV << "state = " << state;
}

// need to handle RGB->BGR which is used by opencv
void defineColorCorrectionMatrix() {

   /* final color matrix
     *   Blue:BGR
     *  Green:BGR;
     *    Red:BGR;
     */

/*
    <Element Row="2" Col="2">0.412400</Element>
    <Element Row="2" Col="1">0.056000</Element>
    <Element Row="2" Col="0">0.062000</Element>
    <Element Row="1" Col="2">0.274300</Element>
    <Element Row="1" Col="1">0.700600</Element>
    <Element Row="1" Col="0">0.075800</Element>
    <Element Row="0" Col="2">0.018000</Element>
    <Element Row="0" Col="1">-0.583300</Element>
    <Element Row="0" Col="0">1.133600</Element>
*/

   // generated using xrite matching
   float rgb[3][3] = {
        { 1.444000, -0.263100, -0.156700},
        {-0.413900,  1.277400, -0.090500},
        {-0.247300,  0.262400,  0.711200},
    };


    float bgr[3][3];
    bgr[0][0] = rgb[2][2];
    bgr[0][1] = rgb[2][1];
    bgr[0][2] = rgb[2][0];
    bgr[1][0] = rgb[1][3];
    bgr[1][1] = rgb[1][1];
    bgr[1][2] = rgb[1][0];
    bgr[2][0] = rgb[0][2];
    bgr[2][1] = rgb[0][1];
    bgr[2][2] = rgb[0][0];

    g.ccm = Mat(3, 3, CV_32FC1, bgr).t();

    LOGV << "RGB color correction matrix g.ccm:\n" << g.ccm;

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
        defineColorCorrectionMatrix();
        namedWindow(g.mainWindowName, WINDOW_NORMAL);
        resizeWindow(g.mainWindowName, g.imagewidth, g.imageheight);
        createButton("save image",buttonCallback,NULL,QT_PUSH_BUTTON,1);
        //int cvCreateTrackbar(const char* trackbar_name, const char* window_name, int* value, int count, CvTrackbarCallback on_change=NULL )¶
        createTrackbar( "gain", g.mainWindowName, &opt.gain, 480,   tbCallbackGain);
        createTrackbar( "expo", g.mainWindowName, &opt.expo, 8256,  tbCallbackExpo);
    }

    return;

}


void performHotkeyActions(Imagedata* image, int key) {

    switch (key) {
        case 'f' : 
            g.toggle_f_windowsize = ! g.toggle_f_windowsize;
            LOGI << "resize window: " << g.toggle_f_windowsize;
            if (g.toggle_f_windowsize) {
                resizeWindow(g.mainWindowName, 2 * g.imagewidth, 2 * g.imageheight);
            } else {
                resizeWindow(g.mainWindowName, 1 * g.imagewidth, 1 * g.imageheight);
            }
            break;
        case 'c' : 
            g.toggle_c_colorcorrect = ! g.toggle_c_colorcorrect;
            LOGI << " color correct: " << g.toggle_c_colorcorrect;
            break;
        case 's' : 
            LOGI << "saving " << image->basename;
            saveRaw(image) ;
            saveDng(image) ;
            saveJpg(image) ;
            break;
        case 'q' : 
            LOGI << "quitting.";
            stopCapture();
            exit(0);
            break; // never get here
        default :
            LOGI << "unknown key " << key;
    }

}



// from https://stackoverflow.com/questions/10167534/how-to-find-out-what-type-of-a-mat-object-is-with-mattype-in-opencv
// CV_8U  - 8-bit unsigned integers ( 0..255 )
// CV_8S  - 8-bit signed integers ( -128..127 )
// CV_16U - 16-bit unsigned integers ( 0..65535 )
// CV_16S - 16-bit signed integers ( -32768..32767 )
// CV_32S - 32-bit signed integers ( -2147483648..2147483647 )
// CV_32F - 32-bit floating-point numbers ( -FLT_MAX..FLT_MAX, INF, NAN )
// CV_64F - 64-bit floating-point numbers ( -DBL_MAX..DBL_MAX, INF, NAN )

string getMatType(Mat M) {
  int type =  M.type();
  string r;

  uchar depth = type & CV_MAT_DEPTH_MASK;
  uchar chans = 1 + (type >> CV_CN_SHIFT);

  switch ( depth ) {
    case CV_8U:  r = "8U"; break;
    case CV_8S:  r = "8S"; break;
    case CV_16U: r = "16U"; break;
    case CV_16S: r = "16S"; break;
    case CV_32S: r = "32S"; break;
    case CV_32F: r = "32F"; break;
    case CV_64F: r = "64F"; break;
    default:     r = "User"; break;
  }

  r += "C";
  r += (chans+'0');

  return r;
}

void printMatStats(string name, Mat M) {
    double min, max;
    minMaxLoc(M, &min, &max);

    int max_length = 30;
    string spacer = "";
    if (name.length() < max_length) {
        spacer = string(max_length - name.length(), ' ');
    }

    LOGV << " Mat=" << name << spacer << " " << getMatType(M) << " : " << M.size() << " x " << M.channels() <<  " (min,max) = "  << Point(min,max) ;
    string file = "images/" + name + ".jpg";
    imwrite(file, M);
}



// use ccm to "try" and correct colors
Mat correctImageColors (Imagedata* image, Mat mat_in_int) {

    printMatStats("mat_in_int",     mat_in_int);

    // multiplication needs floating
    Mat mat_in_float = Mat(image->height, image->width, CV_32F);
    mat_in_int.convertTo(mat_in_float, CV_32F, 1.0 / 255);
    printMatStats("mat_in_float",   mat_in_float);
    
    // ok linearize it so we can multiply with CCM matrix
    Mat mat_float_linear = mat_in_float.reshape(1, image->height*image->width);
    printMatStats("mat_float_linear",  mat_float_linear);

    // multiply with CCM
    Mat mat_multiplied_linear = mat_float_linear * g.ccm;
    printMatStats("mat_multiplied_linear",  mat_multiplied_linear);

    // get back to original shape
    Mat mat_multipled = mat_multiplied_linear.reshape(3, image->height);
    normalize(mat_multipled, mat_multipled, 255, 0, NORM_MINMAX);
    printMatStats("mat_multipled",  mat_multipled);

    // convert back to int
    Mat mat_multipled_int = Mat(image->height, image->width, CV_8U);
    mat_multipled.convertTo(mat_multipled_int, CV_8U, 1);
    printMatStats("mat_multipled_int",  mat_multipled_int);

    /*
    //white balance
    Ptr<xphoto::WhiteBalancer> wb;
    wb = xphoto::createGrayworldWB();

    Mat mat_wb_int;
    wb->balanceWhite(mat_multipled_int, mat_wb_int);
    printMatStats("mat_wb_int",   mat_wb_int);

    Mat mat_wb_norm;
    normalize(mat_wb_int, mat_wb_norm, 255, 0.0, NORM_MINMAX);
    printMatStats("mat_wb_norm",   mat_wb_norm);
    */

    //white balance
    Ptr<xphoto::WhiteBalancer> wb;
    wb = xphoto::createSimpleWB();

    Mat mat_wb;
    wb->balanceWhite(mat_multipled, mat_wb);
    printMatStats("mat_wb", mat_wb);

    Mat mat_wb_norm;
    normalize(mat_wb, mat_wb_norm, 1.0, 0.0, NORM_MINMAX);
    printMatStats("mat_wb_norm",   mat_wb_norm);

    return mat_wb_norm;
}

void showImage(Imagedata* image) {
    // step 1: import blob into opencv
    // step is number of bytes each matrix row occupies including padding
    int new_width = 728; // width goes from 768 to 728
    int step = image->width * image->pixel_depth;
    Mat mat_crop(image->height, new_width, CV_16U, image->data, step);
    image->width = new_width;
    printMatStats("mat_crop", mat_crop);

    Mat mat_bayer1(image->height, image->width, CV_8U);
    demosaicing(mat_crop, mat_bayer1, COLOR_BayerRG2RGB);

    printMatStats("mat_bayer1", mat_bayer1);
    Mat mat_todisplay;
    if (g.toggle_c_colorcorrect) {
        mat_todisplay = correctImageColors(image, mat_bayer1);
    } else {
        mat_todisplay = mat_bayer1;
    }

    imshow(g.mainWindowName,mat_todisplay);

    //annotations
	image->createAnnoText();
    string text_help = "    hotkeys: (s) save (f) resize (c) color (w) whitebalance   (q) quit ";
    string text_status = image->text_east + text_help;
    displayOverlay(g.mainWindowName, image->text_north, 0);
    displayStatusBar(g.mainWindowName, text_status, 0);

    //hotkeys
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

int endLoopCalback( void *ptr, int datalength, struct v4l2_buffer buf ) {
    if (lp.num_of_dummy_pics > 0) {
        // skip advancing on this round
        return(0);
    }

   // actually store the image
   Imagedata *image = new Imagedata;
   image->datalength = datalength;
   image->height = 544;
   image->width = image->datalength / ( image->height * image->pixel_depth);
   // TODO: do we really need memcpy?
   image->data = new char [image->datalength];
   memcpy(image->data,ptr,image->datalength);

   if (image->data == nullptr) {
       LOGE << "Unable to save frame, invalid image data" ;
       return(-1);
   }

   //string num = to_string(lp.num_frames++);
   //num.insert(num.begin(), lp.maxIntegerWidth - num.length(), '0');
   //string basename     = "frame" + num ;

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
   fps_mean_accumulator(1000/delta_ms);
   lp.fps = boost::accumulators::rolling_mean(fps_mean_accumulator);


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
    image->fps=lp.fps;
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
    LOGI << "Saved " << lp.num_frames << " frames at " << fixed << setprecision(0) << lp.fps << " fps" ;
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

