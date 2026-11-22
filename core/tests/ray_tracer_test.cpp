// Tests for the ray tracer (model/ray_tracer.h): pure geometry, no drawing.

#include "../model/optics.h"
#include "../model/ray_tracer.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

static Layout World()
{
    Layout world;
    UpdateLayout(world, 624, 441);                         // canvas 624 x 341, axis at 170
    return world;
}

// A cannon aimed straight along the axis (aim 0), and a mirror at x = 400.
static Scene CannonAndMirror(int mirrorWidth)
{
    Scene scene;
    const Layout world = World();
    scene.add({ ElementType::Cannon, 100, world.axisWindowY, 0, ShapeConvex, 0 });
    scene.add({ ElementType::Mirror, 400, world.axisWindowY, 340, ShapeConvex, mirrorWidth });
    return scene;
}

// Every segment that ends on the mirror's face ends at the first point of the body along
// the ray: inside the body, with the point one unit before it outside.
static void testRaysStopExactlyOnTheMirrorFace()
{
    const Layout world  = World();
    const Scene  scene  = CannonAndMirror(10);
    const Element& m    = scene[1];
    const std::vector<Segment> rays = TraceRays(scene, world, 0);

    int onFace = 0;
    for (const Segment& s : rays)
    {
        if (s.x1 != m.x || s.x2 <= m.x) continue;            // walks from the mirror's x onto its face
        ++onFace;
        CHECK(InsideMirrorBody(m, s.x2, s.y2, world.axisY));
        CHECK(!InsideMirrorBody(m, s.x2 - 1, s.y2, world.axisY));   // horizontal beam: same row
    }
    CHECK(onFace == CannonRays);
}

// A ray along the axis meets the face at its widest point. (The beam's rays are 10 apart,
// 5, 15, ... either side of the cannon, so a cannon 5 below the axis puts one on it.)
static void testTheAxisRayMeetsTheFaceAtItsVertex()
{
    const Layout world = World();
    Scene scene;
    scene.add({ ElementType::Cannon, 100, world.axisWindowY + 5, 0, ShapeConvex, 0 });
    scene.add({ ElementType::Mirror, 400, world.axisWindowY, 340, ShapeConvex, 10 });
    bool found = false;
    for (const Segment& s : TraceRays(scene, world, 0))
        if (s.x1 == 400 && s.y1 == world.axisY && s.y2 == world.axisY)
        {
            CHECK(s.x2 == 400 + MirrorArcHalfWidth + 1);
            found = true;
        }
    CHECK(found);
}

// Rays further from the axis meet the curved face closer to the mirror's x.
static void testOuterRaysMeetTheFaceEarlier()
{
    const Layout world = World();
    const Scene  scene = CannonAndMirror(0);                 // face 80 high
    int previousX = 1000, previousDistance = -1;
    for (const Segment& s : TraceRays(scene, world, 0))
    {
        if (s.x1 != 400 || s.x2 <= 400 || s.y2 < world.axisY) continue;   // the lower half of the beam
        const int distance = s.y2 - world.axisY;
        if (distance > previousDistance && previousDistance >= 0) CHECK(s.x2 <= previousX);
        previousDistance = distance;
        previousX = s.x2;
    }
}

// A ray that passes above or below the mirror misses the face and leaves the canvas.
static void testARayPassingTheMirrorMissesIt()
{
    const Layout world = World();
    Scene scene;
    scene.add({ ElementType::Cannon, 100, world.axisWindowY + 140, 0, ShapeConvex, 0 });  // beam 95..185 below the axis
    scene.add({ ElementType::Mirror, 400, world.axisWindowY, 340, ShapeConvex, 0 });      // face 80 high
    int walks = 0;
    for (const Segment& s : TraceRays(scene, world, 0))
        if (s.x1 == 400)
        {
            ++walks;
            CHECK(s.x2 == world.canvasWidth);                // ran to the edge: no reflection
        }
    CHECK(walks == CannonRays);
}

int main()
{
    testRaysStopExactlyOnTheMirrorFace();
    testTheAxisRayMeetsTheFaceAtItsVertex();
    testOuterRaysMeetTheFaceEarlier();
    testARayPassingTheMirrorMissesIt();

    if (failures == 0) std::printf("All ray tracer tests passed.\n");
    return failures == 0 ? 0 : 1;
}
