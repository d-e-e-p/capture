/*
 * postprocess raw files to extract position info
 *
 * (c) 2020 tensorfield ag
 *
 *
 *
 */

#include "tracker.hpp"

#include <iostream>
#include <assert.h>
#include <stdio.h>

#include <vector>
#include <string>
#include <experimental/filesystem> // http://en.cppreference.com/w/cpp/experimental/fs
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui_c.h>
#include <sys/queue.h>

// for threading
#include <future>
#include <thread>
#include <chrono>


using namespace cv;
using namespace std;
namespace fs = std::experimental::filesystem;

Ptr<Stitcher> stitcher;



int setup_stitcher() {

    double work_scale = 1, seam_scale = 1, compose_scale = 1;
    bool is_work_scale_set = false, is_seam_scale_set = false, is_compose_scale_set = false;

    LOGLN("setting up stitching pipeline...");
	stitcher = Stitcher::create(Stitcher::SCANS);

    Ptr<Feature2D> finder;
    finder = ORB::create();
    //finder = xfeatures2d::SURF::create();
    //finder = xfeatures2d::SIFT::create();
	stitcher->setFeaturesFinder(finder);

    Ptr<FeaturesMatcher> matcher;
	bool full_affine = false; bool try_use_gpu = true; float match_conf = 0.3f; int num_matches_thresh1 = 6 ;
     matcher = makePtr<detail::AffineBestOf2NearestMatcher>( full_affine , try_use_gpu , match_conf , num_matches_thresh1);
	// TODO: free the last matcher
	stitcher->setFeaturesMatcher(matcher);

	float conf_thresh = 0.1f; // threshold for two images are adjacent
	Ptr<detail::BundleAdjusterBase> adjuster = stitcher->bundleAdjuster();
    adjuster->setConfThresh(conf_thresh);

	return 0;

}

 
std::vector<fs::path> get_filenames( fs::path path ) {

    vector<fs::path> filenames ;
    
    // http://en.cppreference.com/w/cpp/experimental/fs/directory_iterator
    const fs::directory_iterator end{} ;
    
    for( fs::directory_iterator iter{path} ; iter != end ; ++iter ) {
        // http://en.cppreference.com/w/cpp/experimental/fs/is_regular_file 
        if( fs::is_regular_file(*iter) ) // comment out if all names (names of directories tc.) are required
            filenames.push_back( iter->path() ) ;
    }

	sort(filenames.begin(), filenames.end());
    return filenames ;
}

vector<Mat> image(2);
vector<ImageFeatures> features(2);


pair<float,float> process_two_files(fs::path lastfile, fs::path nextfile) {

	Ptr<Feature2D> finder = stitcher->featuresFinder();

	// assume old image is cached
	if (image[0].empty()) {
		image[0] = imread(lastfile.string());
    	computeImageFeatures(finder, image[0], features[0]);
	}
	image[1] = imread(nextfile.string());
    computeImageFeatures(finder, image[1], features[1]);

	string basename = lastfile.stem().string();
	LOG("Features in image " << basename << " = " << features[1].keypoints.size());

	Ptr<FeaturesMatcher> matcher = stitcher->featuresMatcher();
	vector<MatchesInfo> pairwise_matches;
   (*matcher)(features, pairwise_matches);
    matcher->collectGarbage();
	LOGLN(" matching confidence = " << pairwise_matches[1].confidence);

    vector<CameraParams> cameras;
	Ptr<Estimator> estimator = stitcher->estimator();
    if (!(*estimator)(features, pairwise_matches, cameras)) {
        cout << "Homography estimation failed.\n";
        return make_pair(0, 0);
    }

    for (size_t i = 0; i < cameras.size(); ++i) {
        Mat R;
        cameras[i].R.convertTo(R, CV_32F);
        cameras[i].R = R;
        //LOGLN("Initial camera intrinsics #" << indices[i]+1 << ":\nK:\n" << cameras[i].K() << "\nR:\n" << cameras[i].R);
    }

	Ptr<detail::BundleAdjusterBase> adjuster = stitcher->bundleAdjuster();
    if (!(*adjuster)(features, pairwise_matches, cameras)) {
        cout << "Camera parameters adjusting failed.\n";
        return make_pair(0, 0);
    }

	//LOGLN("camera1 R: \n" << cameras[1].R << "\n");

	float convertion_to_m = 0.0006;
    float x = - cameras[1].R.at<float>(0,2) * convertion_to_m;
    float y = - cameras[1].R.at<float>(1,2) * convertion_to_m;

	pair<float,float> delta = make_pair(x,y);


	//features[0] = features[1].clone();
	swap(image[0],image[1]);
	swap(features[0],features[1]);


    // Draw SIFT keypoints
    //cv::drawKeypoints(img, keypoints_sift, output, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
    //cv::imshow("Output", output);
    return delta;

}

int annotate_image(fs::path from, fs::path to, string text) {

    string programName = "convert -pointsize 30 -fill yellow -font Inconsolata -draw \'text 10,50 \" " + text + " \" \'   " + from.string() + " " + to.string();
    //LOGLN("running cmd: " << programName);
    system( (const char *) programName.c_str());

}

void save_image_position(fs::path jpgname, string bufstr) {

        fs::path trkname = jpgname;

        // if raw directory is called "raw", then replace with "dng"
        if (trkname.parent_path().stem() == "jpg") {
            string newpath = trkname.parent_path().parent_path().string() + "/trk/";
            fs::create_directories(newpath);
            trkname = newpath + trkname.stem().string() + ".jpg";
        }

        LOGLN("name = " << jpgname << " -> " << trkname);
        annotate_image(jpgname,trkname, bufstr);
}


int main(int argc, char** argv ) {

  if ( argc != 2 ) {
    LOGLN("usage: postprocess <dir_with_raw_image_files>");
    return -1;
  }
  fs::path dir = argv[1];

  setup_stitcher();

  // setup for loop
  fs::path lastfile = "";
  pair<float,float> pos (0,0);
  char buf[BUFSIZ];
  vector<thread> threads;

  for( const auto& nextfile : get_filenames( dir ) ) {
    if (nextfile.extension() != ".jpg") {
        continue;
    }

    if (lastfile == "") {
        lastfile = nextfile;
        continue;
    }
    LOGLN("processing " << lastfile << " + " << nextfile);
    pair<float,float> delta = process_two_files(lastfile, nextfile);
	pos.first  += delta.first;
	pos.second += delta.second;
	string basename = lastfile.stem().string().c_str();
	sprintf (buf, "%s (%07.3f,%07.3f)m", &basename[0], pos.first, pos.second);
	string bufstr(buf);
	LOGLN(bufstr);

	threads.push_back(thread(save_image_position, lastfile, bufstr));

    lastfile = nextfile;
  }

  for (auto& th : threads) {
	th.join();
  }


}

