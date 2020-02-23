#pragma once

#include "node.hpp"
#include "sv_processing_node.hpp"
#include "image_processor.hpp"

namespace common
{
    class CvProcessingNode : public Node<cv::UMat>
    {
        public:
            
            explicit CvProcessingNode(SvProcessingNode &svProcessingNode, ICamera *camera) 
                : svProcessingNode(svProcessingNode), camera(camera), debayer(false), resizeOptions({}), showFps(true), acquisitionFps(0), displayFps(0)
            {

            }

            ~CvProcessingNode()
            {

            }

            void SetDebayer(bool enable)
            {
                debayer = enable;
            }

            void SetResizeOptions(ResizeOptions resizeOptions)
            {
                this->resizeOptions = resizeOptions;
            }

            void ToggleFps()
            {
                showFps = !showFps;
            }

            void SetFps(uint32_t acquisitionFps, uint32_t displayFps)
            {   
                this->acquisitionFps = acquisitionFps;
                this->displayFps = displayFps;
            }

            using Node::ReturnOutput;

        protected:

            void InitializeAction() override
            {
                pixelFormat = camera->GetImageInfo().pixelFormat;
            }

            void PerformAction(cv::UMat &output) override 
            {
                IProcessedImage image = svProcessingNode.GetOutputBlocking();

                processor.AllocateMat(image, output);

                svProcessingNode.ReturnOutput();

                if (debayer) {       

                    /**
                     * OpenCV function imshow implicitly converts the image bit depth down to 8 bit before displaying the image.
                     * This results in data loss that is not visible to the human eye.
                     * Converting the image bit depth manually before performing image processing can lead to better performance.
                     * Another advantage of this approach is that it facilitates pipelining by separating the tasks of converting
                     * the image bit depth from displaying the image.
                     *
                     * In this case, the image bit depth is manually converted down to 8bit before further processing. This increases 
                     * performance because processing is faster to perform on a smaller 8 bit image.
                     */
                    processor.ConvertTo8Bit(output);
                    processor.DebayerImage(output, pixelFormat);
                }

                if (resizeOptions.enable) {
                    processor.ResizeImage(output, resizeOptions);
                }

                if (showFps) {
                    processor.DrawFps(output, acquisitionFps, displayFps);
                }
            }

        private:
            SvProcessingNode &svProcessingNode;
            ICamera *camera;
            uint32_t pixelFormat;
            ImageProcessor processor;
            std::atomic<bool> debayer;
            ResizeOptions resizeOptions;
            std::atomic<bool> showFps;
            std::atomic<uint32_t> acquisitionFps;
            std::atomic<uint32_t> displayFps;
    };
}