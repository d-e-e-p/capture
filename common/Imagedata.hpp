#pragma once


#include <iostream>
#include <iomanip>
#include <climits>
#include <nlohmann/json.hpp>
#include <experimental/filesystem>
namespace fs=std::experimental::filesystem;
#include <iostream>
#include <fstream>
#include <istream>
#include <ostream>
#include <sstream>
#include <assert.h>

#include <regex>                                                                                         

// for threading
#include <future>
#include <thread>
#include <chrono>

//logging
#include <plog/Log.h>
//exif
#include "exif/ExifTool.h"

//magick api
#define MAGICKCORE_HDRI_ENABLE 1
#define MAGICKCORE_QUANTUM_DEPTH 16

//#include <Magick++.h>
#include "MagickCore/studio.h"
#include "MagickCore/exception.h"
#include "MagickCore/exception-private.h"
#include "MagickCore/image.h"
#include "MagickWand/MagickWand.h"
#include "MagickWand/magick-cli.h"

//for parsing args 
#include "boost/program_options/parsers.hpp"

// libtiff
#include <tiffio.h>
#include <tiffio.hxx>


// fake attributes
//#include "../common/attr_0411.hpp"
//#include "../common/attr_comment_0411.hpp"

map<string,string> attr = {};
map<string,string> attr_comment = {};

using json = nlohmann::json;
using namespace std;
using namespace cv;




class Imagedata{
 public:

    // image settings
    string src; 
    string dst;
    char* data = nullptr;

    string header="Tensorfield Ag (c) 2020 😀";
    string basename;
    string datestamp;

    long system_clock_ns;
    long steady_clock_ns;

    int gain;
    int expo;
    int sweep_index = 0;
    int sweep_total = 0;
    float fps;
    float delta_ms;
    float focus1 = 0;
    float focus2 = 0;
    float focus3 = 0;
    float focus4 = 0;

    int width  = 0;
    int height = 0;

    int bpp    = 16; // bits per pixel
    int pixel_depth = bpp / CHAR_BIT;
    int bytesperline  = width * pixel_depth;
    int datalength  = bytesperline * height;

    string comment;
    string command;

    string text_north;
    string text_south;
    string text_east;

    bool dont_delete_me_yet = false;

    // share setup with all object members
    static struct Setup {
        // for job handling
         future <void> job_queue;
        // TODO: also need to call job_queue.wait();
        //dng header 
         string dng_headerfile;
         char*  dng_headerdata;
         int    dng_headerlength;
         int    dng_attribute_start;

         string dt_stylefile;
         string rt_stylefile;

         // for exif
         ExifTool* exiftool;

         // for jpg
         bool use_threaded_jpg;
    } s ;

    //Destructor
    ~Imagedata(){
        delete data;
    }

    //Constructor
    Imagedata() {
        data = nullptr;
    }

    // fake out copy constructor so we can use default copy...
    Imagedata* cloneobj() {
        Imagedata *that = new Imagedata;
        that = this; // shallow copy....too much work to write assignment operator in class
        that->data = new char [datalength];
        memcpy(that->data, data, datalength);
        return that;
    }

    // HACK: just use filename to construct fake data
    void fakedata_to_attributes(fs::path filename) {

        src = filename;
        header="Tensorfield Ag (c) 2020 😀";
        basename = filename.stem().string(); //eg basename="img000000";
        datestamp="202003281227";
        system_clock_ns=1584752184371828704;
        steady_clock_ns=2466899309120;
        gain=100;
        expo=1500;
        sweep_index=0;
        sweep_total=0;
        fps=60.0;
        delta_ms=251.198;

        bpp = 16;
        width = 728;
        height = 544;
        datalength = 835584;

        comment="run with lower camera";
        text_north= "";
        text_east= "";
        command="../bin/capture --frames 100000 --gain 100 --exposure 1500 --comment \"run with lower camera\"";

    }

    // use side file to construct data
    void sidefile_to_attributes(fs::path srcp, fs::path dstp) {
        src = srcp;
        dst = dstp;
        basename = srcp.stem().string();
        datestamp = dstp.parent_path().parent_path().stem().string();
        string key = datestamp + "/" + basename;
        if (attr.find(key) == attr.end() ) {
            LOGE << "dst = " << dst;
            LOGE << "datestamp = " << datestamp;
            LOGE << "basename = " << basename;
            LOGE << "attr has no map entry for : " << key;
            exit(-1);
        }
        // override comment
        if (attr_comment.find(datestamp) == attr_comment.end() ) {
            LOGE << "attr_comment has no map entry for : " << datestamp;
            exit(-1);
        }

        string jstr = attr[key];
        json_to_attributes(jstr);

        //overrides
        comment = attr_comment[datestamp];
        src = srcp;
        dst = dstp;
    }


    void loadImage(fs::path filename) { 
        src = filename;

        // get data in
        int filesize = fs::file_size(filename);
        // make sure size matches width * height * bpp / CHAR_BIT
        data = new char [filesize];
        ifstream fin (filename, ios::in|ios::binary);
        fin.read (data, filesize);
        fin.close();

         // update width to reflect new filesize
         datalength = filesize;
         width = datalength / ( height * pixel_depth );
        //LOGV << "loaded " << src << " filesize:" << filesize
        //    << " = w:" << width << " * h:" << height << " * bpp:" << bpp << " / CHAR_BIT:" << CHAR_BIT;
    }

    void writeRaw(fs::path filename) {

        //fs::path filename = fp.folder_raw / (image.basename + ".raw");
        remove(filename.c_str());
        int fd = open (filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            LOGE << "Unable to save frame, cannot open file " << filename.string();
            return;
        }

        int writenBytes = write(fd, data, datalength);
        if (writenBytes < 0) {
            LOGE << "Error writing to file " << filename.string() ;
        } else if ( (uint32_t) writenBytes != datalength) {
            LOGE << "Error: " << writenBytes << " out of " << datalength << " were written to file " << filename.string() ;
        }

        close(fd);
    }


    void createAnnoText() { 

        char buff[BUFSIZ];
        int delta_ms_precision = (delta_ms > 10) ? 0 : 1 ;
        snprintf(buff, sizeof(buff), "%s %3.0ffps %3.*fms gain=%d exp=%d focus=%.0f%%", 
                basename.c_str(), fps, delta_ms_precision, delta_ms, gain, expo, focus1);
        text_north = buff;

        if (sweep_total > 0) {
            int length = to_string(sweep_total).length();
            snprintf(buff, sizeof(buff), " sweep=%*d/%d",
                length,sweep_index,sweep_total);
            text_north += buff;
        }

        time_t rawtime = system_clock_ns / 1e9; // who needs chrono...
        struct tm * timeinfo = localtime (&rawtime);

        strftime (buff,sizeof(buff),": %d %a %b %d %I:%M%p : ",  timeinfo);

        text_east = datestamp + buff + comment;

    }

    void writeJpgDir(fs::path folder_src, fs::path folder_dest, bool overwrite=true) {

        src = folder_src;
        dst = folder_dest;
        string y_option = overwrite ? " -Y " : " " ;
        string command = "rawtherapee-cli " + y_option + " -q -o " +  dst + " -p " + s.rt_stylefile + " -j100 -js3 -b8 -c " +  src;
        LOGV << "cmd: " << command ;
        runThreadedCommand(command);
    }


    // launch and return if threaded
    void writeJpg(fs::path file_dest, bool run_threaded=true, bool run_rawtherapee=true) {

        dst = file_dest;
        string command;
        if (run_rawtherapee) {
            command = "rawtherapee-cli -Y -q -o " +  dst + " -p " + s.rt_stylefile + " -j100 -js3 -b8 -c " +  src;
        } else {
            command = "darktable-cli --bpp 8 " + src +  " " + s.dt_stylefile + " " + dst;
        }
        LOGV << "cmd: " << command ;
        LOGV << "run_threaded: " << run_threaded ;
        if (run_threaded) {
            chrono::nanoseconds ns(1);
            auto status = s.job_queue.wait_for(ns);
            if (status == future_status::ready) {
                s.job_queue = async(launch::async, runThreadedCommand, command);
            }
        } else {
            runThreadedCommand(command);
        }
    }

    void writeDngAndJpgString(string file_dng, string file_jpg,  bool run_threaded=false ) {
        LOGV << "start writeDngAndJpgString: " << file_jpg << " run_threaded = " << run_threaded;
        writeDngAndJpg(file_dng, file_jpg, run_threaded);
    }

    // launch and return if threaded
    // WARNING: async doesn't work with files declared as fs::path
    void writeDngAndJpg(fs::path file_dng, fs::path file_jpg,  bool run_threaded=true, bool run_rawtherapee=true) {
        LOGV << "start writeDngAndJpg: " << file_jpg.string() << " run_threaded = " << run_threaded;

        if (run_threaded) {
            chrono::nanoseconds ns(1);
            auto status = s.job_queue.wait_for(ns);
            if (status == future_status::ready) {
                // make a copy, launch and return..
                Imagedata *imgobj = cloneobj();
                auto func = std::bind(&Imagedata::writeDngAndJpgString, imgobj, file_dng.string(), file_jpg.string(), false);
                s.job_queue = async(launch::async, func);
            }
        } else {

            LOGI << attributes_to_json(-1);

            src = "camera"; dst = file_dng;
            writeDng(file_dng);
            src = file_dng; dst = file_jpg;
            writeJpg(file_jpg, false);   // not threaded
            // warning: destroy yourself--end of the road for this Imagedata object
            delete this;
        }
    }


    void getExif() {

        string exif;
        // read metadata from the image
        TagInfo *info = s.exiftool->ImageInfo(src.c_str(),"-UniqueCameraModel");
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
        char *err = s.exiftool->GetError();
        if (err) LOGE << err;

        if (exif.length() == 0) {
            exif = R"(
{"basename":"dummy","bpp":16,"command":"command ","comment":"dummy","datalength":835584,"datestamp":"202004241432","delta_ms":100,"expo":100,"fps":3,"gain":0,"header":"Tensorfield Ag (c) 2020 😀","height":544,"src":"camera","steady_clock_ns":1000000000000,"sweep_index":0,"sweep_total":0,"system_clock_ns":1587763925123609152,"text_east":"","text_north":"","width":768})";
        }

        json_to_attributes(exif);
        //LOGI << " sweep_total " << sweep_total ;

    }



    void writeAnnotated( fs::path to, bool label) {

        dst = to;

        string cmd;
        if (label) {
            // determined by experiments
            string pointsize_north = to_string( (float) 1000 / (float) text_north.length() - 2 );
            string pointsize_east  = to_string( (float)  800 / (float) text_east.length()  - 2 );
            // 728 x 544 -> 768 x 584
            //cmd = "/usr/local/bin/magick  " + src + " -clahe 25x25%+128+2 -quality 100 -gravity Southwest -background black -extent 768x584 -font Inconsolata -pointsize " + pointsize_north + " -fill yellow -gravity North -annotate 0x0+0+15 \"" + text_north + "\" -gravity East -pointsize " + pointsize_east + " -annotate 90x90+20+232 \"" + text_east + "\" -quality 100 " + dst;
            cmd = "/usr/local/bin/magick  " + src + " -quality 100 -gravity Northwest -background black -extent 768x584 -font Inconsolata -pointsize " + pointsize_north + " -fill yellow -gravity South -annotate 0x0+0+15 \"" + text_north + "\" -gravity East -pointsize " + pointsize_east + " -annotate 90x90+20+232 \"" + text_east + "\" -quality 100 " + dst;
        } else {
            cmd = "magick  " + src + " -clahe 25x25%+128+2 -quality 100 " + dst;
        }

        runMagick(cmd);


    }

   void writeAheDir(string src_file_ext, string dst_file_ext) {

        bool fix_vignette= true;
        string cmd_fix_vignette =  " ";
        if (fix_vignette) {
            cmd_fix_vignette =  "  /home/user/build/tensorfield/snappy/bin/flatfield.pgm -fx '(u/v)*v.p{w/2,h/2}' ";
        }

        string cmd = "magick mogrify -quality 100 -format " + dst_file_ext + " -path " + dst + cmd_fix_vignette +  " -clahe 25x25%+128+2 " + src + "/*." + src_file_ext;
        runMagick(cmd);

    }


   void writeAhe(fs::path from, fs::path to) {

        bool fix_vignette= true;
        string cmd_fix_vignette =  " ";
        if (fix_vignette) {
            cmd_fix_vignette =  "  /home/user/build/tensorfield/snappy/bin/flatfield.pgm -fx '(u/v)*v.p{w/2,h/2}' ";
        }

        string cmd = "magick -quality 100  " + from.string() + cmd_fix_vignette +  " -clahe 25x25%+128+2 " + to.string();
        runMagick(cmd);

    }

/*
// using actual magick api
int writeAnnotated(Imagedata image, fs::path dst) {
    Magick::InitializeMagick("");
    Magick::Blob  blob(image->data, image->filesize);
    Magick::Image mimg(blob);
    mimg.process("analyze",0,0);
    //image.crop(Geometry(100, 100, 0, 0));
    //image.repage();

    mimg.draw( Magick::DrawableLine( 300,100, 300,500 ) );

    LOGI << "filter:brightness:mean: " <<  mimg.attribute("filter:brightness:mean");
    LOGI << "filter:saturation:mean: " <<  mimg.attribute("filter:saturation:mean");

    mimg.write( dst );

}
*/
    void writeDngAttr(fs::path dngname) {

        string imageinfo = attributes_to_json(0);
        LOGV << imageinfo;

        Exiv2::ExifData exifData;
        exifData["Exif.Image.UniqueCameraModel"]           = imageinfo;
        exifData["Exif.Image.BitsPerSample"]               = "16";                                                                     
        exifData["Exif.Image.SamplesPerPixel"]             = "1";
        exifData["Exif.Image.Make"]                        = "__SNAPPY__";
        exifData["Exif.Image.Model"]                       = "__WEED__";
        exifData["Exif.Image.ColorMatrix1"]                = "1 0 0 0 1 0 0 0 1";
        exifData["Exif.Image.ColorMatrix2"]                = "1 0 0 0 1 0 0 0 1";
        exifData["Exif.Image.AsShotNeutral"]               = "1 1 1";
                                                
        exifData["Exif.Image.DNGVersion"]                  = "1.4.0.0";
        exifData["Exif.Image.DNGBackwardVersion"]          = "1.3.0.0";
        exifData["Exif.Image.SubfileType"]                 = "Full-resolution Image";
        exifData["Exif.Image.PhotometricInterpretation"]   = "Color Filter Array";
        exifData["Exif.Image.Orientation"]                 = "Horizontal";
        exifData["Exif.Image.MakerNotes:WB_RBLevels"]      = "1 1 1 1";
        exifData["Exif.Image.WB_RBLevels"]                 = "1 1 1 1";
        exifData["Exif.Image.AnalogBalance"]               = "1 1 1";
                                                
        exifData["Exif.Image:CFARepeatPatternDim"]         = "2 2";
        exifData["Exif.Image:CFAPattern2"]                 = "0 1 1 2";
        exifData["Exif.Image:CalibrationIlluminant1"]      = "Standard Light A";
        exifData["Exif.Image:CalibrationIlluminant2"]      = "D65";
        exifData["Exif.Image:CalibrationIlluminant1"]      = "Standard Light A";
        exifData["Exif.Image:CalibrationIlluminant2"]      = "D65";


        Exiv2::Image::AutoPtr image = Exiv2::ImageFactory::open(dngname.c_str());
        try {
            image->setExifData(exifData);
            image->writeMetadata();
        } catch (Exiv2::AnyError& e) {
            LOGE << "Caught Exiv2 exception '" << e << "'";
            exit(-1);
        }

    }

    void writeDngAttrExiftool(fs::path dngname) {

        ExifTool *et = new ExifTool();
        //auto et = s.exiftool;

        string imageinfo = attributes_to_json(-1);

        et->SetNewValue("UniqueCameraModel",            imageinfo.c_str());

        et->SetNewValue("BitsPerSample",                "16");
        et->SetNewValue("SamplesPerPixel",              "1");
        et->SetNewValue("Make",                         "__SNAPPY__");
        et->SetNewValue("Model",                        "__WEED__");
        et->SetNewValue("ColorMatrix1",                 "1 0 0 0 1 0 0 0 1");
        et->SetNewValue("ColorMatrix2",                 "1 0 0 0 1 0 0 0 1");
        et->SetNewValue("AsShotNeutral",                "1 1 1");

        et->SetNewValue("DNGVersion",                   "1.4.0.0");
        et->SetNewValue("DNGBackwardVersion",           "1.3.0.0");
        et->SetNewValue("EXIF:SubfileType",             "Full-resolution Image");
        et->SetNewValue("PhotometricInterpretation",    "Color Filter Array");
        et->SetNewValue("Orientation",                  "Horizontal");
        et->SetNewValue("MakerNotes:WB_RBLevels",       "1 1 1 1");
        et->SetNewValue("WB_RBLevels",                  "1 1 1 1");
        et->SetNewValue("AnalogBalance",                "1 1 1");

        et->SetNewValue("IFD0:CFARepeatPatternDim",     "2 2");
        et->SetNewValue("IFD0:CFAPattern2",             "0 1 1 2");
        et->SetNewValue("IFD0:CalibrationIlluminant1",  "Standard Light A");
        et->SetNewValue("IFD0:CalibrationIlluminant2",  "D65");
        et->SetNewValue("IFD0:CalibrationIlluminant1",  "Standard Light A");
        et->SetNewValue("IFD0:CalibrationIlluminant2",  "D65");




        // write exif to file 
        et->WriteInfo(dngname.c_str(), "-overwrite_original_in_place");

        bool debug_exif = true;
        if (debug_exif) {
            // wait for exiftool to finish writing
            int result = et->Complete(10);

            if (result > 0) {
                // checking either the number of updated images or the number of update
                // errors should be sufficient since we are only updating one file,
                // but check both for completeness
                int n_updated = et->GetSummary(SUMMARY_IMAGE_FILES_UPDATED);
                int n_errors = et->GetSummary(SUMMARY_FILE_UPDATE_ERRORS);
                if (n_updated == 1 && n_errors <= 0) {
                    //LOGV << "Exif Success!"
                } else {
                    LOGE << "exiftool problem";
                }
                // print any exiftool messages
                char *out = et->GetOutput();
                if (out) LOGV << out;
                char *err = et->GetError();
                if (err) LOGE << err;
            } else {
                LOGE << "Error executing exiftool !";
            }
        }
        delete et;

    }

    // assume input data is in raw format
    int writeJson(fs::path jsonname) {
        string imageinfo = attributes_to_json(4);
        LOGV << "writing to : " << jsonname.string() ;
        //fs::remove(dngname);
        ofstream outfile (jsonname);
        outfile << imageinfo;
        outfile.close();
    }


    int writeDng(string dngname, bool wb_and_cc = false) {
 
        enum illuminant {
            ILLUMINANT_UNKNOWN = 0,
            ILLUMINANT_DAYLIGHT,
            ILLUMINANT_FLUORESCENT,
            ILLUMINANT_TUNGSTEN,
            ILLUMINANT_FLASH,
            ILLUMINANT_FINE_WEATHER = 9,
            ILLUMINANT_CLOUDY_WEATHER,
            ILLUMINANT_SHADE,
            ILLUMINANT_DAYLIGHT_FLUORESCENT,
            ILLUMINANT_DAY_WHITE_FLUORESCENT,
            ILLUMINANT_COOL_WHITE_FLUORESCENT,
            ILLUMINANT_WHITE_FLUORESCENT,
            ILLUMINANT_STANDARD_A = 17,
            ILLUMINANT_STANDARD_B,
            ILLUMINANT_STANDARD_C,
            ILLUMINANT_D55,
            ILLUMINANT_D65,
            ILLUMINANT_D75,
            ILLUMINANT_D50,
            ILLUMINANT_ISO_TUNGSTEN,
        };

        enum tiff_cfa_color {
            CFA_RED = 0,
            CFA_GREEN = 1,
            CFA_BLUE = 2,
        };

        enum cfa_pattern {
            CFA_BGGR = 0,
            CFA_GBRG,
            CFA_GRBG,
            CFA_RGGB,
            CFA_NUM_PATTERNS,
        };

        static const char cfa_patterns[4][CFA_NUM_PATTERNS] = {                                                                                       
            [CFA_BGGR] = {CFA_BLUE, CFA_GREEN, CFA_GREEN, CFA_RED},
            [CFA_GBRG] = {CFA_GREEN, CFA_BLUE, CFA_RED, CFA_GREEN},
            [CFA_GRBG] = {CFA_GREEN, CFA_RED, CFA_BLUE, CFA_GREEN},
            [CFA_RGGB] = {CFA_RED, CFA_GREEN, CFA_GREEN, CFA_BLUE},
        };

        /* Default ColorMatrix1, when none provided */
        static const float default_color_matrix[] = {
             2.005, -0.771, -0.269,
            -0.752,  1.688,  0.064,
            -0.149,  0.283,  0.745
        };


        unsigned int pattern = CFA_RGGB;                                                                                                          
   static const short bayerPatternDimensions[] = { 2, 2 };
   static const float ColorMatrix[] = {
      1.0, 0.0, 0.0,
      0.0, 1.0, 0.0,
      0.0, 0.0, 1.0,
   };

   static const float AsShotNeutral[] = {
      0.8, 1.0, 0.6,
   };


    // create Mat object from raw data
    Mat mat_crop(height, width, CV_16U, data, bytesperline);
    printMatStats("mat_crop", mat_crop);

    // encode to tiff
    vector<int> params;
    params.push_back(IMWRITE_TIFF_COMPRESSION);
    params.push_back(COMPRESSION_NONE);
    vector<uchar> buffer;
    cv::imencode(".tiff", mat_crop, buffer, params);

    // load into 
     istringstream in (std::string(buffer.begin(), buffer.end()));
     TIFF* intif = TIFFStreamOpen("MemTIFF", &in);
     //TIFF* intif = TIFFOpen("temp.tif", "r");
     LOGV << "created MemTIFF from data len=" << buffer.size();

    string imageinfo = attributes_to_json(-1);

    TIFF* tif = TIFFOpen (dngname.c_str(), "w");


    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_DNGVERSION, "\001\004\0\0");
    TIFFSetField(tif, TIFFTAG_DNGBACKWARDVERSION, "\001\004\0\0");


    TIFFSetField (tif, TIFFTAG_SUBFILETYPE, 0);
    TIFFSetField (tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField (tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField (tif, TIFFTAG_BITSPERSAMPLE, 16);
    TIFFSetField (tif, TIFFTAG_ROWSPERSTRIP, 1);
    TIFFSetField (tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField (tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_CFA);
    TIFFSetField (tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField (tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField (tif, TIFFTAG_SAMPLEFORMAT, SAMPLEFORMAT_UINT);
    TIFFSetField (tif, TIFFTAG_CFAREPEATPATTERNDIM, bayerPatternDimensions);
    TIFFSetField(tif, TIFFTAG_CFAPATTERN, cfa_patterns[pattern]);
    // needs to have the "4" depending on definition in ./libtiff/tif_dirinfo.c
    //TIFFSetField(tif, TIFFTAG_CFAPATTERN, 4, cfa_patterns[pattern]);
    //

   //TIFFSetField (tif, TIFFTAG_CFAPATTERN, "\00\01\01\02");
   //TIFFSetField(tif, TIFFTAG_CFAREPEATPATTERNDIM, "\02\02");

    TIFFSetField (tif, TIFFTAG_MAKE, "__SNAPPY__");
    TIFFSetField (tif, TIFFTAG_MODEL, "__WEED__");
    TIFFSetField (tif, TIFFTAG_UNIQUECAMERAMODEL, imageinfo.c_str());
    TIFFSetField (tif, TIFFTAG_COLORMATRIX1, 9, default_color_matrix);
    TIFFSetField (tif, TIFFTAG_COLORMATRIX2, 9, default_color_matrix);
    TIFFSetField (tif, TIFFTAG_ASSHOTNEUTRAL, 3, AsShotNeutral);
    TIFFSetField (tif, TIFFTAG_CFALAYOUT, 1);
    TIFFSetField (tif, TIFFTAG_CFAPLANECOLOR, 3, "\00\01\02");

    uint32* scan_line = (uint32 *)malloc(width*(sizeof(uint32)));

    for (int i = 0; i < height; i++) {
         TIFFReadScanline(intif, scan_line, i);
         TIFFWriteScanline(tif, scan_line, i, 0);
    }

     LOGV << "TIFFNumberOfStrips: " <<  TIFFNumberOfStrips(intif) << " -> " <<  TIFFNumberOfStrips(tif);
     LOGV << "TIFFNumberOfTiles:  " <<  TIFFNumberOfTiles(intif) << " -> " <<  TIFFNumberOfTiles(tif);

    TIFFClose (intif);
    TIFFClose (tif);

    return 0;

    }


    // assume input data is in raw format
    int writeDngV2(fs::path dngname, bool wb_and_cc = false) {

        Mat mat_crop(height, width, CV_16U, data, bytesperline);
        printMatStats("mat_crop", mat_crop);

        vector<int> params;
        params.push_back(IMWRITE_TIFF_COMPRESSION);
        params.push_back(1/*COMPRESSION_NONE*/);
        vector<uchar> buffer;
        cv::imencode(".tiff", mat_crop, buffer, params);

        LOGV << "writing to : " << dngname.string() ;
        //fs::remove(dngname);
        ofstream outfile (dngname,ofstream::binary);
        outfile.write (reinterpret_cast<const char*>(&buffer[0]), buffer.size());
        outfile.close();

        // now add dng exif attributes
        //writeDngAttr(dngname);
        writeDngAttr(dngname);

    }
   
    // assume input data is in raw format!
    int writeDngV1(fs::path dngname, bool wb_and_cc = false) {

        // assume g.dng_headerdata is loaded 
        assert(s.dng_headerdata != nullptr);

        string imageinfo = attributes_to_json(0);
        LOGV << "imageinfo = " << imageinfo; 
        // write attributes
        // reset image size i
        vector<char> mod_headerdata(s.dng_headerdata,s.dng_headerdata + s.dng_headerlength);
        for (int i = 0; i < imageinfo.length(); i++) {
            mod_headerdata.at(i+s.dng_attribute_start) = imageinfo[i];
        }
        //LOGV << "mod_headerdata = " << mod_headerdata; 

        //reshape the size of data to get rid of band if needed ... assume width = 768 or 728
        char* cropdata = crop_wb_cc(data, wb_and_cc);

        LOGV << "writing to : " << dngname.string() ;
        //fs::remove(dngname);
        ofstream outfile (dngname,ofstream::binary);
        outfile.write (reinterpret_cast<const char*>(&mod_headerdata[0]), s.dng_headerlength);
        outfile.write (cropdata, datalength);
        outfile.close();

        delete cropdata;

    }


    int writeJpgFromRaw() {
        //int bpp = 16;
        //int height = 544;
        //int pixel_depth = bpp / CHAR_BIT;

        int old_width  = 768;
        int new_width = 728;
        int step = old_width * pixel_depth;

        Mat mat_crop(height, new_width, CV_16U, data, step);
        printMatStats("mat_crop", mat_crop);
        //update globals
        width = new_width;
        bytesperline = width * pixel_depth;
        datalength = bytesperline * height;
         
        LOGV << "copy = " << mat_crop.size() << " cont = " << mat_crop.isContinuous() ;
        
        Mat mat_bayer1(height, width, CV_16U);
        demosaicing(mat_crop, mat_bayer1, COLOR_BayerRG2RGB);
        printMatStats("mat_bayer1", mat_bayer1);

        //Mat mat_out(height, new_width, CV_16UC1);
        Mat mat_out = correctImageColors(mat_bayer1);
        printMatStats("mat_out", mat_out);

        imwrite(dst,mat_out);

      }


    //class init just once
    static void init(fs::path dng_headerfile, fs::path dt_stylefile, fs::path rt_stylefile, bool use_threaded_jpg) {

        s.dng_headerfile = dng_headerfile;
        s.dt_stylefile   = dt_stylefile;
        s.rt_stylefile   = rt_stylefile;
        s.use_threaded_jpg = use_threaded_jpg;

        //load in header
        s.dng_headerlength = fs::file_size(s.dng_headerfile);
        //LOGI << " reading dng header : " <<  s.dng_headerfile << " size = " << s.dng_headerlength ;
        s.dng_headerdata = new char [s.dng_headerlength];
        ifstream fhead (s.dng_headerfile, ios::in|ios::binary);
        fhead.read (s.dng_headerdata, s.dng_headerlength);
        fhead.close();

        // should be something like 5310...
        s.dng_attribute_start = findStartOfSetting(s.dng_headerdata, s.dng_headerlength);

        // init exiftool interface
        s.exiftool = new ExifTool();
        // also need to delete exiftool;

        // init magick?
        MagickCoreGenesis(NULL,MagickFalse);

        s.job_queue = async(launch::async, []{ });

     } 


 private:

    // assume string input is in json format 
    void json_to_attributes(string jstr) {

        // TODO: fix bug
        jstr = trim(jstr);
        //LOGV << "trim = " << jstr ;
        json jin = json::parse(jstr);

        src =               jin["src"];
        dst =               jin["dst"];
        header =            jin["header"];
        basename =          jin["basename"];
        datestamp =         jin["datestamp"];
        system_clock_ns =   jin["system_clock_ns"];
        steady_clock_ns =   jin["steady_clock_ns"];
        gain =              jin["gain"];
        expo =              jin["expo"];
        sweep_index =       jin["sweep_index"];
        sweep_total =       jin["sweep_total"];
        fps =               jin["fps"];
        delta_ms =          jin["delta_ms"];
        bpp  =              jin["bpp"];
        width  =            jin["width"];
        height  =           jin["height"];
        bytesperline  =     jin["bytesperline"];
        datalength  =       jin["datalength"];
        comment =           jin["comment"];
        text_north =        jin["text_north"];
        text_east =         jin["text_east"];
        command =           jin["command"];

    
    }

    // from https://stackoverflow.com/questions/1343890/how-do-i-restrict-a-float-value-to-only-two-places-after-the-decimal-point-in-c
    float prd(const double x, const int decDigits) {
        stringstream ss;
        ss << fixed;
        ss.precision(decDigits); // set # places after decimal
        ss << x;
        return stoi(ss.str());
    }

    string attributes_to_json(int pad) {

        json jout;

        jout["info"]["basename"]=           basename;
        jout["info"]["src"]=                src;
        jout["info"]["dst"]=                dst;
        jout["info"]["header"]=             header;
        jout["info"]["command"]=            command;
        jout["info"]["comment"]=            comment;
        jout["info"]["text_north"]=         text_north;
        jout["info"]["text_east"]=          text_east;

        jout["time"]["datestamp"]=          datestamp;
        jout["time"]["system_clock_ns"]=    system_clock_ns;
        jout["time"]["steady_clock_ns"]=    steady_clock_ns;
        jout["time"]["fps"]=                prd(fps,0);
        jout["time"]["delta_ms"]=           prd(delta_ms,0);

        jout["camera"]["gain"]=             gain;
        jout["camera"]["expo"]=             expo;
        jout["camera"]["sweep_index"]=      sweep_index;
        jout["camera"]["sweep_total"]=      sweep_total;

        jout["image"]["bpp"]=               bpp ;
        jout["image"]["width"]=             width ;
        jout["image"]["height"]=            height ;
        jout["image"]["bytesperline"]=      bytesperline ;
        jout["image"]["datalength"]=        datalength ;


        return jout.dump(pad);
    }


/*
    static void runObjFunction(Imagedata *imgobj, fs::path file_dng, fs::path file_jpg){
        LOGV << "start runObjFunction";
        imgobj->writeDngAndJpg(file_dng, file_jpg, false);
    }
*/

    static void runThreadedCommand(string command) {
        string result = exec(command);
        if (result.length()) {
            LOGV << "result from running cmd=" << command ;
            LOGV << result;
        }
        //fs::copy(file_jpg, fp.file_current_jpg, fs::copy_options::overwrite_existing);
    }

    // https://stackoverflow.com/questions/478898/how-do-i-execute-a-command-and-get-the-output-of-the-command-     ↪ within-c-using-po
    static string exec(string cmd) {

        array<char, 128> buffer;
        string result;
        unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
        if (!pipe) {
            throw runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            //LOGV << buffer.data();
            result += buffer.data();
        }
        // remove blank lines from result
        // https://stackoverflow.com/questions/22395333/removing-empty-lines-from-a-string-c
        result.erase(unique(result.begin(), result.end(),
                      [] (char a, char b) { return a == '\n' && b == '\n'; }),
                  result.end());
        return result;
    }

    // romove junk like _ and space characters until first '{' from front and '}' from back
    static string trim(const std::string &s) {
        //cout << "start = " << s << endl;
        auto start = s.begin();
        if (*start != '{') 
            while (start != s.end() and *start != '{') 
                start++;

        auto end = s.end();
        if (*end != '}') 
            while (end != start and *end != '}') 
                end--;

        string result = string(start, end + 1);

        // now make sure there are no blank "" -- crashes json
        regex patt ("\"src\"");
        result = regex_replace (result,patt,"\"dst\":\"\",\"src\"");

        return result;
    }

    static bool matchHelper(const vector<char>& data, const vector<char>& patt, int dataindex) {                                                                            
      int match_count = 0;
      int match_count_total = patt.size();
   
      for (int i = 0; i < patt.size(); i++) {
          int offset = i + dataindex;
          if (patt[i] != data[offset]) {
                  return false;
          }
        match_count++;
        //cout << "match at d[" << offset << "] = " << data[offset] << " " <<  " p[" << i << "] =" << patt[i] << " count = " << match_count << endl;
          if (match_count == match_count_total) {
              return true;
          }
      }
  
  
  }
  


    // stupid c++ regex can't deal with binary data...so a 1 liner in python/perl becomes:
    static int findStartOfSetting (char *headerdata, int headerlength) {

      string str(900,'_'); // look for  ______ 1024 long line

      vector<char> data(headerdata,headerdata + headerlength);
      vector<char> patt(str.begin(), str.end());
      LOGV << "looking in dng header for " << str ;

      for (int i = 0; i < data.size(); i++) {
          if (matchHelper(data, patt, i)) {
                  return i;
          }
      }
      LOGE << "pattern not found in headerfile :" << str;
      exit(-1);
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

        if (false)
            return;

        minMaxLoc(M, &min, &max);
        int max_length = 30;
        string spacer = "";
        if (name.length() < max_length) {
            spacer = string(max_length - name.length(), ' ');
        }

        LOGV << " Mat=" << name << spacer << " " << getMatType(M) << " : " << M.size() << " x " << M.channels() <<  " (min,max) = "  << Point(min,max) ;
        //LOGV << " Mat=" << M;
        if (true) {
            string file = "images/" + name + ".jpg";
            imwrite(file, M);
        }
    }



    char* crop_wb_cc(char* indata, bool wb_and_cc) {

        // one line to create opencv image from foreign data
        Mat mat_crop(height, width, CV_16U, indata, bytesperline);
        printMatStats("mat_crop", mat_crop);
        LOGV << "copy = " << mat_crop.size() << " cont = " << mat_crop.isContinuous() ;

        Mat mat_out(height, width, CV_16UC1);
        mat_crop.copyTo(mat_out);
        printMatStats("mat_out", mat_out);


        char* res = new char [datalength];
        memcpy(res, mat_out.data, datalength);
        return res;

      }

     char* crop_direct(char* old_data) {
      // reduce from 768*2 = 1536 bytes per line to 728*2 = 1456
      // throwing away away excess stride
         //reshape the size of data to get rid of band if needed ... assume width = 768 or 728

        int old_width = 768; // or take from current width var?
        int old_bytesperline = old_width * pixel_depth;
        int old_datalength = old_bytesperline * height;

        int new_width = 728;
        int new_bytesperline = new_width * pixel_depth;
        int new_datalength = new_bytesperline * height;

          vector<char> indata (old_data, old_data + old_datalength); // fill vec with data
          vector<char> outdata(old_data, old_data + new_datalength);
          for (int x = 0; x < old_bytesperline ; x++) {
              for (int y=0;y < height ; y++) {
                  if (x < new_bytesperline) {
                      int old_pixel = x + y * old_bytesperline;
                      int new_pixel = x + y * new_bytesperline;
                      outdata.at(new_pixel) = indata.at(old_pixel);
                  }
              }
          }

          char* res = new char [new_datalength];
          memcpy(res, &outdata[0], new_datalength);
          return res;

      }

    #include<cmath>
    float colorCorrectPixel(string RGB, float r, float g, float b) {

    /*
        const float m[3][7] = {
            {2.0420 , 1.3335 , -0.2554 , 2.5708,  0.3313 , -2.4732},
            {0.4303 , 3.1096 ,  0.1063 ,-0.9487,  1.7227 , -0.1972},
            {2.6054 , 0.5803 ,  6.3472 ,-0.8337, -1.5363 ,  5.3990},
         };

    // works well--based on snappy-flat
    const float m[3][7] = { 
     { 3.4328 , -1.8594 , -1.0440 , -1.5200 , 1.6588 , 0.3699 , -0.0085 },
     { -0.8496 , 1.2262 , -0.5005 , 0.0360 , 0.8066 , 0.1036 , 0.1664 },
     { -0.0066 , -1.1836 , 2.2361 , 0.0992 , 0.0745 , 1.1667 , -0.1906 }
    };               
    */

    const float m[3][7] = { 
     {  3.3420 , -0.8352 , -2.4423 , -1.4195 ,  0.8897 , 1.6165 ,  0.0678 },
     { -1.4032 ,  2.6204 , -1.6586 ,  0.4133 , -0.2037 , 1.1936 ,  0.2043 },
     { -0.0363 , -0.2740 ,  1.2202 ,  0.0385 , -0.6503 , 2.2812 , -0.2017 }
    };               


    // TODO: instead of loop do in one step in opencv
    // Remap to float [0, 1], perform gamma correction
    //float fGamma = 2.2f;
    //imgDeBayer.convertTo( imgDeBayer, CV_32FC3, 1. / 4096);//4096-> 2^12 ( 12-Bit)
    //cv::pow( imgDeBayer, fGamma, imgDeBayer );

        const float gamma = 2.2;
        float rg = pow(r, (1 / gamma));
        float gg = pow(g, (1 / gamma));
        float bg = pow(b, (1 / gamma));

        float out;
        if (RGB == "R") {
            out = m[0][0] * rg + m[0][1] * gg + m[0][2] * bg + 
                  m[0][3] * r   + m[0][4] * g   + m[0][5] * b   +
                  m[0][6];
        }
        if (RGB == "G") {
            out = m[1][0] * rg + m[1][1] * gg + m[1][2] * bg + 
                  m[1][3] * r   + m[1][4] * g   + m[1][5] * b   +
                  m[1][6];
        }
        if (RGB == "B") {
            out = m[2][0] * rg + m[2][1] * gg + m[2][2] * bg + 
                  m[2][3] * r   + m[2][4] * g   + m[2][5] * b   +
                  m[2][6];
        }

        // gamma correct

        return out;
    }

    // use ccm to "correct" colors
    Mat correctImageColors (Mat mat_in_int) {

        // multiplication needs floating
        Mat mat_out_float = Mat(height, width, CV_32F);
        mat_in_int.convertTo(mat_out_float, CV_32F, 1.0 / 65535);
        printMatStats("mat_out_float_initial",   mat_out_float);

        // loop for now..
        // using https://stackoverflow.com/questions/7899108/opencv-get-pixel-channel-value-from-mat-image
        int cn = mat_out_float.channels();
        float ro,go,bo;
        float rn,gn,bn;

        for(int i = 0; i < mat_out_float.rows; i++) {
            float* rowPtr = mat_out_float.ptr<float>(i);
            for(int j = 0; j < mat_out_float.cols; j++) {

                // get
                bo = rowPtr[j*cn + 0]; // B
                go = rowPtr[j*cn + 1]; // G
                ro = rowPtr[j*cn + 2]; // R

                rn = colorCorrectPixel("R",ro,go,bo);
                gn = colorCorrectPixel("G",ro,go,bo);
                bn = colorCorrectPixel("B",ro,go,bo);

                // put
                rowPtr[j*cn + 0] = bn ; // B
                rowPtr[j*cn + 1] = gn ; // G
                rowPtr[j*cn + 2] = rn ; // R

            }
        }

        normalize(mat_out_float, mat_out_float, 1, 0, NORM_MINMAX);
        printMatStats("mat_out_float_final",   mat_out_float);
        Mat mat_out_int = Mat(height, width, CV_8U);
        mat_out_float.convertTo(mat_out_int, CV_32F, 255);
        return mat_out_int;
    }


    void runMagick(string cmd) {
        ImageInfo *image_info = AcquireImageInfo();
        ExceptionInfo *exception = AcquireExceptionInfo();

        auto parts = boost::program_options::split_unix(cmd);
        std::vector<char*> cstrings ;
        for(auto& str : parts){
            cstrings.push_back(const_cast<char*> (str.c_str()));
        }

        int argc = (int)cstrings.size();
        char** argv = cstrings.data();

        //LOGV << "cmd = " << cmd;
        //copy( argv, argv+argc, ostream_iterator<const char*>( cout, " " ) ) ;
        //cout << "\n";

        LOGV << "cmd = " << cmd;
        (void) MagickImageCommand(image_info, argc, argv, NULL, exception);

       if (exception->severity != UndefinedException) {
            CatchException(exception);
            LOGE << "magick error";
        }

        image_info=DestroyImageInfo(image_info);
        exception=DestroyExceptionInfo(exception);
    }


}; /* end Class */


struct Imagedata::Setup Imagedata::s;


