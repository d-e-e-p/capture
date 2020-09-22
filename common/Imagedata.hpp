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
    string lens = "unknown";
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
        string clahe = " -clahe 25x25%+128+2 ";

        if (label) {
            // scale factor determined by experiments
            string pointsize_north = " -pointsize " + to_string( (float) 2120 / (float) text_north.length() - 2 );
            string pointsize_east  = " -pointsize " + to_string( (float) 2000 / (float) text_east.length()  - 2 );
            // 728 x 544 -> 768 x 584
            //cmd = "/usr/local/bin/magick  " + src + " -clahe 25x25%+128+2 -quality 100 -gravity Southwest -background black -extent 768x584 -font Inconsolata -pointsize " + pointsize_north + " -fill yellow -gravity North -annotate 0x0+0+15 \"" + text_north + "\" -gravity East -pointsize " + pointsize_east + " -annotate 90x90+20+232 \"" + text_east + "\" -quality 100 " + dst;
            string extent = " -extent " + to_string(width + 34) + "x" + to_string(height + 62);
            cmd = "/usr/local/bin/magick  " + src + clahe + " -quality 100 -gravity Northwest -background black " + extent + " -font Inconsolata " + pointsize_north + " -fill yellow -gravity South -annotate 0x0+0+15 \"" + text_north + "\" -gravity East " + pointsize_east + " -annotate 90x90+20+400 \"" + text_east + "\" -quality 100 " + dst;
        } else {
            cmd = "magick  " + src + clahe + " -quality 100 " + dst;
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

    /*
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
    */

    // assume input data is in raw format
    int writeJson(fs::path jsonname) {
        string imageinfo = attributes_to_json(4);
        LOGV << "writing to : " << jsonname.string() ;
        //fs::remove(dngname);
        ofstream outfile (jsonname);
        outfile << imageinfo;
        outfile.close();
    }

    int readJson(fs::path jsonname) {
        LOGV << "reading from  : " << jsonname.string() ;
        ifstream infile(jsonname);
        stringstream strStream;
        strStream << infile.rdbuf(); 
        string jstr = strStream.str(); 
        infile.close();
        json_to_attributes(jstr);
    }


	string getLensGainExpo() {
	
	
	    // eg, from :
	    //    string lens = "L66892";
	    //    int gain = 0;
	    //    int expo = 750;
	    // return :
	    //      L66892_G000E0750
	
	    char buff[BUFSIZ];
	    snprintf(buff, sizeof(buff), "%s_G%03dE%04d", lens.c_str(), gain, expo);
	    string lens_gain_expo = buff;
	
	    return lens_gain_expo;
	        
	}
	
	// see https://stackoverflow.com/questions/13172158/c-split-string-by-line/48837366
	vector<string> split_string(const string& str, const string& delimiter) {
	    vector<string> strings;
	 
	    string::size_type pos = 0;
	    string::size_type prev = 0;
	    while ((pos = str.find(delimiter, prev)) != string::npos)
	    {
	        strings.push_back(str.substr(prev, pos - prev));
	        prev = pos + 1;
	    }
	 
	    // To get the last substring (or only, if delimiter is not found)
	    strings.push_back(str.substr(prev));
	 
	    return strings;
	}
	 
	
	void define_ccm( float (*colorMatrix)[9], float (*asShotNeutral)[3], float *blacklevel, long *whitelevel) {
	
	
	    string res =  R"STR(
	img_L66892_G000E0500/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5011979284197485 1 0.5407604376894778\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.2958177228228411 -0.5332626919964376 -0.1351604723718533 -0.02289028335453374 0.6595808399945965 0.08178592999915879 0.13552872891520645 -0.011773095419839754 0.8323192361736788\" -ColorMatrix2=\"1.2958177228228411 -0.5332626919964376 -0.1351604723718533 -0.02289028335453374 0.6595808399945965 0.08178592999915879 0.13552872891520645 -0.011773095419839754 0.8323192361736788\" -IFD0:BlackLevel=3887 -IFD0:WhiteLevel=41982 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3887 -SubIFD:WhiteLevel=41982  -o results/img_L66892_G000E0500/corrected.dng  inputs/images/img_L66892_G000E0500.dng",
	img_L66892_G000E0750/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5036122862855583 1 0.4746498693995717\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.5221647274263255 -0.5991518843915478 -0.18663442804069663 0.014646276285915253 0.8655161464242636 0.0640009457298411 0.2816238741133832 0.11374075233421263 1.1589449407522703\" -ColorMatrix2=\"1.5221647274263255 -0.5991518843915478 -0.18663442804069663 0.014646276285915253 0.8655161464242636 0.0640009457298411 0.2816238741133832 0.11374075233421263 1.1589449407522703\" -IFD0:BlackLevel=3904 -IFD0:WhiteLevel=65535 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3904 -SubIFD:WhiteLevel=65535  -o results/img_L66892_G000E0750/corrected.dng  inputs/images/img_L66892_G000E0750.dng",
	img_L66892_G010E0500/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5022584650803694 1 0.5490771623896834\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.3072384905033592 -0.4924552198198806 -0.15411938488212162 0.01622567678222471 0.6487025215576826 0.044743595907065975 0.14838784525090634 0.1084873898672322 0.7614764142898587\" -ColorMatrix2=\"1.3072384905033592 -0.4924552198198806 -0.15411938488212162 0.01622567678222471 0.6487025215576826 0.044743595907065975 0.14838784525090634 0.1084873898672322 0.7614764142898587\" -IFD0:BlackLevel=3864 -IFD0:WhiteLevel=48309 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3864 -SubIFD:WhiteLevel=48309  -o results/img_L66892_G010E0500/corrected.dng  inputs/images/img_L66892_G010E0500.dng",
	img_L66892_G010E0750/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5199995994247161 1 0.41967757911589837\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"0.9462332331412311 -0.42187910095723086 -0.0761388482247373 0.048947855763703925 0.8682793053325203 0.1405836996154995 0.26845496139334357 0.08959554754249749 1.4424358673940543\" -ColorMatrix2=\"0.9462332331412311 -0.42187910095723086 -0.0761388482247373 0.048947855763703925 0.8682793053325203 0.1405836996154995 0.26845496139334357 0.08959554754249749 1.4424358673940543\" -IFD0:BlackLevel=3740 -IFD0:WhiteLevel=65535 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3740 -SubIFD:WhiteLevel=65535  -o results/img_L66892_G010E0750/corrected.dng  inputs/images/img_L66892_G010E0750.dng",
	img_L66892_G010E1000/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.49125349480840586 1 0.5414605072377565\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1713793168848952 -0.4729608728669792 -0.165124582727417 0.1496465348112792 0.8927539329924519 0.05511477480922973 0.1668742664960823 0.03676630277547498 1.0430034307481963\" -ColorMatrix2=\"1.1713793168848952 -0.4729608728669792 -0.165124582727417 0.1496465348112792 0.8927539329924519 0.05511477480922973 0.1668742664960823 0.03676630277547498 1.0430034307481963\" -IFD0:BlackLevel=4233 -IFD0:WhiteLevel=65535 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=4233 -SubIFD:WhiteLevel=65535  -o results/img_L66892_G010E1000/corrected.dng  inputs/images/img_L66892_G010E1000.dng",
	img_L66892_G025E0500/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5038893363044865 1 0.5395972524409923\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.2164301088543605 -0.4831815001194897 -0.1542815440395757 0.047799415048431826 0.8769597885667084 0.05656477145713695 0.18196802287471409 0.03471622213214183 0.8361537470041409\" -ColorMatrix2=\"1.2164301088543605 -0.4831815001194897 -0.1542815440395757 0.047799415048431826 0.8769597885667084 0.05656477145713695 0.18196802287471409 0.03471622213214183 0.8361537470041409\" -IFD0:BlackLevel=3961 -IFD0:WhiteLevel=54277 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3961 -SubIFD:WhiteLevel=54277  -o results/img_L66892_G025E0500/corrected.dng  inputs/images/img_L66892_G025E0500.dng",
	img_L66892_G025E0750/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.49392369826711213 1 0.5366382315730087\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.2804800009050061 -0.4687383883650139 -0.17444136546747202 0.15531027594304297 0.840080800638925 0.07617530241038767 0.16384805474778702 0.032498533185912414 0.9190173350224982\" -ColorMatrix2=\"1.2804800009050061 -0.4687383883650139 -0.17444136546747202 0.15531027594304297 0.840080800638925 0.07617530241038767 0.16384805474778702 0.032498533185912414 0.9190173350224982\" -IFD0:BlackLevel=4196 -IFD0:WhiteLevel=65535 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=4196 -SubIFD:WhiteLevel=65535  -o results/img_L66892_G025E0750/corrected.dng  inputs/images/img_L66892_G025E0750.dng",
	img_L66892_G025E1000/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.49786740324351164 1 0.5253682041507989\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.2864062820751776 -0.6178964937469533 -0.1392740069677594 0.43301500170025997 0.9617494104362232 0.1532783159236128 0.16587699867296601 -0.059164801578771894 0.9402011891474028\" -ColorMatrix2=\"1.2864062820751776 -0.6178964937469533 -0.1392740069677594 0.43301500170025997 0.9617494104362232 0.1532783159236128 0.16587699867296601 -0.059164801578771894 0.9402011891474028\" -IFD0:BlackLevel=4203 -IFD0:WhiteLevel=65535 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=4203 -SubIFD:WhiteLevel=65535  -o results/img_L66892_G025E1000/corrected.dng  inputs/images/img_L66892_G025E1000.dng",
	img_L69262_G000E1000/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5297454254192975 1 0.5237138213011162\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1172564735844537 -0.4915336329213369 -0.14229156540987842 0.14069816487650372 0.8365620910800411 0.05529019391607774 0.16332302283849298 0.03426562894541191 0.8481173198511673\" -ColorMatrix2=\"1.1172564735844537 -0.4915336329213369 -0.14229156540987842 0.14069816487650372 0.8365620910800411 0.05529019391607774 0.16332302283849298 0.03426562894541191 0.8481173198511673\" -IFD0:BlackLevel=3854 -IFD0:WhiteLevel=32804 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3854 -SubIFD:WhiteLevel=32804  -o results/img_L69262_G000E1000/corrected.dng  inputs/images/img_L69262_G000E1000.dng",
	img_L69262_G025E0750/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5333930916368275 1 0.5254899782591469\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1095049828979313 -0.4823846125961201 -0.13780252288475095 0.14851156627852577 0.8266083253381107 0.05537088080805124 0.16156153086843753 0.034414507384565036 0.825454790484315\" -ColorMatrix2=\"1.1095049828979313 -0.4823846125961201 -0.13780252288475095 0.14851156627852577 0.8266083253381107 0.05537088080805124 0.16156153086843753 0.034414507384565036 0.825454790484315\" -IFD0:BlackLevel=3833 -IFD0:WhiteLevel=30789 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3833 -SubIFD:WhiteLevel=30789  -o results/img_L69262_G025E0750/corrected.dng  inputs/images/img_L69262_G025E0750.dng",
	img_L69262_G025E1000/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5343713313079377 1 0.5280081030131426\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.148009553177839 -0.47578116445103735 -0.1416571178874706 -0.0013003504869671945 0.6904853547583646 0.04593064554548883 0.1347847621268289 0.07211459200373635 0.9012647125574795\" -ColorMatrix2=\"1.148009553177839 -0.47578116445103735 -0.1416571178874706 -0.0013003504869671945 0.6904853547583646 0.04593064554548883 0.1347847621268289 0.07211459200373635 0.9012647125574795\" -IFD0:BlackLevel=3835 -IFD0:WhiteLevel=40506 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3835 -SubIFD:WhiteLevel=40506  -o results/img_L69262_G025E1000/corrected.dng  inputs/images/img_L69262_G025E1000.dng",
	img_L69262_G050E0500/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5239088539494565 1 0.5216607118223326\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.0875726721547596 -0.45527572087781865 -0.14018257465686415 0.1394470897799676 0.8337647835673403 0.05537443095378753 0.16419264288626187 0.03452512256007418 0.8237951617842922\" -ColorMatrix2=\"1.0875726721547596 -0.45527572087781865 -0.14018257465686415 0.1394470897799676 0.8337647835673403 0.05537443095378753 0.16419264288626187 0.03452512256007418 0.8237951617842922\" -IFD0:BlackLevel=3933 -IFD0:WhiteLevel=26441 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3933 -SubIFD:WhiteLevel=26441  -o results/img_L69262_G050E0500/corrected.dng  inputs/images/img_L69262_G050E0500.dng",
	img_L69262_G050E0750/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5305403726875716 1 0.5199054575672025\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1272116994219872 -0.48108497229079605 -0.1395498153615613 0.10023878970855149 0.8506553706262815 0.0750094490948341 0.1570955922574256 0.014898494054985861 0.7576277569779336\" -ColorMatrix2=\"1.1272116994219872 -0.48108497229079605 -0.1395498153615613 0.10023878970855149 0.8506553706262815 0.0750094490948341 0.1570955922574256 0.014898494054985861 0.7576277569779336\" -IFD0:BlackLevel=3904 -IFD0:WhiteLevel=39992 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3904 -SubIFD:WhiteLevel=39992  -o results/img_L69262_G050E0750/corrected.dng  inputs/images/img_L69262_G050E0750.dng",
	img_L69262_G050E1000/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5361238091616716 1 0.5264241280721809\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.2500806592420843 -0.5338592618004717 -0.1466470277093814 0.09336602040862428 0.8155884855311438 0.09316507336475904 0.17155643564558815 0.03839063611884328 0.8479538818219754\" -ColorMatrix2=\"1.2500806592420843 -0.5338592618004717 -0.1466470277093814 0.09336602040862428 0.8155884855311438 0.09316507336475904 0.17155643564558815 0.03839063611884328 0.8479538818219754\" -IFD0:BlackLevel=3891 -IFD0:WhiteLevel=56926 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3891 -SubIFD:WhiteLevel=56926  -o results/img_L69262_G050E1000/corrected.dng  inputs/images/img_L69262_G050E1000.dng",
	img_L69262_G075E0500/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5338674100242935 1 0.5266023127910471\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1329890615738374 -0.4881732658829427 -0.14196922554867467 0.12775718833055238 0.8431271681911314 0.08440771313404193 0.1465669651024354 0.033960732243553705 0.857507580048626\" -ColorMatrix2=\"1.1329890615738374 -0.4881732658829427 -0.14196922554867467 0.12775718833055238 0.8431271681911314 0.08440771313404193 0.1465669651024354 0.033960732243553705 0.857507580048626\" -IFD0:BlackLevel=3908 -IFD0:WhiteLevel=36438 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3908 -SubIFD:WhiteLevel=36438  -o results/img_L69262_G075E0500/corrected.dng  inputs/images/img_L69262_G075E0500.dng",
	img_L69262_G075E0750/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5444208523700148 1 0.52669439528777\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1643977886892405 -0.4826074249827946 -0.14283419648934859 0.14663792142752274 0.8702174590603671 0.05608020435417549 0.20091584624504055 0.03643521907080845 0.884627609334895\" -ColorMatrix2=\"1.1643977886892405 -0.4826074249827946 -0.14283419648934859 0.14663792142752274 0.8702174590603671 0.05608020435417549 0.20091584624504055 0.03643521907080845 0.884627609334895\" -IFD0:BlackLevel=3818 -IFD0:WhiteLevel=52617 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3818 -SubIFD:WhiteLevel=52617  -o results/img_L69262_G075E0750/corrected.dng  inputs/images/img_L69262_G075E0750.dng",
	img_L69262_G100E0500/dng_wb_ccm_bl.json:            "exifcmd": "exiftool -AsShotNeutral=\"0.5374788680995894 1 0.5297571374157877\"   -ForwardMatrix1= -ForwardMatrix2= -ColorMatrix1=\"1.1760576085015804 -0.4401699570650032 -0.15740251438469904 0.10610808307308084 0.8398551338185003 0.04719755548169963 0.186306874938527 0.18147555453893865 0.8248486750420352\" -ColorMatrix2=\"1.1760576085015804 -0.4401699570650032 -0.15740251438469904 0.10610808307308084 0.8398551338185003 0.04719755548169963 0.186306874938527 0.18147555453893865 0.8248486750420352\" -IFD0:BlackLevel=3846 -IFD0:WhiteLevel=47494 -SubIFD:BlackLevelRepeatDim= -SubIFD:BlackLevel=3846 -SubIFD:WhiteLevel=47494  -o results/img_L69262_G100E0500/corrected.dng  inputs/images/img_L69262_G100E0500.dng",
	)STR";
	    //cout << " res = " << res;
	
	    // eg string lens_gain_expo = "_L69262_G100E0500";
	    string lens_gain_expo = getLensGainExpo();

	    auto lines = split_string(res, "\n");
	    string match_bl;
	    int i = 1;
	    for (auto itr = lines.begin(); itr != lines.end(); itr++) {
	        string line = *itr;
	        //cout << "line: " << i++ << " - \"" << line << "\"\n";
	        regex re;
	        smatch match;
	
	        re = "img_" + lens_gain_expo; 
	
	        regex_search(line, match, re);
	        LOGV << "looking for : " << "img_" + lens_gain_expo << " match.size= " << match.size() ;
	        //LOGV << "line = " << line;

	        if (match.size() > 0) {
	            LOGV << "FOUND";
	            re = "BlackLevel=([0-9]*)";
	            if (regex_search(line, match, re) && match.size() > 1) {
	                *blacklevel = stof(match.str(1));
	            } else {
	                LOGE << "NO MATCH BL in: " << line;
	            }
	
	            re = "WhiteLevel=([0-9]*)";
	            if (regex_search(line, match, re) && match.size() > 1) {
	                *whitelevel = stol(match.str(1));
	            } else {
	                LOGE << "NO MATCH WL in: " << line;
	            }
	
	            re = "AsShotNeutral=..([[:digit:].[:space:]]*)";
	            if (regex_search(line, match, re) && match.size() > 1) {
	                string match_str = match.str(1);
	                //cout << "asshotneutral = " << match_str << endl;
	                auto nums = split_string(match_str, " ");
	                int j = 0;
	                for (auto itr2 = nums.begin(); itr2 != nums.end(); itr2++) {
	                    string match_value = *itr2;
	                    float value = stof(match_value);
	                    (*asShotNeutral)[j++] = value;
	                    //cout << "asShotNeutral "<< j << "  = " << value << endl;
	                }
	            } else {
	                LOGE << "NO MATCH WB in: " << line;
	            }
	
	            re = "ColorMatrix1=..([[:digit:]-.[:space:]]*)";
	            if (regex_search(line, match, re) && match.size() > 1) {
	                string match_str = match.str(1);
	                //cout << "ccm = " << match_str << endl;
	                auto nums = split_string(match_str, " ");
	                int j = 0;
	                for (auto itr2 = nums.begin(); itr2 != nums.end(); itr2++) {
	                    string match_value = *itr2;
	                    float value = stof(match_value);
	                    (*colorMatrix)[j++] = value;
	                    //cout << "colorMatrix "<< j << "  = " << value << endl;
	                }
	            } else {
	                LOGE << "NO MATCH CCM in: " << line;
	            }
	
	
	
	            return;
	       }
	
	    }

        LOGE << "no match in lens/gain/expo -> ccm table for key  = " << lens_gain_expo;
	    // no match!
	
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

   unsigned int pattern = CFA_RGGB;                                                                                                          
   static const short bayerPatternDimensions[] = { 2, 2 };


   /* Default ColorMatrix1, when none provided */
   static const float default_color_matrix1[] = {
        2.005, -0.771, -0.269,
       -0.752,  1.688,  0.064,
       -0.149,  0.283,  0.745
   };


   float blacklevel = 0;
   long  whitelevel = 65535;
   float asShotNeutral[] = {1,1,1};
   float colorMatrix[]   = {1,1,1, 1,1,1, 1,1,1};

   define_ccm( &colorMatrix, &asShotNeutral, &blacklevel, &whitelevel);

   LOGV << "lens = " << lens  << " gain = " << gain << " expo = " << expo ;
   LOGV << "blacklevel, whitelevel = " << blacklevel << " , " << whitelevel;


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
    TIFFSetField (tif, TIFFTAG_CFAPATTERN, cfa_patterns[pattern]);
    // needs to have the "4" depending on definition in ./libtiff/tif_dirinfo.c
    //TIFFSetField(tif, TIFFTAG_CFAPATTERN, 4, cfa_patterns[pattern]);
    //

   //TIFFSetField (tif, TIFFTAG_CFAPATTERN, "\00\01\01\02");
   //TIFFSetField(tif, TIFFTAG_CFAREPEATPATTERNDIM, "\02\02");

    TIFFSetField (tif, TIFFTAG_MAKE, "__SNAPPY__");
    TIFFSetField (tif, TIFFTAG_MODEL, "__WEED__");
    TIFFSetField (tif, TIFFTAG_UNIQUECAMERAMODEL, imageinfo.c_str());
    TIFFSetField (tif, TIFFTAG_COLORMATRIX1, 9, colorMatrix);
    TIFFSetField (tif, TIFFTAG_COLORMATRIX2, 9, colorMatrix);
    TIFFSetField (tif, TIFFTAG_ASSHOTNEUTRAL, 3, asShotNeutral);
    TIFFSetField (tif, TIFFTAG_CFALAYOUT, 1);
    TIFFSetField (tif, TIFFTAG_CFAPLANECOLOR, 3, "\00\01\02");

    TIFFSetField (tif, TIFFTAG_BLACKLEVEL, 1, &blacklevel);
    TIFFSetField (tif, TIFFTAG_WHITELEVEL, 1, &whitelevel);

    // store image
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

    /*

    // assume input data is in raw format
    int writeDngV2(fs::path dngname, bool wb_and_cc = false) {

        Mat mat_crop(height, width, CV_16U, data, bytesperline);
        printMatStats("mat_crop", mat_crop);

        vector<int> params;
        params.push_back(IMWRITE_TIFF_COMPRESSION);
        params.push_back(1); // no compression
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

    */


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

    Mat scaleRaw(Mat mat_in, Mat mat_scale) {

        Mat mat_out(height, width, CV_16UC1);
        const char color_table[2][2] = {
           { 'b' , 'g'} ,
           { 'g' , 'r'} ,
        };
        
        int cn = mat_in.channels();
        float rp,gp,bp;
        for(int i = 0; i < mat_in.rows; i++) {
            float* rowPtr = mat_in.ptr<float>(i);
            for(int j = 0; j < mat_in.cols; j++) {

                // get
                bp = rowPtr[j*cn + 0]; // B
                gp = rowPtr[j*cn + 1]; // G
                rp = rowPtr[j*cn + 2]; // R

                char color = color_table[i%2][j%2];
                LOGV << "i = " << i << " j = " << j << " color = " << color;
                int mp; 
                switch ( color ) {
                    case 'r':  mp = rp; break;
                    case 'g':  mp = gp; break;
                    case 'b':  mp = bp; break;
                }

                mat_out.at<uchar>(j,i) = mp; 

            }
        }
        
        return mat_out;

    }


    Mat correctRawImage(Mat mat_crop) {
         
        LOGV << "mat_crop = " << mat_crop.size() << " cont = " << mat_crop.isContinuous() ;
        
        Mat mat_bayer1(height, width, CV_16U);
        demosaicing(mat_crop, mat_bayer1, COLOR_BayerRG2RGB);
        printMatStats("mat_bayer1", mat_bayer1);

        //Mat mat_out(height, new_width, CV_16UC1);
        Mat mat_out = correctImageColors(mat_bayer1);
        printMatStats("mat_out", mat_out);

        mat_out = scaleRaw(mat_crop, mat_out);

        return mat_out;

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
        LOGV << "trim = " << jstr ;
        json jin = json::parse(jstr);
        LOGV << "done parsing json";

        // add info/comment key if it exists
        if (jin.find("info") != jin.end()) {
           if (jin["info"].find("comment") != jin["info"].end()) {
                comment = jin["info"]["comment"] ;
           }
        }

        // add camera/lens key if it exists
        if (jin.find("camera") != jin.end()) {
           if (jin["camera"].find("lens") != jin["camera"].end()) {
                lens = jin["camera"]["lens"] ;
           } 
        }

        // return if json doesn't have time key
        if (jin.find("time") == jin.end()) {
            return;
        }

        // TODO: test all keys for existence before adding

        basename =          jin["info"]["basename"] ;
        src =               jin["info"]["src"] ;
        dst =               jin["info"]["dst"] ;
        header =            jin["info"]["header"] ;
        command =           jin["info"]["command"] ;
        text_north =        jin["info"]["text_north"] ;
        text_east =         jin["info"]["text_east"] ;

        datestamp =         jin["time"]["datestamp"] ;
        system_clock_ns =   jin["time"]["system_clock_ns"] ;
        steady_clock_ns =   jin["time"]["steady_clock_ns"] ;
        fps =               jin["time"]["fps"] ;
        delta_ms =          jin["time"]["delta_ms"] ;

        gain =              jin["camera"]["gain"] ;
        expo =              jin["camera"]["expo"] ;
        sweep_index =       jin["camera"]["sweep_index"] ;
        sweep_total =       jin["camera"]["sweep_total"] ;

        bpp  =              jin["image"]["bpp"] ;
        width  =            jin["image"]["width"] ;
        height  =           jin["image"]["height"] ;
        bytesperline  =     jin["image"]["bytesperline"] ;
        datalength  =       jin["image"]["datalength"] ;

    
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

        jout["camera"]["lens"]=             lens;
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


