#pragma once

#include "sv/sv.h"

#include "resize_options.hpp"

#include <opencv2/imgproc/imgproc.hpp>

namespace common
{    
    struct DisplayOptions {
        bool debayer;    
        ResizeOptions resizeOptions;
        bool convertTo8Bit;
    };

    class ImageProcessor 
    {

        public:
            ImageProcessor();
            ~ImageProcessor();

            void AllocateMat(const IProcessedImage &input, cv::UMat &output);
            void ConvertTo8Bit(cv::UMat &mat);
            void DebayerImage(cv::UMat &mat, uint32_t pixelFormat);
            void ResizeImage(cv::UMat &image, const ResizeOptions &options);
            void DrawFps(cv::UMat &mat, uint32_t acquisitionFps, uint32_t displayFps);
            void DrawText(cv::UMat &mat, std::string text);
            void SaveImageAsPng(cv::UMat &mat, std::string name);
            void SaveImageAsJpeg(cv::UMat &mat, std::string name);
            void SaveImageAsTiff(cv::UMat &mat, std::string name);

        private:
            uint8_t GetBpp(uint32_t pixelFormat);
            std::string GetAvailableName(std::string firstPart, std::string secondPart);
            bool FileExists(std::string file);
    };
}
