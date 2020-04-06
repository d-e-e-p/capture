/*
 * postprocess raw/jpg files
 *
 * (c) 2020 tensorfield ag
 *
 * convert dir of raw images to processed jpg
 *
 */


#include "postprocess.hpp"
//#include "202003281227_attr.hpp"
map<string,string> attr = {};

// global vars -- but in a struct so that makes it ok :-)
struct Options {
    string convert = "raw2dng";
    bool overwrite = false;
    bool verbose = false;
    int  threads = 4;
} opt;

struct Globals {
    // jpg file too small to be valid so ok to overwrite
    int smallsize = 1000;
    // options for source and destination extensions
    string src_dir_ext;
    string dst_dir_ext;
    string src_file_ext;
    string dst_file_ext;
    //dng header
    char* dng_headerdata = nullptr;
    int   dng_headerlength = 0;
    int   dng_attribute_start = 5310;
    //
    string dateStamp;
    ExifTool *exiftool;
} g;

struct FilePaths {

    vector <fs::path> folder_src_list ;

    //dng header
    fs::path headerfile = "/home/user/build/tensorfield/snappy/bin/dng_header.bin";

} fp;

struct Imagedata {
    char* data;  // actual image data
    int filesize;

    string header;
    string basename;
    long syst_timestamp;
    long stdy_timestamp;
    long datestamp;
    int gain;
    int expo;
    int sweep_index;
    int sweep_total;
    int fps;
    float delta_ms;
    int length;
    string comment;

    string text_north;
    string text_east;
};


void processArgs(int argc, char** argv) {

   const char* const short_opts = "-:";

   static struct option long_opts[] = {
        {"convert",     required_argument,  0, 42 },
        {"overwrite",   no_argument,        0, 43 },
        {"verbose",     no_argument,        0, 44 },
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

    if (g.dst_dir_ext == "ann") { g.dst_file_ext = "png" ;}
    if (g.dst_dir_ext == "trk") { g.dst_file_ext = "jpg" ;}
    if (g.dst_dir_ext == "ahe") { g.dst_file_ext = "png" ;}

    LOGI << "convert : " << g.src_dir_ext << " -> " << g.dst_dir_ext;

    LOGV << "Option processing complete";
}

// Pixel Format  : 'RG10' (10-bit Bayer RGRG/GBGB)
// see https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-srggb10.html

void readDngHeaderData() {                                                                                              
    g.dng_headerlength = fs::file_size(fp.headerfile);
    LOGI << " dng header : " << fp.headerfile << " size = " << g.dng_headerlength ;
    g.dng_headerdata = new char [g.dng_headerlength];
    ifstream fhead (fp.headerfile, ios::in|ios::binary);
    fhead.read (g.dng_headerdata, g.dng_headerlength);
    fhead.close();
}

string constructImageInfoTag(Imagedata image) {

     stringstream ss;
     ss << "header="             << image.header          << ":";
     ss << "frame="              << image.basename        << ":";
     ss << "syst_timestamp="     << image.syst_timestamp  << ":";
     ss << "stdy_timestamp="     << image.stdy_timestamp  << ":";
     ss << "datestamp="          << image.datestamp       << ":";
     ss << "gain="               << image.gain            << ":";
     ss << "expo="               << image.expo            << ":";
     ss << "sweep_index="        << image.sweep_index     << ":";
     ss << "sweep_total="        << image.sweep_total     << ":";
     ss << "fps="                << image.fps             << ":";
     ss << "delta_ms="           << image.delta_ms        << ":";
     ss << "image.length="       << image.length          << ":";
     ss << "comment="            << image.comment         << ":";

     
     /*
     ss << "image.id="           << image.id            << ":";
     ss << "image.width="        << image.width         << ":";
     ss << "image.height="       << image.height        << ":";
     ss << "image.pixelFormat="  << image.pixelFormat   << ":";
     ss << "image.stride="       << image.stride        << ":";
     ss << "image.timestamp.s="  << image.timestamp.s   << ":";
     ss << "image.timestamp.us=" << image.timestamp.us  << ":";
     */

     return ss.str();
}

// when no attr exists..
string getAttributeFromHack(fs::path filename) {
    string basename = filename.stem().string();
    Imagedata image;
    image.header          = "Tensorfield Ag (c) 2020";
    image.basename        = basename;
    image.syst_timestamp  = 1585423182229943136;
    image.stdy_timestamp  = 2017757423840;
    image.datestamp       = 202003201656;
    image.gain            = 100;
    image.expo            = 1500;
    image.sweep_index     = 0;
    image.sweep_total     = 0;
    image.fps             = 60;
    image.delta_ms        = 10;
    image.length          = 835584;
    image.comment         = "run with lower camera";

    string imageinfo = constructImageInfoTag(image);                                                                    
    return imageinfo;
}

string getAttributeFromFile(fs::path filename) {
    
    Imagedata image;

    string basename = filename.stem().string();
    string imageinfo = attr[basename];
    if (imageinfo.length() == 0) {
        LOGE << "imageinfo for " << basename << " = " << imageinfo;
    }

    LOGV << "imageinfo for " << basename << " = " << imageinfo;
   
    regex pattern (".*header=([^:]+):frame=([^:]+):syst_timestamp=([^:]+):stdy_timestamp=([^:]+):datestamp=([^:]+):gain=([^:]+):expo=([^:]+):sweep_index=([^:]+):sweep_total=([^:]+):fps=([^:]+):delta_ms=([^:]+):image.length=([^:]+):comment=([^:]*):.*");                                                                                                            
    smatch sm;
    regex_match (imageinfo,sm,pattern);
    if (sm.size() < 13) {
        LOGE << "patt not matched in: " << imageinfo <<  " sm.size() = " << sm.size() ;
        exit(-1); 
    }
    
     image.header          = sm[1];
     image.basename        = sm[2];
     image.syst_timestamp  = stol(sm[3]);
     image.stdy_timestamp  = stol(sm[4]);
     image.datestamp       = stol(sm[5]);
     image.gain            = stoi(sm[6]);
     image.expo            = stoi(sm[7]);
     image.sweep_index     = stoi(sm[8]);
     image.sweep_total     = stoi(sm[9]);
     image.fps             = stoi(sm[10]);
     image.delta_ms        = stof(sm[11]);
     image.length          = stoi(sm[12]);
     image.comment         = sm[13];
     // hack
     if (image.comment == "") {
        image.comment         = "sweep expo 500,1000 gain 20,100";
     }
     //LOGI << " image->sweep_total " << image->sweep_total ;
 
    imageinfo = constructImageInfoTag(image);                                                                    
    return imageinfo;
}

int process_raw2dng(fs::path rawname, fs::path dngname) {
    char *imagedata = NULL;

    // determine size of raw file assumed to be 16 bit raw 

    //Image dimension.
    //  width is 768 stride = 728 width + 40 margin
  	//  but some setting sometimes seems to omit stride...so safer to compute width
	//
    int imagesize = fs::file_size(rawname);
    int height = 544;
    int width =  imagesize / (2 * height); // 2 bytes per pixel
    //int width =  728; // 768 stride = 728 width + 40 margin
    int framesize = width * height; 
	LOGV << " in : " << rawname << " size = " << imagesize <<  " res = " << Point(width, height);

	imagedata = new char [imagesize];
    ifstream fraw (rawname, ios::in|ios::binary);
  	fraw.read (imagedata, imagesize);
    fraw.close();

   // read in header data once
    if (g.dng_headerdata == nullptr) {
        readDngHeaderData();
    }

    // hack
    //string imageinfo = getAttributeFromFile(rawname);
    string imageinfo = getAttributeFromHack(rawname);

    // write attributes
    vector<char> mod_headerdata(g.dng_headerdata,g.dng_headerdata + g.dng_headerlength);
    for (int i = 0; i < imageinfo.size(); i++) {
        mod_headerdata.at(i+g.dng_attribute_start) = imageinfo[i];
    }

    //correct the size of data

    int old_width  = 768*2;
    height = 544;

    int new_width = 728*2;
    int newdatalength = new_width * height;

    vector<char> indata(imagedata, imagedata + imagesize);
    vector<char> outdata(newdatalength, 0);

    for (int x = 0; x < old_width ; x++) {
        for (int y=0;y < height ; y++) {
            if (x < new_width) {
                int old_pixel = x + y * old_width;
                int new_pixel = x + y * new_width;
                outdata.at(new_pixel) = indata.at(old_pixel);
            }
        }
    }

    



    LOGV << "out : " << dngname ;
    //fs::remove(dngname);
    ofstream outfile (dngname,ofstream::binary);
    outfile.write (reinterpret_cast<const char*>(&mod_headerdata[0]), g.dng_headerlength);                              
    outfile.write (reinterpret_cast<const char*>(&outdata[0]), newdatalength);
    outfile.close();


}


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

    for( fs::directory_iterator iter {path}; iter != end; ++iter ) {
        file = iter->path();
        if( fs::is_regular_file(file) and file.extension() == "." + g.src_file_ext ) {
            filenames.push_back( iter->path() );
        }
    }

    sort(filenames.begin(), filenames.end());
    return filenames;
}

fs::path getDestFromSrc (fs::path src) {

    src = fs::absolute(src);
	if (src.parent_path().stem() != g.src_dir_ext) {
        LOGE << "error assuming dir of file " << src << " should be : " << g.src_dir_ext << " and not " << src.parent_path().stem() ;
    }

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
	    LOGV << "folder_src = " << folder_src;
	    for( const auto& src : get_filenames( folder_src ) ) {
            dst = getDestFromSrc(src);
            // TODO: also overwrite if size is small
		    if (! opt.overwrite and fs::exists( fs::status(dst) ) and (fs::file_size(dst) > g.smallsize)) {
			    LOGV << "exists so skipping : " << src << " -> " << dst;
            } else {
			    LOGV << "adding to list : " << src << " -> " << dst;
                allfiles.push_back( make_pair(src,dst) );
            }
        }
   }

   return allfiles;
    
}


int getExif(fs::path src, Imagedata* image) {


    // create our ExifTool object
    string exif;
    // read metadata from the image
    TagInfo *info = g.exiftool->ImageInfo(src.string().c_str(),"-UniqueCameraModel");
    if (info) {
        for (TagInfo *i=info; i; i=i->next) {
            if (strcmp(i->name, "UniqueCameraModel") == 0) {
                exif = i->value ;
            }
        }
        // we are responsible for deleting the information when done
        delete info;
    }
    // print exiftool stderr messages
    char *err = g.exiftool->GetError();
    if (err) LOGE << err;

    // header=Tensorfield Ag (c) 2020:frame=img000000:syst_timestamp=1585423182229943136:stdy_timestamp=2017757423840:gain=50:expo=750:fps=0:delta_ms=240.104:image.length=835584:comment=young carrots way up:
    // __header=Tensorfield Ag (c) 2020:frame=img000000_G000E0050:syst_timestamp=1585897404224762530:stdy_timestamp=122878985295355:datestamp=202004030003:gain=0:expo=50:sweep_index=1:sweep_total=0:fps=0:delta_ms=251.642:image.length=835584:comment=sweep gain exp lighting down to 4000lux:

    regex pattern (".*header=([^:]+):frame=([^:]+):syst_timestamp=([^:]+):stdy_timestamp=([^:]+):datestamp=([^:]+):gain=([^:]+):expo=([^:]+):sweep_index=([^:]+):sweep_total=([^:]+):fps=([^:]+):delta_ms=([^:]+):image.length=([^:]+):comment=([^:]*):.*");

    smatch sm;
    regex_match (exif,sm,pattern);
    if (sm.size() < 10) {
        LOGE << "patt not matched in: " << exif <<  " sm.size() = " << sm.size() ;
        exit(-1);
    }

     image->header          = sm[1];
     image->basename        = sm[2];
     image->syst_timestamp  = stol(sm[3]);
     image->stdy_timestamp  = stol(sm[4]);
     image->datestamp       = stol(sm[5]);
     image->gain            = stoi(sm[6]);
     image->expo            = stoi(sm[7]);
     image->sweep_index     = stoi(sm[8]);
     image->sweep_total     = stoi(sm[9]);
     image->fps             = stoi(sm[10]);
     image->delta_ms        = stof(sm[11]);
     image->length          = stoi(sm[12]);
     image->comment         = sm[13];
     // hack
     if (image->comment == "") {
        image->comment         = "sweep expo 500,1000 gain 20,100";
     }
     //LOGI << " image->sweep_total " << image->sweep_total ;

    return 0;
}

// end

int loadImage(fs::path src , Imagedata* image) {

    //struct Imagedata* image = (struct Imagedata*) malloc(sizeof(struct Imagedata));

    // get data in
    int filesize = fs::file_size(src);
	image->data = new char [filesize];
    image->filesize = filesize;
    ifstream fin (src, ios::in|ios::binary);
  	fin.read (image->data, filesize);
    fin.close();
    LOGV << "loaded " << src << " size : " << filesize;

    return 0;

}

/*
int writeAnnotated(Imagedata image, fs::path dst) {
    Magick::InitializeMagick("");
    Magick::Blob  blob(image->data, image->filesize);
    Magick::Image mimg(blob);
    mimg.process("analyze",0,0);

    mimg.draw( Magick::DrawableLine( 300,100, 300,500 ) );

    LOGI << "filter:brightness:mean: " <<  mimg.attribute("filter:brightness:mean");
    LOGI << "filter:saturation:mean: " <<  mimg.attribute("filter:saturation:mean");

    mimg.write( dst.string() );

}
*/

int createAnnoText(Imagedata* img) {

    char buff[BUFSIZ];

    if (img->delta_ms > 10) {
       snprintf(buff, sizeof(buff), "%s %3dfps %3.0fms gain=%d exp=%d",
            img->basename.c_str(), img->fps, img->delta_ms, img->gain, img->expo);
    } else {
       snprintf(buff, sizeof(buff), "%s %3dfps %3.1fms gain=%d exp=%d",
            img->basename.c_str(), img->fps, img->delta_ms, img->gain, img->expo);
    }
    img->text_north = buff;

    if (img->sweep_total > 0) {
        int length = to_string(img->sweep_total).length();
        snprintf(buff, sizeof(buff), " sweep=%*d/%d",
            length,img->sweep_index,img->sweep_total);
        img->text_north += buff;
    }

    struct tm *timeinfo;
    time_t rawtime;
    rawtime = img->syst_timestamp / 1e9;
    timeinfo = localtime (&rawtime);

    strftime (buff,sizeof(buff),": %d %a %b %d %I:%M%p : ",  timeinfo);
    img->text_east = to_string(img->datestamp);
    img->text_east += buff;
    img->text_east += img->comment;


}

int writeAhe(fs::path from, fs::path to) {

    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    string cmd = "magick  " + from.string() + " -clahe 25x25%+128+2 " + to.string();
    LOGV << "cmd = " << cmd;

    auto parts = boost::program_options::split_unix(cmd);
    std::vector<char*> cstrings ;
    for(auto& str : parts){
        cstrings.push_back(const_cast<char*> (str.c_str()));
    }

    int argc = (int)cstrings.size();
    char** argv = cstrings.data();

    //copy( argv, argv+argc, ostream_iterator<const char*>( cout, " " ) ) ;
    //cout << "\n";

    (void) MagickImageCommand(image_info, argc, argv, NULL, exception);

   if (exception->severity != UndefinedException) {
        CatchException(exception);
        LOGE << "Magick: Major Error Detected ";
        LOGE << exception->reason;
        LOGE << exception->description;
    }

    image_info=DestroyImageInfo(image_info);
    exception=DestroyExceptionInfo(exception);

}

int writeAnnotated(Imagedata* image, fs::path from, fs::path to) {

    ImageInfo *image_info = AcquireImageInfo();
    ExceptionInfo *exception = AcquireExceptionInfo();

    // determined by experiments
    int pointsize_north = (float) 1000 / (float) image->text_north.length() - 2 ;
    int pointsize_east  = (float) 1000 / (float) image->text_east.length() - 2 ;

    // 728 x 544 -> 768 x 584
    //string cmd = "magick  " + from.string() + " -clahe 25x25%+128+2 -gravity Southwest -background black -extent 768x584 -font Inconsolata -pointsize " + to_string(pointsize_north) + " -fill yellow -gravity North -annotate 0x0+0+15 \"" + image->text_north + "\" -gravity East -pointsize " + to_string(pointsize_east) + " -annotate 90x90+20+232 \"" + image->text_east + "\" " + to.string();
    string cmd = "magick  " + from.string() + " -clahe 25x25%+128+2 " + to.string();
    LOGV << "cmd = " << cmd;

    auto parts = boost::program_options::split_unix(cmd);
    std::vector<char*> cstrings ;
    for(auto& str : parts){
        cstrings.push_back(const_cast<char*> (str.c_str()));
    }

    int argc = (int)cstrings.size();
    char** argv = cstrings.data();

    //copy( argv, argv+argc, ostream_iterator<const char*>( cout, " " ) ) ;
    //cout << "\n";

    (void) MagickImageCommand(image_info, argc, argv, NULL, exception);

   if (exception->severity != UndefinedException) {
        CatchException(exception);
        LOGE << "Magick: Major Error Detected ";
        LOGE << exception->reason;
        LOGE << exception->description;
    }

    image_info=DestroyImageInfo(image_info);
    exception=DestroyExceptionInfo(exception);

}



int process_dng2jpg(fs::path src, fs::path dst) { }
int process_jpg2trk(fs::path src, fs::path dst) { }
int process_any2ahe(fs::path src, fs::path dst) { 
    writeAhe(src, dst);
}

int process_any2ann(fs::path src, fs::path dst) { 

    struct Imagedata obj;
    struct Imagedata *img = &obj;
    
    loadImage(src, img);
    getExif(src, img);
    createAnnoText(img);
    writeAnnotated(img,src, dst);
    delete img->data;

}

// must be a better way to do this in c
int process_src2dst(string srcstr , string dststr) {

   fs::path src (srcstr);
   fs::path dst (dststr);
    
   if (opt.convert == "raw2dng") { return process_raw2dng(src,dst); }
   if (opt.convert == "dng2jpg") { return process_dng2jpg(src,dst); }
   if (opt.convert == "jpg2ann") { return process_any2ann(src,dst); }
   if (opt.convert == "png2ahe") { return process_any2ahe(src,dst); }
   if (opt.convert == "jpg2trk") { return process_jpg2trk(src,dst); }
}

int main(int argc, char** argv ) {

    // logging
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::info, &consoleAppender);

    // exif and annotation
    g.exiftool = new ExifTool();
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
		LOGI << fixed << setprecision(2) << percent << "% batch : " << src << " -> " << dst;
        results.emplace_back(
            //pool.push(process_src2dst, src.string(),dst.string())
            pool.enqueue(process_src2dst, src.string(), dst.string())
        );
        //process_src2dst(src,dst);
     }

    int barwidth = 80;
    indicators::ProgressBar bar{
      indicators::option::BarWidth{barwidth},
      indicators::option::Start{" ["},
      indicators::option::Fill{"█"},
      indicators::option::Lead{"█"},
      indicators::option::Remainder{"-"},
      indicators::option::End{"]"},
      indicators::option::PrefixText{opt.convert},
      indicators::option::ForegroundColor{indicators::Color::yellow},
      indicators::option::ShowElapsedTime{true},
      indicators::option::ShowRemainingTime{true},
    };

    //  wait for return values in sequence
    index = 0;
    for(auto && result: results) {
        result.get();
        float percent = 100.0 * (float) index++ / float (size);
        bar.set_progress(percent);
		//LOGI << fixed << setprecision(2) << percent << "% convert : " << index << "/" << size << " files.";
    }
    

    // shutdown
    pool.~ThreadPool();
    delete g.exiftool;      // delete our ExifTool object
    MagickCoreTerminus();
    cout << "\e[?25h";
    cout << "all done\n";
    sleep(10);
   
    return 0;
}

