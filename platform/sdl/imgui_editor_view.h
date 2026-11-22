#pragma once

// The editor's view for the SDL + Dear ImGui front end.
//
// The controller talks to its view as if drawing were retained, as on Windows: it
// redraws the scene into a back buffer (redrawScene), shows that buffer (present), and
// draws previews over what is shown until the next present. Dear ImGui instead draws
// everything again on every frame. This class bridges the two by keeping state only:
//
//   canvas   - the Picture the last redrawScene / redrawCandidate / clearScreen made:
//              a copy of the scene with its rays and images already traced;
//   shown    - the Picture on screen, taken from the canvas by present();
//   preview  - the outline following the mouse, cleared by present().
//
// Each frame, ScenePainter draws shown() and preview(). Nothing here depends on SDL or
// ImGui, so it can be tested on its own.

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "controller/editor_view.h"
#include "layout.h"
#include "model/imaging.h"
#include "model/ray_tracer.h"
#include "model/scene.h"

// A scene ready to be painted: its elements, the rays of its light sources and, when
// switched on, the images its objects form. World coordinates.
struct Picture {
    Scene                scene;
    std::vector<Segment> rays;
    std::vector<Image>   images;
};

// The outline of an element under the mouse, at (x, y) in world window coordinates.
struct Preview {
    ElementType type;
    int         x       = 0;
    int         y       = 0;
    int         width   = 0;        // lens thickness or mirror depth
    bool        concave = false;    // lens shape
};

class ImGuiEditorView : public EditorView {
public:
    ImGuiEditorView(const Scene& scene, const Layout& layout);

    void clearScreen() override;
    void redrawScene() override                                 { canvas_ = paint(scene_); }
    void redrawCandidate(const Scene& candidate) override       { canvas_ = paint(candidate); }
    void present() override;

    void previewCannon(int x, int y) override                   { preview_ = Preview{ ElementType::Cannon, x, y }; }
    void previewObject(int x, int y) override                   { preview_ = Preview{ ElementType::Object, x, y }; }
    void previewSource(int x, int y) override                   { preview_ = Preview{ ElementType::Source, x, y }; }
    void previewLens(int x, int width, bool concave) override   { preview_ = Preview{ ElementType::Lens, x, 0, width, concave }; }
    void previewMirror(int x, int width) override               { preview_ = Preview{ ElementType::Mirror, x, 0, width }; }

    void showMessage(const char* text) override                 { messages_.emplace_back(text); }

    // The images objects form through lenses and mirrors (shown unless switched off).
    void toggleImages()      { showImages_ = !showImages_; }
    bool showsImages() const { return showImages_; }

    // ---- what the painter and the application read ------------------------------
    const Picture&                shown() const   { return *shown_; }
    const std::optional<Preview>& preview() const { return preview_; }
    std::optional<std::string>    takeMessage();  // the oldest message not yet shown

private:
    std::shared_ptr<const Picture> paint(const Scene& scene) const;

    const Scene&  scene_;
    const Layout& layout_;
    bool          showImages_ = true;

    std::shared_ptr<const Picture> canvas_;
    std::shared_ptr<const Picture> shown_;
    std::optional<Preview>         preview_;
    std::deque<std::string>        messages_;
};
