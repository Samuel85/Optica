#pragma once

// The editor's view on Windows: implements EditorView with a GdiRenderer.

#include <windows.h>

#include "controller/editor_view.h"
#include "layout.h"
#include "model/scene.h"
#include "gdi_renderer.h"

class GdiEditorView : public EditorView {
public:
    GdiEditorView(HWND window, GdiRenderer& renderer, const Scene& scene, const Layout& layout);

    void clearScreen() override;
    void redrawScene() override                                { draw(scene_); }
    void redrawCandidate(const Scene& candidate) override      { draw(candidate); }
    void present() override                                    { renderer_.present(); }

    void previewCannon(int x, int y) override                  { renderer_.previewCannon(x, y); }
    void previewObject(int x, int y) override                  { renderer_.previewObject(x, y); }
    void previewSource(int x, int y) override                  { renderer_.previewSource(x, y); }
    void previewLens(int x, int width, bool concave) override  { renderer_.previewLens(x, width, concave); }
    void previewMirror(int x, int width) override              { renderer_.previewMirror(x, width); }

    void showMessage(const char* text) override;

    // The images objects form through lenses and mirrors (shown unless switched off).
    void toggleImages()      { showImages_ = !showImages_; }
    bool showsImages() const { return showImages_; }

private:
    void draw(const Scene& scene);

    HWND          window_;
    GdiRenderer&  renderer_;
    const Scene&  scene_;
    const Layout& layout_;
    bool          showImages_ = true;
};
