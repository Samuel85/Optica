#pragma once

// Draws what ImGuiEditorView holds with a Dear ImGui draw list: the canvas (elements,
// rays, images, axis) between the two button bars, and the outline following the
// mouse. It draws the same shapes in the same colours as the Windows GdiRenderer:
//
//   GDI                     here
//   Chord / Ellipse / Arc   polygons and polylines through points on the ellipse
//   PS_DASH / PS_DOT pens   lines cut into dashes (ImGui has no dashed lines)
//   TextOut                 AddText
//
// Everything arrives in world coordinates and is zoomed to the window with the same
// Viewport as on Windows, then placed at `origin` (the corner of the window's safe area).

#include <optional>
#include <vector>

#include "imgui.h"

#include "imgui_editor_view.h"
#include "layout.h"
#include "view/viewport.h"

class ScenePainter {
public:
    ScenePainter(const Layout& world, const Viewport& viewport, const ImVec2& origin);

    void paint(ImDrawList& list, const Picture& picture, const std::optional<Preview>& preview) const;

private:
    // ---- the canvas ------------------------------------------------------------
    void drawElements(ImDrawList& list, const Scene& scene) const;
    void drawMirror(ImDrawList& list, const Element& mirror) const;
    void drawConcaveLens(ImDrawList& list, const Element& lens) const;
    void drawOther(ImDrawList& list, const Element& element) const;
    void drawImage(ImDrawList& list, const Image& image) const;
    void drawImageLabel(ImDrawList& list, const Image& image, ImVec2 tip) const;
    void drawAxis(ImDrawList& list) const;

    void objectArrow(ImDrawList& list, int x, int canvasY, bool pointsUp) const;
    void focusTick(ImDrawList& list, int focusX) const;
    void focusMarker(ImDrawList& list, int focusX, ImU32 fill) const;

    // ---- the outline following the mouse ------------------------------------------
    void drawPreview(ImDrawList& list, const Preview& preview) const;

    // ---- world -> screen ---------------------------------------------------------
    // Canvas coordinates (as the model's) and window coordinates (as the mouse's), both
    // zoomed by the viewport; the result is the centre of a pixel, so lines stay crisp.
    ImVec2 canvasPoint(double x, double y) const;
    ImVec2 windowPoint(double x, double y) const;
    float  length(double worldLength) const;

    const Layout&   layout_;
    const Viewport& viewport_;
    const ImVec2&   origin_;
};
