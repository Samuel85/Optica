#pragma once

// Optica's application for SDL3 + Dear ImGui: the counterpart of the Windows MainWindow.
// Owns the window, the renderer, the cursors and Dear ImGui, and the editor's parts
// (scene, view state, controller, painter); turns SDL events into calls to the
// controller and draws one frame per SDL_AppIterate.
//
// Screen density: everything (layout, viewport, Dear ImGui, the painter) works in
// "logical" units, as on Windows at 100 %. One logical unit is uiScale_ window
// coordinates: the display's content scale (e.g. 2.75 on a phone, 1.5 on a desktop set
// to 150 %), reduced if needed so the smallest layout (MinClientWidth x MinClientHeight)
// still fits on the screen. SDL events are converted to logical units on arrival.
//
// Touch: SDL turns a finger into mouse events (marked SDL_TOUCH_MOUSEID); a finger
// press moves the "mouse" there first, so the controller finds what is under it as it
// does for a hovering mouse. For a finger, the controller's grab margin is widened.
//
// Safe area: on phones with a notch, camera cut-out or home indicator, the canvas and
// both button bars are laid out inside the window's safe area (SDL_GetWindowSafeArea),
// starting at origin_. On desktops the safe area is the whole window.

#include <SDL3/SDL.h>

#include <memory>
#include <string>

#include "controller/editor_controller.h"
#include "imgui_editor_view.h"
#include "layout.h"
#include "model/scene.h"
#include "scene_painter.h"
#include "toolbar.h"
#include "view/viewport.h"

class SdlApp {
public:
    SdlApp();
    ~SdlApp();

    SdlApp(const SdlApp&) = delete;
    SdlApp& operator=(const SdlApp&) = delete;

    SDL_AppResult init();                               // window, renderer, Dear ImGui
    SDL_AppResult handleEvent(const SDL_Event& event);
    SDL_AppResult iterate();                            // one frame

private:
    struct SdlDeleter {
        void operator()(SDL_Window* window) const     { SDL_DestroyWindow(window); }
        void operator()(SDL_Renderer* renderer) const { SDL_DestroyRenderer(renderer); }
        void operator()(SDL_Cursor* cursor) const     { SDL_DestroyCursor(cursor); }
    };

    void relayout();                                    // viewport and origin_ for the window's size and safe area
    SDL_Rect safeArea() const;                          // in window coordinates
    void onButton(ToolbarButton button);
    void showMessages();
    void updateCursor();
    void toggleFullscreen();
    bool fullscreen() const;

    int  worldX(float windowX) const;                   // mouse position, window coordinates -> world
    int  worldY(float windowY) const;
    bool onCanvas(float windowY) const;                 // between the button bars, no message open
    SDL_Event toLogical(const SDL_Event& event) const;  // mouse positions in logical units, for Dear ImGui
    void updateScale();                                 // uiScale_ for the window's display and size

    std::unique_ptr<SDL_Window, SdlDeleter>   window_;
    std::unique_ptr<SDL_Renderer, SdlDeleter> renderer_;
    std::unique_ptr<SDL_Cursor, SdlDeleter>   arrowCursor_, handCursor_, crossCursor_;
    bool imguiReady_ = false;
    float uiScale_   = 1.0f;                            // window coordinates per logical unit
    ImVec2 origin_   = ImVec2(0.0f, 0.0f);              // the safe area's top left corner, logical units

    Scene            scene_;
    Layout           layout_;           // the world: the layout the window had when it was created
    Viewport         viewport_;         // how the world is zoomed into the window's current size
    ImGuiEditorView  view_;
    EditorController controller_;
    ScenePainter     painter_;

    bool        pressedOnCanvas_ = false;   // the left button went down on the canvas, not on a button
    bool        overButton_      = false;   // the mouse is over a toolbar button (last frame)
    bool        messageOpen_     = false;
    std::string message_;
    int         shownCursor_     = -1;      // CursorKind last set, -1 before the first
    bool        quit_            = false;
};
