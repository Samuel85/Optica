#include "gdi_renderer.h"

#include <cmath>
#include <cstdio>

static const COLORREF RealImageColour    = RGB(0, 200, 255);    /* cyan */
static const COLORREF VirtualImageColour = RGB(255, 110, 220);  /* magenta */

namespace {

LOGBRUSH SolidBrush(COLORREF colour)
{
    LOGBRUSH style{};
    style.lbStyle = BS_SOLID;
    style.lbColor = colour;
    return style;
}

} // namespace

GdiRenderer::GdiRenderer(HWND window, const Layout& world, const Viewport& viewport)
    : window_(window),
      layout_(world),
      viewport_(viewport),
      windowDc_(GetDC(window)),                       /* private DC (CS_OWNDC), kept for the life of the window */
      canvas_(CreateCompatibleDC(windowDc_)),
      redStyle_(SolidBrush(RGB(255, 0, 0))),
      blackStyle_(SolidBrush(RGB(0, 0, 0))),
      whiteStyle_(SolidBrush(RGB(255, 255, 255))),
      redBrush_(CreateBrushIndirect(&redStyle_)),
      blackBrush_(CreateBrushIndirect(&blackStyle_)),
      whiteBrush_(CreateBrushIndirect(&whiteStyle_)),
      solidPen_(CreatePen(PS_SOLID, 1, RGB(100, 200, 2))),
      lightPen_(CreatePen(PS_SOLID, 1, RGB(255, 255, 0))),
      blackPen_(CreatePen(PS_SOLID, 1, RGB(0, 0, 0))),
      dottedPen_(CreatePen(PS_DOT, 1, RGB(100, 100, 100))),
      realImagePen_(CreatePen(PS_SOLID, 2, RealImageColour)),
      virtualImagePen_(CreatePen(PS_DASH, 1, VirtualImageColour)),
      constructionPen_(CreatePen(PS_DOT, 1, RGB(200, 90, 180)))
{
    SelectObject(canvas_, solidPen_.get());
    SelectObject(windowDc_, dottedPen_.get());
    SetBkMode(canvas_, TRANSPARENT);      /* the gaps in dashed and dotted lines, and around
                                             text, stay see-through */
    SelectObject(canvas_, GetStockObject(DEFAULT_GUI_FONT));   /* stock: never deleted */
}

GdiRenderer::~GdiRenderer()
{
    if (bitmap_.get() != nullptr)
    {
        SelectObject(canvas_, originalBitmap_);
        bitmap_.reset(nullptr);
    }
    DeleteDC(canvas_);
    ReleaseDC(window_, windowDc_);
    /* the pens and brushes are deleted by their GdiObject members */
}

HPEN GdiRenderer::pen(Pen which) const
{
    switch (which)
    {
    case Pen::Solid:  return solidPen_.get();
    case Pen::Light:  return lightPen_.get();
    case Pen::Black:  return blackPen_.get();
    case Pen::Dotted: return dottedPen_.get();
    }
    return solidPen_.get();
}

HBRUSH GdiRenderer::brush(Brush which) const
{
    switch (which)
    {
    case Brush::White: return whiteBrush_.get();
    case Brush::Black: return blackBrush_.get();
    case Brush::Red:   return redBrush_.get();
    }
    return whiteBrush_.get();
}

// ---- whole canvas ------------------------------------------------------------

void GdiRenderer::resize()
{
    HBITMAP newBitmap = CreateCompatibleBitmap(windowDc_, viewport_.canvasWidth,
                                               viewport_.canvasHeight + BackBufferMargin);
    if (newBitmap == nullptr) return;   /* keep the old bitmap if there is not enough memory */

    HBITMAP previousBitmap = static_cast<HBITMAP>(SelectObject(canvas_, newBitmap));
    if (bitmap_.get() == nullptr) originalBitmap_ = previousBitmap;   /* first time: the DC's own bitmap */
    bitmap_.reset(newBitmap);
}

void GdiRenderer::clearCanvas()
{
    RECT canvasRect = { 0, 0, viewport_.canvasWidth, viewport_.canvasHeight };   /* in pixels: the whole canvas */
    FillRect(canvas_, &canvasRect, blackBrush_.get());
}

/* The axis runs across the whole drawing area, including the empty margins the zoom
   leaves at the sides of the bench when the window is wider than the bench's shape. */
void GdiRenderer::drawAxis()
{
    const int y = viewport_.toCanvasPixelY(layout_.axisY);
    MoveToEx(canvas_, 0, y, nullptr);
    LineTo(canvas_, viewport_.canvasWidth, y);
}

void GdiRenderer::present()
{
    BitBlt(windowDc_, 0, ToolbarHeight, viewport_.canvasWidth, viewport_.canvasHeight, canvas_, 0, 0, SRCCOPY);
}

void GdiRenderer::paint()
{
    PAINTSTRUCT paintInfo;
    windowDc_ = BeginPaint(window_, &paintInfo);   /* CS_OWNDC: the same DC every time */
    SelectObject(windowDc_, dottedPen_.get());
    drawAxis();
    present();
    EndPaint(window_, &paintInfo);
}

void GdiRenderer::drawRays(const std::vector<Segment>& rays)
{
    if (rays.empty()) return;

    SelectionScope rayPen(canvas_, lightPen_.get());
    for (const Segment& segment : rays)
    {
        moveTo(Surface::Canvas, segment.x1, segment.y1);
        lineTo(Surface::Canvas, segment.x2, segment.y2);
    }
}

/* An image as an arrow standing on the axis: solid cyan for a real image; dashed magenta
   for a virtual one, with dotted lines from where the rays leave the lens or mirror back
   to its tip - the point the rays seem to come from. Images off the canvas, and those
   fewer than two drawn rays meet at, are not drawn. */
void GdiRenderer::drawImage(const Image& image)
{
    if (!ShownByRays(image)) return;

    const double tipY = layout_.axisY - image.height;
    if (image.x < 0 || image.x > layout_.canvasWidth ||
        tipY < -layout_.canvasHeight || tipY > 2 * layout_.canvasHeight)
        return;

    const int x   = static_cast<int>(std::lround(image.x));
    const int tip = static_cast<int>(std::lround(tipY));

    if (!image.real)
    {
        SelectionScope construction(canvas_, constructionPen_.get());
        for (const ImagePoint& exit : image.rayExits)
        {
            moveTo(Surface::Canvas, exit.x, exit.y);
            lineTo(Surface::Canvas, x, tip);
        }
    }

    SelectionScope arrow(canvas_, image.real ? realImagePen_.get() : virtualImagePen_.get());
    const int headY = (image.height > 0) ? tip + ArrowSize : tip - ArrowSize;   /* points away from the axis */
    moveTo(Surface::Canvas, x, layout_.axisY);
    lineTo(Surface::Canvas, x, tip);
    moveTo(Surface::Canvas, x, tip);
    lineTo(Surface::Canvas, x + ArrowSize, headY);
    moveTo(Surface::Canvas, x, tip);
    lineTo(Surface::Canvas, x - ArrowSize, headY);

    imageLabel(image, x, tip);
}

/* The label beside an image's tip, in the image's colour: real or virtual, and the
   magnification from the object (negative: inverted). Right of the arrow, or left of it
   when there is no room before the canvas's right edge; on the side away from the axis.
   The text keeps its size when the window is zoomed. */
void GdiRenderer::imageLabel(const Image& image, int x, int tip)
{
    char text[32];
    const int length = std::snprintf(text, sizeof text, "%s  m = %+.2f",
                                     image.real ? "real" : "virtual", image.magnification);
    if (length <= 0) return;

    const Pixel tipPixel  = toPixel(Surface::Canvas, x, tip);
    const Pixel edgePixel = toPixel(Surface::Canvas, layout_.canvasWidth, tip);
    SIZE size{};
    GetTextExtentPoint32A(canvas_, text, length, &size);

    const int gap = ArrowSize + ImageLabelGap;
    const bool fitsOnRight = tipPixel.x + gap + size.cx <= edgePixel.x;
    const int textX = fitsOnRight ? tipPixel.x + gap : tipPixel.x - gap - size.cx;
    const int textY = (image.height > 0) ? tipPixel.y - size.cy : tipPixel.y;

    SetTextColor(canvas_, image.real ? RealImageColour : VirtualImageColour);
    TextOutA(canvas_, textX, textY, text, length);
}

/* Mirrors first, then concave lenses, then everything else, so the white blocks the
   ray tracer looks for are on the canvas before any rays are traced. */
void GdiRenderer::drawElements(const Scene& scene)
{
    for (const Element& element : scene)
        if (element.type == ElementType::Mirror)
            drawMirror(element);

    for (const Element& element : scene)
        if (element.type == ElementType::Lens && element.shape != ShapeConvex)
            drawConcaveLens(element);

    for (const Element& element : scene)
        drawOther(element);
}

void GdiRenderer::drawMirror(const Element& mirror)
{
    selectCanvas(Pen::Solid);
    selectCanvas(Brush::White);
    mirrorBody(mirror.x, mirror.width);
    selectCanvas(Brush::Black);
    selectCanvas(Pen::Black);
    mirrorFace(mirror.x, mirror.width);
    selectCanvas(Pen::Solid);

    focusTick(mirror.focus);
    rectangle(Surface::Canvas, mirror.focus - MarkerHalf, layout_.axisY + MarkerHalf,
                       mirror.focus + MarkerHalf, layout_.axisY - MarkerHalf);
    drawAxis();
}

void GdiRenderer::drawConcaveLens(const Element& lens)
{
    const int focalLength = lens.focus - lens.x;
    selectCanvas(Pen::Solid);
    selectCanvas(Brush::White);
    concaveLensShape(lens.x, lens.width);
    focusTicks(lens.x, focalLength);
    focusMarkers(lens.x, focalLength);
}

void GdiRenderer::drawOther(const Element& element)
{
    switch (element.type)
    {
    case ElementType::Cannon:
    {
        {
            SelectionScope body(canvas_, blackBrush_.get());
            ellipse(Surface::Canvas, element.x - CannonHalfWidth, element.y - CannonHalfHeight - ToolbarHeight,
                             element.x + CannonHalfWidth, element.y + CannonHalfHeight - ToolbarHeight);
        }
        /* aim marker */
        rectangle(Surface::Canvas, element.x + CannonFocusOffset - MarkerHalf, element.y + element.focus + MarkerHalf - ToolbarHeight,
                           element.x + CannonFocusOffset + MarkerHalf, element.y + element.focus - MarkerHalf - ToolbarHeight);
        break;
    }
    case ElementType::Object:
        objectArrow(element.x, element.y - ToolbarHeight, element.y < layout_.axisWindowY + 10);
        selectCanvas(Pen::Solid);
        selectCanvas(Brush::White);
        break;

    case ElementType::Source:
    {
        SelectionScope body(canvas_, redBrush_.get());
        rectangle(Surface::Canvas, element.x - SourceHalfSize, element.y - SourceHalfSize - ToolbarHeight,
                           element.x + SourceHalfSize, element.y + SourceHalfSize - ToolbarHeight);
        break;
    }
    case ElementType::Lens:
        if (element.shape == ShapeConvex)
        {
            const int focalLength = element.focus - element.x;
            selectCanvas(Pen::Solid);
            selectCanvas(Brush::White);
            convexLensShape(element.x, element.width);
            focusTicks(element.x, focalLength);
            focusMarkers(element.x, focalLength);
        }
        break;

    default:   /* mirrors and concave lenses are drawn earlier; apertures are not used */
        break;
    }
}

// ---- canvas drawing state ----------------------------------------------------

void GdiRenderer::selectCanvas(Pen which)     { SelectObject(canvas_, pen(which)); }
void GdiRenderer::selectCanvas(Brush which)   { SelectObject(canvas_, brush(which)); }
SelectionScope GdiRenderer::useCanvas(Pen which) { return SelectionScope(canvas_, pen(which)); }

// ---- element shapes on the canvas ------------------------------------------------

void GdiRenderer::objectArrow(int x, int canvasY, bool pointsUp)
{
    const int headY = pointsUp ? canvasY + ArrowSize : canvasY - ArrowSize;
    moveTo(Surface::Canvas, x, layout_.axisY);
    lineTo(Surface::Canvas, x, canvasY);
    moveTo(Surface::Canvas, x, canvasY);
    lineTo(Surface::Canvas, x + ArrowSize, headY);
    moveTo(Surface::Canvas, x, canvasY);
    lineTo(Surface::Canvas, x - ArrowSize, headY);
}

void GdiRenderer::convexLensShape(int x, int width)
{
    const int top = layout_.axisY - ElementHalfHeight, bottom = layout_.axisY + ElementHalfHeight;
    chord(Surface::Canvas, x - width, top, x + width, bottom, x, bottom, x, top);
    chord(Surface::Canvas, x - width, top, x + width, bottom, x, top, x, bottom);
}

/* A white block with a black curve cut out of each side. Uses the current pen and brush
   for the block, then the black ones for the cuts. Leaves the black brush selected. */
void GdiRenderer::concaveLensShape(int x, int width)
{
    const int top = layout_.axisY - ElementHalfHeight, bottom = layout_.axisY + ElementHalfHeight;
    rectangle(Surface::Canvas, x - 2*width, top, x + 2*width, bottom + 1);
    selectCanvas(Brush::Black);
    SelectionScope cuts = useCanvas(Pen::Black);
    chord(Surface::Canvas, x + width, top, x + 3*width, bottom, x + 2*width, top, x + 2*width, bottom);
    chord(Surface::Canvas, x - width, top, x - 3*width, bottom, x - 2*width, bottom, x - 2*width, top);
}

void GdiRenderer::focusTick(int focusX)
{
    moveTo(Surface::Canvas, focusX, layout_.axisY - AxisTickHalf);
    lineTo(Surface::Canvas, focusX, layout_.axisY + AxisTickHalf);
}

void GdiRenderer::focusTicks(int x, int focalLength)
{
    focusTick(x - focalLength);
    focusTick(x + focalLength);
}

void GdiRenderer::focusMarkers(int x, int focalLength)
{
    rectangle(Surface::Canvas, x + focalLength - MarkerHalf, layout_.axisY + MarkerHalf,
                       x + focalLength + MarkerHalf, layout_.axisY - MarkerHalf);
    rectangle(Surface::Canvas, x - focalLength - MarkerHalf, layout_.axisY + MarkerHalf,
                       x - focalLength + MarkerHalf, layout_.axisY - MarkerHalf);
}

void GdiRenderer::mirrorBody(int x, int width)
{
    rectangle(Surface::Canvas, x, layout_.axisY - (ElementHalfHeight + width),
                       x + MirrorBackWidth + 1, layout_.axisY + (ElementHalfHeight + 1 + width));
}

void GdiRenderer::mirrorFace(int x, int width)
{
    const int top = layout_.axisY - (ElementHalfHeight + width), bottom = layout_.axisY + (ElementHalfHeight + width);
    chord(Surface::Canvas, x - MirrorArcHalfWidth, top, x + MirrorArcHalfWidth, bottom, x, bottom, x, top);
}

// ---- previews on the window ----------------------------------------------------

void GdiRenderer::previewCannon(int x, int y)
{
    SelectionScope body(windowDc_, blackBrush_.get());
    ellipse(Surface::Window, x - CannonHalfWidth, y - CannonHalfHeight, x + CannonHalfWidth, y + CannonHalfHeight);
}

void GdiRenderer::previewObject(int x, int y)
{
    moveTo(Surface::Window, x, layout_.axisWindowY);
    lineTo(Surface::Window, x, y);
    if (y < layout_.axisWindowY)               /* arrow head only above the axis */
    {
        lineTo(Surface::Window, x + ArrowSize, y + ArrowSize);
        moveTo(Surface::Window, x, y);
        lineTo(Surface::Window, x - ArrowSize, y + ArrowSize);
    }
}

void GdiRenderer::previewSource(int x, int y)
{
    SelectionScope body(windowDc_, redBrush_.get());
    rectangle(Surface::Window, x - SourceHalfSize, y - SourceHalfSize, x + SourceHalfSize, y + SourceHalfSize);
}

void GdiRenderer::previewLens(int x, int width, bool concave)
{
    const int top = layout_.axisWindowY - ElementHalfHeight, bottom = layout_.axisWindowY + ElementHalfHeight;
    if (!concave)
    {
        arc(Surface::Window, x - width, top, x + width, bottom, x, bottom, x, top);
        arc(Surface::Window, x - width, top, x + width, bottom, x, top, x, bottom);
    }
    else
    {
        arc(Surface::Window, x + width, top, x + 3*width, bottom, x + 2*width, top, x + 2*width, bottom);
        arc(Surface::Window, x - width, top, x - 3*width, bottom, x - 2*width, bottom, x - 2*width, top);
        moveTo(Surface::Window, x - 2*width, top);
        lineTo(Surface::Window, x + 2*width, top);
        moveTo(Surface::Window, x - 2*width, bottom);
        lineTo(Surface::Window, x + 2*width, bottom);
    }
}

void GdiRenderer::previewMirror(int x, int width)
{
    const int top = layout_.axisWindowY - (ElementHalfHeight + width), bottom = layout_.axisWindowY + (ElementHalfHeight + width);
    arc(Surface::Window, x - MirrorArcHalfWidth, top, x + MirrorArcHalfWidth, bottom, x, bottom, x, top);
    moveTo(Surface::Window, x, top);
    lineTo(Surface::Window, x + MirrorBackWidth, top);
    lineTo(Surface::Window, x + MirrorBackWidth, bottom);
    lineTo(Surface::Window, x, bottom);
}

// ---- drawing in world coordinates, zoomed to pixels ----------------------------------

GdiRenderer::Pixel GdiRenderer::toPixel(Surface surface, int x, int y) const
{
    if (surface == Surface::Canvas)
        return { viewport_.toPixelX(x), viewport_.toCanvasPixelY(y) };
    return { viewport_.toPixelX(x), viewport_.toWindowPixelY(y) };
}

void GdiRenderer::moveTo(Surface surface, int x, int y)
{
    const Pixel p = toPixel(surface, x, y);
    MoveToEx(dc(surface), p.x, p.y, nullptr);
}

void GdiRenderer::lineTo(Surface surface, int x, int y)
{
    const Pixel p = toPixel(surface, x, y);
    LineTo(dc(surface), p.x, p.y);
}

void GdiRenderer::rectangle(Surface surface, int left, int top, int right, int bottom)
{
    const Pixel a = toPixel(surface, left, top), b = toPixel(surface, right, bottom);
    Rectangle(dc(surface), a.x, a.y, b.x, b.y);
}

void GdiRenderer::ellipse(Surface surface, int left, int top, int right, int bottom)
{
    const Pixel a = toPixel(surface, left, top), b = toPixel(surface, right, bottom);
    Ellipse(dc(surface), a.x, a.y, b.x, b.y);
}

void GdiRenderer::chord(Surface surface, int left, int top, int right, int bottom, int x1, int y1, int x2, int y2)
{
    const Pixel a = toPixel(surface, left, top), b = toPixel(surface, right, bottom);
    const Pixel p = toPixel(surface, x1, y1),    q = toPixel(surface, x2, y2);
    Chord(dc(surface), a.x, a.y, b.x, b.y, p.x, p.y, q.x, q.y);
}

void GdiRenderer::arc(Surface surface, int left, int top, int right, int bottom, int x1, int y1, int x2, int y2)
{
    const Pixel a = toPixel(surface, left, top), b = toPixel(surface, right, bottom);
    const Pixel p = toPixel(surface, x1, y1),    q = toPixel(surface, x2, y2);
    Arc(dc(surface), a.x, a.y, b.x, b.y, p.x, p.y, q.x, q.y);
}
