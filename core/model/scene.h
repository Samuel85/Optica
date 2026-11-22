#pragma once

// The optical elements on the bench, kept in order of their x position.
//
// Replaces the hand-written doubly linked list (lista.h). Elements are stored by
// value in a std::vector; "next/previous element along the axis" is index +/- 1.
//
// The scene also tracks two element references that the UI needs between
// window messages: the selected element (being edited or dragged) and the
// hovered element (last one examined under the mouse). These are indices that
// the scene keeps pointing at the same element when elements are added,
// removed or re-sorted.

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <vector>

enum class ElementType {
    Cannon   = 1,
    Object   = 2,
    Source   = 3,
    Lens     = 4,
    Mirror   = 5,
    Aperture = 6
};

enum ElementShape {
    ShapeConvex  = 0,
    ShapeConcave = 1
};

struct Element {
    ElementType type;
    int x;       // position along the axis, window coordinates
    int y;       // window coordinates
    int focus;   // lens/mirror: x of the focal point; cannon: aim offset in y
    int shape;   // ElementShape (lenses)
    int width;   // lens thickness or mirror depth
};

class Scene {
public:
    using Index = std::size_t;

    // Inserts in x order, after any elements with the same x. Returns the new index.
    Index add(const Element& element)
    {
        auto position = std::ranges::upper_bound(elements_, element.x, {}, &Element::x);
        Index index = static_cast<Index>(std::distance(elements_.begin(), position));
        elements_.insert(position, element);
        shiftAfterInsert(index, selected_);
        shiftAfterInsert(index, hovered_);
        return index;
    }

    void remove(Index index)
    {
        elements_.erase(elements_.begin() + static_cast<std::ptrdiff_t>(index));
        shiftAfterRemove(index, selected_);
        shiftAfterRemove(index, hovered_);
    }

    // Moves an element whose x has changed to its sorted place. Returns the new index;
    // the selection and hover follow the element.
    Index reposition(Index index)
    {
        const bool wasSelected = selected_ == index;
        const bool wasHovered  = hovered_ == index;
        Element element = elements_[index];
        remove(index);
        Index newIndex = add(element);
        if (wasSelected) selected_ = newIndex;
        if (wasHovered)  hovered_  = newIndex;
        return newIndex;
    }

    void clear()
    {
        elements_.clear();
        selected_.reset();
        hovered_.reset();
    }

    // ---- Selected element ----------------------------------------------
    void select(std::optional<Index> index) { selected_ = index; }
    std::optional<Index> selectedIndex() const { return selected_; }
    Element* selected() { return selected_ ? &elements_[*selected_] : nullptr; }

    void removeSelected()
    {
        if (selected_) remove(*selected_);
    }

    // ---- Hovered element -----------------------------------------------
    void hover(std::optional<Index> index) { hovered_ = index; }
    std::optional<Index> hoveredIndex() const { return hovered_; }
    Element* hovered() { return hovered_ ? &elements_[*hovered_] : nullptr; }

    // ---- Access ----------------------------------------------------------
    Element&       operator[](Index index)       { return elements_[index]; }
    const Element& operator[](Index index) const { return elements_[index]; }
    std::size_t size()  const { return elements_.size(); }
    bool        empty() const { return elements_.empty(); }

    auto begin()       { return elements_.begin(); }
    auto end()         { return elements_.end(); }
    auto begin() const { return elements_.begin(); }
    auto end()   const { return elements_.end(); }

private:
    static void shiftAfterInsert(Index inserted, std::optional<Index>& reference)
    {
        if (reference && *reference >= inserted) ++*reference;
    }

    static void shiftAfterRemove(Index removed, std::optional<Index>& reference)
    {
        if (!reference) return;
        if (*reference == removed)     reference.reset();
        else if (*reference > removed) --*reference;
    }

    std::vector<Element> elements_;
    std::optional<Index> selected_;
    std::optional<Index> hovered_;
};

