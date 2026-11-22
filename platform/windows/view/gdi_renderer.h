#pragma once

// Draws Optica with GDI: the canvas (an off-screen bitmap), the elements, the rays and
// the previews that follow the mouse. Owns the device contexts, the bitmap and the pens
// and brushes, and releases them when destroyed.
//
// Two surfaces are drawn on:
//   the canvas  - the off-screen bitmap; present() copies it to the window
//                 ToolbarHeight pixels below the top;
//   the window  - drawn directly (previews under the mouse).
// Everything is given in world coordinates (see viewport.h) and zoomed to the window
// on the way to GDI, by the small drawing helpers at the end of the class.

#include <windows.h>

#include <vector>

#include "layout.h"
#include "model/imaging.h"
#include "model/ray_tracer.h"
#include "model/scene.h"
#include "view/viewport.h"

// Owns a GDI object (pen, brush, bitmap) and deletes it when destroyed.
template <class Handle>
class GdiObject {
public:
    GdiObject() = default;
    explicit GdiObject(Handle handle) : handle_(handle) {}
    ~GdiObject() { if (handle_ != nullptr) DeleteObject(handle_); }

    GdiObject(const GdiObject&) = delete;
    GdiObject& operator=(const GdiObject&) = delete;

    Handle get() const { return handle_; }
    void reset(Handle handle) { if (handle_ != nullptr) DeleteObject(handle_); handle_ = handle; }

private:
    Handle handle_ = nullptr;
};

// Selects an object into a device context and selects the previous one back when it
// goes out of scope.
class SelectionScope {
public:
    SelectionScope(HDC dc, HGDIOBJ object) : dc_(dc), previous_(SelectObject(dc, object)) {}
    ~SelectionScope() { SelectObject(dc_, previous_); }

    SelectionScope(const SelectionScope&) = delete;
    SelectionScope& operator=(const SelectionScope&) = delete;

private:
    HDC     dc_;
    HGDIOBJ previous_;
};

class GdiRenderer {
public:
    enum class Pen   { Solid, Light, Black, Dotted };
    enum class Brush { White, Black, Red };

    GdiRenderer(HWND window, const Layout& world, const Viewport& viewport);
    ~GdiRenderer();

    GdiRenderer(const GdiRenderer&) = delete;
    GdiRenderer& operator=(const GdiRenderer&) = delete;

    // ---- whole canvas ----------------------------------------------------
    void resize();                                      // new off-screen bitmap for the current viewport
    void clearCanvas();                                 // fill with black
    void drawAxis();                                    // the optical axis
    void present();                                     // copy the canvas to the window
    void paint();                                       // WM_PAINT: BeginPaint, axis, present, EndPaint
    void drawElements(const Scene& scene);              // every element, without rays
    void drawRays(const std::vector<Segment>& rays);    // rays from one source
    void drawImage(const Image& image);                 // an image an object forms through a lens or mirror

    // ---- previews on the window, following the mouse (world window coordinates) ----
    void previewCannon(int x, int y);
    void previewObject(int x, int y);
    void previewSource(int x, int y);
    void previewLens(int x, int width, bool concave);
    void previewMirror(int x, int width);

private:
    // ---- canvas drawing state (the edit code selects these explicitly) ---
    void selectCanvas(Pen pen);
    void selectCanvas(Brush brush);
    SelectionScope useCanvas(Pen pen);                  // selected until the scope ends

    // ---- element shapes on the canvas (x in window coordinates = canvas x) ----
    void objectArrow(int x, int canvasY, bool pointsUp); // line from the axis, arrow head at the tip
    void convexLensShape(int x, int width);
    void concaveLensShape(int x, int width);
    void focusTicks(int x, int focalLength);            // ticks at x - f and x + f
    void focusTick(int focusX);
    void mirrorBody(int x, int width);                  // the white block
    void mirrorFace(int x, int width);                  // the curved reflecting face

    // ---- drawing in world coordinates, zoomed to pixels ------------------------
    enum class Surface { Canvas, Window };
    struct Pixel { int x, y; };
    HDC   dc(Surface surface) const { return surface == Surface::Canvas ? canvas_ : windowDc_; }
    Pixel toPixel(Surface surface, int x, int y) const;
    void  moveTo(Surface surface, int x, int y);
    void  lineTo(Surface surface, int x, int y);
    void  rectangle(Surface surface, int left, int top, int right, int bottom);
    void  ellipse(Surface surface, int left, int top, int right, int bottom);
    void  chord(Surface surface, int left, int top, int right, int bottom, int x1, int y1, int x2, int y2);
    void  arc(Surface surface, int left, int top, int right, int bottom, int x1, int y1, int x2, int y2);

    void  imageLabel(const Image& image, int x, int tip); // "real  m = -0.75" beside an image's tip

    HPEN   pen(Pen pen) const;
    HBRUSH brush(Brush brush) const;
    void   focusMarkers(int x, int focalLength);
    void   drawMirror(const Element& mirror);
    void   drawConcaveLens(const Element& lens);
    void   drawOther(const Element& element);

    HWND            window_;
    const Layout&   layout_;                           // the world
    const Viewport& viewport_;                         // how the world is zoomed into the window
    HDC           windowDc_;
    HDC           canvas_;
    HBITMAP       originalBitmap_ = nullptr;           // the canvas DC's own bitmap, put back at the end
    GdiObject<HBITMAP> bitmap_;

    LOGBRUSH redStyle_, blackStyle_, whiteStyle_;
    GdiObject<HBRUSH>  redBrush_, blackBrush_, whiteBrush_;
    GdiObject<HPEN>    solidPen_, lightPen_, blackPen_, dottedPen_;
    GdiObject<HPEN>    realImagePen_, virtualImagePen_, constructionPen_;

};
