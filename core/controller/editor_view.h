#pragma once

// What the editor controller asks of the screen. Implemented with GDI by GdiEditorView;
// a test can implement it with a fake that records the calls.

#include "../model/scene.h"

class EditorView {
public:
    virtual ~EditorView() = default;

    // ---- whole scene ---------------------------------------------------------
    virtual void clearScreen() = 0;          // empty bench, shown at once
    virtual void redrawScene() = 0;          // elements, rays and axis, ready to be shown
    // The same for a scene that is not the editor's: the scene as it would be if the
    // element being dragged or adjusted were let go now, so rays and images follow the mouse.
    virtual void redrawCandidate(const Scene& candidate) = 0;
    virtual void present() = 0;              // show the scene (erases any previews)

    // ---- previews over the scene, following the mouse (window coordinates) ---
    virtual void previewCannon(int x, int y) = 0;
    virtual void previewObject(int x, int y) = 0;
    virtual void previewSource(int x, int y) = 0;
    virtual void previewLens(int x, int width, bool concave) = 0;
    virtual void previewMirror(int x, int width) = 0;

    // ---- messages to the user ------------------------------------------------
    virtual void showMessage(const char* text) = 0;
};
