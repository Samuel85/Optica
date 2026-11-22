// Tests for ImGuiEditorView: that it keeps what the Windows canvas and window would show,
// driven directly and through the real EditorController. No SDL or Dear ImGui needed.

#include "imgui_editor_view.h"

#include "controller/editor_controller.h"

#include <cstdio>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

struct Editor {
    Scene            scene;
    Layout           layout;            // default size: 640 x 350 canvas, axis at 230
    ImGuiEditorView  view{ scene, layout };
    EditorController controller{ scene, layout, view };

    void click(int x, int y)
    {
        controller.mouseMove(x, y);
        controller.mouseDown(x, y);
        controller.mouseUp(x, y);
    }
};

static void testStartsWithAnEmptyBench()
{
    Editor editor;
    CHECK(editor.view.shown().scene.size() == 0);
    CHECK(editor.view.shown().rays.empty());
    CHECK(!editor.view.preview());
}

static void testARedrawIsShownOnlyAfterPresent()
{
    Editor editor;
    editor.scene.add({ ElementType::Object, 100, 190, 0, ShapeConvex, 0 });
    editor.view.redrawScene();
    CHECK(editor.view.shown().scene.size() == 0);          // still the old picture
    editor.view.present();
    CHECK(editor.view.shown().scene.size() == 1);
}

static void testThePictureIsACopy()
{
    Editor editor;
    editor.scene.add({ ElementType::Object, 100, 190, 0, ShapeConvex, 0 });
    editor.view.redrawScene();
    editor.view.present();
    editor.scene[0].x = 200;                                // the scene changes, the picture not
    CHECK(editor.view.shown().scene[0].x == 100);
}

static void testPresentClearsThePreview()
{
    Editor editor;
    editor.view.previewLens(300, 15, true);
    CHECK(editor.view.preview().has_value());
    if (editor.view.preview())
    {
        CHECK(editor.view.preview()->type == ElementType::Lens);
        CHECK(editor.view.preview()->x == 300 && editor.view.preview()->width == 15);
        CHECK(editor.view.preview()->concave);
    }
    editor.view.present();
    CHECK(!editor.view.preview());
}

static void testRaysAndImagesAreTracedWhenRedrawn()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(160, 190);                                 // 40 above the axis (230)
    editor.controller.lensButton();
    editor.click(300, 200);                                 // f = 30: image at 300 + 1/(1/30 - 1/140)

    const Picture& picture = editor.view.shown();
    CHECK(picture.scene.size() == 2);
    CHECK(!picture.rays.empty());
    CHECK(picture.images.size() == 1);
    if (picture.images.size() == 1)
    {
        CHECK(picture.images[0].real);
        CHECK(picture.images[0].x > 330 && picture.images[0].x < 345);
    }
}

static void testImagesCanBeSwitchedOff()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(160, 190);
    editor.controller.lensButton();
    editor.click(300, 200);

    editor.view.toggleImages();
    editor.view.redrawScene();
    editor.view.present();
    CHECK(!editor.view.showsImages());
    CHECK(editor.view.shown().images.empty());
    CHECK(!editor.view.shown().rays.empty());
}

static void testDraggingShowsTheCandidate()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.mouseMove(100, 150);
    editor.controller.mouseDown(100, 150);
    editor.controller.mouseMove(250, 160);

    CHECK(editor.view.shown().scene[0].x == 250);           // shown where it would be dropped
    CHECK(editor.scene[0].x == 100);                        // not moved yet
    CHECK(editor.view.preview().has_value());               // its outline under the mouse
    editor.controller.mouseUp(250, 160);
    CHECK(editor.scene[0].x == 250);
    CHECK(!editor.view.preview());
}

static void testClearAllShowsAnEmptyBench()
{
    Editor editor;
    editor.controller.objectButton();
    editor.click(100, 150);
    editor.controller.clearAllButton();
    CHECK(editor.view.shown().scene.size() == 0);
}

static void testMessagesAreQueued()
{
    Editor editor;
    editor.controller.clearLastButton();                    // nothing selected
    editor.view.showMessage("second");
    const std::optional<std::string> first = editor.view.takeMessage();
    CHECK(first && *first == "No element selected to clear.");
    const std::optional<std::string> second = editor.view.takeMessage();
    CHECK(second && *second == "second");
    CHECK(!editor.view.takeMessage());
}

int main()
{
    testStartsWithAnEmptyBench();
    testARedrawIsShownOnlyAfterPresent();
    testThePictureIsACopy();
    testPresentClearsThePreview();
    testRaysAndImagesAreTracedWhenRedrawn();
    testImagesCanBeSwitchedOff();
    testDraggingShowsTheCandidate();
    testClearAllShowsAnEmptyBench();
    testMessagesAreQueued();

    if (failures == 0) std::printf("All ImGuiEditorView tests passed.\n");
    return failures == 0 ? 0 : 1;
}
