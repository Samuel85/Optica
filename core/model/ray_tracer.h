#pragma once

// Traces the rays leaving a cannon, object or source through the scene. Part of the
// model: it computes line segments and draws nothing. The view draws the segments.

#include <vector>

#include "../layout.h"
#include "scene.h"

// A straight piece of a ray, in canvas coordinates.
struct Segment {
    int x1, y1;
    int x2, y2;
};

// The segments of all rays leaving scene[source], in drawing order. Empty if there is
// no lens or mirror to the right of the source. Pure geometry: the result depends only
// on the scene and the (world) layout, not on how or where it is drawn.
std::vector<Segment> TraceRays(const Scene& scene, const Layout& layout, Scene::Index source);
