#pragma once

// Geometry of a ray meeting the optical elements. Pure functions, no drawing and no
// Windows dependency, so they can be unit-tested (tests/optics_test.cpp).
//
// All coordinates are canvas coordinates. The rounding (floor / ceil) and the float
// arithmetic match the original ray tracer exactly, so the drawn pixels do not change.

#include <cmath>
#include <cstdlib>

#include "../layout.h"
#include "scene.h"

enum class Direction { Right, Left };

// A ray at a point, with its slope (dy/dx).
struct Ray {
    int   x;
    int   y;
    float slope;
};

// x where a ray travelling in `direction` reaches `element`. Rays travelling left reach
// a mirror from behind, so they arrive at its back, MirrorBackWidth right of its face.
inline int ArrivalX(const Element& element, Direction direction)
{
    if (direction == Direction::Left && element.type == ElementType::Mirror)
        return element.x + MirrorBackWidth;
    return element.x;
}

// y of a straight ray at x. Used for all segments except those leaving a mirror.
inline int StraightY(const Ray& ray, int x)
{
    return static_cast<int>(floor(ray.slope * (x - ray.x))) + ray.y;
}

// y of a ray at x after it has been reflected by a mirror (the original rounds up here).
inline int ReflectedY(const Ray& ray, int x)
{
    return static_cast<int>(ceil(ray.slope * (x - ray.x) + ray.y));
}

// A lens bends only the rays that reach it within its height.
inline bool InsideAperture(int y, int axisY)
{
    return abs(axisY - y) < ElementHalfHeight;
}

// Slope of a ray after passing through `lens` at (ray.x, ray.y). A lens seen from the
// other side (ray travelling left) acts as the opposite shape.
inline float Refract(const Ray& ray, const Element& lens, Direction direction, int axisY)
{
    const bool concave     = (lens.shape != ShapeConvex) != (direction == Direction::Left);
    const int  focalLength = lens.focus - lens.x;
    const float bend       = static_cast<float>(-ray.y + axisY) / focalLength;
    return concave ? ray.slope - bend : ray.slope + bend;
}

// Slope of a ray after reflecting off `mirror` at (ray.x, ray.y).
inline float Reflect(const Ray& ray, const Element& mirror, int axisY)
{
    return ray.slope * (-1) + static_cast<float>(ray.y - axisY) / (ray.x - mirror.focus);
}

// Whether (x, y) is on a mirror's reflecting body, as the mirror is drawn: a block from
// the mirror's x to its back (MirrorBackWidth further right) with the right half of an
// ellipse cut out of its front. The ellipse is centred on the mirror's x and the axis,
// MirrorArcHalfWidth wide and ElementHalfHeight + width high; its edge is the curved,
// reflecting face. A ray travelling right first touches the body on that face.
inline bool InsideMirrorBody(const Element& mirror, int x, int y, int axisY)
{
    const double halfHeight = ElementHalfHeight + mirror.width;
    const double dy         = y - axisY;
    if (std::abs(dy) > halfHeight) return false;

    const double faceX = mirror.x + MirrorArcHalfWidth * std::sqrt(1.0 - (dy * dy) / (halfHeight * halfHeight));
    return x > faceX && x <= mirror.x + MirrorBackWidth;
}

// Whether a ray reaching the back of `mirror` at height y hits it (and stops there).
inline bool HitsBack(int y, const Element& mirror, int axisY)
{
    return abs(y - axisY) <= ElementHalfHeight + mirror.width;
}
