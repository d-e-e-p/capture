#include "image_processor.hpp"

#include <fstream>
#include <opencv2/imgcodecs/imgcodecs.hpp>
#include <linux/videodev2.h>

namespace common 
{

#ifndef V4L2_PIX_FMT_SBGGR12P
#define V4L2_PIX_FMT_SBGGR12P v4l2_fourcc('p', 'B', 'C', 'C')
#endif

#ifndef V4L2_PIX_FMT_SGBRG12P
#define V4L2_PIX_FMT_SGBRG12P v4l2_fourcc('p', 'G', 'C', 'C')
#endif

#ifndef V4L2_PIX_FMT_SGRBG12P
#define V4L2_PIX_FMT_SGRBG12P v4l2_fourcc('p', 'g', 'C', 'C')
#endif

#ifndef V4L2_PIX_FMT_SRGGB12P
#define V4L2_PIX_FMT_SRGGB12P v4l2_fourcc('p', 'R', 'C', 'C')
#endif

ImageProcessor::ImageProcessor()
{

}

ImageProcessor::~ImageProcessor()
{
    
}

void ImageProcessor::AllocateMat(const IProcessedImage &image, cv::UMat &output)
{
    int8_t bpp = GetBpp(image.pixelFormat);

    int type = bpp == 8 ? CV_8U : CV_16U;
    
    cv::Mat(image.height, image.width, type, image.data).copyTo(output);
}

void ImageProcessor::ConvertTo8Bit(cv::UMat &mat)
{
    if (mat.type() != CV_8U) {
        constexpr auto CONVERSION_SCALE_16_TO_8 = 0.00390625;
        mat.convertTo(mat, CV_8U, CONVERSION_SCALE_16_TO_8);
    }
}

uint8_t ImageProcessor::GetBpp(uint32_t pixelFormat)
{
    switch (pixelFormat) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SRGGB8:
        return 8;
    case V4L2_PIX_FMT_SBGGR10:
    case V4L2_PIX_FMT_SBGGR10P:
    case V4L2_PIX_FMT_SGBRG10:
    case V4L2_PIX_FMT_SGBRG10P:
    case V4L2_PIX_FMT_SGRBG10:
    case V4L2_PIX_FMT_SGRBG10P:
    case V4L2_PIX_FMT_SRGGB10:
    case V4L2_PIX_FMT_SRGGB10P:
        return 10;
    case V4L2_PIX_FMT_SBGGR12:
    case V4L2_PIX_FMT_SBGGR12P:
    case V4L2_PIX_FMT_SGBRG12:
    case V4L2_PIX_FMT_SGBRG12P:
    case V4L2_PIX_FMT_SGRBG12:
    case V4L2_PIX_FMT_SGRBG12P:
    case V4L2_PIX_FMT_SRGGB12:
    case V4L2_PIX_FMT_SRGGB12P:
        return 12;
    }

    throw std::invalid_argument("Could not detect bit depth based on pixel format " + std::to_string(pixelFormat));
}

void ImageProcessor::DebayerImage(cv::UMat &mat, uint32_t pixelFormat)
{
    switch (pixelFormat) {
    case V4L2_PIX_FMT_SBGGR8:
    case V4L2_PIX_FMT_SBGGR10:
    case V4L2_PIX_FMT_SBGGR10P:
    case V4L2_PIX_FMT_SBGGR12:
    case V4L2_PIX_FMT_SBGGR12P:
        cv::cvtColor(mat, mat, cv::COLOR_BayerBG2RGB);
        break;
    case V4L2_PIX_FMT_SGBRG8:
    case V4L2_PIX_FMT_SGBRG10:
    case V4L2_PIX_FMT_SGBRG10P:
    case V4L2_PIX_FMT_SGBRG12:
    case V4L2_PIX_FMT_SGBRG12P:
        cv::cvtColor(mat, mat, cv::COLOR_BayerGB2RGB);
        break;
    case V4L2_PIX_FMT_SGRBG8:
    case V4L2_PIX_FMT_SGRBG10:
    case V4L2_PIX_FMT_SGRBG10P:
    case V4L2_PIX_FMT_SGRBG12:
    case V4L2_PIX_FMT_SGRBG12P:
        cv::cvtColor(mat, mat, cv::COLOR_BayerGR2RGB);
        break;
    case V4L2_PIX_FMT_SRGGB8:
    case V4L2_PIX_FMT_SRGGB10:
    case V4L2_PIX_FMT_SRGGB10P:
    case V4L2_PIX_FMT_SRGGB12:
    case V4L2_PIX_FMT_SRGGB12P:
        cv::cvtColor(mat, mat, cv::COLOR_BayerRG2RGB);
        break;
    }
}

void ImageProcessor::ResizeImage(cv::UMat &image, const ResizeOptions &options)
{
    cv::UMat inputImage = image;
    cv::resize(inputImage, image, cv::Size(options.width, options.height));
}

void ImageProcessor::DrawText(cv::UMat &mat, std::string text)
{
    int fpsFont = cv::FONT_HERSHEY_SIMPLEX;
    double fpsScale = 0.7;
    int fpsThickness = 1;
    cv::Point fpsPoint(10, 30);

    uint32_t grayBackground;
    uint32_t grayText;
    if (mat.depth() == CV_8U) {
        grayBackground = 0x20;
        grayText = 0xd0;
    } else if (mat.depth() == CV_16U) {
        grayBackground = 0x2000;
        grayText = 0xd000;
    } else {
        throw std::invalid_argument("Unsupported bit depth");
    }

    int fpsBaseline;

    cv::Size fpsSize = cv::getTextSize(text, fpsFont, fpsScale, fpsThickness, &fpsBaseline);

    cv::rectangle(
        mat,
        fpsPoint + cv::Point(0, fpsBaseline),
        fpsPoint + cv::Point(fpsSize.width, -fpsSize.height - 5),
        cv::Scalar(grayBackground, grayBackground, grayBackground),
        cv::FILLED
    );

    cv::putText(
        mat, 
        text,
        fpsPoint,
        fpsFont,
        fpsScale,
        cv::Scalar(grayText, grayText, grayText),
        fpsThickness
    );
}

void ImageProcessor::DrawFps(cv::UMat &mat, uint32_t acquisitionFps, uint32_t displayFps)
{
    std::string fpsText =  std::to_string(acquisitionFps) + "/" + std::to_string(displayFps) + " ";
    int fpsFont = cv::FONT_HERSHEY_SIMPLEX;
    double fpsScale = 0.7;
    int fpsThickness = 1;
    cv::Point fpsPoint(10, 30);

    uint32_t grayBackground;
    uint32_t grayText;
    if (mat.depth() == CV_8U) {
        grayBackground = 0x20;
        grayText = 0xd0;
    } else if (mat.depth() == CV_16U) {
        grayBackground = 0x2000;
        grayText = 0xd000;
    } else {
        throw std::invalid_argument("Unsupported bit depth");
    }

    int fpsBaseline;

    cv::Size fpsSize = cv::getTextSize(fpsText, fpsFont, fpsScale, fpsThickness, &fpsBaseline);

    cv::rectangle(
        mat,
        fpsPoint + cv::Point(0, fpsBaseline),
        fpsPoint + cv::Point(fpsSize.width, -fpsSize.height - 5),
        cv::Scalar(grayBackground, grayBackground, grayBackground),
        cv::FILLED
    );

    cv::putText(
        mat, 
        fpsText,
        fpsPoint,
        fpsFont,
        fpsScale,
        cv::Scalar(grayText, grayText, grayText),
        fpsThickness
    );
}

void ImageProcessor::SaveImageAsPng(cv::UMat &mat, std::string name)
{
    //cv::imwrite(GetAvailableName(name, ".png"), mat);
}

void ImageProcessor::SaveImageAsJpeg(cv::UMat &mat, std::string name)
{
    if (mat.depth() != CV_8U) {
        ConvertTo8Bit(mat);
    }
    //cv::imwrite(GetAvailableName(name, ".jpg"), mat);
    cv::imwrite(name, mat);
}

void ImageProcessor::SaveImageAsTiff(cv::UMat &mat, std::string name)
{
    //cv::imwrite(GetAvailableName(name, ".tiff"), mat);
    cv::imwrite(name, mat);
}

std::string ImageProcessor::GetAvailableName(std::string firstPart, std::string secondPart)
{
    uint32_t index = 0;

    std::string file = firstPart + "_" + std::to_string(index) + secondPart;
    while(FileExists(file)) {
        index++;
        file = firstPart + "_" +  std::to_string(index) + secondPart;
    }

    return file;
}

bool ImageProcessor::FileExists(std::string file)
{
    return std::ifstream(file).good();
}

}
