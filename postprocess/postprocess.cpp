/*
 * postprocess raw/jpg files
 *
 * (c) 2020 tensorfield ag
 *
 * convert dir of raw images to processed jpg
 *
 */


#include "postprocess.hpp"
#include "Imagedata.hpp"

//#include "202003281227_attr.hpp"
map<string,string> attr = {};

// global vars -- but in a struct so that makes it ok :-)
struct Options {
    string convert = "raw2dng";
    bool overwrite = false;
    bool verbose = false;
    bool sample = false;
    int  threads = 4;
} opt;

struct Globals {
    // file too small to be valid so ok to overwrite
    int smallsize = 1000;
    // options for source and destination extensions
    string src_dir_ext;
    string dst_dir_ext;
    string src_file_ext;
    string dst_file_ext;
    //
    string dateStamp;
} g;

struct FilePaths {

    vector <fs::path> folder_src_list ;

    //dng header
    fs::path headerfile   = "/home/user/build/tensorfield/snappy/bin/dng_header.bin";
    fs::path dt_stylefile = "/home/user/build/tensorfield/snappy/bin/darktable.xml";

} fp;

// init Imagedata class
Imagedata::ClassInit Imagedata::readDngHeaderData(fp.headerfile, fp.dt_stylefile);


void processArgs(int argc, char** argv) {

   const char* const short_opts = "-:";

   static struct option long_opts[] = {
        {"convert",     required_argument,  0, 42 },
        {"overwrite",   no_argument,        0, 43 },
        {"verbose",     no_argument,        0, 44 },
        {"sample",      no_argument,        0, 45 },
        {NULL, 0, NULL, 0}
    };

    // echo params out to record in run log
    // http://www.cplusplus.com/forum/beginner/211100/
    stringstream ss;
    copy( argv, argv+argc, ostream_iterator<const char*>( ss, " " ) ) ;
    LOGI << ss.str() ;

    while (true) {
        int option_index = 0;
        int options = getopt_long_only(argc, argv, short_opts, long_opts, &option_index);

        //printf("optind=%i optopt=%c c=%d optarg=%s arg=%s\n", optind, optopt, options, optarg, argv[optind-1]);
        if (options == -1)
            break;

        switch (options) {
         case 1:
            fp.folder_src_list.push_back (optarg);
            LOGI << " add to fp.folder_src_list = " << optarg;
            break;
         case 42:
            opt.convert = optarg;
            LOGI << " opt.convert = " << opt.convert;
            break;
         case 43:
            opt.overwrite = true;
            LOGI << " opt.overwrite = " << opt.overwrite;
            break;
         case 44:
            opt.verbose = true;
            plog::get()->setMaxSeverity(plog::verbose);
            LOGI << " opt.verbose = " << opt.verbose;
            break;
         case 45:
            opt.sample = true;
            LOGI << " opt.sample = " << opt.sample;
            break;
         case ':':
            LOGE << " missing option arg" << argv[optind-1];
            exit(-1);
         case '?':
            LOGE << " invalid option " << argv[optind-1];
            exit(-1);
         default:
            LOGE << "can't understand option: " << argv[optind-1] ;
            exit(-1);
        }
    }

    // populate global extries
    // jpg2trk convert option split to produce .jpg and .trk
    regex pattern ("([^2]+)2([^2]+)");
    smatch sm;
    regex_match (opt.convert,sm,pattern);
     if (sm.size() < 2) {
         LOGE << "opt.convert patt not matched! " << opt.convert <<  " sm.size() = " << sm.size() ;
            exit(-1);
     }
    g.src_dir_ext = sm[1];
    g.dst_dir_ext = sm[2];

    g.src_file_ext = g.src_dir_ext;
    g.dst_file_ext = g.dst_dir_ext;

    // file type should match convert option except when:
    if (g.dst_dir_ext == "ann") { g.dst_file_ext = "png" ;}
    if (g.dst_dir_ext == "trk") { g.dst_file_ext = "jpg" ;}
    if (g.dst_dir_ext == "ahe") { g.dst_file_ext = "png" ;}

    LOGI << "convert : " << g.src_dir_ext << "/file." <<  g.src_dir_ext << " -> " << g.dst_dir_ext << "/file." << g.dst_file_ext;

    LOGV << "Option processing complete";
}

// Pixel Format  : 'RG10' (10-bit Bayer RGRG/GBGB)
// see https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-srggb10.html



int ColorPlaneInterpolation(fs::path rawname, fs::path pgmname) {
    FILE *fp = NULL;
    char *imagedata = NULL;

    // determine size of raw file assumed to be 16 bit raw 
    int filesize = fs::file_size(rawname);

   // Read in the raw image

    //Image dimension.
    //  width is 768 stride = 728 width + 40 margin
  	//  but some setting sometimes seems to omit stride...so safer to compute
    int height = 544;
    int width =  filesize / (2 * height); // 2 bytes per pixel
    //int width =  728; // 768 stride = 728 width + 40 margin
    int framesize = width * height; 
	fp = fopen(rawname.c_str(), "rb");
	cout << "reading in " << rawname ;

	//Memory allocation for bayer image data buffer.
    //Read image data and store in buffer.
    imagedata = (char*) malloc (sizeof(char) * filesize);
    fread(imagedata, sizeof(char), filesize, fp);
    fclose(fp);
	cout << " size = " << filesize <<  " image size = " << Point(width, height) << endl;

    //Create Opencv mat structure for image dimension. For 8 bit bayer, type should be CV_8UC1.
    Mat image_16bit(height, width, CV_16UC1);
    memcpy(image_16bit.data, imagedata, filesize);
    free(imagedata);

    //cout << "dump1 of image_16bit" << endl;
    //cout << image_16bit << endl;
	//image_16bit.convertTo(image_16bit, CV_16UC1, 0.01);
    //cout << "dump2" << endl;
    //cout << image_16bit << endl;

	cout << "results copied from file to image_16bit " << std::endl;
	double min, max;
	minMaxLoc(image_16bit, &min, &max);
	cout << "image_16bit (min,max) = " << Point(min,max) <<  endl;
    cout << "writing to : " << pgmname << endl;
    imwrite(pgmname.c_str(), image_16bit);
	//image_16bit.convertTo(image_16bit, CV_16UC1, 256.0/max);
    //imwrite("image_16bit.pgm", image_16bit);
    //cout << image_16bit << endl;

	return 0;

	// TODO: use constexpr
	const int CONVERSION_SCALE_10_TO_8 = 0.0078125; // 256/1024
    Mat image_8bit(height, width, CV_8UC1);
	image_16bit.convertTo(image_8bit, CV_8UC1, 1);
	//image_16bit.copyTo(image_8bit);
	//Mat(height, width, CV_8U, image_16bit.data).copyTo(image_orig);
    //imwrite("output/image_8bit.jpg", image_8bit);
	cout << "10->8 converted" << std::endl;

    //cout << image_orig << endl;
    Mat image_bayer1(height, width, CV_16UC3);
    Mat image_bayer2(height, width, CV_16UC3);
    Mat image_bayer3(height, width, CV_16UC3);
    Mat image_bayer4(height, width, CV_16UC3);

	demosaicing(image_16bit, image_bayer1, COLOR_BayerRG2RGB); 	//	imwrite("output/bayer1.jpg", image_bayer1);
	demosaicing(image_16bit, image_bayer2, COLOR_BayerBG2BGR_EA);// 	imwrite("output/bayer2.jpg", image_bayer2);
	demosaicing(image_16bit, image_bayer3, COLOR_BayerBG2BGRA); //	imwrite("output/bayer3.jpg", image_bayer3);
	//demosaicing(image_8bit, image_orgg, COLOR_BayerRG2RGB_VNG); 	imwrite("output/COLOR_BayerRG2RGB_VNG.jpg", image_orgg);


    Mat rimage(height, width, CV_16UC1);
    Mat gimage(height, width, CV_16UC1);
    Mat bimage(height, width, CV_16UC1);
    //Mat mono_bayer = Mat::zeros(height, width, CV_8UC1);
	cout << "size of rimage "  << rimage.size() << std::endl;


    // separate out colors
    Vec3b color_value ;
    for (int x = 0; x < width * 2; ++x) {
        for (int y = 0; y < height; ++y) {
			color_value = image_bayer1.at<Vec3b>(y,x);
			//cout << "colod_value at " << Point(x,y) << " = " << color_value << " : " << std::endl;
			// BGR ordering?
		 	rimage.at<char>(y,x) = color_value[1];
		 	gimage.at<char>(y,x) = color_value[0];
		 	bimage.at<char>(y,x) = color_value[2];
		}
   	}

	//imwrite("output/R.jpg", rimage);
	//imwrite("output/G.jpg", gimage);
	//imwrite("output/B.jpg", bimage);

	cout << "bayer2 converted" << std::endl;

	// colorize
	// src: https://stackoverflow.com/questions/47939482/color-correction-in-opencv-with-color-correction-matrix
	// src: https://html.developreference.com/article/16995365/Fastest+way+to+apply+color+matrix+to+RGB+image+using+OpenCV+3.0%3F
	Mat bayer1_float = Mat(height, width, CV_32FC3);
	//double min, max;
	minMaxLoc(image_bayer1, &min, &max);
	cout << "image_bayer1l (min,max) = " << Point(min,max) <<  std::endl;
	image_bayer1.convertTo(bayer1_float, CV_32FC3, 1.0 / max);


/*

	color matrix


Red:RGB; Green:RGB; Blue:RGB
1.8786   -0.8786    0.0061
-0.2277    1.5779   -0.3313
0.0393   -0.6964    1.6321

float m[3][3] = {{1.6321, -0.6964, 0.0393},
                {-0.3313, 1.5779, -0.2277}, 
                {0.0061, -0.8786, 1.8786 }};

Red:RGB; Green:RGB; Blue:RGB
a   b    c
d    e   f
g   h    i

float m[3][3] = {{i, h, g},
                {f, e, d}, 
                {c, b, a }};




Red:RGB; Green:RGB; Blue:RGB
[[ 1.08130365 -0.3652276   0.18837898]
 [-0.06880907  0.59591615  0.21430298]
 [-0.08991182 -0.00185282  1.16162897]]


 [ 1.08130365 -0.3652276   0.18837898]
 [-0.06880907  0.59591615  0.21430298]
 [-0.08991182 -0.00185282  1.16162897]

*/

	/*color matrix 
	 *	Blue:BGR
	 *	Green:BGR; 
	 *	Red:BGR; 
	 */

	float m[3][3] = 
		{{ 1.16162897 , -0.00185282 ,-0.08991182},
		 { 0.21430298 ,  0.59591615 ,-0.06880907},
		 { 0.18837898 , -0.36522760 , 1.08130365}} ;

/*
	float m[3][3] = 
		{{ 0.46776504 ,  0.04178844 ,  0.14766951},
		 { 0.16525479 ,  0.41161491 ,  0.26941704},
		 { 0.25469232 , -0.13120429 ,  0.62369546}} ;
*/


	cout << "color correction matrix = " << m << std::endl;

	Mat M = Mat(3, 3, CV_32FC1, m).t();
	Mat bayer1_float_linear = bayer1_float.reshape(1, height*width);
	Mat color_matrixed_linear = bayer1_float_linear*M;
	//Mat final_color_matrixed = color_matrixed_linear.reshape(3, height); // or should be reshape (height, width, CV_32FC3);
	Mat final_color_matrixed = color_matrixed_linear.reshape(3, height); // or should be reshape (height, width, CV_32FC3);
	cout << "final_color_matrixed size = " << final_color_matrixed.size() << std::endl;
	final_color_matrixed.convertTo(final_color_matrixed, CV_32FC3, 512);
	imwrite(pgmname.c_str(), final_color_matrixed);


  return 0;
}

std::vector<fs::path> get_filenames( fs::path path ) {

    vector<fs::path> filenames;
    fs::path file;

    // http://en.cppreference.com/w/cpp/experimental/fs/directory_iterator
    const fs::directory_iterator end {};

    bool is_matched     = true;
    bool is_not_dotfile = true;
    bool is_regular     = true;
    bool is_ext_correct = true;
    for( fs::directory_iterator iter {path}; iter != end; ++iter ) {
        file = iter->path();
        // only sample files..skip every 000
        if (opt.sample) {
            string patt = "00." + g.src_file_ext;
            is_matched = (file.string().find(patt) != string::npos);
        }
        string patt = "/.";
        is_not_dotfile  = (file.string().find(patt) == string::npos);
        is_regular      = fs::is_regular_file(file);
        is_ext_correct  = file.extension() == "." + g.src_file_ext ;


        //cout << " file = " << file.string() << " is_matched = " << is_matched << " r=" <<  fs::is_regular_file(file) <<    " : " << (file.extension() == "." + g.src_file_ext) << endl;
        if( is_matched and is_not_dotfile and is_regular and is_ext_correct) {
            filenames.push_back( iter->path() );
        }
    }

    sort(filenames.begin(), filenames.end());
    return filenames;
}

fs::path getDestFromSrc (fs::path src) {

    src = fs::absolute(src);

    //it's OK!
	//if (src.parent_path().stem() != g.src_dir_ext) {
    //   LOGE << "error assuming dir of file " << src.string() << " should be : " << g.src_dir_ext << " and not " << src.parent_path().stem().string() ;
    //}

	string dst = src.parent_path().parent_path().string() + "/" + g.dst_dir_ext + "/";
	fs::create_directories(dst);

    dst += src.stem().string() + "." + g.dst_file_ext;

    return fs::path(dst);

}

 
vector< pair <fs::path,fs::path> > getAllFiles(void) {

    vector< pair <fs::path,fs::path> > allfiles; 

    fs::path dst;
    for (auto it = fp.folder_src_list.begin() ; it != fp.folder_src_list.end(); ++it) {
        fs::path folder_src = *it;
	    LOGV << "folder_src = " << folder_src.string();
	    for( const auto& src : get_filenames( folder_src ) ) {
            dst = getDestFromSrc(src);
            // TODO: also overwrite if size is small
		    if (! opt.overwrite and fs::exists( fs::status(dst) ) and (fs::file_size(dst) > g.smallsize)) {
			    LOGV << "exists so skipping : " << src.string() << " -> " << dst.string();
            } else {
			    LOGV << "adding to list : " << src.string() << " -> " << dst.string();
                allfiles.push_back( make_pair(src,dst) );
            }
        }
   }

   return allfiles;
    
}




int process_raw2dng(fs::path src, fs::path dst) { 

    Imagedata img;
    
    img.loadImage(src);

    // hack: raw files don't have attributes so we need to fake them or get them from a side file...
    string datestamp = dst.parent_path().parent_path().string();
    if (datestamp == "202003281227" or datestamp == "202003281219") { 
        img.sidefile_to_attributes(src,attr); 
    } else {
        img.fakedata_to_attributes(src);
    }
    img.writeDng(dst);

}



int process_dng2any(fs::path src, fs::path dst) { 
    if (opt.overwrite  and fs::exists(dst)) {
        fs::remove(dst);
    }

    Imagedata image;
    image.src = src;
    image.dst = dst;
    image.writeJpgDirect(dst);
    // image object is gone but writing may not be complete...
}
int process_jpg2trk(fs::path src, fs::path dst) { }
int process_any2ahe(fs::path src, fs::path dst) { 
    Imagedata img;
    img.writeAhe(src, dst);
}

int process_any2ann(fs::path src, fs::path dst) { 

    Imagedata img;
    
    img.loadImage(src);
    img.getExif();
    img.createAnnoText();
    img.writeAnnotated(dst, false);

}

indicators::ProgressBar* createProgressBar() {

    string prefixtext = opt.convert;
    string timetext = "[00m:36s<01m:01s]";
    int extra_gutter = 20;

    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int barwidth = w.ws_col - prefixtext.length() - timetext.length() - extra_gutter;

    indicators::ProgressBar* bar = new indicators::ProgressBar{
      indicators::option::BarWidth{barwidth},
      indicators::option::Start{" ["},
      indicators::option::Fill{"█"},
      indicators::option::Lead{"█"},
      indicators::option::Remainder{"-"},
      indicators::option::End{"]"},
      indicators::option::PrefixText{prefixtext},
      indicators::option::ForegroundColor{indicators::Color::yellow},
      indicators::option::ShowElapsedTime{true},
      indicators::option::ShowRemainingTime{true},
    };


    return bar;
}
  

// must be a better way to do this in c
int process_src2dst(string srcstr , string dststr) {

   fs::path src (srcstr); fs::path dst (dststr); 
    
   if (opt.convert == "raw2dng") { return process_raw2dng(src,dst); }
   if (opt.convert == "dng2jpg") { return process_dng2any(src,dst); }
   if (opt.convert == "dng2png") { return process_dng2any(src,dst); }
   if (opt.convert == "jpg2ann") { return process_any2ann(src,dst); }
   if (opt.convert == "png2ahe") { return process_any2ahe(src,dst); }
   if (opt.convert == "jpg2trk") { return process_jpg2trk(src,dst); }
}

int main(int argc, char** argv ) {

    // logging
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::info, &consoleAppender);

    // magick
    MagickCoreGenesis(argv[0],MagickFalse);

    processArgs(argc, argv);   

    auto allfiles = getAllFiles();
    int size = allfiles.size();
	LOGI << " about to convert " << size << " files from " << opt.convert;
    int index = 0;

    // see https://github.com/progschj/ThreadPool
    ThreadPool pool(opt.threads);
    vector< future<int> > results;

    for(const auto& srcdst: allfiles) {
        float percent = 100.0 * (float) index++ / float (size);
        fs::path src = srcdst.first;
        fs::path dst = srcdst.second;
		LOGI << fixed << setprecision(1) << percent << "% batch : " << src.string() << " -> " << dst.string();
        results.emplace_back(
            //pool.push(process_src2dst, src.string(),dst.string())
            pool.enqueue(process_src2dst, src.string(), dst.string())
        );
        //process_src2dst(src,dst);
     }

    auto bar = createProgressBar();

    //  wait for return values in dispatch order, assume no single long running job in queue
    index = 0;
    for(auto && result: results) {
        result.get();
        float percent = 100.0 * (float) index++ / float (size);
        bar->set_progress(percent);
		//LOGI << fixed << setprecision(2) << percent << "% convert : " << index << "/" << size << " files.";
    }
    

    // shutdown
    pool.~ThreadPool();
    MagickCoreTerminus();
    cout << "\e[?25h";
    cout << "all done\n";
    sleep(10);
   
    return 0;
}

