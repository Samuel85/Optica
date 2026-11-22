#include "imgui_editor_view.h"

ImGuiEditorView::ImGuiEditorView(const Scene& scene, const Layout& layout)
    : scene_(scene), layout_(layout),
      canvas_(std::make_shared<const Picture>()),
      shown_(canvas_)
{
}

static bool IsLightSource(const Element& element)
{
    return element.type == ElementType::Object || element.type == ElementType::Cannon ||
           element.type == ElementType::Source;
}

/* The same work GdiEditorView::draw does, kept instead of drawn: the rays of every light
   source and, if switched on, the images of every object. */
std::shared_ptr<const Picture> ImGuiEditorView::paint(const Scene& scene) const
{
    auto picture = std::make_shared<Picture>();
    picture->scene = scene;

    for (Scene::Index index = 0; index < scene.size(); ++index)
        if (IsLightSource(scene[index]))
        {
            const std::vector<Segment> rays = TraceRays(scene, layout_, index);
            picture->rays.insert(picture->rays.end(), rays.begin(), rays.end());
        }

    if (showImages_)
        for (Scene::Index index = 0; index < scene.size(); ++index)
            if (scene[index].type == ElementType::Object)
            {
                const std::vector<Image> images = FormImages(scene, layout_, index);
                picture->images.insert(picture->images.end(), images.begin(), images.end());
            }

    return picture;
}

/* An empty bench, shown at once. */
void ImGuiEditorView::clearScreen()
{
    canvas_ = std::make_shared<const Picture>();
    present();
}

void ImGuiEditorView::present()
{
    shown_ = canvas_;
    preview_.reset();
}

std::optional<std::string> ImGuiEditorView::takeMessage()
{
    if (messages_.empty()) return std::nullopt;
    std::string message = std::move(messages_.front());
    messages_.pop_front();
    return message;
}
