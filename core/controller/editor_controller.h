#pragma once

// The editing rules: what the toolbar buttons and the mouse do to the scene, and what
// the view should show meanwhile. Knows nothing about Windows: it is driven by plain
// calls and draws only through EditorView.
//
// The editor is always in exactly one of these states:
//
//   Idle            nothing in hand. Moving the mouse finds what is under it: an
//                   element's body (press to drag it) or a focal point (press to move it).
//   Placing         a toolbar button was pressed; the next press on the canvas places
//                   a new element there.
//   Dragging        the button went down on an element's body; it follows the mouse
//                   and is dropped where the button is released.
//   AdjustingFocus  the button went down on a lens's or mirror's focal point, or on a
//                   cannon's aim marker; releasing the button sets the new focus or aim.
//
// While Dragging or AdjustingFocus the scene itself is changed only when the button is
// released, but every mouse move redraws a copy of it with the change already made, so
// the rays and the images follow the mouse.

#include <optional>
#include <variant>

#include "../layout.h"
#include "../model/scene.h"
#include "editor_view.h"

class EditorController {
public:
    EditorController(Scene& scene, const Layout& layout, EditorView& view);

    EditorController(const EditorController&) = delete;
    EditorController& operator=(const EditorController&) = delete;

    // ---- toolbar buttons ---------------------------------------------------
    void cannonButton();
    void objectButton();
    void sourceButton();
    void lensButton();
    void mirrorButton();
    void clearAllButton();
    void clearLastButton();

    // ---- mouse, in window coordinates ---------------------------------------
    void mouseDown(int x, int y);
    void mouseMove(int x, int y);
    void mouseUp(int x, int y);

    CursorKind cursor() const;          // which mouse cursor to show

    // Extra distance, in world units, at which elements and focal points can still be
    // grabbed: 0 for a mouse (the default), more for a finger on a touch screen.
    void setGrabMargin(int margin)      { grabMargin_ = margin; }

private:
    struct Idle {};

    struct Placing {
        ElementType type;
        int         width;              // lens thickness or mirror depth of the new element
    };

    struct Dragging {
        Scene::Index index;
    };

    struct AdjustingFocus {
        Scene::Index index;
        int  pressX;                    // which side of a lens the drag started on
        int  width;                     // lens thickness / mirror depth so far
        bool concave;                   // lens shape so far
        int  aimOffset;                 // cannon aim so far
    };

    using State = std::variant<Idle, Placing, Dragging, AdjustingFocus>;

    enum class Part { Body, Focus };
    struct Hit {
        Scene::Index index;
        Part         part;
    };

    bool insideCanvas(int x, int y) const;
    std::optional<Hit> hitTest(int x, int y) const;
    void preview(ElementType type, int x, int y, int width, bool concave);
    void showScene();
    void endActionOnElement();

    void hoverAt(int x, int y);
    void adjustTowards(AdjustingFocus& adjusting, int x, int y);
    void place(const Placing& placing, int x, int y);
    static void drop(Scene& scene, const Dragging& dragging, int x, int y);
    static void finishAdjusting(Scene& scene, const AdjustingFocus& adjusting, int x);

    Scene&             scene_;
    const Layout&      layout_;
    EditorView&        view_;
    State              state_ = Idle{};
    std::optional<Hit> hover_;          // what is under the mouse (Idle only)
    int                grabMargin_ = 0;
};
