/*
 * postprocess raw files to extract position info
 *
 * (c) 2020 tensorfield ag
 *
 *
 *
 */

#include "tracker.hpp"

// stitcher only considers 2 images at a time and finds the offset
// features in last image considered are momoized
Ptr<Stitcher> stitcher;
vector<Mat> image(2);
vector<ImageFeatures> features(2);

struct Options {
    // determined experimentally by tuning
    bool  generate_annotated_images = true;
    float conversion_factor = 2050;
    bool  direction_pagedown = true;

    //// for short run correlation
    //float starting_position = 4.930;
    //int   start_image = 1425;
    //int   end_image   = 2608;

    // for long run correlation
    //float starting_position = 0.0;      // in m
    //int   start_image = 4509;
    //int   end_image   = 5761;

    //// for short run video
    float starting_position = 4.932;
    int   start_image = 1400;
    int   end_image   = 2700;

    // for long run video
    //float starting_position = -0.327;      // in m
    //int   start_image = 4440;
    //int   end_image   = 9300;


} opt;

struct Loc {
    float x = 0;
    float y = 0;
    float vx = 0;
    float vy = 0;
    bool  moving = false;
} delta, pos;


/*
 * experiment on 202003201636 data
 *
 *                           opt.conversion_factor
 *                       2060    2050
 *               real    estimates ->
 *   img0001425  4.930   4.930   4.930  
 *   img0002608  0.000  -0.130  -0.063
 *   delta       4.930     3%     1%
 *   error               
 *
 *
 *                           opt.conversion_factor
 *                        2300  2060    2050
 *               real    estimates ->
 *   img0004509  0.000   0.000  0.000   0.000
 *   img0005761  5.170   4.633  5.173   5.198
 *   error                10%    0.1%    0.5%
 *
 */


int setup_stitcher() {

    double work_scale = 1, seam_scale = 1, compose_scale = 1;
    bool is_work_scale_set = false, is_seam_scale_set = false, is_compose_scale_set = false;

    LOGLN("setting up stitching pipeline \n");
    LOGLN("     opt.conversion_factor = "   << opt.conversion_factor);
    LOGLN("     opt.direction_pagedown = "  << opt.direction_pagedown);
    LOGLN("     opt.starting_position = "   << opt.starting_position);
    LOGLN("     opt.start_image = "         << opt.start_image);
    LOGLN("     opt.end_image = "           << opt.end_image);

    pos.y = opt.starting_position;  // set starting position

    stitcher = Stitcher::create(Stitcher::SCANS);

    Ptr<Feature2D> finder;
    finder = ORB::create();
    //finder = xfeatures2d::SURF::create();
    //finder = xfeatures2d::SIFT::create();
    stitcher->setFeaturesFinder(finder);

    Ptr<FeaturesMatcher> matcher;
    bool full_affine = false; bool try_use_gpu = true; float match_conf = 0.3f; int num_matches_thresh1 = 6;
    matcher = makePtr<detail::AffineBestOf2NearestMatcher>( full_affine, try_use_gpu, match_conf, num_matches_thresh1);
    // TODO: free the last matcher
    stitcher->setFeaturesMatcher(matcher);

    float conf_thresh = 0.1f; // threshold for two images are adjacent
    Ptr<detail::BundleAdjusterBase> adjuster = stitcher->bundleAdjuster();
    adjuster->setConfThresh(conf_thresh);

    return 0;

}


std::vector<fs::path> get_filenames( fs::path path ) {

    vector<fs::path> filenames;

    // http://en.cppreference.com/w/cpp/experimental/fs/directory_iterator
    const fs::directory_iterator end {};

    for( fs::directory_iterator iter {path}; iter != end; ++iter ) {
        // http://en.cppreference.com/w/cpp/experimental/fs/is_regular_file
        if( fs::is_regular_file(*iter) ) // comment out if all names (names of directories tc.) are required
            filenames.push_back( iter->path() );
    }

    sort(filenames.begin(), filenames.end());
    return filenames;
}



string compute_delta_displacement(fs::path lastfile, fs::path nextfile) {

    Ptr<Feature2D> finder = stitcher->featuresFinder();

    // assume old image is cached
    if (image[0].empty()) {
        image[0] = imread(lastfile.string());
        computeImageFeatures(finder, image[0], features[0]);
    }
    image[1] = imread(nextfile.string());
    computeImageFeatures(finder, image[1], features[1]);

    string basename = lastfile.stem().string();
    if (features[1].keypoints.size() < 100) {
        LOGLN(basename << " : low features in image = "  << features[1].keypoints.size());
    }
    // Draw SIFT keypoints
    //cv::drawKeypoints(img, keypoints_sift, output, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    //cv::imshow("Output", output);

    Ptr<FeaturesMatcher> matcher = stitcher->featuresMatcher();
    vector<MatchesInfo> pairwise_matches;
    (*matcher)(features, pairwise_matches);
    matcher->collectGarbage();
    if (pairwise_matches[1].confidence < 1.0) {
        LOGLN(basename << " : low matching confidence = " << pairwise_matches[1].confidence);
    }

    vector<CameraParams> cameras;
    Ptr<Estimator> estimator = stitcher->estimator();
    if (!(*estimator)(features, pairwise_matches, cameras)) {
        cerr << "Homography estimation failed.\n";
        return "ERROR";
    }

    for (size_t i = 0; i < cameras.size(); ++i) {
        Mat R;
        cameras[i].R.convertTo(R, CV_32F);
        cameras[i].R = R;
        //LOGLN("Initial camera intrinsics #" << indices[i]+1 << ":\nK:\n" << cameras[i].K() << "\nR:\n" << cameras[i].R);
    }

    Ptr<detail::BundleAdjusterBase> adjuster = stitcher->bundleAdjuster();
    if (!(*adjuster)(features, pairwise_matches, cameras)) {
        cerr << "Camera parameters adjusting failed.\n";
        return "ERROR";
    }

    //LOGLN("camera1 R: \n" << cameras[1].R << "\n");
    float x =  cameras[1].R.at<float>(0,2) / opt.conversion_factor;
    float y =  cameras[1].R.at<float>(1,2) / opt.conversion_factor;

    pos.x += x;
    pos.y += opt.direction_pagedown ? y : -y ;

    // if no change then set state to stopped
    float epsilon = 0.00005 ; // 0.00005m = 0.05mm
    pos.moving = ( (x < epsilon) and (y < epsilon) ) ? false : true ;
    char moving_ch = pos.moving ? 'M' : 'S';

    //features[0] = features[1].clone();
    swap(image[0],image[1]);
    swap(features[0],features[1]);

    // come on c++ there has to be a better way to use sprintf (and not setfill/setw)
    string basename_cstr = basename.c_str();
    char buf[BUFSIZ];
    sprintf (buf, "     %s (%07.3f,%07.3f)m %c", &basename_cstr[0], pos.x, pos.y, moving_ch);
    string bufstr(buf);
    LOGLN(bufstr);

    return bufstr;

}

int annotate_image_api(fs::path from, fs::path to, string text) {


    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    /*
    TODO: auto convert string to this array form with NULL termination...
    char *args[] = { "magick", "-size", "100x100", "xc:red",
                     "(", "rose:", "-rotate", "-90", ")",
                     "+append", "show:", NULL };
    */

    //char cmd[] = "magick in.jpg -clahe 25x25%+128+2 -pointsize 15 -font Inconsolata -fill yellow -gravity East -draw \' translate 20,230 rotate 90 text 0,0 \"202003201656 img0002112 / 25577 gain:100 exp:1500\nfinal run with lowered camera \"  \'   out.jpg";
    string drawcmd = "translate 15,220 rotate 90 text 0,0 \'" + text + "\'";

    // needs (char *) somewhere?
    char *args[] = {
        "magick",
        strdup(from.c_str()),
        "-clahe",
        "25x25%+128+2",
        "-pointsize",
        "30",
        "-font",
        "Inconsolata",
        "-fill",
        "yellow",
        "-gravity",
        "East",
        "-draw",
        strdup(drawcmd.c_str()),
        strdup(to.c_str()),
        NULL
    };

    int arg_count;
    for(arg_count = 0; args[arg_count] != (char *) NULL; arg_count++);

    //copy( args, args+arg_count, ostream_iterator<const char*>( cout, " " ) ) ;
    //cout << "\n";

    (void) MagickImageCommand(image_info, arg_count, args, NULL, exception);

    if (exception->severity != UndefinedException) {
      CatchException(exception);
        cerr << "Magick: Major Error Detected\n";
        cerr <<  (exception)->reason << "\n";
        cerr <<  (exception)->description << "\n";
    }

    image_info=DestroyImageInfo(image_info);
    exception=DestroyExceptionInfo(exception);

}


int annotate_image_external(fs::path from, fs::path to, string text) {


    string cmd = "magick " + from.string() + " -clahe 25x25%+128+2 -pointsize 30 -font Inconsolata -fill yellow -gravity East -draw \'translate 15,220 rotate 90 text 0,0 \" " + text + " \" \'   " + to.string();

    //string cmd = "magick -pointsize 30 -fill yellow -font Inconsolata -draw \'text 10,50 \" " + text + " \" \'   " + from.string() + " " + to.string();
    LOGLN("running cmd: " << cmd);
    system( (const char *) cmd.c_str());

}

void save_image_position(fs::path jpgname, string bufstr) {

    fs::path trkname = jpgname;

    // if source directory is called "jpg", then replace with "jpg_tracking"
    if (trkname.parent_path().stem() == "jpg") {
        string newpath = trkname.parent_path().parent_path().string() + "/jpg_tracking/";
        fs::create_directories(newpath);
        trkname = newpath + trkname.stem().string() + ".jpg";
    }

    //LOGLN("name = " << jpgname << " -> " << trkname);
    //annotate_image_external(jpgname,trkname, bufstr);
    annotate_image_api(jpgname,trkname, bufstr);
}


int main(int argc, char** argv ) {


    if ( argc != 2 ) {
        LOGLN("usage: postprocess <dir_with_raw_image_files>");
        return -1;
    }
    fs::path dir = argv[1];

    setup_stitcher();

    // init for loop
    fs::path lastfile = "";
    vector<thread> threads;
    string bufstr;
    regex pattern ("img(\\d+)");
    smatch sm;
    clock_t start_time = clock();
    int num_images = 0;
    MagickCoreGenesis(argv[0],MagickFalse);

    for( const auto& nextfile : get_filenames( dir ) ) {
        if (nextfile.extension() != ".jpg") {
            lastfile = nextfile;
            continue;
        }

        if (lastfile == "") {
            lastfile = nextfile;
            continue;
        }

        // skip files based in image number sequence
        // expect img0008946.jpg or img0008946_...
        string basename = lastfile.stem().string();
        regex_match (basename,sm,pattern);
        if (sm.size() < 1) {
            cerr << "file not matched! " << basename <<  " sm.size() = " << sm.size() << "\n";
            return -1;
        }
        int index = stoi(sm[1]);
        if (index < opt.start_image or index > opt.end_image ) {
            //LOGLN("skipping image not in range " << basename );
            lastfile = nextfile;
            continue;
        }

        // finally ready to do the actual work
        //LOGLN("processing " << lastfile << " vs " << nextfile);
        bufstr = compute_delta_displacement(lastfile, nextfile);
    
        if (opt.generate_annotated_images and pos.moving) {
            threads.push_back(thread(save_image_position, lastfile, bufstr));
            //save_image_position(lastfile, bufstr);
        }

        lastfile = nextfile;
        num_images++;
    }

    float duration = ( clock() - start_time ) / (float) CLOCKS_PER_SEC;
    float fps = num_images / duration;
    LOGLN(argv[0] << " : processed " << num_images << " images in " << (int) duration << "s  fps=" << (int) fps);

    // shutdown
    for (auto& th : threads) {
        th.join();
    }
    MagickCoreTerminus();

}

