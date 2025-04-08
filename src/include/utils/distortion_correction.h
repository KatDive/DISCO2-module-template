#ifndef DISTORTION_CORRECTION_H
#define DISTORTION_CORRECTION_H

#include <opencv2/opencv.hpp>  // For cv::Mat and OpenCV functions
#include "module.h"             // Include your module-related headers
#include "util.h"               // Include your utilities (e.g., for image handling)

extern "C" {
    // Declare the module function which applies distortion correction
    void module();  // Function that performs the distortion correction on input images
}

#endif // DISTORTION_CORRECTION_H
