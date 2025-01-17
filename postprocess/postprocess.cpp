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

// global vars -- but in a struct so that makes it ok :-)
struct Options {
    string convert  = "raw2dng";
    bool overwrite  = false;
    bool verbose    = false;
    bool sample     = false;
    bool wb_and_cc  = false;
    int  threads    = 4;
} opt;

struct Globals {
    // file too small to be valid so ok to overwrite
    int smallsize = 1000;
    // options for source and destination extensions
    string src_dir_ext   = "raw";
    string dst_dir_ext   = "raw";
    string json_dir_ext  = "json";
    string src_file_ext  = "dng";
    string dst_file_ext  = "dng";
    string json_file_ext = "json";
    //
    string dateStamp;
} g;

struct FilePaths {

    vector <fs::path> folder_src_list ;

    // dng header and profile files relative to exe
    fs::path dng_headerfile = "<profiledir>/sample_dng_header.bin";
    fs::path dt_stylefile   = "<profiledir>/darktable.xml";
    fs::path rt_stylefile   = "<profiledir>/rawtherapee.pp3";

} fp;



void processArgs(int argc, char** argv) {

   const char* const short_opts = "-:";

   static struct option long_opts[] = {
        {"convert",     required_argument,  0, 42 },
        {"overwrite",   no_argument,        0, 43 },
        {"verbose",     no_argument,        0, 44 },
        {"sample",      no_argument,        0, 45 },
        {"threads",     required_argument,  0, 46 },
        {"wb_and_cc",   no_argument,        0, 47 },
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
         case 46:
            opt.threads = stoi(optarg);
            LOGI << " opt.threads = " << opt.threads;
            break;
         case 47:
            opt.wb_and_cc = true;
            LOGI << " opt.wb_and_cc = " << opt.wb_and_cc;
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
    // png should produce png, jpg -> jpg 
    if (g.dst_dir_ext == "ann") { g.dst_file_ext = g.src_file_ext ;}
    if (g.dst_dir_ext == "trk") { g.dst_file_ext = g.src_file_ext ;}
    if (g.dst_dir_ext == "ahe") { g.dst_file_ext = g.src_file_ext ;}

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


        LOGV << " file = " << file.string() << " is_matched=" << is_matched << " reqular=" <<  is_regular <<    " : ext=" << is_ext_correct;
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

fs::path getJsonFromSrc (fs::path src) {

    src = fs::absolute(src);

	string jsn = src.parent_path().parent_path().string() + "/" + g.json_dir_ext + "/";
	fs::create_directories(jsn);

    jsn += src.stem().string() + "." + g.json_file_ext;

    return fs::path(jsn);

}

fs::path getJsonFromRun (fs::path src) {

    src = fs::absolute(src);

    // raw/img0000000.raw -> run.json
	string jsn = src.parent_path().parent_path().string() + "/run.json";

    return fs::path(jsn);

}

bool file_is_unicode(fs::path src) {
    auto handle = ::magic_open(MAGIC_NONE|MAGIC_COMPRESS);
    ::magic_load(handle, NULL);
    auto type = ::magic_file(handle, src.c_str());
    string mimetype(type);
    string expected ("UTF-8 Unicode text");
    if (mimetype.compare(expected) == 0) {       
        return true;
    } else {
        return false;
    }
}
 
vector< pair <fs::path,fs::path> > getAllFiles(void) {

    vector< pair <fs::path,fs::path> > allfiles; 
	LOGV << "getallfiles:";

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

// TODO: make it work with paths that have trailing separator
vector< pair <fs::path,fs::path> > getAllDirs(void) {

    vector< pair <fs::path,fs::path> > alldirs; 
	LOGV << "getalldirs:";

    for (auto it = fp.folder_src_list.begin() ; it != fp.folder_src_list.end(); ++it) {
        fs::path folder_src = *it;
        fs::path folder_src_absolute = fs::absolute(folder_src);
	    fs::path folder_dst = folder_src_absolute.parent_path() / g.dst_dir_ext;
	    LOGV << "folder_src = " << folder_src.string() << " -> " << folder_dst.string();
        alldirs.push_back( make_pair(folder_src,folder_dst) );
   }

   return alldirs;
    
}

int process_raw2jpg(fs::path src, fs::path dst) { 

    Imagedata img;
    
    img.loadImage(src);
    img.dst = dst;

    // TODO : get attr from json sidefile if it exists

    img.writeJpgFromRaw();

}




int process_raw2dng(fs::path src, fs::path dst) { 

    Imagedata img;
    
    fs::path jsn_img = getJsonFromSrc(src);
    fs::path jsn_run = getJsonFromRun(src);
    //LOGI << "jsn_imag = " << jsn_img.string();
    //LOGI << "jsn_run = " << jsn_run.string();
    
    if (! fs::exists(jsn_img)) {
        LOGW << "skipping image--json file does not exist: " << jsn_img.string();
        return 0;
    }

    if (fs::is_empty(jsn_img)) {
        LOGW << "skipping image--json file is empty: " << jsn_img.string();
        return 0;
    }

    if (! file_is_unicode(jsn_img)) {
        LOGW << "skipping image--json file is not unicode: " << jsn_img.string();
        return 0;
    }


    img.readJson(jsn_img);

    if (fs::exists(jsn_run)) {
        img.readJson(jsn_run);
    }

    img.loadImage(src);
    img.dst = dst;


    // hack: raw files don't have attributes so we need to fake them or get them from a side file...
    //img.sidefile_to_attributes(src,dst); 

    /*
    string datestamp = dst.parent_path().parent_path().string();
    if (datestamp == "202003281227" or datestamp == "202003281219") { 
        img.sidefile_to_attributes(src,attr); 
    } else {
        img.fakedata_to_attributes(src);
    }
    */
    img.writeDng(dst, opt.wb_and_cc);
    LOGV << "dng write complete to " << dst.string();

}

// call tool on entire dir
int process_dir_dng2any(fs::path src, fs::path dst) {
    Imagedata image;
    image.src = src;
    image.dst = dst;
    image.writeJpgDir(src, dst, opt.overwrite); // direct run..don't thread
}

int process_dir_any2ahe(fs::path src, fs::path dst) {
    // assert opt.overwrite
    if (!opt.overwrite) {
        LOGE << " overrite needs to be on for this option!";
        exit(-1);
    }
    Imagedata image;
    image.src = src;
    image.dst = dst;
    image.writeAheDir(g.src_file_ext, g.dst_file_ext); // direct run..thread at app level
}


int process_dng2any(fs::path src, fs::path dst) { 
    if (opt.overwrite  and fs::exists(dst)) {
        fs::remove(dst);
    }

    Imagedata image;
    image.src = src;
    image.dst = dst;
    image.writeJpg(dst, false); // direct run..don't thread
    // image object is gone but writing may not be complete...
}
int process_jpg2trk(fs::path src, fs::path dst) { }
int process_any2ahe(fs::path src, fs::path dst) { 
    Imagedata img;
    img.writeAhe(src, dst);
}

int process_any2ann(fs::path src, fs::path dst) { 

    Imagedata img;
    img.src = src;
    img.getExif();

    // overwrite src
    img.src = src;
    img.dst = dst;
    img.createAnnoText();

    img.writeAnnotated(dst, true);

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
  
// process dir at a time
int process_dir_src2dst(string srcstr , string dststr) {

   fs::path src (srcstr); fs::path dst (dststr); 
    
//   if (opt.convert == "raw2dng") { return process_dir_raw2dng(src,dst); }
   if (opt.convert == "dng2jpg") { return process_dir_dng2any(src,dst); }
   if (opt.convert == "dng2png") { return process_dir_dng2any(src,dst); }
//   if (opt.convert == "jpg2ann") { return process_dir_any2ann(src,dst); }
   if (opt.convert == "jpg2ahe") { return process_dir_any2ahe(src,dst); }
   if (opt.convert == "png2ahe") { return process_dir_any2ahe(src,dst); }
//   if (opt.convert == "jpg2trk") { return process_dir_jpg2trk(src,dst); }
}

// process file at a time
// must be a better way to do this in c
int process_src2dst(string srcstr , string dststr) {

   fs::path src (srcstr); fs::path dst (dststr); 
    
   if (opt.convert == "raw2dng") { return process_raw2dng(src,dst); }
   if (opt.convert == "raw2jpg") { return process_raw2jpg(src,dst); }
   if (opt.convert == "dng2jpg") { return process_dng2any(src,dst); }
   if (opt.convert == "dng2png") { return process_dng2any(src,dst); }
   if (opt.convert == "jpg2ahe") { return process_any2ahe(src,dst); }
   if (opt.convert == "jpg2ann") { return process_any2ann(src,dst); }
   if (opt.convert == "jpg2trk") { return process_jpg2trk(src,dst); }
   if (opt.convert == "png2ahe") { return process_any2ahe(src,dst); }
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
    fs::path profiledir = file_exe.parent_path().parent_path() / "profiles";

    fp.dng_headerfile   =  profiledir / "dng_header_081100056.bin";
    fp.dt_stylefile     =  profiledir / "darktable.xml";
    //fp.rt_stylefile     =  profiledir / "basic_black_apr26_autorgb.pp3";
    fp.rt_stylefile     =  profiledir / "autoiso.pp3 ";

    LOGI << " using dng header:" << fp.dng_headerfile.string() ;
    LOGV << " using darktable style:" << fp.dt_stylefile.string() ;
    LOGV << " using rawtherapee style:" << fp.rt_stylefile.string() ;

}


int main(int argc, char** argv ) {

    // location of header and style files
    setupDirs();

    // logging
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::info, &consoleAppender);

    processArgs(argc, argv);   

    // init imagedata
    bool use_threaded_jpg = true;
    Imagedata::init(fp.dng_headerfile, fp.dt_stylefile, fp.rt_stylefile, use_threaded_jpg);

    // see https://github.com/progschj/ThreadPool
    ThreadPool pool(opt.threads);
    vector< future<int> > results;
    int index = 0;
    int size = 0;

    // operate on dirs if tool allows it
    bool run_dir_dng2any = (opt.convert == "dng2jpg") or (opt.convert == "dng2png");
    bool run_dir_any2ahe = opt.overwrite and ((opt.convert == "jpg2ahe") or (opt.convert == "png2ahe"));
    bool run_dir_mode = (! opt.sample) and (! opt.wb_and_cc) and (run_dir_dng2any or run_dir_any2ahe);
    LOGI << " run_dir_mode = " << run_dir_mode;
    if (run_dir_mode) {
        auto alldirs = getAllDirs();
        size = alldirs.size();
        LOGI << " about to convert " << size << " dirs from " << opt.convert;
        // turn off openmp threading if running multiple instances
        if ((opt.threads > 1) and (size > opt.threads)) {
            char env[]="OMP_NUM_THREADS=1"; 
            putenv( env ); 
            LOGI << " turning off app level multithreading: " << env;
        }

        for(const auto& srcdst: alldirs) {
            float percent = 100.0 * (float) index++ / float (size);
            fs::path src = srcdst.first;
            fs::path dst = srcdst.second;
		    LOGI << fixed << setprecision(1) << percent << "% batch : " << src.string() << " -> " << dst.string();
            if (opt.threads == 0) {
                process_dir_src2dst(src.string(), dst.string());
            } else {
                results.emplace_back(
                    pool.enqueue(process_dir_src2dst, src.string(), dst.string())
                );
            }
        }
     } else {
        auto allfiles = getAllFiles();
        size = allfiles.size();
        LOGI << " about to convert " << size << " files from " << opt.convert;
        if ((opt.threads > 1) and (size > opt.threads)) {
            char env[]="OMP_NUM_THREADS=1"; 
            putenv( env ); 
            LOGI << " turning off app level multithreading: " << env;
        }

        for(const auto& srcdst: allfiles) {
            float percent = 100.0 * (float) index++ / float (size);
            fs::path src = srcdst.first;
            fs::path dst = srcdst.second;
            LOGI << fixed << setprecision(1) << percent << "% batch : " << src.string() << " -> " << dst.string();

            if (opt.threads == 0) {
                process_src2dst(src.string(), dst.string());
            } else {
                results.emplace_back(
                    pool.enqueue(process_src2dst, src.string(), dst.string())
                );
            }
         }
    }


    //  wait for return values in dispatch order, assume no single long running job in queue
    index = 0;
    auto bar = createProgressBar();
    for(auto && result: results) {
        result.get();
        float percent = 100.0 * (float) index++ / float (size);
        bar->set_progress(percent);
		//LOGI << fixed << setprecision(2) << percent << "% convert : " << index << "/" << size << " files.";
		//LOGI << fixed << setprecision(2) << percent << "% convert : " << index << "/" << size << " files.";
    }
    

    // shutdown
    //pool.~ThreadPool();
    MagickCoreTerminus();
    cout << "\e[?25h";
    cout << "all done\n";
    //sleep(10);
   
    return 0;
}



