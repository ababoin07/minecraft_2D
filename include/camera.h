#ifndef CAMERA_H
#define CAMERA_H

#include "includes.h"

/**
 * @brief Camera state: position and zoom level.
 */
struct CameraImpl
{
    double x, y;   /**< World coordinates (in chunks) of the camera centre. */
    double zoom;   /**< Zoom factor (pixels per block). */
};

#endif // CAMERA_H