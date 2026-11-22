#pragma once

// The images an object forms through the lenses and mirrors after it, from the
// thin-lens / mirror equation:
//
//     1/do + 1/di = 1/f        di = 1 / (1/f - 1/do)        m = -di / do
//
// do: object distance, di: image distance (positive: real image, on the side the light
// goes to; negative: virtual image, on the side it comes from), m: magnification
// (negative: inverted). Each image is the object for the next element along the light's
// path, as in the ray tracer: lenses keep the direction, a mirror sends the light back.
//
// Coordinates are world coordinates; x along the axis, heights above (+) or below (-) it.

#include <vector>

#include "../layout.h"
#include "scene.h"

struct ImagePoint {
    int x, y;                   // canvas coordinates
};

struct Image {
    double       x = 0;                  // position on the axis
    double       height = 0;             // tip of the image arrow, above (+) or below (-) the axis
    double       magnification = 0;      // relative to the original object (negative: inverted)
    bool         real = false;           // false: virtual (the rays only seem to come from it)
    bool         atInfinity = false;     // object at the focal point: the rays leave parallel
    Scene::Index formedBy = 0;           // the lens or mirror that forms it
    std::vector<ImagePoint> rayExits;    // where the object's traced rays leave that element
    int          raysMeeting = 0;        // how many of those rays, as drawn, pass through its tip
                                         // (real) or, extended backwards, seem to come from it (virtual)
};

// An image is shown only where the drawn rays show it: where at least two of them meet.
// One ray fixes no point; a ray that misses the lens's edge, or rays stopped by another
// element before they converge, leave the image unshown. It still takes part in the
// chain: the next lens or mirror forms its image from it, as in the optics.
constexpr int MinRaysMeeting = 2;

inline bool ShownByRays(const Image& image)
{
    return !image.atInfinity && image.raysMeeting >= MinRaysMeeting;
}

// The images of the object at scene[object], in the order the light forms them. An
// element only forms an image if at least one of the object's traced rays reaches it
// (and is bent or reflected); the chain stops at the first one that none reaches, at an
// image at infinity, or where the light reaches the back of a mirror. Whether enough
// rays meet at each image to show it is in raysMeeting (see ShownByRays).
std::vector<Image> FormImages(const Scene& scene, const Layout& layout, Scene::Index object);
