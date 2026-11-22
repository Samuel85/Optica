#include "scene_painter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace {

// ---- colours (the GDI renderer's pens and brushes) --------------------------------

constexpr ImU32 Colour(int red, int green, int blue, int alpha = 255)
{
    return static_cast<ImU32>(red) | (static_cast<ImU32>(green) << 8) |
           (static_cast<ImU32>(blue) << 16) | (static_cast<ImU32>(alpha) << 24);
}

constexpr ImU32 Green              = Colour(100, 200, 2);     // elements and the axis
constexpr ImU32 Yellow             = Colour(255, 255, 0);     // rays
constexpr ImU32 Black              = Colour(0, 0, 0);
constexpr ImU32 White              = Colour(255, 255, 255);
constexpr ImU32 Red                = Colour(255, 0, 0);       // light sources
constexpr ImU32 PreviewGrey        = Colour(100, 100, 100);   // outline following the mouse
constexpr ImU32 RealImageColour    = Colour(0, 200, 255);     // cyan
constexpr ImU32 VirtualImageColour = Colour(255, 110, 220);   // magenta
constexpr ImU32 ConstructionColour = Colour(200, 90, 180);    // dotted lines to a virtual image

// GDI's dashed and dotted pens, in pixels: drawn, then skipped.
constexpr float DashOn = 18.0f, DashOff = 6.0f;
constexpr float DotOn  = 3.0f,  DotOff  = 3.0f;

constexpr float ThinLine       = 1.0f;
constexpr float RealImageWidth = 2.0f;
constexpr int   HalfEllipseSegments = 48;

constexpr double Pi = 3.14159265358979323846;

ImVec2 Add(ImVec2 a, ImVec2 b)      { return ImVec2(a.x + b.x, a.y + b.y); }
ImVec2 Scale(ImVec2 a, float k)     { return ImVec2(a.x * k, a.y * k); }

/* Points on the ellipse with this centre and these radii, from one angle to another, in
   degrees, measured as on the screen: 0 is to the right, 90 straight down. */
std::vector<ImVec2> EllipseArc(ImVec2 centre, ImVec2 radius, double fromDegrees, double toDegrees)
{
    const int segments = std::max(2, static_cast<int>(std::lround(HalfEllipseSegments * std::abs(toDegrees - fromDegrees) / 180.0)));
    std::vector<ImVec2> points;
    points.reserve(static_cast<std::size_t>(segments) + 1);
    for (int i = 0; i <= segments; ++i)
    {
        const double angle = (fromDegrees + (toDegrees - fromDegrees) * i / segments) * Pi / 180.0;
        points.emplace_back(centre.x + radius.x * static_cast<float>(std::cos(angle)),
                            centre.y + radius.y * static_cast<float>(std::sin(angle)));
    }
    return points;
}

/* A polyline drawn as dashes: `on` pixels drawn, `off` skipped, continuing across
   corners as a GDI pen does. */
void StrokeDashed(ImDrawList& list, const std::vector<ImVec2>& points, ImU32 colour,
                  float on, float off, float thickness = ThinLine)
{
    bool  drawing = true;
    float left    = on;          // of the current dash or gap
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const ImVec2 a = points[i - 1], b = points[i];
        const float  length = std::hypot(b.x - a.x, b.y - a.y);
        if (length <= 0.0f) continue;
        const ImVec2 direction = Scale(ImVec2(b.x - a.x, b.y - a.y), 1.0f / length);

        float done = 0.0f;
        while (done < length)
        {
            const float step = std::min(left, length - done);
            if (drawing)
                list.AddLine(Add(a, Scale(direction, done)), Add(a, Scale(direction, done + step)), colour, thickness);
            done += step;
            left -= step;
            if (left <= 0.0f)
            {
                drawing = !drawing;
                left    = drawing ? on : off;
            }
        }
    }
}

void StrokeDotted(ImDrawList& list, const std::vector<ImVec2>& points, ImU32 colour)
{
    StrokeDashed(list, points, colour, DotOn, DotOff);
}

/* A filled polygon that may be concave. Dear ImGui's triangulation needs the points in
   clockwise order on the screen, so they are put in that order first. */
void FillPolygon(ImDrawList& list, std::vector<ImVec2> points, ImU32 colour)
{
    double twiceArea = 0.0;            /* positive: clockwise, as y grows downwards */
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const ImVec2 a = points[i], b = points[(i + 1) % points.size()];
        twiceArea += static_cast<double>(a.x) * b.y - static_cast<double>(b.x) * a.y;
    }
    if (twiceArea < 0.0)
        std::reverse(points.begin(), points.end());
    list.AddConcavePolyFilled(points.data(), static_cast<int>(points.size()), colour);
}

void Append(std::vector<ImVec2>& to, const std::vector<ImVec2>& points)
{
    to.insert(to.end(), points.begin(), points.end());
}

} // namespace

ScenePainter::ScenePainter(const Layout& world, const Viewport& viewport, const ImVec2& origin)
    : layout_(world), viewport_(viewport), origin_(origin)
{
}

// ---- world -> screen ----------------------------------------------------------------

ImVec2 ScenePainter::canvasPoint(double x, double y) const
{
    return ImVec2(origin_.x + static_cast<float>(viewport_.offsetX + x * viewport_.scale) + 0.5f,
                  origin_.y + static_cast<float>(ToolbarHeight + viewport_.offsetY + y * viewport_.scale) + 0.5f);
}

ImVec2 ScenePainter::windowPoint(double x, double y) const
{
    return canvasPoint(x, y - ToolbarHeight);
}

float ScenePainter::length(double worldLength) const
{
    return static_cast<float>(worldLength * viewport_.scale);
}

// ---- the whole picture ----------------------------------------------------------------

/* As GdiEditorView::draw and the previews: black canvas, elements, rays, images, axis;
   then the outline under the mouse. The canvas is clipped to the area between the bars. */
void ScenePainter::paint(ImDrawList& list, const Picture& picture, const std::optional<Preview>& preview) const
{
    const ImVec2 canvasTopLeft(origin_.x, origin_.y + ToolbarHeight);
    const ImVec2 canvasBottomRight(origin_.x + viewport_.canvasWidth,
                                   origin_.y + ToolbarHeight + viewport_.canvasHeight);
    list.PushClipRect(canvasTopLeft, canvasBottomRight, false);
    list.AddRectFilled(canvasTopLeft, canvasBottomRight, Black);

    drawElements(list, picture.scene);
    for (const Segment& ray : picture.rays)
        list.AddLine(canvasPoint(ray.x1, ray.y1), canvasPoint(ray.x2, ray.y2), Yellow, ThinLine);
    for (const Image& image : picture.images)
        drawImage(list, image);
    drawAxis(list);

    list.PopClipRect();

    if (preview)
        drawPreview(list, *preview);
}

/* Across the whole drawing area, margins included, as on Windows. */
void ScenePainter::drawAxis(ImDrawList& list) const
{
    const float y = canvasPoint(0, layout_.axisY).y;
    list.AddLine(ImVec2(origin_.x, y), ImVec2(origin_.x + viewport_.canvasWidth, y), Green, ThinLine);
}

// ---- elements ---------------------------------------------------------------------------

/* Mirrors, then concave lenses, then everything else: the order of GdiRenderer::drawElements. */
void ScenePainter::drawElements(ImDrawList& list, const Scene& scene) const
{
    for (const Element& element : scene)
        if (element.type == ElementType::Mirror)
            drawMirror(list, element);

    for (const Element& element : scene)
        if (element.type == ElementType::Lens && element.shape != ShapeConvex)
            drawConcaveLens(list, element);

    for (const Element& element : scene)
        drawOther(list, element);
}

/* A white block, x to x + MirrorBackWidth, with the right half of an ellipse cut out of
   its left side: the curved reflecting face. Green outline on the block's straight sides. */
void ScenePainter::drawMirror(ImDrawList& list, const Element& mirror) const
{
    const int halfHeight = ElementHalfHeight + mirror.width;
    const int top = layout_.axisY - halfHeight, bottom = layout_.axisY + halfHeight;
    const int back = mirror.x + MirrorBackWidth;

    std::vector<ImVec2> body = EllipseArc(canvasPoint(mirror.x, layout_.axisY),
                                          ImVec2(length(MirrorArcHalfWidth), length(halfHeight)), -90.0, 90.0);
    body.push_back(canvasPoint(back, bottom));
    body.push_back(canvasPoint(back, top));
    FillPolygon(list, std::move(body), White);

    const ImVec2 outline[] = { canvasPoint(mirror.x, top), canvasPoint(back, top),
                               canvasPoint(back, bottom), canvasPoint(mirror.x, bottom) };
    list.AddPolyline(outline, 4, Green, ThinLine);

    focusTick(list, mirror.focus);
    focusMarker(list, mirror.focus, Black);
}

/* A white block, x - 2w to x + 2w, with half an ellipse cut out of each side. */
void ScenePainter::drawConcaveLens(ImDrawList& list, const Element& lens) const
{
    const int w = lens.width;
    const int top = layout_.axisY - ElementHalfHeight, bottom = layout_.axisY + ElementHalfHeight;
    const ImVec2 radius(length(w), length(ElementHalfHeight));

    std::vector<ImVec2> body = EllipseArc(canvasPoint(lens.x + 2 * w, layout_.axisY), radius, 270.0, 90.0);
    Append(body, EllipseArc(canvasPoint(lens.x - 2 * w, layout_.axisY), radius, 90.0, -90.0));
    FillPolygon(list, std::move(body), White);

    list.AddLine(canvasPoint(lens.x - 2 * w, top),    canvasPoint(lens.x + 2 * w, top),    Green, ThinLine);
    list.AddLine(canvasPoint(lens.x - 2 * w, bottom), canvasPoint(lens.x + 2 * w, bottom), Green, ThinLine);

    const int focalLength = lens.focus - lens.x;
    focusTick(list, lens.x - focalLength);
    focusTick(list, lens.x + focalLength);
    focusMarker(list, lens.x + focalLength, Black);
    focusMarker(list, lens.x - focalLength, Black);
}

void ScenePainter::drawOther(ImDrawList& list, const Element& element) const
{
    switch (element.type)
    {
    case ElementType::Cannon:
    {
        const ImVec2 centre = canvasPoint(element.x, element.y - ToolbarHeight);
        const ImVec2 radius(length(CannonHalfWidth), length(CannonHalfHeight));
        list.AddEllipseFilled(centre, radius, Black);
        list.AddEllipse(centre, radius, Green, 0.0f, 0, ThinLine);

        const int markerX = element.x + CannonFocusOffset, markerY = element.y + element.focus - ToolbarHeight;
        const ImVec2 a = canvasPoint(markerX - MarkerHalf, markerY - MarkerHalf);
        const ImVec2 b = canvasPoint(markerX + MarkerHalf, markerY + MarkerHalf);
        list.AddRectFilled(a, b, White);
        list.AddRect(a, b, Green);
        break;
    }
    case ElementType::Object:
        objectArrow(list, element.x, element.y - ToolbarHeight, element.y < layout_.axisWindowY + 10);
        break;

    case ElementType::Source:
    {
        const int y = element.y - ToolbarHeight;
        const ImVec2 a = canvasPoint(element.x - SourceHalfSize, y - SourceHalfSize);
        const ImVec2 b = canvasPoint(element.x + SourceHalfSize, y + SourceHalfSize);
        list.AddRectFilled(a, b, Red);
        list.AddRect(a, b, Green);
        break;
    }
    case ElementType::Lens:
        if (element.shape == ShapeConvex)
        {
            /* a white ellipse, outlined, with a line down its middle */
            const ImVec2 centre = canvasPoint(element.x, layout_.axisY);
            const ImVec2 radius(length(element.width), length(ElementHalfHeight));
            list.AddEllipseFilled(centre, radius, White);
            list.AddEllipse(centre, radius, Green, 0.0f, 0, ThinLine);
            list.AddLine(canvasPoint(element.x, layout_.axisY - ElementHalfHeight),
                         canvasPoint(element.x, layout_.axisY + ElementHalfHeight), Green, ThinLine);

            const int focalLength = element.focus - element.x;
            focusTick(list, element.x - focalLength);
            focusTick(list, element.x + focalLength);
            focusMarker(list, element.x + focalLength, White);
            focusMarker(list, element.x - focalLength, White);
        }
        break;

    default:   /* mirrors and concave lenses are drawn earlier; apertures are not used */
        break;
    }
}

void ScenePainter::objectArrow(ImDrawList& list, int x, int canvasY, bool pointsUp) const
{
    const int headY = pointsUp ? canvasY + ArrowSize : canvasY - ArrowSize;
    const ImVec2 tip = canvasPoint(x, canvasY);
    list.AddLine(canvasPoint(x, layout_.axisY), tip, Green, ThinLine);
    list.AddLine(tip, canvasPoint(x + ArrowSize, headY), Green, ThinLine);
    list.AddLine(tip, canvasPoint(x - ArrowSize, headY), Green, ThinLine);
}

void ScenePainter::focusTick(ImDrawList& list, int focusX) const
{
    list.AddLine(canvasPoint(focusX, layout_.axisY - AxisTickHalf),
                 canvasPoint(focusX, layout_.axisY + AxisTickHalf), Green, ThinLine);
}

void ScenePainter::focusMarker(ImDrawList& list, int focusX, ImU32 fill) const
{
    const ImVec2 a = canvasPoint(focusX - MarkerHalf, layout_.axisY - MarkerHalf);
    const ImVec2 b = canvasPoint(focusX + MarkerHalf, layout_.axisY + MarkerHalf);
    list.AddRectFilled(a, b, fill);
    list.AddRect(a, b, Green);
}

// ---- images -------------------------------------------------------------------------------

/* As GdiRenderer::drawImage: an arrow standing on the axis, solid cyan for a real image,
   dashed magenta for a virtual one with dotted lines back to where the rays leave the
   lens or mirror; then its label. Images off the canvas, and those fewer than two drawn
   rays meet at, are not drawn. */
void ScenePainter::drawImage(ImDrawList& list, const Image& image) const
{
    if (!ShownByRays(image)) return;

    const double tipY = layout_.axisY - image.height;
    if (image.x < 0 || image.x > layout_.canvasWidth ||
        tipY < -layout_.canvasHeight || tipY > 2 * layout_.canvasHeight)
        return;

    const int x   = static_cast<int>(std::lround(image.x));
    const int tip = static_cast<int>(std::lround(tipY));
    const ImVec2 tipPoint = canvasPoint(x, tip);

    if (!image.real)
        for (const ImagePoint& exit : image.rayExits)
            StrokeDotted(list, { canvasPoint(exit.x, exit.y), tipPoint }, ConstructionColour);

    const int headY = (image.height > 0) ? tip + ArrowSize : tip - ArrowSize;   /* points away from the axis */
    const std::vector<ImVec2> strokes[] = {
        { canvasPoint(x, layout_.axisY), tipPoint },
        { tipPoint, canvasPoint(x + ArrowSize, headY) },
        { tipPoint, canvasPoint(x - ArrowSize, headY) },
    };
    for (const std::vector<ImVec2>& stroke : strokes)
    {
        if (image.real)
            list.AddLine(stroke[0], stroke[1], RealImageColour, RealImageWidth);
        else
            StrokeDashed(list, stroke, VirtualImageColour, DashOn, DashOff);
    }

    drawImageLabel(list, image, tipPoint);
}

/* "real  m = -0.75" beside the tip, away from the axis; left of the arrow when it does not
   fit before the canvas's right edge. The text keeps its size when the window is zoomed. */
void ScenePainter::drawImageLabel(ImDrawList& list, const Image& image, ImVec2 tip) const
{
    char text[32];
    const int length = std::snprintf(text, sizeof text, "%s  m = %+.2f",
                                     image.real ? "real" : "virtual", image.magnification);
    if (length <= 0) return;

    const ImVec2 size  = ImGui::CalcTextSize(text);
    const float  gap   = static_cast<float>(ArrowSize + ImageLabelGap);
    const float  right = canvasPoint(layout_.canvasWidth, 0).x;
    const bool   fitsOnRight = tip.x + gap + size.x <= right;

    const ImVec2 position(fitsOnRight ? tip.x + gap : tip.x - gap - size.x,
                          image.height > 0 ? tip.y - size.y : tip.y);
    list.AddText(position, image.real ? RealImageColour : VirtualImageColour, text);
}

// ---- the outline following the mouse ---------------------------------------------------------

/* As the GdiRenderer previews: dotted grey outlines, in window coordinates. */
void ScenePainter::drawPreview(ImDrawList& list, const Preview& preview) const
{
    const int x = preview.x, y = preview.y, axis = layout_.axisWindowY;

    switch (preview.type)
    {
    case ElementType::Cannon:
    {
        const ImVec2 centre = windowPoint(x, y);
        const ImVec2 radius(length(CannonHalfWidth), length(CannonHalfHeight));
        list.AddEllipseFilled(centre, radius, Black);
        StrokeDotted(list, EllipseArc(centre, radius, 0.0, 360.0), PreviewGrey);
        break;
    }
    case ElementType::Object:
        StrokeDotted(list, { windowPoint(x, axis), windowPoint(x, y) }, PreviewGrey);
        if (y < axis)                          /* arrow head only above the axis */
        {
            StrokeDotted(list, { windowPoint(x, y), windowPoint(x + ArrowSize, y + ArrowSize) }, PreviewGrey);
            StrokeDotted(list, { windowPoint(x, y), windowPoint(x - ArrowSize, y + ArrowSize) }, PreviewGrey);
        }
        break;

    case ElementType::Source:
    {
        const ImVec2 a = windowPoint(x - SourceHalfSize, y - SourceHalfSize);
        const ImVec2 b = windowPoint(x + SourceHalfSize, y + SourceHalfSize);
        list.AddRectFilled(a, b, Red);
        StrokeDotted(list, { a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y), a }, PreviewGrey);
        break;
    }
    case ElementType::Lens:
    {
        const int w = preview.width;
        const int top = axis - ElementHalfHeight, bottom = axis + ElementHalfHeight;
        const ImVec2 radius(length(w), length(ElementHalfHeight));
        if (!preview.concave)
            StrokeDotted(list, EllipseArc(windowPoint(x, axis), radius, 0.0, 360.0), PreviewGrey);
        else
        {
            StrokeDotted(list, EllipseArc(windowPoint(x + 2 * w, axis), radius, 270.0, 90.0), PreviewGrey);
            StrokeDotted(list, EllipseArc(windowPoint(x - 2 * w, axis), radius, 90.0, -90.0), PreviewGrey);
            StrokeDotted(list, { windowPoint(x - 2 * w, top),    windowPoint(x + 2 * w, top) },    PreviewGrey);
            StrokeDotted(list, { windowPoint(x - 2 * w, bottom), windowPoint(x + 2 * w, bottom) }, PreviewGrey);
        }
        break;
    }
    case ElementType::Mirror:
    {
        const int halfHeight = ElementHalfHeight + preview.width;
        const int top = axis - halfHeight, bottom = axis + halfHeight;
        StrokeDotted(list, EllipseArc(windowPoint(x, axis), ImVec2(length(MirrorArcHalfWidth), length(halfHeight)),
                                      -90.0, 90.0), PreviewGrey);
        StrokeDotted(list, { windowPoint(x, top), windowPoint(x + MirrorBackWidth, top),
                             windowPoint(x + MirrorBackWidth, bottom), windowPoint(x, bottom) }, PreviewGrey);
        break;
    }
    default:
        break;
    }
}
