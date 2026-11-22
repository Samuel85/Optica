#include "editor_controller.h"

#include <algorithm>
#include <cstdlib>

EditorController::EditorController(Scene& scene, const Layout& layout, EditorView& view)
    : scene_(scene), layout_(layout), view_(view)
{
}

// ---- toolbar buttons ----------------------------------------------------------

void EditorController::cannonButton() { state_ = Placing{ ElementType::Cannon, 0 }; }
void EditorController::objectButton() { state_ = Placing{ ElementType::Object, 0 }; }
void EditorController::sourceButton() { state_ = Placing{ ElementType::Source, 0 }; }
void EditorController::lensButton()   { state_ = Placing{ ElementType::Lens,   DefaultLensWidth }; }
void EditorController::mirrorButton() { state_ = Placing{ ElementType::Mirror, 0 }; }

void EditorController::clearAllButton()
{
    view_.clearScreen();
    view_.present();
    scene_.clear();
    endActionOnElement();
}

/* Deletes the element last touched: hovered, dragged or adjusted. */
void EditorController::clearLastButton()
{
    if (scene_.selected() == nullptr)
    {
        view_.showMessage("No element selected to clear.");
        return;
    }
    scene_.removeSelected();
    endActionOnElement();
    view_.redrawScene();
}

/* After elements are removed: forget what was under the mouse, and stop dragging or
   adjusting (the element may be gone). A tool chosen for placing stays chosen. */
void EditorController::endActionOnElement()
{
    hover_.reset();
    if (std::holds_alternative<Dragging>(state_) || std::holds_alternative<AdjustingFocus>(state_))
        state_ = Idle{};
}

CursorKind EditorController::cursor() const
{
    if (std::holds_alternative<Dragging>(state_) || std::holds_alternative<AdjustingFocus>(state_))
        return CURSOR_HAND;
    if (std::holds_alternative<Placing>(state_))
        return CURSOR_CROSS;
    return hover_ ? CURSOR_HAND : CURSOR_NORMAL;
}

// ---- mouse ------------------------------------------------------------------------

void EditorController::mouseMove(int x, int y)
{
    if (std::holds_alternative<Idle>(state_))
    {
        hoverAt(x, y);
    }
    else if (auto* placing = std::get_if<Placing>(&state_))
    {
        view_.present();
        preview(placing->type, x, y, placing->width, false);
    }
    else if (auto* dragging = std::get_if<Dragging>(&state_))
    {
        /* Outside the canvas the element is not shown moved, as its outline is not either */
        if (insideCanvas(x, y))
        {
            Scene candidate = scene_;
            drop(candidate, *dragging, x, y);
            view_.redrawCandidate(candidate);
        }
        else
            view_.redrawScene();

        const Element& element = scene_[dragging->index];
        view_.present();
        preview(element.type, x, y, element.width, element.shape != ShapeConvex);
    }
    else if (auto* adjusting = std::get_if<AdjustingFocus>(&state_))
    {
        adjustTowards(*adjusting, x, y);
    }
}

void EditorController::mouseDown(int x, int y)
{
    if (std::holds_alternative<Idle>(state_) && hover_)
    {
        const Element& element = scene_[hover_->index];
        scene_.select(hover_->index);
        if (hover_->part == Part::Focus)
            state_ = AdjustingFocus{ hover_->index, x, element.width,
                                     element.shape != ShapeConvex, element.focus };
        else
            state_ = Dragging{ hover_->index };
        hover_.reset();
    }
    else if (auto* placing = std::get_if<Placing>(&state_))
    {
        if (insideCanvas(x, y))              /* outside the canvas: nothing, the tool stays */
        {
            place(*placing, x, y);
            state_ = Idle{};
        }
    }
    showScene();
}

void EditorController::mouseUp(int x, int y)
{
    if (auto* dragging = std::get_if<Dragging>(&state_))
    {
        drop(scene_, *dragging, x, y);
        state_ = Idle{};
    }
    else if (auto* adjusting = std::get_if<AdjustingFocus>(&state_))
    {
        finishAdjusting(scene_, *adjusting, x);
        state_ = Idle{};
    }
    hover_.reset();
    showScene();
}

// ---- Idle: what is under the mouse ------------------------------------------------

bool EditorController::insideCanvas(int x, int y) const
{
    return x >= 0 && x <= layout_.canvasWidth && y > ToolbarHeight && y < layout_.canvasBottomY;
}

/* The first element, in x order, whose body or focal point is under (x, y).
   Focal points of lenses and mirrors are picked up on their marker on the axis line
   (within MarkerHalf of it, so they can still be reached when the window is zoomed out).
   Every tolerance is widened by the grab margin (0 for a mouse). */
std::optional<EditorController::Hit> EditorController::hitTest(int x, int y) const
{
    if (!insideCanvas(x, y)) return std::nullopt;

    const int marker = MarkerHalf + grabMargin_;
    const int grab   = GrabHalfWidth + grabMargin_;
    const bool onAxis = abs(y - layout_.axisWindowY) < marker;

    for (Scene::Index index = 0; index < scene_.size(); ++index)
    {
        const Element& e = scene_[index];
        switch (e.type)
        {
        case ElementType::Cannon:
            if (abs(x - (e.x + CannonFocusOffset)) <= marker && abs(y - (e.y + e.focus)) <= marker)
                return Hit{ index, Part::Focus };                  /* the aim marker */
            if (abs(x - e.x) <= CannonHalfWidth + grabMargin_ && abs(y - e.y) <= CannonHalfHeight + grabMargin_)
                return Hit{ index, Part::Body };
            break;

        case ElementType::Object:
        case ElementType::Source:
            if (abs(x - e.x) <= grab)
                return Hit{ index, Part::Body };
            break;

        case ElementType::Lens:
        {
            const int focalLength = abs(e.x - e.focus);
            if (onAxis && (abs(x - e.focus) < marker || abs(x - (e.x - focalLength)) < marker))
                return Hit{ index, Part::Focus };                  /* either focal point */
            if (abs(x - e.x) <= grab)
                return Hit{ index, Part::Body };
            break;
        }
        case ElementType::Mirror:
            if (onAxis && abs(x - e.focus) < marker)
                return Hit{ index, Part::Focus };
            if (x >= e.x + MirrorArcHalfWidth - grabMargin_ && x <= e.x + MirrorBackWidth + grabMargin_)
                return Hit{ index, Part::Body };                   /* the back of the mirror */
            break;

        default:
            break;
        }
    }
    return std::nullopt;
}

/* Under the mouse: an element's body is outlined in place; elsewhere the canvas is
   shown clean. The element touched last is the one the Delete button removes. */
void EditorController::hoverAt(int x, int y)
{
    hover_ = hitTest(x, y);
    if (hover_)
        scene_.select(hover_->index);

    if (hover_ && hover_->part == Part::Body)
    {
        const Element& element = scene_[hover_->index];
        const bool onAxisLine = (element.type == ElementType::Lens || element.type == ElementType::Mirror);
        preview(element.type, element.x, onAxisLine ? y : element.y, element.width,
                element.shape != ShapeConvex);
    }
    else
    {
        view_.present();
    }
}

// ---- drawing ------------------------------------------------------------------------

/* Outline of an element at (x, y) on the window, where it would go. */
void EditorController::preview(ElementType type, int x, int y, int width, bool concave)
{
    if (!insideCanvas(x, y)) return;

    switch (type)
    {
    case ElementType::Cannon:
        if (y > layout_.cannonMinY && y < layout_.cannonMaxY)
            view_.previewCannon(x, y);
        break;
    case ElementType::Object:
        view_.previewObject(x, y);
        break;
    case ElementType::Source:
        if (y > SourceMinY)
            view_.previewSource(x, y);
        break;
    case ElementType::Lens:
        view_.previewLens(x, width, concave);
        break;
    case ElementType::Mirror:
        view_.previewMirror(x, width);
        break;
    default:
        break;
    }
}

void EditorController::showScene()
{
    view_.redrawScene();
    view_.present();
}

// ---- finishing an action --------------------------------------------------------------

void EditorController::place(const Placing& placing, int x, int y)
{
    switch (placing.type)
    {
    case ElementType::Cannon: scene_.add({ ElementType::Cannon, x, y, 0, ShapeConvex, 0 }); break;
    case ElementType::Object: scene_.add({ ElementType::Object, x, y, 0, ShapeConvex, 0 }); break;
    case ElementType::Source: scene_.add({ ElementType::Source, x, y, 0, ShapeConvex, 0 }); break;
    case ElementType::Lens:
        scene_.add({ ElementType::Lens, x, y, x + DefaultFocalLength, ShapeConvex, DefaultLensWidth });
        break;
    case ElementType::Mirror:
        scene_.add({ ElementType::Mirror, x, y, x, ShapeConvex, 0 });   /* focus starts on the mirror */
        break;
    default:
        break;
    }
}

/* The element moves to the mouse; a lens or mirror keeps its focal length.
   Applied to the editor's scene on release, and to a copy on every move before that. */
void EditorController::drop(Scene& scene, const Dragging& dragging, int x, int y)
{
    Element& element = scene[dragging.index];
    const int focalLength = abs(element.focus - element.x);
    element.x = x;
    element.y = y;

    /* Re-sorting moves elements inside the scene, so look the element up again */
    Element& dropped = scene[scene.reposition(dragging.index)];
    if (dropped.type == ElementType::Lens)
        dropped.focus = x + focalLength;
    else if (dropped.type == ElementType::Mirror)
        dropped.focus = x - focalLength;
}

/* While a focal point is moved, the scene is redrawn as if it were let go here, and the
   element's preview shows what it will become:
     lens   - thicker while the focus is close (within LensResizeRange), thinner further
              away, one step per mouse move; concave once the focus is dragged across
              to the other side of the lens;
     mirror - deeper the further its focus is moved from the mirror's face;
     cannon - its aim follows the mouse's height. */
void EditorController::adjustTowards(AdjustingFocus& adjusting, int x, int y)
{
    const Element& element = scene_[adjusting.index];

    if (adjusting.pressX < element.x)
        adjusting.concave = x > element.x;
    else
    {
        if (adjusting.pressX > element.x)
            adjusting.concave = x < element.x;
        adjusting.aimOffset = y - element.y;
    }

    if (abs(x - element.x) < LensResizeRange)
    {
        if (adjusting.width < MaxLensWidth) adjusting.width++;
    }
    else if (adjusting.width > MinLensWidth)
        adjusting.width--;

    if (element.type == ElementType::Mirror)
        adjusting.width = std::clamp(element.focus + element.width - x, 0, MaxMirrorWidth);

    Scene candidate = scene_;
    finishAdjusting(candidate, adjusting, x);
    view_.redrawCandidate(candidate);

    view_.present();
    preview(element.type, element.x, element.y, adjusting.width, adjusting.concave);
}

void EditorController::finishAdjusting(Scene& scene, const AdjustingFocus& adjusting, int x)
{
    Element& element = scene[adjusting.index];
    const int focalLength = abs(x - element.x);
    element.width = adjusting.width;

    switch (element.type)
    {
    case ElementType::Cannon:
        element.focus = adjusting.aimOffset;
        break;
    case ElementType::Lens:
        element.focus = element.x + focalLength;      /* stored on the right; the shape says which kind */
        element.shape = adjusting.concave ? ShapeConcave : ShapeConvex;
        break;
    case ElementType::Mirror:
        element.focus = std::min(x, element.x);       /* a mirror's focus is in front of it */
        break;
    default:
        break;
    }
}
