#include "ray_tracer.h"

#include <cmath>

#include "optics.h"

/* Each ray is followed on its own, recursively: it leaves an element, travels to the
   next element along the axis, that element acts on it (bends, reflects, stops or lets
   it through), and the ray leaves that element in turn. A ray ends when it reaches the
   edge of the canvas, misses a mirror's face, or hits a mirror's back. */

namespace {

enum class Rounding { Floor, Ceil };

class RayTracer {
public:
    RayTracer(const Scene& scene, const Layout& layout)
        : scene_(scene), layout_(layout) {}

    std::vector<Segment> trace(Scene::Index sourceIndex);

private:
    void draw(const Ray& from, const Ray& to)
    {
        segments_.push_back({ from.x, from.y, to.x, to.y });
    }

    int elementCount() const { return static_cast<int>(scene_.size()); }

    /* A ray only visits each element at most twice (once each way: it can turn back at
       a mirror but never turn right again), so this limit is never reached in practice. */
    int maxDepth() const { return 2 * elementCount() + 2; }

    bool traceToFace(const Ray& from, const Element& mirror, Ray& to) const;
    void leave(const Ray& ray, int index, Direction direction, Rounding rounding, int depth);
    void reflectFromFace(const Ray& ray, int index, int depth);
    void arriveAt(Ray ray, int index, Direction direction, int depth);

    const Scene&         scene_;
    const Layout&        layout_;
    std::vector<Segment> segments_;
};

/* Follows a ray one unit at a time until it reaches `mirror`'s reflecting face or leaves
   the canvas on the right. `to` receives the end point. Returns true if the ray reached
   the face inside the canvas. */
bool RayTracer::traceToFace(const Ray& from, const Element& mirror, Ray& to) const
{
    int   x         = from.x;
    float y         = static_cast<float>(from.y);
    bool  keepGoing = true;
    bool  hit       = false;

    while (keepGoing)
    {
        if (InsideMirrorBody(mirror, x, static_cast<int>(floor(y + 0.5)), layout_.axisY))
        {
            keepGoing = false;
            hit = true;
        }
        else
        {
            x++;
            y += from.slope;
        }

        if (x > layout_.canvasWidth)
        {
            x = layout_.canvasWidth;
            keepGoing = false;
            hit = false;
        }
    }

    to = { x, static_cast<int>(floor(y + 0.5)), from.slope };
    if (floor(y + 0.5) > layout_.canvasHeight) hit = false;
    return hit;
}

/* The ray leaves the element at `index` travelling in `direction`. It is drawn to the next
   element, which then acts on it, or to the edge of the canvas if there is none. */
void RayTracer::leave(const Ray& ray, int index, Direction direction, Rounding rounding, int depth)
{
    if (depth > maxDepth()) return;

    const int  next   = (direction == Direction::Right) ? index + 1 : index - 1;
    const bool atEdge = next < 0 || next >= elementCount();

    Ray arrived = ray;
    if (atEdge)
        arrived.x = (direction == Direction::Right) ? layout_.canvasWidth : 0;
    else
        arrived.x = ArrivalX(scene_[next], direction);
    arrived.y = (rounding == Rounding::Floor) ? StraightY(ray, arrived.x) : ReflectedY(ray, arrived.x);

    draw(ray, arrived);

    if (!atEdge)
        arriveAt(arrived, next, direction, depth + 1);
}

/* The ray, travelling right, has reached the mirror at `index` (or is heading for it):
   follow it to the mirror's face and reflect it back to the left. */
void RayTracer::reflectFromFace(const Ray& ray, int index, int depth)
{
    Ray onFace;
    const bool hit = traceToFace(ray, scene_[index], onFace);
    draw(ray, onFace);
    if (!hit) return;                       /* missed the face: the ray leaves the canvas */

    onFace.slope = Reflect(onFace, scene_[index], layout_.axisY);
    leave(onFace, index, Direction::Left, Rounding::Ceil, depth);
}

/* The ray has reached the element at `index`: apply the element's effect and carry on. */
void RayTracer::arriveAt(Ray ray, int index, Direction direction, int depth)
{
    const Element& element = scene_[index];

    switch (element.type)
    {
    case ElementType::Lens:
        if (InsideAperture(ray.y, layout_.axisY))
            ray.slope = Refract(ray, element, direction, layout_.axisY);
        leave(ray, index, direction, Rounding::Floor, depth);
        break;

    case ElementType::Mirror:
        if (direction == Direction::Right)
            reflectFromFace(ray, index, depth);
        else if (!HitsBack(ray.y, element, layout_.axisY))
            leave(ray, index, direction, Rounding::Floor, depth);   /* passes above or below */
        /* else: hits the mirror's back and stops */
        break;

    default:                                /* cannons, objects, sources are transparent */
        leave(ray, index, direction, Rounding::Floor, depth);
        break;
    }
}

/* The rays start towards the first lens or mirror to the right of the source. */
std::vector<Segment> RayTracer::trace(Scene::Index sourceIndex)
{
    const int      source  = static_cast<int>(sourceIndex);
    const Element& emitter = scene_[sourceIndex];

    int first = source + 1;
    while (first < elementCount() &&
           scene_[first].type != ElementType::Lens && scene_[first].type != ElementType::Mirror)
        first++;
    if (first >= elementCount()) return {};

    const Element& target = scene_[first];
    const int x = emitter.x;
    const int y = emitter.y - ToolbarHeight;              /* canvas coordinates */

    if (emitter.type == ElementType::Object && target.type == ElementType::Mirror)
    {
        /* The three principal rays: parallel to the axis, towards the mirror's centre of
           curvature, and through its focus. They are followed straight onto the face. */
        const int   heightToAxis = layout_.axisY - emitter.y + ToolbarHeight;
        const float slopes[ObjectRays] = {
            0.0f,
            static_cast<float>(heightToAxis) / (target.x - x + MirrorArcHalfWidth),
            static_cast<float>(heightToAxis) / (target.focus - x),
        };
        for (float slope : slopes)
            reflectFromFace({ x, y, slope }, first, 1);
        return segments_;
    }

    Ray rays[MaxRays];
    int rayCount = 0;

    if (emitter.type == ElementType::Cannon)
    {
        /* A parallel beam, aimed by the cannon's aim marker */
        for (int i = 0; i < CannonRays; i++)
            rays[rayCount++] = { emitter.x,
                                 emitter.y - ToolbarHeight - CannonBeamHalfWidth + i*CannonRaySpacing,
                                 static_cast<float>(emitter.focus) / CannonFocusOffset };
    }
    else if (emitter.type == ElementType::Source)
    {
        /* A fan of rays */
        for (int i = 0; i < SourceRays; i++)
            rays[rayCount++] = { emitter.x + SourceHalfSize,
                                 emitter.y - ToolbarHeight - SourceHalfSize + SourceRaySpacing*i,
                                 static_cast<float>(SourceFirstSlope + SourceSlopeStep*i) };
    }
    else if (emitter.type == ElementType::Object && target.type == ElementType::Lens)
    {
        /* The three principal rays: parallel to the axis, through the lens centre,
           and through the focal point in front of the lens. */
        const int heightToAxis = layout_.axisY - emitter.y + ToolbarHeight;
        const int focalLength  = target.focus - target.x;
        const float slopes[ObjectRays] = {
            0.0f,
            static_cast<float>(heightToAxis) / (target.x - x),
            static_cast<float>(heightToAxis) / (target.x - focalLength - x),
        };
        for (float slope : slopes)
            rays[rayCount++] = { x, y, slope };
    }

    /* Straight to the first lens or mirror (passing any cannons, objects or sources
       in between), which then acts on the ray. */
    for (int i = 0; i < rayCount; i++)
    {
        Ray arrived = { target.x, StraightY(rays[i], target.x), rays[i].slope };
        draw(rays[i], arrived);
        arriveAt(arrived, first, Direction::Right, 1);
    }
    return segments_;
}

} // namespace

std::vector<Segment> TraceRays(const Scene& scene, const Layout& layout, Scene::Index source)
{
    return RayTracer(scene, layout).trace(source);
}
