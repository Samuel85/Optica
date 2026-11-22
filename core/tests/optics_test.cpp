// Unit tests for the ray geometry in optics.h. Plain C++20, no Windows needed.

#include "../model/optics.h"

#include <cstdio>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

constexpr int Axis = 180;

static Element lens(int x, int focus, int shape = ShapeConvex)
{
    return Element{ ElementType::Lens, x, 0, focus, shape, 15 };
}

static Element mirror(int x, int focus, int width = 10)
{
    return Element{ ElementType::Mirror, x, 0, focus, ShapeConvex, width };
}

static void testArrivalX()
{
    CHECK(ArrivalX(lens(100, 130), Direction::Right) == 100);
    CHECK(ArrivalX(lens(100, 130), Direction::Left) == 100);
    CHECK(ArrivalX(mirror(200, 150), Direction::Right) == 200);
    CHECK(ArrivalX(mirror(200, 150), Direction::Left) == 200 + MirrorBackWidth);  // its back
}

static void testRounding()
{
    // floor(0.5 * 3) = 1, floor(-0.5 * 3) = -2
    CHECK(StraightY(Ray{ 0, 10, 0.5f }, 3) == 11);
    CHECK(StraightY(Ray{ 0, 10, -0.5f }, 3) == 8);
    // after a reflection the original rounds up: ceil(1.5 + 10) = 12, ceil(-1.5 + 10) = 9
    CHECK(ReflectedY(Ray{ 0, 10, 0.5f }, 3) == 12);
    CHECK(ReflectedY(Ray{ 0, 10, -0.5f }, 3) == 9);
    // a horizontal ray keeps its height
    CHECK(StraightY(Ray{ 50, 77, 0.0f }, 400) == 77);
}

static void testAperture()
{
    CHECK(InsideAperture(Axis, Axis));
    CHECK(InsideAperture(Axis + ElementHalfHeight - 1, Axis));
    CHECK(InsideAperture(Axis - ElementHalfHeight + 1, Axis));
    CHECK(!InsideAperture(Axis + ElementHalfHeight, Axis));
    CHECK(!InsideAperture(Axis - ElementHalfHeight, Axis));
}

static void testRefract()
{
    // A ray parallel to the axis, 30 px above it, meets a lens with focal length 30.
    const Ray parallel{ 100, Axis - 30, 0.0f };

    // Convex lens, travelling right: bent down by 30/30, so it crosses the axis at the focus.
    CHECK(Refract(parallel, lens(100, 130, ShapeConvex), Direction::Right, Axis) == 1.0f);
    // Concave lens: bent away from the axis.
    CHECK(Refract(parallel, lens(100, 130, ShapeConcave), Direction::Right, Axis) == -1.0f);
    // Seen from the other side, a convex lens acts as a concave one and vice versa.
    CHECK(Refract(parallel, lens(100, 130, ShapeConvex), Direction::Left, Axis) == -1.0f);
    CHECK(Refract(parallel, lens(100, 130, ShapeConcave), Direction::Left, Axis) == 1.0f);

    // A ray along the axis is not bent.
    CHECK(Refract(Ray{ 100, Axis, 0.25f }, lens(100, 130), Direction::Right, Axis) == 0.25f);
}

static void testReflect()
{
    // Ray 30 px above the axis hits the face at x = 100; the mirror's focus is at x = 70.
    CHECK(Reflect(Ray{ 100, Axis - 30, 0.0f }, mirror(160, 70), Axis) == -1.0f);
    // The incoming slope is mirrored.
    CHECK(Reflect(Ray{ 100, Axis, 0.5f }, mirror(160, 70), Axis) == -0.5f);
}

static void testHitsBack()
{
    const Element m = mirror(200, 150, 10);                // half height 80 + 10
    CHECK(HitsBack(Axis, m, Axis));
    CHECK(HitsBack(Axis + ElementHalfHeight + 10, m, Axis));
    CHECK(HitsBack(Axis - ElementHalfHeight - 10, m, Axis));
    CHECK(!HitsBack(Axis + ElementHalfHeight + 11, m, Axis));
    CHECK(!HitsBack(Axis - ElementHalfHeight - 11, m, Axis));
}

static void testMirrorBody()
{
    const Element m = mirror(200, 150, 10);                // face: ellipse 50 wide, 90 high
    // On the axis the face is at the ellipse's widest point, 50 in front of the back block.
    CHECK(!InsideMirrorBody(m, 200 + MirrorArcHalfWidth, Axis, Axis));
    CHECK(InsideMirrorBody(m, 200 + MirrorArcHalfWidth + 1, Axis, Axis));
    CHECK(InsideMirrorBody(m, 200 + MirrorBackWidth, Axis, Axis));         // its back edge
    CHECK(!InsideMirrorBody(m, 200 + MirrorBackWidth + 1, Axis, Axis));    // behind the mirror
    // Higher up the face curves towards the mirror's x: at 3/5 of the height, sqrt(1 - 0.36) = 0.8.
    CHECK(!InsideMirrorBody(m, 200 + 40, Axis - 54, Axis));
    CHECK(InsideMirrorBody(m, 200 + 41, Axis - 54, Axis));
    CHECK(InsideMirrorBody(m, 200 + 41, Axis + 54, Axis));                 // symmetric
    // At the top and bottom edges the whole block is body; above and below there is none.
    CHECK(InsideMirrorBody(m, 201, Axis - 90, Axis));
    CHECK(!InsideMirrorBody(m, 230, Axis - 91, Axis));
    CHECK(!InsideMirrorBody(m, 230, Axis + 91, Axis));
}

int main()
{
    testArrivalX();
    testRounding();
    testAperture();
    testRefract();
    testReflect();
    testHitsBack();
    testMirrorBody();

    if (failures == 0) std::printf("All optics tests passed.\n");
    return failures == 0 ? 0 : 1;
}
