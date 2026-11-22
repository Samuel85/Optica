#include "gdi_editor_view.h"

#include "model/imaging.h"
#include "model/ray_tracer.h"

GdiEditorView::GdiEditorView(HWND window, GdiRenderer& renderer, const Scene& scene, const Layout& layout)
    : window_(window), renderer_(renderer), scene_(scene), layout_(layout)
{
}

static bool IsLightSource(const Element& element)
{
    return element.type == ElementType::Object || element.type == ElementType::Cannon ||
           element.type == ElementType::Source;
}

/* Clears the canvas to an empty bench and shows it. */
void GdiEditorView::clearScreen()
{
    renderer_.clearCanvas();
    renderer_.drawAxis();
    renderer_.present();
}

/* Redraws a whole scene on the canvas: elements, rays, the objects' images, axis.
   Does not show it. */
void GdiEditorView::draw(const Scene& scene)
{
    renderer_.clearCanvas();
    renderer_.drawElements(scene);
    for (Scene::Index index = 0; index < scene.size(); ++index)
        if (IsLightSource(scene[index]))
            renderer_.drawRays(TraceRays(scene, layout_, index));
    if (showImages_)
        for (Scene::Index index = 0; index < scene.size(); ++index)
            if (scene[index].type == ElementType::Object)
                for (const Image& image : FormImages(scene, layout_, index))
                    renderer_.drawImage(image);
    renderer_.drawAxis();
}

void GdiEditorView::showMessage(const char* text)
{
    MessageBox(window_, text, "Optica", MB_OK);
}
