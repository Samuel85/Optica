// Optica: an optical bench. Place light sources, objects, lenses and mirrors on an
// optical axis and see how the rays travel.
//
//   model/       the scene, the optics and the ray tracer (no Windows code)
//   controller/  the editing rules: what the mouse and the toolbar do (no Windows code)
//   view/        drawing with GDI, and the main window

#include <windows.h>

#include "view/main_window.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int showCommand)
{
    MainWindow window(instance);
    if (!window.create(showCommand))
        return 0;
    return window.run();
}
