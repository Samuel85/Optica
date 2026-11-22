// Unit tests for Scene (scene.h). Plain C++20, no Windows or test framework needed.
// Build and run with:  cmake --build build --target scene_test && ctest --test-dir build

#include "../model/scene.h"

#include <cstdio>
#include <vector>

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

// Elements are identified in the tests by their y value.
static Element make(int x, int id, ElementType type = ElementType::Lens)
{
    return Element{ type, x, id, 0, ShapeConvex, 0 };
}

static std::vector<int> ids(const Scene& s)
{
    std::vector<int> result;
    for (const Element& e : s) result.push_back(e.y);
    return result;
}

static void testAddKeepsXOrder()
{
    Scene s;
    s.add(make(300, 1));
    s.add(make(100, 2));
    s.add(make(200, 3));
    s.add(make(400, 4));
    CHECK((ids(s) == std::vector<int>{ 2, 3, 1, 4 }));
}

static void testEqualXGoesAfterExisting()
{
    Scene s;
    s.add(make(100, 1));
    s.add(make(100, 2));
    s.add(make(50, 3));
    s.add(make(100, 4));
    CHECK((ids(s) == std::vector<int>{ 3, 1, 2, 4 }));
}

static void testAddReturnsIndex()
{
    Scene s;
    s.add(make(100, 1));
    s.add(make(300, 2));
    CHECK(s.add(make(200, 3)) == 1);
    CHECK(s.add(make(50, 4)) == 0);
    CHECK(s.add(make(999, 5)) == 4);
}

static void testRemove()
{
    Scene s;
    for (int i = 0; i < 4; ++i) s.add(make(i * 100, i));
    s.remove(0);
    CHECK((ids(s) == std::vector<int>{ 1, 2, 3 }));
    s.remove(2);
    CHECK((ids(s) == std::vector<int>{ 1, 2 }));
    s.remove(0);
    s.remove(0);
    CHECK(s.empty());
}

static void testRepositionMovesToSortedPlace()
{
    Scene s;
    for (int i = 0; i < 4; ++i) s.add(make(i * 100, i));   // ids 0..3 at x 0..300
    s[0].x = 250;
    CHECK(s.reposition(0) == 2);
    CHECK((ids(s) == std::vector<int>{ 1, 2, 0, 3 }));
    s[3].x = -10;
    CHECK(s.reposition(3) == 0);
    CHECK((ids(s) == std::vector<int>{ 3, 1, 2, 0 }));
}

static void testSelectionFollowsElement()
{
    Scene s;
    s.add(make(100, 1));
    s.add(make(200, 2));
    s.select(1);                       // id 2
    s.add(make(50, 3));                // inserted before it
    CHECK(s.selected() && s.selected()->y == 2);
    s.add(make(500, 4));               // inserted after it
    CHECK(s.selected() && s.selected()->y == 2);
    s.remove(0);                       // remove id 3, before it
    CHECK(s.selected() && s.selected()->y == 2);
    s.selected()->x = 1000;
    s.reposition(*s.selectedIndex());
    CHECK(s.selected() && s.selected()->y == 2);
    CHECK(*s.selectedIndex() == s.size() - 1);
}

static void testHoverFollowsElementIndependently()
{
    Scene s;
    s.add(make(100, 1));
    s.add(make(200, 2));
    s.add(make(300, 3));
    s.select(0);
    s.hover(2);
    s[2].x = 0;
    s.reposition(2);
    CHECK(s.hovered() && s.hovered()->y == 3);
    CHECK(s.selected() && s.selected()->y == 1);
}

static void testRemovingSelectedClearsIt()
{
    Scene s;
    s.add(make(100, 1));
    s.add(make(200, 2));
    s.select(1);
    s.hover(1);
    s.removeSelected();
    CHECK(s.selected() == nullptr);
    CHECK(s.hovered() == nullptr);
    CHECK((ids(s) == std::vector<int>{ 1 }));
    s.removeSelected();                // nothing selected: no effect
    CHECK(s.size() == 1);
}

static void testClear()
{
    Scene s;
    s.add(make(100, 1));
    s.select(0);
    s.hover(0);
    s.clear();
    CHECK(s.empty());
    CHECK(s.selected() == nullptr);
    CHECK(s.hovered() == nullptr);
}

int main()
{
    testAddKeepsXOrder();
    testEqualXGoesAfterExisting();
    testAddReturnsIndex();
    testRemove();
    testRepositionMovesToSortedPlace();
    testSelectionFollowsElement();
    testHoverFollowsElementIndependently();
    testRemovingSelectedClearsIt();
    testClear();

    if (failures == 0) std::printf("All Scene tests passed.\n");
    return failures == 0 ? 0 : 1;
}
