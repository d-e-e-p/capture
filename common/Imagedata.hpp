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



using json = nlohmann::json;
using namespace std;

class Imagedata{
 public:
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

    int bpp    = 16;
    int width  = 728;
    int height = 544;
    int size   = width * height * bpp / CHAR_BIT;

    string comment;
    string text_north;
    string text_east;

    string command;

    //Destructor
    ~Imagedata(){
        delete data;
    }

    //Constructor
    Imagedata() {
    }

    // assume string input is in json format 
    void json_to_attributes(string jstr) {

        jstr = trim(jstr);
        //cout << "trim = " << jstr << endl;
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
        size  =             jin["size"];
        comment =           jin["comment"];
        text_north =        jin["text_north"];
        text_east =         jin["text_east"];
        command =           jin["command"];
    
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
        width = 544;
        height = 728;
        size = 835584;

        comment="run with lower camera";
        text_north= "";
        text_east= "";
        command="../bin/capture --frames 100000 --gain 100 --exposure 1500 --comment \"run with lower camera\"";

    }

    // use side file to construct data
    void sidefile_to_attributes(fs::path filename, map<string,string> &attr) {
        basename = filename.stem().string();
        if (attr.find(basename) == attr.end() ) {
            LOGE << "No map entry for : " << basename;
        }
        string jstr = attr[basename];
        json_to_attributes(jstr);
        src = filename;
    }


    string attributes_to_json() {

        json jout;

        jout["src"]=                src;
        jout["header"]=             header;
        jout["basename"]=           basename;
        jout["datestamp"]=          datestamp;
        jout["system_clock_ns"]=    system_clock_ns;
        jout["steady_clock_ns"]=    steady_clock_ns;
        jout["gain"]=               gain;
        jout["expo"]=               expo;
        jout["sweep_index"]=        sweep_index;
        jout["sweep_total"]=        sweep_total;
        jout["fps"]=                fps;
        jout["delta_ms"]=           delta_ms;
        jout["bpp"]=                bpp ;
        jout["width"]=              width ;
        jout["height"]=             height ;
        jout["size"]=               size ;
        jout["comment"]=            comment;
        jout["text_north"]=         text_north;
        jout["text_east"]=          text_east;
        jout["command"]=            command;

        return jout.dump();
    }

    void loadImage(fs::path filename) { 
        src = filename;

        // get data in
        size = fs::file_size(filename);
        // make sure size matches width * height * bpp / CHAR_BIT
        data = new char [size];
        ifstream fin (filename, ios::in|ios::binary);
        fin.read (data, size);
        fin.close();

        width = size / ( height * bpp / CHAR_BIT);
        LOGV << "loaded " << src << " size:" << size
            << " = w:" << width << " * h:" << height << " * bpp:" << bpp << " / CHAR_BIT:" << CHAR_BIT;
    }

    void writeRaw(fs::path filename) {

        //fs::path filename = fp.folder_raw / (image.basename + ".raw");
        remove(filename.c_str());
        int fd = open (filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (fd == -1) {
            LOGE << "Unable to save frame, cannot open file " << filename.string();
            return;
        }

        int writenBytes = write(fd, data, size);
        if (writenBytes < 0) {
            LOGE << "Error writing to file " << filename.string() ;
        } else if ( (uint32_t) writenBytes != size) {
            LOGE << "Error: " << writenBytes << " out of " << size << " were written to file " << filename.string() ;
        }

        close(fd);
    }


    void createAnnoText() { 

        char buff[BUFSIZ];                                                                                                                                             
        int delta_ms_precision = (delta_ms > 10) ? 0 : 1 ;
        snprintf(buff, sizeof(buff), "%s %3.0ffps %3.*fms gain=%d exp=%d", 
                basename.c_str(), fps, delta_ms_precision, delta_ms, gain, expo);
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

    void getExif() {

        string exif;
        // read metadata from the image
        TagInfo *info = exiftool->ImageInfo(src.c_str(),"-UniqueCameraModel");
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
        char *err = exiftool->GetError();
        if (err) LOGE << err;

        json_to_attributes(exif);
        LOGI << " sweep_total " << sweep_total ;

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

        LOGV << "cmd = " << cmd;
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

    void writeAnnotated( fs::path to, bool label) {

        dst = to;

        string cmd;
        if (label) {
            // determined by experiments
            string pointsize_north = to_string( (float) 1000 / (float) text_north.length() - 2 );
            string pointsize_east  = to_string( (float) 1000 / (float) text_east.length()  - 2 );
            // 728 x 544 -> 768 x 584
            cmd = "magick  " + src + " -clahe 25x25%+128+2 -gravity Southwest -background black -extent 768x584 -font Inconsolata -pointsize " + pointsize_north + " -fill yellow -gravity North -annotate 0x0+0+15 \"" + text_north + "\" -gravity East -pointsize " + pointsize_east + " -annotate 90x90+20+232 \"" + text_east + "\" " + dst;
        } else {
            cmd = "magick  " + src + " -clahe 25x25%+128+2 " + dst;
        }

        runMagick(cmd);


    }

    void writeAhe(fs::path from, fs::path to) {

        string cmd = "magick  " + from.string() + " -clahe 25x25%+128+2 " + to.string();
        runMagick(cmd);

    }

/*
// using actual magick api
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

   
    // assume input data is in raw format!
    int writeDng(fs::path dngname) {

        // assume g.dng_headerdata is loaded 
        assert(dng_headerdata != nullptr);

        string imageinfo = attributes_to_json();
        // write attributes
        vector<char> mod_headerdata(dng_headerdata,dng_headerdata + dng_headerlength);
        for (int i = 0; i < imageinfo.size(); i++) {
            mod_headerdata.at(i+dng_attribute_start) = imageinfo[i];
        }

        //reshape the size of data to get rid of band if needed ... assume width = 768 or 728
        int old_line_width = width * bpp / CHAR_BIT;
        int new_line_width = 728   * bpp / CHAR_BIT;
        //height = 544;

        int newdatalength = new_line_width * height;

        vector<char> indata(data, data + size); // create vector with header
        vector<char> outdata(newdatalength, 0); // fill with zeros
        for (int x = 0; x < old_line_width ; x++) {
            for (int y=0;y < height ; y++) {
                if (x < new_line_width) {
                    int old_pixel = x + y * old_line_width;
                    int new_pixel = x + y * new_line_width;
                    outdata.at(new_pixel) = indata.at(old_pixel);
                }
            }
        }

        //LOGV << "out : " << dngname ;
        //fs::remove(dngname);
        ofstream outfile (dngname,ofstream::binary);
        outfile.write (reinterpret_cast<const char*>(&mod_headerdata[0]), dng_headerlength);
        outfile.write (reinterpret_cast<const char*>(&outdata[0]), newdatalength);
        outfile.close();

    }



    // backward flip to init static class in c++11
    static class ClassInit{
      public:
            ClassInit(fs::path headerfile) {
                dng_headerlength = fs::file_size(headerfile);
                cout << " reading dng header : " << headerfile << " size = " << dng_headerlength << endl ;
                dng_headerdata = new char [dng_headerlength];
                ifstream fhead (headerfile, ios::in|ios::binary);
                fhead.read (dng_headerdata, dng_headerlength);
                fhead.close();

                dng_attribute_start = 5310;

                // init exiftool interface
                exiftool = new ExifTool();
                // also need to delete exiftool;

                // init magick?
                //MagickCoreGenesis();

            }

     } readDngHeaderData;

    // just once

 private:
    //dng header 
    static char*  dng_headerdata;
    static int    dng_headerlength;
    static int    dng_attribute_start;

    // for exif
    static ExifTool *exiftool;


    // romove junk like _ and space characters until first '{' from front and '}' from back
    string trim(const std::string &s) {
        //cout << "start = " << s << endl;
        auto start = s.begin();
        while (start != s.end() and *start != '{') 
            start++;

        auto end = s.end();
        while (end != start and *end != '}') 
            end--;

        return string(start, end + 1);
    }


}; /* end Class */


// bring init vars to life
int Imagedata::dng_headerlength = 0;
int Imagedata::dng_attribute_start = 0;
char* Imagedata::dng_headerdata = nullptr;
ExifTool *Imagedata::exiftool = nullptr;


