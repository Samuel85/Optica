#include "imaging.h"

#include <algorithm>
#include <cmath>

#include "optics.h"
#include "ray_tracer.h"

namespace {

/* How far, in canvas units, a drawn ray may pass from an image's tip and still count as
   passing through it: the drawn rays are rounded to whole units at both ends, so the
   error grows with the distance from where the ray leaves the lens or mirror. */
constexpr double RayMeetTolerance       = 1.5;
constexpr double RayMeetToleranceGrowth = 1.0 / 150.0;   /* per unit of distance */
constexpr int    RayEndTolerance        = 1;             /* a real image this far past a ray's end still counts */

bool IsLensOrMirror(const Element& element)
{
    return element.type == ElementType::Lens || element.type == ElementType::Mirror;
}

/* The traced ray segments leaving `element`, travelling in `leaving` direction, after it
   has bent or reflected them. Lenses only bend the rays that reach them within their height. */
std::vector<Segment> LeavingRays(const std::vector<Segment>& rays, const Element& element,
                                 Direction leaving, int axisY)
{
    std::vector<Segment> leavingRays;
    for (const Segment& ray : rays)
    {
        const bool goesThatWay = (leaving == Direction::Right) ? ray.x2 > ray.x1 : ray.x2 < ray.x1;
        if (!goesThatWay) continue;

        const bool leavesElement = (element.type == ElementType::Lens)
            ? ray.x1 == element.x && InsideAperture(ray.y1, axisY)
            : InsideMirrorBody(element, ray.x1, ray.y1, axisY);
        if (leavesElement)
            leavingRays.push_back(ray);
    }
    return leavingRays;
}

/* How many of the leaving rays meet at the image's tip, as they are drawn. For a real
   image a ray must reach the tip: a segment that ends before it (stopped by the next
   element) does not count. For a virtual image the ray's line, extended backwards from
   where it leaves, must pass through the tip. */
int RaysMeeting(const std::vector<Segment>& leavingRays, const Image& image, int axisY)
{
    const double tipX = image.x;
    const double tipY = axisY - image.height;
    int meeting = 0;
    for (const Segment& ray : leavingRays)
    {
        if (image.real && (tipX < std::min(ray.x1, ray.x2) - RayEndTolerance ||
                           tipX > std::max(ray.x1, ray.x2) + RayEndTolerance))
            continue;

        const double slope    = static_cast<double>(ray.y2 - ray.y1) / (ray.x2 - ray.x1);
        const double yAtTip   = ray.y1 + slope * (tipX - ray.x1);
        const double distance = std::abs(tipX - ray.x1);
        if (std::abs(yAtTip - tipY) <= RayMeetTolerance + distance * RayMeetToleranceGrowth)
            ++meeting;
    }
    return meeting;
}

} // namespace

std::vector<Image> FormImages(const Scene& scene, const Layout& layout, Scene::Index objectIndex)
{
    std::vector<Image> images;
    const Element& object = scene[objectIndex];
    if (object.type != ElementType::Object) return images;

    double x      = object.x;
    double height = layout.axisWindowY - object.y;
    if (height == 0) return images;                   /* an object on the axis has no size */

    const std::vector<Segment> rays = TraceRays(scene, layout, objectIndex);
    const int elementCount = static_cast<int>(scene.size());
    double    magnification = 1.0;
    Direction direction = Direction::Right;
    int       index = static_cast<int>(objectIndex);

    /* the light passes each element at most twice (it can turn back once, at a mirror) */
    for (int step = 0; step < 2 * elementCount + 2; ++step)
    {
        /* the next lens or mirror in the light's direction (other elements let it through) */
        const int stepDirection = (direction == Direction::Right) ? 1 : -1;
        int next = index + stepDirection;
        while (next >= 0 && next < elementCount && !IsLensOrMirror(scene[next]))
            next += stepDirection;
        if (next < 0 || next >= elementCount) break;

        const Element& element = scene[next];
        const bool isMirror = element.type == ElementType::Mirror;
        if (isMirror && direction == Direction::Left) break;          /* the back of a mirror */

        const Direction leaving = isMirror ? Direction::Left : direction;
        Image image;
        image.formedBy = static_cast<Scene::Index>(next);
        const std::vector<Segment> leavingRays = LeavingRays(rays, element, leaving, layout.axisY);
        for (const Segment& ray : leavingRays)
            image.rayExits.push_back({ ray.x1, ray.y1 });
        if (image.rayExits.empty()) break;                            /* no light gets there */

        /* a lens acts at its x; a mirror at the vertex of its curved face */
        const double vertex = isMirror ? element.x + MirrorArcHalfWidth : element.x;
        double focalLength;
        if (isMirror)
            focalLength = vertex - element.focus;                     /* concave: focus in front */
        else
            focalLength = (element.shape == ShapeConvex ? 1.0 : -1.0) * (element.focus - element.x);

        const double objectDistance = (direction == Direction::Right) ? vertex - x : x - vertex;
        if (focalLength == 0 || objectDistance == 0) break;

        const double inverse = 1.0 / focalLength - 1.0 / objectDistance;
        if (std::abs(inverse) < 1e-9)
        {
            image.atInfinity = true;                                  /* object at the focal point */
            images.push_back(image);
            break;
        }

        const double imageDistance = 1.0 / inverse;
        const double m = -imageDistance / objectDistance;
        magnification *= m;

        /* a real image forms where the light goes: past a lens, in front of a mirror */
        image.real          = imageDistance > 0;
        image.x             = vertex + (leaving == Direction::Right ? imageDistance : -imageDistance);
        image.height        = height * m;
        image.magnification = magnification;
        image.raysMeeting   = RaysMeeting(leavingRays, image, layout.axisY);
        images.push_back(image);

        x         = image.x;
        height    = image.height;
        index     = next;
        direction = leaving;
    }
    return images;
}
