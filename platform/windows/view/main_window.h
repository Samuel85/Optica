#pragma once

// Optica's main window: the Win32 side of the program. Owns the scene, its layout and
// the viewport that zooms it into the window,
// the toolbar buttons, the cursors, and the editor's three parts (renderer, view,
// controller); translates Windows messages into calls to the controller.

#include <windows.h>

#include <optional>

#include "controller/editor_controller.h"
#include "layout.h"
#include "model/scene.h"
#include "gdi_editor_view.h"
#include "gdi_renderer.h"
#include "view/viewport.h"

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    bool create(int showCommand);       // registers the window class, creates and shows the window
    int  run();                         // the message loop; returns the exit code

    const Scene& scene() const { return scene_; }

private:
    struct Buttons {
        HWND cannon = nullptr, object = nullptr, source = nullptr, clearAll = nullptr;
        HWND lens = nullptr, mirror = nullptr, clearLast = nullptr, images = nullptr, exit = nullptr;
    };

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handle(UINT message, WPARAM wParam, LPARAM lParam);

    void onCreate();
    void onResize(int width, int height);
    void onButton(HWND button);
    void onMinimumSize(MINMAXINFO& limits) const;
    void setCursor() const;

    HWND createButton(const char* title);
    void positionButtons(int width, int height);
    void toggleFullscreen();
    void destroyEditor();

    HINSTANCE instance_;
    HWND      window_ = nullptr;

    Scene    scene_;
    Layout   layout_;       // the world: the layout the window had when it was created
    Viewport viewport_;     // how the world is zoomed into the window's current size

    std::optional<GdiRenderer>      renderer_;     // view: drawing with GDI
    std::optional<GdiEditorView>    editorView_;   // view: what the controller sees
    std::optional<EditorController> controller_;   // controller: the editing rules

    Buttons buttons_;
    HCURSOR arrowCursor_;
    HCURSOR handCursor_;
    HCURSOR crossCursor_;

    WINDOWPLACEMENT previousPlacement_ = {};
    bool            fullscreen_ = false;
};
