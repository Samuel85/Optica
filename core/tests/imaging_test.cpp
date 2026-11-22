// Tests for image formation (model/imaging.h): textbook cases, and agreement with the
// rays the ray tracer draws.

#include "../model/imaging.h"
#include "../model/optics.h"
#include "../model/ray_tracer.h"

#include <cmath>
#include <cstdio>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

static bool Near(double a, double b, double tolerance = 1e-6) { return std::abs(a - b) <= tolerance; }

static Layout World()
{
    Layout world;
    UpdateLayout(world, 624, 441);                         // canvas 624 x 341, axis at window y 220
    return world;
}

static Element ObjectAt(const Layout& world, int x, int height)
{
    return { ElementType::Object, x, world.axisWindowY - height, 0, ShapeConvex, 0 };
}

static Element Lens(int x, int focalLength, int shape = ShapeConvex)
{
    return { ElementType::Lens, x, 0, x + focalLength, shape, DefaultLensWidth };
}

// ---- one convex lens, f = 30 at x = 300 ------------------------------------------------

static void testObjectAtTwiceTheFocalLength()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 240, 40));                   // do = 60 = 2f
    scene.add(Lens(300, 30));
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(images[0].real);
    CHECK(Near(images[0].x, 360));                         // di = 2f, after the lens
    CHECK(Near(images[0].height, -40));                    // same size, inverted
    CHECK(Near(images[0].magnification, -1));
    CHECK(images[0].formedBy == 1);
}

static void testObjectBetweenFAndTwiceF()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 255, 20));                   // do = 45
    scene.add(Lens(300, 30));
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(images[0].real);
    CHECK(Near(images[0].x, 390));                         // di = 1 / (1/30 - 1/45) = 90
    CHECK(Near(images[0].magnification, -2));              // magnified, inverted
    CHECK(Near(images[0].height, -40));
}

static void testObjectInsideTheFocalLengthGivesAVirtualImage()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 285, 30));                   // do = 15 = f/2
    scene.add(Lens(300, 30));
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(!images[0].real);
    CHECK(Near(images[0].x, 270));                         // di = -30: before the lens
    CHECK(Near(images[0].magnification, 2));               // upright, twice the size
    CHECK(Near(images[0].height, 60));
    CHECK(!images[0].rayExits.empty());                    // for the dashed construction lines
}

static void testObjectAtTheFocalPoint()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 270, 30));                   // do = f
    scene.add(Lens(300, 30));
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(images[0].atInfinity);
}

static void testConcaveLensAlwaysGivesASmallerVirtualImage()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 240, 30));                   // do = 60
    scene.add(Lens(300, 30, ShapeConcave));                // f = -30
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(!images[0].real);
    CHECK(Near(images[0].x, 280));                         // di = 1 / (-1/30 - 1/60) = -20
    CHECK(Near(images[0].magnification, 1.0 / 3));
    CHECK(Near(images[0].height, 10));
}

// ---- a concave mirror: face vertex at x + 50, focus 40 in front of it ------------------

static void testMirrorImageInFront()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 370, 30));                   // vertex 450: do = 80 = 2f
    scene.add({ ElementType::Mirror, 400, world.axisWindowY, 410, ShapeConvex, 10 });
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(images[0].real);
    CHECK(Near(images[0].x, 370));                         // di = 80, in front of the mirror
    CHECK(Near(images[0].height, -30));
}

static void testMirrorVirtualImageBehind()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 390, 20));                   // vertex 450, f = 120: do = 60 = f/2
    scene.add({ ElementType::Mirror, 400, world.axisWindowY, 330, ShapeConvex, 10 });
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(!images[0].real);
    CHECK(Near(images[0].x, 570));                         // di = -120: behind the mirror
    CHECK(Near(images[0].magnification, 2));
    CHECK(Near(images[0].height, 40));
}

// ---- two lenses: the first image is the second lens's object ---------------------------

static void testTwoLensesInARow()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 240, 40));
    scene.add(Lens(300, 30));                              // image 1: x 360, height -40
    scene.add(Lens(450, 30));                              // do = 90: di = 45, m = -1/2
    const auto images = FormImages(scene, world, 0);

    CHECK(images.size() == 2);
    if (images.size() != 2) return;
    CHECK(Near(images[1].x, 495));
    CHECK(Near(images[1].height, 20));                     // upright again
    CHECK(Near(images[1].magnification, 0.5));
    CHECK(images[1].real && images[1].formedBy == 2);
}

static void testNoLensMeansNoImage()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 240, 40));
    CHECK(FormImages(scene, world, 0).empty());
}

// ---- agreement with the ray tracer -------------------------------------------------------

// The rays the tracer draws after a lens pass through the computed real image's tip.
// (A principal ray that reaches the lens beyond its edge is not bent, so is not counted.)
static void testTracedRaysMeetAtTheRealImage()
{
    const Layout world = World();
    for (int objectX : { 180, 220, 240, 255 })             // do = 120, 80, 60, 45
    {
        Scene scene;
        scene.add(ObjectAt(world, objectX, 40));
        scene.add(Lens(300, 30));
        const Image image = FormImages(scene, world, 0).at(0);
        const double tipY = world.axisY - image.height;

        int checked = 0;
        for (const Segment& s : TraceRays(scene, world, 0))
        {
            if (s.x1 != 300 || s.x2 <= 300) continue;        // the rays leaving the lens...
            if (!InsideAperture(s.y1, world.axisY)) continue; // ...that it bends (not those past its edge)
            const double yAtImage = s.y1 + (s.y2 - s.y1) * (image.x - s.x1) / (s.x2 - s.x1);
            CHECK(std::abs(yAtImage - tipY) <= 2.0);         // integer rounding of the drawn rays
            ++checked;
        }
        CHECK(checked >= 2);                                 // two rays already fix the image point
    }
}

// For a virtual image, the refracted rays extended backwards meet at its tip.
static void testRefractedRaysExtendBackToTheVirtualImage()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 285, 30));
    scene.add(Lens(300, 30));
    const Image image = FormImages(scene, world, 0).at(0);
    const double tipY = world.axisY - image.height;

    int checked = 0;
    for (const Segment& s : TraceRays(scene, world, 0))
    {
        if (s.x1 != 300 || s.x2 <= 300) continue;
        const double slope = static_cast<double>(s.y2 - s.y1) / (s.x2 - s.x1);
        CHECK(std::abs(s.y1 + slope * (image.x - s.x1) - tipY) <= 2.0);
        ++checked;
    }
    CHECK(checked == 3);
}

// A sweep over focal lengths and object distances: wherever the image is on the canvas,
// the rays the lens bends meet at its tip (real) or seem to come from it (virtual).
static void testTracedRaysAgreeAcrossManySetups()
{
    const Layout world = World();
    int setups = 0;
    for (int f : { 20, 30, 50, 80 })
        for (int objectDistance = 6; objectDistance <= 200; objectDistance += 7)
        {
            if (std::abs(objectDistance - f) < 4) continue;            /* image far off the canvas */
            Scene scene;
            scene.add(ObjectAt(world, 300 - objectDistance, 30));
            scene.add(Lens(300, f));
            const auto images = FormImages(scene, world, 0);
            if (images.size() != 1 || images[0].atInfinity) { CHECK(false); continue; }
            const Image& image = images[0];
            if (image.x < 0 || image.x > world.canvasWidth) continue;
            const double tipY = world.axisY - image.height;
            ++setups;

            int bent = 0;
            for (const Segment& s : TraceRays(scene, world, 0))
            {
                if (s.x1 != 300 || s.x2 <= 300 || !InsideAperture(s.y1, world.axisY)) continue;
                const double slope = static_cast<double>(s.y2 - s.y1) / (s.x2 - s.x1);
                const double distance = std::abs(image.x - s.x1);
                /* the drawn rays are rounded to whole pixels at both ends */
                CHECK(std::abs(s.y1 + slope * (image.x - s.x1) - tipY) <= 1.5 + distance / 150.0);
                ++bent;
            }
            /* every ray the lens bends is counted as meeting at the image, and so the image
               is shown exactly when at least two of them reach the lens */
            CHECK(image.raysMeeting == bent);
            CHECK(ShownByRays(image) == (bent >= 2));
        }
    CHECK(setups > 50);
}

// ---- which images are shown: those at least two drawn rays meet at ----------------------

static void testTextbookImagesAreShown()
{
    const Layout world = World();
    for (int objectX : { 240, 285 })                         // real (do = 60), virtual (do = 15)
    {
        Scene scene;
        scene.add(ObjectAt(world, objectX, 30));
        scene.add(Lens(300, 30));
        const auto images = FormImages(scene, world, 0);
        CHECK(images.size() == 1);
        if (images.size() == 1)
        {
            CHECK(images[0].raysMeeting == 3);               // all three principal rays
            CHECK(ShownByRays(images[0]));
        }
    }
}

// An object taller than the lens: the ray parallel to the axis passes above the lens and
// the ray through the focal point below it; only the ray through the centre is bent.
static void testAnImageOnlyOneRayReachesIsNotShown()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 255, 90));                     // do = 45: image at 390, 180 below the axis
    scene.add(Lens(300, 30));
    const auto images = FormImages(scene, world, 0);
    CHECK(images.size() == 1);
    if (images.size() != 1) return;
    CHECK(images[0].real);
    CHECK(Near(images[0].x, 390.0));                         // still computed...
    CHECK(images[0].raysMeeting == 1);
    CHECK(!ShownByRays(images[0]));                          // ...but not shown
}

// The first lens would form a real image at x = 342.9, but a second lens at 320 bends
// the rays before they meet there: that image is not shown. The second lens forms its
// own image from it, which the rays do reach.
static void testRaysStoppedBeforeTheyMeetShowNoImage()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 200, 30));
    scene.add(Lens(300, 30));
    scene.add(Lens(320, 30));
    const auto images = FormImages(scene, world, 0);
    CHECK(images.size() == 2);
    if (images.size() != 2) return;
    CHECK(images[0].real && images[0].x > 320);              // where the rays would have met
    CHECK(images[0].raysMeeting == 0);
    CHECK(!ShownByRays(images[0]));
    CHECK(ShownByRays(images[1]));
}

static void testAnImageAtInfinityIsNotShown()
{
    const Layout world = World();
    Scene scene;
    scene.add(ObjectAt(world, 270, 30));                     // at the focal point
    scene.add(Lens(300, 30));
    const auto images = FormImages(scene, world, 0);
    CHECK(images.size() == 1 && images[0].atInfinity);
    if (!images.empty()) CHECK(!ShownByRays(images[0]));
}

int main()
{
    testObjectAtTwiceTheFocalLength();
    testObjectBetweenFAndTwiceF();
    testObjectInsideTheFocalLengthGivesAVirtualImage();
    testObjectAtTheFocalPoint();
    testConcaveLensAlwaysGivesASmallerVirtualImage();
    testMirrorImageInFront();
    testMirrorVirtualImageBehind();
    testTwoLensesInARow();
    testNoLensMeansNoImage();
    testTracedRaysMeetAtTheRealImage();
    testRefractedRaysExtendBackToTheVirtualImage();
    testTracedRaysAgreeAcrossManySetups();
    testTextbookImagesAreShown();
    testAnImageOnlyOneRayReachesIsNotShown();
    testRaysStoppedBeforeTheyMeetShowNoImage();
    testAnImageAtInfinityIsNotShown();

    if (failures == 0) std::printf("All imaging tests passed.\n");
    return failures == 0 ? 0 : 1;
}
