// Unit tests for Viewport (view/viewport.h): how the world is zoomed into the window.

#include "../view/viewport.h"

#include <cstdio>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

// The world: a window created with a 624 x 441 client area (canvas 624 x 341).
static Layout World()
{
    Layout world;
    UpdateLayout(world, 624, 441);
    return world;
}

static void testTheStartingSizeIsNotZoomed()
{
    const Layout world = World();
    Viewport v;
    UpdateViewport(v, world, 624, 441);
    CHECK(v.scale == 1.0);
    CHECK(v.offsetX == 0 && v.offsetY == 0);
    CHECK(v.toPixelX(300) == 300);
    CHECK(v.toWindowPixelY(200) == 200);
    CHECK(v.toWorldX(300) == 300 && v.toWorldWindowY(200) == 200);
}

static void testTwiceTheSizeDrawsTwiceAsBig()
{
    const Layout world = World();
    Viewport v;
    UpdateViewport(v, world, 2 * 624, 2 * 341 + ToolbarHeight + BottomBarHeight);
    CHECK(v.scale == 2.0);
    CHECK(v.offsetX == 0 && v.offsetY == 0);
    CHECK(v.toPixelX(100) == 200);
    CHECK(v.toCanvasPixelY(world.axisY) == 2 * world.axisY);
    CHECK(v.toWindowPixelY(world.axisWindowY) == ToolbarHeight + 2 * world.axisY);
}

static void testAWideWindowCentresTheBench()
{
    const Layout world = World();
    Viewport v;
    UpdateViewport(v, world, 1920, 1080);                 // canvas 1920 x 980
    const double expected = 980.0 / 341;                  // the height decides
    CHECK(v.scale > expected - 1e-9 && v.scale < expected + 1e-9);
    CHECK(v.offsetY == 0);
    CHECK(v.offsetX > 0);
    CHECK(v.toPixelX(0) == v.offsetX);                     // equal bands left and right
    CHECK(v.canvasWidth - v.toPixelX(world.canvasWidth) - v.offsetX <= 1);
}

static void testATallWindowCentresTheBenchVertically()
{
    const Layout world = World();
    Viewport v;
    UpdateViewport(v, world, 624, 1200);                  // canvas 624 x 1100: the width decides
    CHECK(v.scale == 1.0);
    CHECK(v.offsetX == 0);
    CHECK(v.offsetY == (1100 - 341) / 2 || v.offsetY == (1100 - 341 + 1) / 2);
}

static void testTheMouseLandsWhereItPoints()
{
    const Layout world = World();
    Viewport v;
    UpdateViewport(v, world, 1920, 1080);
    for (int x = 0; x <= world.canvasWidth; x += 7)       // zoomed in: every world point is reachable
        CHECK(v.toWorldX(v.toPixelX(x)) == x);
    for (int y = ToolbarHeight; y <= world.canvasBottomY; y += 5)
        CHECK(v.toWorldWindowY(v.toWindowPixelY(y)) == y);
}

static void testZoomedOut()
{
    const Layout world = World();
    Viewport v;
    UpdateViewport(v, world, MinClientWidth, MinClientHeight);
    CHECK(v.scale < 1.0);
    CHECK(v.toPixelX(world.canvasWidth) <= v.canvasWidth);
    CHECK(v.toCanvasPixelY(world.canvasHeight) <= v.canvasHeight);
}

int main()
{
    testTheStartingSizeIsNotZoomed();
    testTwiceTheSizeDrawsTwiceAsBig();
    testAWideWindowCentresTheBench();
    testATallWindowCentresTheBenchVertically();
    testTheMouseLandsWhereItPoints();
    testZoomedOut();

    if (failures == 0) std::printf("All viewport tests passed.\n");
    return failures == 0 ? 0 : 1;
}
