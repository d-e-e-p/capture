#pragma once

#include "sv/sv.h"

namespace common
{

struct ConfiguredCamera
{
    ICamera *camera;
    bool debayering;
    ResizeOptions resizeOptions;
};

using ConfiguredCameras = std::vector<ConfiguredCamera>;

}
