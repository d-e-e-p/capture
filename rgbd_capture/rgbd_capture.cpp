// include the librealsense C++ header file
#include <librealsense2/rs.hpp>
#include <librealsense2/rs_advanced_mode.hpp>

// include OpenCV header file
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <experimental/filesystem>
namespace fs=std::experimental::filesystem;

#include <iostream>
#include <fstream>
#include <iomanip>      // std::setfill, std::setw



using namespace std;
using namespace cv;

string getDateStamp() {
    time_t rawtime;
    struct tm * timeinfo;

    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    printf ("Time %s", asctime(timeinfo));

    int year    = timeinfo->tm_year+1900;
    int month   = timeinfo->tm_mon+1;
    int day     = timeinfo->tm_mday;
    int hour    = timeinfo->tm_hour;
    int minute  = timeinfo->tm_min;

    stringstream iss;
    iss << setfill('0');
    iss << setw(4) << year;
    iss << setw(2) << month;
    iss << setw(2) << day;
    iss << setw(2) << hour;
    iss << setw(2) << minute;

    return iss.str(); 

}

std::string operator"" _q(const char* text, std::size_t len) {
    return "\"" + std::string(text, len) + "\"";
}


void metadata_to_json(const rs2::frame& frame, const std::string& filename)
{
    //cout << "Writing metadata to " << filename << endl;
    rs2::frame frm = frame.as<rs2::video_frame>();
    //auto profile = frame.as<rs2::video_stream_profile>();

    float width = 0;
    float height = 0;
    float dist_to_center = 0;
    string stream = rs2_stream_to_string(frm.get_profile().stream_type());
    if (stream == "Depth") {
        rs2::depth_frame depth = frame.as<rs2::depth_frame>();
        width = depth.get_width();
        height = depth.get_height();
        // Query the distance from the camera to the object in the center of the image
        dist_to_center = depth.get_distance(width / 2, height / 2);
        //cout << "d = " << dist_to_center << "\n";
        /*
         auto depth_stream = selection.get_stream(RS2_STREAM_DEPTH)
                             .as<rs2::video_stream_profile>();
auto resolution = std::make_pair(depth_stream.width(), depth_stream.height());
auto i = depth_stream.get_intrinsics();
auto principal_point = std::make_pair(i.ppx, i.ppy);
auto focal_length = std::make_pair(i.fx, i.fy);
rs2_distortion model = i.model;
        auto i = depth.get_intrinsics();
        auto principal_point = std::make_pair(i.ppx, i.ppy);
        auto focal_length = std::make_pair(i.fx, i.fy);
        cout << principal_point << "\n";
        cout << focal_length << "\n";
        */
    }

    std::ofstream json;
    json.open(filename);

    json << "{\n";
    json << "stream"_q << ":" << "\"" << stream << "\"" << ",\n";
    json << "width"_q << ":"  << width << ",\n";
    json << "height"_q << ":" << height << ",\n";
    json << "dist_to_center"_q << ":" << dist_to_center << ",\n";

    // Record all the available metadata attributes
    for (size_t i = 0; i < RS2_FRAME_METADATA_COUNT; i++)
    {
        if (frm.supports_frame_metadata((rs2_frame_metadata_value)i))
        {
            json << "\"" << rs2_frame_metadata_to_string((rs2_frame_metadata_value)i) << "\"" << ":"
                << frm.get_frame_metadata((rs2_frame_metadata_value)i) << ",\n";
        }
    }
    json << "}\n";

    json.close();
}

    int main() {
        string datestamp = getDateStamp();
        fs::path folder_data  = "data/rgbd";
        fs::path folder_base =   folder_data / datestamp;
        fs::path folder_color = folder_base / "color";
        fs::path folder_depth = folder_base / "depth";
        fs::path folder_infra    = folder_base / "infra";


        if (! fs::exists(folder_color)) {
            cout << "creating color dir : " << folder_color << "\n";
            fs::create_directories(folder_color);
        }
        if (! fs::exists(folder_depth)) {
            cout << "creating depth dir : " << folder_depth << "\n";
            fs::create_directories(folder_depth);
        }
        if (! fs::exists(folder_infra)) {
            cout << "creating infra dir : " << folder_infra << "\n";
            fs::create_directories(folder_infra);
        }

        rs2::colorizer color_map;

        //Contruct a pipeline which abstracts the device
        rs2::pipeline pipe;

        //Create a configuration for configuring the pipeline with a non default profile
        rs2::config cfg;

        cout << "begin config\n";
        //Add desired streams to configuration
/*
        cfg.enable_stream(RS2_STREAM_COLOR,      1280,  720, RS2_FORMAT_BGR8, 15);
        cfg.enable_stream(RS2_STREAM_DEPTH,      1280,  720, RS2_FORMAT_Z16,  15);
        cfg.enable_stream(RS2_STREAM_INFRARED,   1280,  720, RS2_FORMAT_BGR8, 15);

*/
        cfg.enable_stream(RS2_STREAM_COLOR,      1280,  720, RS2_FORMAT_BGR8, 6);
        cfg.enable_stream(RS2_STREAM_DEPTH,      1280,  720, RS2_FORMAT_Z16,  6);
        cfg.enable_stream(RS2_STREAM_INFRARED,   1280,  720, RS2_FORMAT_BGR8, 6);
        cout << "end config\n";

        //Instruct pipeline to start streaming with the requested configuration
        auto profile = pipe.start(cfg);
        cout << "pipeline started\n";
        rs2::device dev = profile.get_device();
        auto advanced = dev.as<rs400::advanced_mode>();
        STDepthTableControl depth_table = advanced.get_depth_table();
        std::stringstream ss;
        ss << "depthUnits: "    << depth_table.depthUnits << ", ";
        ss << "depthClampMin: " << depth_table.depthClampMin << ", ";
        ss << "depthClampMax: " << depth_table.depthClampMax << ", ";
        ss << "disparityMode: " << depth_table.disparityMode << ", ";
        ss << "disparityShift: " << depth_table.disparityShift << "\n";
        depth_table.depthUnits = 100;
        depth_table.disparityShift = 128;
        ss << "depthUnits: "    << depth_table.depthUnits << ", ";
        ss << "depthClampMin: " << depth_table.depthClampMin << ", ";
        ss << "depthClampMax: " << depth_table.depthClampMax << ", ";
        ss << "disparityMode: " << depth_table.disparityMode << ", ";
        ss << "disparityShift: " << depth_table.disparityShift << "\n";
        cout << ss.str() << endl;
        //result = set_depth_control(dev, &depth_control);
        //advanced.load_json(str);
        cout << "disparity set \n";

        // see https://github.com/IntelRealSense/librealsense/wiki/API-How-To#controlling-the-laser
        rs2::device selected_device = dev;

        auto depth_sensor = selected_device.first<rs2::depth_sensor>();
        if (depth_sensor.supports(RS2_OPTION_EMITTER_ENABLED)) {
            depth_sensor.set_option(RS2_OPTION_EMITTER_ENABLED, 1.f); // Enable emitter
        }
        if (depth_sensor.supports(RS2_OPTION_LASER_POWER)) {
            // Query min and max values:
            auto range = depth_sensor.get_option_range(RS2_OPTION_LASER_POWER);
            depth_sensor.set_option(RS2_OPTION_LASER_POWER, range.max); // Set max power
        }

        depth_sensor.set_option(RS2_OPTION_ENABLE_AUTO_EXPOSURE, 1); //

        // Camera warmup - dropping several first frames to let auto-exposure stabilize
        rs2::frameset frames;
       //Wait for all configured streams to produce a frame
        cout << "capture frames to set auto expo \n";
        for(int i = 0; i < 30; i++) {
            frames = pipe.wait_for_frames();
        }

        auto color_sensor = selected_device.first<rs2::color_sensor>();
        float exposure = color_sensor.get_option(RS2_OPTION_EXPOSURE);
        float gain = color_sensor.get_option(RS2_OPTION_GAIN);
        cout << "auto gain = " << gain << " expo = " << exposure <<  "\n";

        /*
        exposure *= 0.5;
        gain *= 2.0;
        frames = pipe.wait_for_frames();
        cout << "disable auto exposure\n";
        color_sensor.set_option(RS2_OPTION_EXPOSURE, exposure);
        color_sensor.set_option(RS2_OPTION_GAIN, gain );
        cout << "set  gain = " << gain << " expo = " << exposure <<  "\n";
        */

        int i = 0;
        char buff[BUFSIZ];
        while(1) {
            if ((i % 10) == 0)
                cout << "." << "\n" << flush;
            frames = pipe.wait_for_frames();
            snprintf(buff, sizeof(buff), "img%0*d", 5, i++);
            string basename = buff;
            //cout << "basename=" << basename << "\n" << flush;

            //Get each frame
            rs2::frame infra_data = frames.first(RS2_STREAM_INFRARED);
            rs2::frame depth_data  = frames.get_depth_frame();
            rs2::frame color_frame = frames.get_color_frame();

            rs2::frame infra_frame = infra_data.apply_filter(color_map);
            rs2::frame depth_frame = depth_data.apply_filter(color_map);

            // Creating OpenCV matrix from IR image
            Mat mat_infra(Size(1280, 720), CV_8UC3, (void*)infra_frame.get_data(), Mat::AUTO_STEP);
            Mat mat_depth(Size(1280, 720), CV_8UC3, (void*)depth_frame.get_data(), Mat::AUTO_STEP);
            Mat mat_color(Size(1280, 720), CV_8UC3, (void*)color_frame.get_data(), Mat::AUTO_STEP);


            // Apply Histogram Equalization
            //equalizeHist( infra, infra );
            /*
            Ptr<CLAHE> clahe = createCLAHE();
            clahe->setClipLimit(4);
            clahe->apply(infra, infra);
            applyColorMap(infra, infrac, COLORMAP_TURBO);
            */

            // Display the image in GUI
            string file_infra = folder_infra.string() + "/" + basename + ".jpg";
            string file_depth = folder_depth.string() + "/" + basename + ".jpg";
            string file_color = folder_color.string() + "/" + basename + ".jpg";

            imwrite(file_infra,mat_infra);
            imwrite(file_depth,mat_depth);
            imwrite(file_color,mat_color);

            file_infra = folder_infra.string() + "/" + basename + ".json";
            file_depth = folder_depth.string() + "/" + basename + ".json";
            file_color = folder_color.string() + "/" + basename + ".json";

            metadata_to_json(color_frame, file_color);
            metadata_to_json(depth_data , file_depth);
            metadata_to_json(infra_data , file_infra);
        }

        return 0;
    }
