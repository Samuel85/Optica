#pragma once

// Maps the scene's "world" coordinates to the window's pixels, so the bench zooms with
// the window and keeps its shape.
//
// The world is the layout the window had when it was created (Layout: canvas size,
// axis, ...); the scene, the controller and the ray tracer only ever see world
// coordinates. When the window is resized, the whole world canvas is scaled by one
// factor, the largest that fits the new canvas, and centred: spare space at the sides
// (wide window) or at the bottom and top (tall window) stays empty.
//
// Like the world, the device has two coordinate systems: window pixels (as in mouse
// messages) and canvas pixels (the off-screen bitmap, ToolbarHeight pixels lower).

#include <algorithm>
#include <cmath>

#include "../layout.h"

struct Viewport {
    int    clientWidth  = 0;   // window client area, in pixels
    int    clientHeight = 0;
    int    canvasWidth  = 0;   // drawing area between the button bars, in pixels
    int    canvasHeight = 0;
    double scale        = 1.0; // pixels per world unit
    int    offsetX      = 0;   // where the world canvas starts inside the canvas, in pixels
    int    offsetY      = 0;

    // ---- world -> pixels -------------------------------------------------------
    int toPixelX(int worldX) const
    {
        return offsetX + static_cast<int>(std::lround(worldX * scale));
    }
    int toCanvasPixelY(int worldCanvasY) const
    {
        return offsetY + static_cast<int>(std::lround(worldCanvasY * scale));
    }
    int toWindowPixelY(int worldWindowY) const
    {
        return ToolbarHeight + toCanvasPixelY(worldWindowY - ToolbarHeight);
    }

    // ---- pixels -> world (for the mouse) ----------------------------------------
    int toWorldX(int pixelX) const
    {
        return static_cast<int>(std::lround((pixelX - offsetX) / scale));
    }
    int toWorldWindowY(int pixelY) const
    {
        return ToolbarHeight + static_cast<int>(std::lround((pixelY - ToolbarHeight - offsetY) / scale));
    }
};

// Fits the world canvas of `world` into a client area of the given size.
inline void UpdateViewport(Viewport& viewport, const Layout& world, int clientWidth, int clientHeight)
{
    viewport.clientWidth  = clientWidth;
    viewport.clientHeight = clientHeight;
    viewport.canvasWidth  = (std::max)(clientWidth, 1);
    viewport.canvasHeight = (std::max)(clientHeight - ToolbarHeight - BottomBarHeight,
                                       MinClientHeight - ToolbarHeight - BottomBarHeight);

    const double scaleX = static_cast<double>(viewport.canvasWidth)  / world.canvasWidth;
    const double scaleY = static_cast<double>(viewport.canvasHeight) / world.canvasHeight;
    viewport.scale   = (std::min)(scaleX, scaleY);
    viewport.offsetX = static_cast<int>(std::lround((viewport.canvasWidth  - world.canvasWidth  * viewport.scale) / 2));
    viewport.offsetY = static_cast<int>(std::lround((viewport.canvasHeight - world.canvasHeight * viewport.scale) / 2));
}
