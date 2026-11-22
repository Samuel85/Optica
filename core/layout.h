#pragma once

#include <algorithm>

// Layout and geometry constants for Optica. All values are in pixels.
//
// Two coordinate systems are used:
//   window coordinates - client-area coordinates, as received in mouse messages
//                        and used when drawing directly on the window (hdc);
//   canvas coordinates - coordinates on the off-screen bitmap (memoria), which is
//                        copied to the window ToolbarHeight pixels below the top.
// canvas y = window y - ToolbarHeight.

// ---- Main window -----------------------------------------------------------
constexpr int WindowWidth  = 640;   // initial size
constexpr int WindowHeight = 480;
constexpr int MinClientWidth  = 560;  // smallest client area the layout supports (5 buttons)
constexpr int MinClientHeight = 400;

// ---- Canvas ----------------------------------------------------------------
constexpr int ToolbarHeight      = 50;  // top button bar; the canvas starts below it
constexpr int BottomBarHeight    = 50;  // bottom button bar
constexpr int BackBufferMargin   = 50;  // extra rows below the visible canvas in the off-screen bitmap
constexpr int CanvasBottomMargin = 10;  // mouse this close to the canvas bottom counts as outside

// ---- Toolbar buttons -------------------------------------------------------
constexpr int ButtonWidth     = 100;
constexpr int ButtonHeight    = 30;
constexpr int ButtonPitch     = 110;              // distance between the left edges of two buttons
constexpr int ButtonTopMargin = 10;

// ---- Optical axis ----------------------------------------------------------
constexpr int AxisTickHalf = 5;  // half-length of the focal-point ticks

// ---- Elements --------------------------------------------------------------
constexpr int ElementHalfHeight = 80;  // half-height of lenses and mirrors
constexpr int MarkerHalf        = 3;   // half-size of focal-point markers, also their grab tolerance
constexpr int GrabHalfWidth     = 10;  // horizontal grab tolerance for objects and lenses
constexpr int ArrowSize         = 5;   // arrow head on objects
constexpr int ImageLabelGap     = 4;   // pixels between an image's arrow and its label

constexpr int CannonHalfWidth   = 10;
constexpr int CannonHalfHeight  = 60;
constexpr int CannonFocusOffset = 30;  // x distance from the cannon to its aim marker
constexpr int CannonRangeHalf   = 110; // a cannon can be placed this far above or below the axis

constexpr int SourceHalfSize    = 10;
constexpr int SourceMinY        = ToolbarHeight + SourceHalfSize;  // 60

constexpr int MirrorArcHalfWidth = 50;
constexpr int MirrorBackWidth    = 60;  // depth of the mirror body
constexpr int MaxMirrorWidth     = 50;

constexpr int DefaultLensWidth   = 15;
constexpr int MinLensWidth       = 7;
constexpr int MaxLensWidth       = 15;
constexpr int DefaultFocalLength = 30;
constexpr int LensResizeRange    = 45;  // mouse within this distance of a lens changes its width

// ---- Rays ------------------------------------------------------------------
constexpr int MaxRays    = 10;
constexpr int ObjectRays = 3;
constexpr int CannonRays = 10;
constexpr int SourceRays = 10;

constexpr int CannonRaySpacing   = 10;
constexpr int CannonBeamHalfWidth = (CannonRays - 1) * CannonRaySpacing / 2;  // 45

constexpr int    SourceRaySpacing   = 2;
constexpr double SourceFirstSlope   = -1.0;
constexpr double SourceSlopeStep    = 0.25;

// ---- Values that depend on the window size ---------------------------------
// Recalculated by UpdateLayout() whenever the client area changes size.
struct Layout {
    int clientWidth      = WindowWidth;
    int clientHeight     = WindowHeight;
    int canvasWidth      = 640;  // width of the drawing area
    int canvasHeight     = 350;  // visible height of the drawing area
    int backBufferHeight = 400;  // height of the off-screen bitmap
    int canvasBottomY    = 390;  // window y below which the mouse is outside the canvas
    int axisY            = 180;  // optical axis, canvas coordinates
    int axisWindowY      = 230;  // optical axis, window coordinates
    int cannonMinY       = 120;  // window y range where a cannon can be placed
    int cannonMaxY       = 340;
};

inline void UpdateLayout(Layout& L, int clientWidth, int clientHeight)
{
    L.clientWidth      = clientWidth;
    L.clientHeight     = clientHeight;
    L.canvasWidth      = (std::max)(clientWidth, 1);
    L.canvasHeight     = (std::max)(clientHeight - ToolbarHeight - BottomBarHeight,
                                  MinClientHeight - ToolbarHeight - BottomBarHeight);
    L.backBufferHeight = L.canvasHeight + BackBufferMargin;
    L.canvasBottomY    = ToolbarHeight + L.canvasHeight - CanvasBottomMargin;
    L.axisY            = L.canvasHeight / 2;
    L.axisWindowY      = L.axisY + ToolbarHeight;
    L.cannonMinY       = L.axisWindowY - CannonRangeHalf;
    L.cannonMaxY       = L.axisWindowY + CannonRangeHalf;
}

// ---- Mode codes ------------------------------------------------------------
// The tool or element being placed or dragged (EditorController::tool_, preview_, focusTool_).
// Note: these differ from the element types (ElementType in scene.h) from LENS onwards.
enum Tool {
    TOOL_NONE       = 0,
    TOOL_CANNON     = 1,
    TOOL_OBJECT     = 2,
    TOOL_SOURCE     = 3,
    TOOL_CLEAR_ALL  = 4,
    TOOL_LENS       = 5,
    TOOL_MIRROR     = 6,
    TOOL_APERTURE   = 7,
    TOOL_CLEAR_LAST = 8
};

// Which mouse cursor to show (EditorController::cursor()).
enum CursorKind {
    CURSOR_NORMAL = 0,
    CURSOR_HAND   = 1,
    CURSOR_CROSS  = 2
};
