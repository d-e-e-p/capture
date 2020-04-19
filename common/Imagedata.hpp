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

    int width  = 728;
    int height = 544;

    int bpp    = 16; // bits per pixel
    int pixel_depth = bpp / CHAR_BIT;
    int line_width  = width * pixel_depth;
    int datalength  = line_width * height;

    string comment;
    string text_north;
    string text_south;
    string text_east;

    string command;

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
        snprintf(buff, sizeof(buff), "%s %3.0ffps %3.*fms gain=%d exp=%d focus=%.0f%% %.0f%% %.0f%% %.0f%%", 
                basename.c_str(), fps, delta_ms_precision, delta_ms, gain, expo, focus1, focus2, focus3, focus4);
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

        json_to_attributes(exif);
        LOGI << " sweep_total " << sweep_total ;

    }



    void writeAnnotated( fs::path to, bool label) {

        dst = to;

        string cmd;
        if (label) {
            // determined by experiments
            string pointsize_north = to_string( (float) 1000 / (float) text_north.length() - 2 );
            string pointsize_east  = to_string( (float) 1000 / (float) text_east.length()  - 2 );
            // 728 x 544 -> 768 x 584
            cmd = "/usr/local/bin/magick  " + src + " -clahe 25x25%+128+2 -quality 100 -gravity Southwest -background black -extent 768x584 -font Inconsolata -pointsize " + pointsize_north + " -fill yellow -gravity North -annotate 0x0+0+15 \"" + text_north + "\" -gravity East -pointsize " + pointsize_east + " -annotate 90x90+20+232 \"" + text_east + "\" -quality 100 " + dst;
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

   
    // assume input data is in raw format!
    int writeDng(fs::path dngname) {

        // assume g.dng_headerdata is loaded 
        assert(s.dng_headerdata != nullptr);

        string imageinfo = attributes_to_json();
        LOGV << "imageinfo = " << imageinfo; 
        // write attributes
        // reset image size i
        vector<char> mod_headerdata(s.dng_headerdata,s.dng_headerdata + s.dng_headerlength);
        for (int i = 0; i < imageinfo.length(); i++) {
            mod_headerdata.at(i+s.dng_attribute_start) = imageinfo[i];
        }
        //LOGV << "mod_headerdata = " << mod_headerdata; 

        //reshape the size of data to get rid of band if needed ... assume width = 768 or 728
        char* cropdata = crop_opencv(data);

        LOGV << "writing to : " << dngname.string() ;
        //fs::remove(dngname);
        ofstream outfile (dngname,ofstream::binary);
        outfile.write (reinterpret_cast<const char*>(&mod_headerdata[0]), s.dng_headerlength);
        outfile.write (cropdata, datalength);
        outfile.close();

        delete cropdata;

    }



    //class init just once
    static void init(fs::path dng_headerfile, fs::path dt_stylefile, fs::path rt_stylefile, bool use_threaded_jpg) {

        s.dng_headerfile = dng_headerfile;
        s.dt_stylefile   = dt_stylefile;
        s.rt_stylefile   = rt_stylefile;
        s.use_threaded_jpg = use_threaded_jpg;

        //load in header
        s.dng_headerlength = fs::file_size(s.dng_headerfile);
        LOGI << " reading dng header : " <<  s.dng_headerfile << " size = " << s.dng_headerlength ;
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
        //jstr = trim(jstr);
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
        datalength  =       jin["datalength"];
        comment =           jin["comment"];
        text_north =        jin["text_north"];
        text_east =         jin["text_east"];
        command =           jin["command"];

    
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
        jout["datalength"]=         datalength ;
        jout["comment"]=            comment;
        jout["text_north"]=         text_north;
        jout["text_east"]=          text_east;
        jout["command"]=            command;

        return jout.dump();
    }


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
            while (start != s.end() and *start != '{') 
                while (end != start and *end != '}') 
                    end--;

        return string(start, end + 1);
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



    char* crop_opencv(char* indata) {
        //int bpp = 16;
        //int height = 544;
        //int pixel_depth = bpp / CHAR_BIT;
         
          
        int old_width = 768; // or take from current width var?
        int old_line_width = old_width * pixel_depth;
        int old_datalength = old_line_width * height;

        int new_width = 728;
        int new_line_width = new_width * pixel_depth;
        int new_datalength = new_line_width * height;


        Mat in (height, old_width, CV_16UC1);
        Mat out(height, new_width, CV_16UC1);

        //LOGV << "in = "  << in.size()  ;
        //LOGV << "out = " << out.size() ;

        memcpy(in.data, indata, old_datalength);

        Rect roi = Rect(0,0, new_width, height);
        Mat crop = in(roi);
        //LOGV << "crop = " << crop.size() << " cont = " << crop.isContinuous() ;

        crop.copyTo(out);
        //LOGV << "copy = " << out.size() << " cont = " << out.isContinuous() ;

        //update globals
        width = new_width;
        line_width = new_line_width;
        datalength = new_datalength;

        char* res = new char [new_datalength];
        memcpy(res, out.data, new_datalength);
        return res;

      }

     char* crop_direct(char* old_data) {
      // reduce from 768*2 = 1536 bytes per line to 728*2 = 1456
      // throwing away away excess stride
         //reshape the size of data to get rid of band if needed ... assume width = 768 or 728

        int old_width = 768; // or take from current width var?
        int old_line_width = old_width * pixel_depth;
        int old_datalength = old_line_width * height;

        int new_width = 728;
        int new_line_width = new_width * pixel_depth;
        int new_datalength = new_line_width * height;

          vector<char> indata (old_data, old_data + old_datalength); // fill vec with data
          vector<char> outdata(old_data, old_data + new_datalength);
          for (int x = 0; x < old_line_width ; x++) {
              for (int y=0;y < height ; y++) {
                  if (x < new_line_width) {
                      int old_pixel = x + y * old_line_width;
                      int new_pixel = x + y * new_line_width;
                      outdata.at(new_pixel) = indata.at(old_pixel);
                  }
              }
          }

          char* res = new char [new_datalength];
          memcpy(res, &outdata[0], new_datalength);
          return res;

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


