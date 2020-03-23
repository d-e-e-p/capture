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
    float conversion_factor = 5250;

    //// for short run
    //bool  direction_pagedown = false;
    //float starting_position = 2.040;      // in m
    //int   start_image = 1400;
    //int   end_image   = 2680;

    // for long run
    bool  direction_pagedown = true;
    float starting_position = -0.128;      // in m
    int   start_image = 4440;
    int   end_image   = 9300;

} opt;

struct Loc {
    float x = 0;
    float y = 0;
} delta, pos;



/*
 * experiment on 202003201636 data
 *
 *                           opt.conversion_factor
 *                       5800    5500    5250    5200
 *               real    estimates ->
 *   img0001421  2.040   2.040   2.040   2.040   2.040
 *   img0002657  0.000   0.187   0.086  -0.007  -0.027
 *   delta       2.040   1.853   1.954   2.047   2.067
 *   error                90.8%   95.8%  100.3%  101.3%
 *
 *                           opt.conversion_factor
 *                       5800    5500    5250    5200
 *   img0004457  0.000           0.098   0.006  -0.014
 *   img0009217  2.160           2.147   2.152   2.153
 *   delta       2.160           2.049   2.146   2.167
 *   error                        94.9%   99.3%  100.3%
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


    //features[0] = features[1].clone();
    swap(image[0],image[1]);
    swap(features[0],features[1]);

    // come on c++ there has to be a better way to use sprintf 
    string basename_cstr = basename.c_str();
    char buf[BUFSIZ];
    sprintf (buf, "     %s (%07.3f,%07.3f)m", &basename_cstr[0], pos.x, pos.y);
    string bufstr(buf);
    LOGLN(bufstr);

    return bufstr;

}

int annotate_image(fs::path from, fs::path to, string text) {


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
    annotate_image(jpgname,trkname, bufstr);
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
    
        if (opt.generate_annotated_images) {
            //threads.push_back(thread(save_image_position, lastfile, bufstr));
            save_image_position(lastfile, bufstr);
        }

        lastfile = nextfile;
    }

    // shutdown
    for (auto& th : threads) {
        th.join();
    }


}

