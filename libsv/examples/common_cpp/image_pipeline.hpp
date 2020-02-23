#pragma once

#include "sv/sv.h"
#include "capture_node.hpp"
#include "sv_processing_node.hpp"
#include "cv_processing_node.hpp"
#include "fps_measurer.hpp"

namespace common
{

    class ImagePipeline
    {
        public:

            explicit ImagePipeline(ICamera *camera) : camera(camera)
            {
                std::stringstream stream;
                stream << camera->GetName() << " - " << camera->GetDriverName();
                name = stream.str();
            
                cleanName = name;
                cleanName.erase(std::remove_if(cleanName.begin(), cleanName.end(), isspace), cleanName.end());
                std::replace(cleanName.begin(), cleanName.end(), '/', '_');
                std::replace(cleanName.begin(), cleanName.end(), '-', '_');
                cleanName.erase(0, 1);

                captureNode = std::unique_ptr<CaptureNode>(new CaptureNode(camera));
                svProcessingNode = std::unique_ptr<SvProcessingNode>(new SvProcessingNode(*captureNode, camera));
                cvProcessingNode = std::unique_ptr<CvProcessingNode>(new CvProcessingNode(*svProcessingNode, camera));
            }

            ~ImagePipeline()
            {
                
            }

            void Start()
            {
                captureNode->Start();
                svProcessingNode->Start();
                cvProcessingNode->Start();
            }

            void Stop()
            {
                cvProcessingNode->Stop();
                svProcessingNode->Stop();
                captureNode->Stop();
            }

            cv::UMat GetImage()
            {
                return cvProcessingNode->GetOutputBlocking();
            }

            void ReturnImage()
            {
                cvProcessingNode->ReturnOutput();
                fpsMeasurer.FrameReceived();
                cvProcessingNode->SetFps(captureNode->GetFps(), fpsMeasurer.GetFps());
            }

            std::string GetName()
            {
                return name;
            }

            std::string GetCleanName()
            {
                return cleanName;
            }

            void SetDebayer(bool enable)
            {
                cvProcessingNode->SetDebayer(enable);
            }

            void SetResizeOptions(ResizeOptions resizeOptions)
            {
                cvProcessingNode->SetResizeOptions(resizeOptions);
            }

            void ToggleFps()
            {
                cvProcessingNode->ToggleFps();
            }

            bool IsMaster()
            {
                auto controls = camera->GetControlList();

                auto control = std::find_if(controls.begin(), controls.end(), 
                    [](IControl *control) { 
                        return std::string(control->GetName()) == "Operation Mode"; 
                    }
                );

                if (control == controls.end()) {
                    return true;
                } 

                if (!(*control)->IsMenu()) {
                    return true;
                }

                auto menu = (*control)->GetMenuEntries();
                if (menu.empty()) {
                    return true;
                }

                uint32_t index = (*control)->Get();
                if (menu.size() <= index) {
                    return true;
                }

                return std::string(menu[index].name) == "Master Mode";
            }

        private:
            ICamera *camera;
            std::string name;
            std::string cleanName;
            std::unique_ptr<CaptureNode> captureNode;
            std::unique_ptr<SvProcessingNode> svProcessingNode;
            std::unique_ptr<CvProcessingNode> cvProcessingNode;
            FpsMeasurer fpsMeasurer;
    };
}