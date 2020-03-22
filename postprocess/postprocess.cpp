/*
 * postprocess raw files
 *
 * (c) 2020 tensorfield ag
 *
 *
 *
 */
#include <iostream>
#include <assert.h>
#include <stdio.h>

#include <vector>
#include <string>
#include <experimental/filesystem> // http://en.cppreference.com/w/cpp/experimental/fs
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;
namespace fs = std::experimental::filesystem;


//dng header
char* g_dng_headerdata = NULL;
int   g_dng_headerlength = 0;

enum bayer_pattern_e {
    BGGR,
    RGGB,
    RGRG
};

// Pixel Format  : 'RG10' (10-bit Bayer RGRG/GBGB)
// see https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/pixfmt-srggb10.html

int convertRaw2Dng(fs::path rawname, fs::path dngname) {
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
	cout << " in : " << rawname << " size = " << imagesize <<  " res = " << Point(width, height) << endl;

	imagedata = new char [imagesize];
    ifstream fraw (rawname, ios::in|ios::binary);
  	fraw.read (imagedata, imagesize);
    fraw.close();


   // read in header data once
    if (g_dng_headerdata == nullptr) {
        //fs::path headerfile = "/home/deep/build/snappy/bin/dng_header.bin";
        fs::path headerfile = "/home/user/build/tensorfield/snappy/bin/dng_header.bin";
        g_dng_headerlength = fs::file_size(headerfile);
		cout << " in : " << headerfile << " size = " << g_dng_headerlength << endl;

        g_dng_headerdata = new char [g_dng_headerlength];
		ifstream fhead (headerfile, ios::in|ios::binary);
		fhead.read (g_dng_headerdata, g_dng_headerlength);
		fhead.close();
    }

    cout << "out : " << dngname << endl;
    remove(dngname.c_str());
    ofstream outfile (dngname,ofstream::binary);
    outfile.write (reinterpret_cast<const char*>(g_dng_headerdata), g_dng_headerlength);
    outfile.write (reinterpret_cast<const char*>(imagedata), imagesize);
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



 
std::vector<std::string> get_filenames( fs::path path )
{

    vector<string> filenames ;
    
    // http://en.cppreference.com/w/cpp/experimental/fs/directory_iterator
    const fs::directory_iterator end{} ;
    
    for( fs::directory_iterator iter{path} ; iter != end ; ++iter )
    {
        // http://en.cppreference.com/w/cpp/experimental/fs/is_regular_file 
        if( fs::is_regular_file(*iter) ) // comment out if all names (names of directories tc.) are required
            filenames.push_back( iter->path().string() ) ;
    }

	sort(filenames.begin(), filenames.end());
    return filenames ;
}

int process_one_file(fs::path rawname) {

	if (rawname.extension() == ".raw") {
		fs::path dngname = rawname;
		dngname.replace_extension(".dng");

		// if raw directory is called "raw", then replace with "dng"
		if (dngname.parent_path().stem() == "raw") {
			string newpath = dngname.parent_path().parent_path().string() + "/dng/";
			fs::create_directories(newpath);
			dngname = newpath + dngname.stem().string() + ".dng";
		}

		if (!fs::exists( fs::status(dngname) )) {
			//cout << "convert " << rawname << " -> " << dngname << endl;
			convertRaw2Dng(rawname, dngname);		
		} else {
			cout << "skipping " << rawname << " -> " << dngname << endl;
		}
		
	}


}

int main(int argc, char** argv ) {

  if ( argc < 2 ) {
    printf("usage: postprocess <dir_with_raw_image_files> ...\n");
    return -1;
  }

  for (int i=1; i<argc; i++) {
	fs::path dir = argv[i];
	for( const auto& name : get_filenames( dir ) ) {
		//cout << name << '\n' ;
		process_one_file(name);
	}	
  }
}

