// Tests for EditorController, driven like the window procedure drives it, with a fake
// view that only counts what it is asked to show. Plain C++20, no Windows needed.

#include "../controller/editor_controller.h"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

class FakeView : public EditorView {
public:
    int redraws = 0, presents = 0, clears = 0;
    std::vector<Scene> candidates;              // scenes drawn while dragging or adjusting
    std::vector<std::string> messages;

    void clearScreen() override                 { ++clears; }
    void redrawScene() override                 { ++redraws; }
    void redrawCandidate(const Scene& candidate) override { candidates.push_back(candidate); }
    void present() override                     { ++presents; }
    void previewCannon(int, int) override       {}
    void previewObject(int, int) override       {}
    void previewSource(int, int) override       {}
    void previewLens(int, int, bool) override   {}
    void previewMirror(int, int) override       {}
    void showMessage(const char* text) override { messages.push_back(text); }
};

struct Editor {
    Scene            scene;
    Layout           layout;            // default size: 640 x 350 canvas, axis at 230
    FakeView         view;
    EditorController controller{ scene, layout, view };

    void click(int x, int y)
    {
        controller.mouseMove(x, y);
        controller.mouseDown(x, y);
        controller.mouseUp(x, y);
    }
};

static void testPlacingALens()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);

    CHECK(editor.scene.size() == 1);
    const Element& lens = editor.scene[0];
    CHECK(lens.type == ElementType::Lens);
    CHECK(lens.x == 300);
    CHECK(lens.focus == 300 + DefaultFocalLength);
    CHECK(lens.width == DefaultLensWidth);
    CHECK(editor.view.redraws > 0);
}

static void testPlacingAnObjectAndASource()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.sourceButton();
    editor.click(50, 250);

    CHECK(editor.scene.size() == 2);
    CHECK(editor.scene[0].type == ElementType::Source);   // kept in x order
    CHECK(editor.scene[0].x == 50);
    CHECK(editor.scene[1].type == ElementType::Object);
    CHECK(editor.scene[1].y == 150);
}

static void testClearAll()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    editor.controller.clearAllButton();

    CHECK(editor.scene.empty());
    CHECK(editor.view.clears == 1);
}

static void testDeletingWithNothingSelectedShowsAMessage()
{
    Editor editor;
    editor.controller.clearLastButton();
    CHECK(editor.view.messages.size() == 1);
}

static void testCursorOverAnElement()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.mouseMove(100, 150);                // over the object's line
    CHECK(editor.controller.cursor() == CURSOR_HAND);
    editor.controller.mouseMove(400, 150);                // empty space
    CHECK(editor.controller.cursor() == CURSOR_NORMAL);
}

// ---- placing ----------------------------------------------------------------------

static void testPlacingACannonAndAMirror()
{
    Editor editor;
    editor.controller.cannonButton();
    editor.click(100, 200);
    editor.controller.mirrorButton();
    editor.click(500, 230);

    CHECK(editor.scene.size() == 2);
    CHECK(editor.scene[0].type == ElementType::Cannon);
    CHECK(editor.scene[0].x == 100 && editor.scene[0].y == 200 && editor.scene[0].focus == 0);
    CHECK(editor.scene[1].type == ElementType::Mirror);
    CHECK(editor.scene[1].x == 500 && editor.scene[1].focus == 500 && editor.scene[1].width == 0);
}

static void testClickingEmptySpaceDoesNothing()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    editor.click(450, 300);                               // no tool chosen, nothing there
    CHECK(editor.scene.size() == 1);
}

static void testAToolIsUsedOnce()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.click(200, 150);
    CHECK(editor.scene.size() == 1);
}

// ---- dragging -----------------------------------------------------------------------

static void drag(Editor& editor, int fromX, int fromY, int toX, int toY)
{
    editor.controller.mouseMove(fromX, fromY);
    editor.controller.mouseDown(fromX, fromY);
    editor.controller.mouseMove(toX, toY);
    editor.controller.mouseUp(toX, toY);
}

static void testDraggingAnObject()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    drag(editor, 100, 150, 200, 170);

    CHECK(editor.scene.size() == 1);
    CHECK(editor.scene[0].x == 200 && editor.scene[0].y == 170);
}

static void testDraggingALensKeepsItsFocalLength()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);                               // focus at 330
    drag(editor, 300, 200, 350, 200);

    CHECK(editor.scene[0].x == 350);
    CHECK(editor.scene[0].focus == 380);
}

static void testDraggingKeepsElementsInXOrder()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.lensButton();
    editor.click(300, 200);
    drag(editor, 100, 150, 400, 150);                     // object moves past the lens

    CHECK(editor.scene[0].type == ElementType::Lens);
    CHECK(editor.scene[1].type == ElementType::Object);
    CHECK(editor.scene[1].x == 400);
}

static void testDraggingAMirrorByItsBack()
{
    Editor editor;
    editor.controller.mirrorButton();
    editor.click(500, 230);                               // body strip is x 550..560
    drag(editor, 555, 200, 455, 200);

    CHECK(editor.scene[0].x == 455);
    CHECK(editor.scene[0].focus == 455);                  // focal distance kept (0)
}

// ---- adjusting a focus ------------------------------------------------------------

static void adjustFocus(Editor& editor, int markerX, int markerY, int toX, int toY)
{
    editor.controller.mouseMove(markerX, markerY);
    CHECK(editor.controller.cursor() == CURSOR_HAND);
    editor.controller.mouseDown(markerX, markerY);
    editor.controller.mouseMove(toX, toY);
    editor.controller.mouseUp(toX, toY);
}

static void testMovingALensFocusFurtherAway()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);                               // focus 330, width 15
    adjustFocus(editor, 330, editor.layout.axisWindowY - 1, 360, 230);

    CHECK(editor.scene[0].focus == 360);
    CHECK(editor.scene[0].shape == ShapeConvex);
    CHECK(editor.scene[0].width == DefaultLensWidth - 1); // thinner as the focus moves away
}

static void testDraggingALensFocusAcrossMakesItConcave()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    adjustFocus(editor, 330, editor.layout.axisWindowY - 1, 270, 230);

    CHECK(editor.scene[0].shape == ShapeConcave);
    CHECK(editor.scene[0].focus == 330);                  // focal length 30, stored on the right
}

static void testMovingAMirrorFocus()
{
    Editor editor;
    editor.controller.mirrorButton();
    editor.click(500, 230);                               // focus 500, width 0
    adjustFocus(editor, 500, editor.layout.axisWindowY - 1, 440, 230);

    CHECK(editor.scene[0].focus == 440);
    CHECK(editor.scene[0].width == MaxMirrorWidth);       // 500 - 440 = 60, capped
}

static void testAimingACannon()
{
    Editor editor;
    editor.controller.cannonButton();
    editor.click(100, 200);                               // aim marker at (130, 200)
    adjustFocus(editor, 100 + CannonFocusOffset, 200, 130, 180);

    CHECK(editor.scene[0].focus == -20);
}

// ---- deleting --------------------------------------------------------------------------

static void testDeleteRemovesTheLastElementTouched()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.mouseMove(300, 200);                // over the lens
    editor.controller.clearLastButton();

    CHECK(editor.scene.size() == 1);
    CHECK(editor.scene[0].type == ElementType::Object);
}

// Used to crash: the focal point was remembered after Clear All removed its element.
static void testClearAllWhileOverAFocalPoint()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    editor.controller.mouseMove(330, editor.layout.axisWindowY - 1);   // over the focus
    editor.controller.clearAllButton();
    editor.click(330, editor.layout.axisWindowY - 1);

    CHECK(editor.scene.empty());
    CHECK(editor.controller.cursor() == CURSOR_NORMAL);
}

// ---- behaviour that changed with the state machine ----------------------------------

static void testPressingOutsideTheCanvasKeepsTheTool()
{
    Editor editor;
    editor.controller.cannonButton();
    editor.click(100, ToolbarHeight - 10);                // in the top button bar
    CHECK(editor.scene.empty());
    editor.click(100, 200);
    CHECK(editor.scene.size() == 1);
}

static void testAToolSurvivesClearAllAndDelete()
{
    Editor editor;
    editor.controller.lensButton();
    editor.controller.clearAllButton();
    editor.controller.clearLastButton();
    editor.click(300, 200);
    CHECK(editor.scene.size() == 1);
    CHECK(editor.scene[0].type == ElementType::Lens);
}

// Used to start moving the focus: the old code remembered the focal point after the
// mouse had moved away from it.
static void testAFocalPointIsOnlyGrabbedWhileUnderTheMouse()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    editor.controller.mouseMove(330, editor.layout.axisWindowY - 1);   // over the focus
    editor.controller.mouseMove(500, 300);                             // away again
    CHECK(editor.controller.cursor() == CURSOR_NORMAL);
    editor.controller.mouseDown(500, 300);
    editor.controller.mouseMove(560, 300);
    editor.controller.mouseUp(560, 300);

    CHECK(editor.scene[0].focus == 330);
}

// The old code only remembered some kinds of element as "touched" when hovered.
static void testDeleteAfterHoveringAnObject()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.mouseMove(100, 150);                // over the object
    editor.controller.clearLastButton();

    CHECK(editor.scene.size() == 1);
    CHECK(editor.scene[0].type == ElementType::Lens);
}

// Used to take the thickness from the last convex lens in the scene, not the one adjusted.
static void testAdjustingAFocusStartsFromThatLensThickness()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);                               // lens A, width 15
    editor.controller.lensButton();
    editor.click(450, 200);                               // lens B, width 15
    adjustFocus(editor, 480, editor.layout.axisWindowY - 1, 510, 230);   // B: width 14
    CHECK(editor.scene[1].width == DefaultLensWidth - 1);

    adjustFocus(editor, 330, editor.layout.axisWindowY - 1, 360, 230);   // A: 15 - 1
    CHECK(editor.scene[0].width == DefaultLensWidth - 1);
}

static void testCursorWhilePlacing()
{
    Editor editor;
    editor.controller.sourceButton();
    CHECK(editor.controller.cursor() == CURSOR_CROSS);
    editor.click(200, 200);
    CHECK(editor.controller.cursor() == CURSOR_NORMAL);
}

// A focal point is grabbed anywhere on its marker, not only on one row of pixels.
static void testAFocalPointIsGrabbedAnywhereOnItsMarker()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);                               // focus 330 on the axis
    adjustFocus(editor, 330, editor.layout.axisWindowY + 2, 360, 230);
    CHECK(editor.scene[0].focus == 360);
}

// ---- live redraw while dragging or adjusting ----------------------------------------

static void testDraggingRedrawsTheSceneAsIfDropped()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.lensButton();
    editor.click(300, 200);

    editor.controller.mouseMove(100, 150);
    editor.controller.mouseDown(100, 150);
    editor.view.candidates.clear();
    editor.controller.mouseMove(400, 160);                // past the lens, not released

    CHECK(editor.view.candidates.size() == 1);
    if (editor.view.candidates.size() == 1)
    {
        const Scene& candidate = editor.view.candidates[0];
        CHECK(candidate[0].type == ElementType::Lens);    // re-sorted in the copy
        CHECK(candidate[1].type == ElementType::Object);
        CHECK(candidate[1].x == 400 && candidate[1].y == 160);
    }
    CHECK(editor.scene[0].type == ElementType::Object);   // the scene itself is untouched
    CHECK(editor.scene[0].x == 100);

    editor.controller.mouseUp(400, 160);
    CHECK(editor.scene[1].x == 400);
}

static void testDraggingOutsideTheCanvasShowsTheSceneUnchanged()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.mouseMove(100, 150);
    editor.controller.mouseDown(100, 150);
    editor.view.candidates.clear();
    const int redrawsBefore = editor.view.redraws;
    editor.controller.mouseMove(100, 5);                  // over the toolbar

    CHECK(editor.view.candidates.empty());
    CHECK(editor.view.redraws == redrawsBefore + 1);
    editor.controller.mouseUp(100, 5);
}

static void testAdjustingALensFocusRedrawsTheSceneAsIfLetGo()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);                               // focus at 330
    const int axisY = editor.layout.axisWindowY;

    editor.controller.mouseMove(330, axisY);
    editor.controller.mouseDown(330, axisY);
    editor.view.candidates.clear();
    editor.controller.mouseMove(380, axisY);

    CHECK(editor.view.candidates.size() == 1);
    if (!editor.view.candidates.empty())
        CHECK(editor.view.candidates.back()[0].focus == 380);
    CHECK(editor.scene[0].focus == 330);

    editor.controller.mouseMove(250, axisY);              // across the lens: concave
    if (!editor.view.candidates.empty())
    {
        CHECK(editor.view.candidates.back()[0].shape == ShapeConcave);
        CHECK(editor.view.candidates.back()[0].focus == 350);
    }
    editor.controller.mouseUp(250, axisY);
    CHECK(editor.scene[0].shape == ShapeConcave);
}

// ---- grab margin (touch screens) ---------------------------------------------------

static void testAGrabMarginWidensWhatCanBeGrabbed()
{
    Editor editor;
    editor.controller.lensButton();
    editor.click(300, 200);                               // focus marker at 330 on the axis
    const int axisY = editor.layout.axisWindowY;

    editor.controller.mouseMove(338, axisY + 6);          // 8 right of the marker, 6 below
    CHECK(editor.controller.cursor() == CURSOR_NORMAL);   // out of reach of a mouse

    editor.controller.setGrabMargin(12);
    editor.controller.mouseMove(338, axisY + 6);
    CHECK(editor.controller.cursor() == CURSOR_HAND);     // within reach of a finger
    editor.controller.mouseDown(338, axisY + 6);
    editor.controller.mouseMove(360, axisY);
    editor.controller.mouseUp(360, axisY);
    CHECK(editor.scene[0].focus == 360);
}

static void testAGrabMarginReachesAnObjectFromFurther()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);

    editor.controller.mouseMove(118, 150);                // 18 away: beyond GrabHalfWidth (10)
    CHECK(editor.controller.cursor() == CURSOR_NORMAL);
    editor.controller.setGrabMargin(12);
    editor.controller.mouseMove(118, 150);
    CHECK(editor.controller.cursor() == CURSOR_HAND);
}

int main()
{
    testPlacingALens();
    testPlacingAnObjectAndASource();
    testClearAll();
    testDeletingWithNothingSelectedShowsAMessage();
    testCursorOverAnElement();
    testPlacingACannonAndAMirror();
    testClickingEmptySpaceDoesNothing();
    testAToolIsUsedOnce();
    testDraggingAnObject();
    testDraggingALensKeepsItsFocalLength();
    testDraggingKeepsElementsInXOrder();
    testDraggingAMirrorByItsBack();
    testMovingALensFocusFurtherAway();
    testDraggingALensFocusAcrossMakesItConcave();
    testMovingAMirrorFocus();
    testAimingACannon();
    testDeleteRemovesTheLastElementTouched();
    testClearAllWhileOverAFocalPoint();
    testPressingOutsideTheCanvasKeepsTheTool();
    testAToolSurvivesClearAllAndDelete();
    testAFocalPointIsOnlyGrabbedWhileUnderTheMouse();
    testDeleteAfterHoveringAnObject();
    testAdjustingAFocusStartsFromThatLensThickness();
    testCursorWhilePlacing();
    testAFocalPointIsGrabbedAnywhereOnItsMarker();
    testDraggingRedrawsTheSceneAsIfDropped();
    testDraggingOutsideTheCanvasShowsTheSceneUnchanged();
    testAdjustingALensFocusRedrawsTheSceneAsIfLetGo();
    testAGrabMarginWidensWhatCanBeGrabbed();
    testAGrabMarginReachesAnObjectFromFurther();

    if (failures == 0) std::printf("All controller tests passed.\n");
    return failures == 0 ? 0 : 1;
}
