#include "toolbar.h"

#include <cstddef>
#include <span>

#include "imgui.h"

#include "layout.h"

namespace {

struct ButtonInfo {
    const char*   title;
    ToolbarButton button;
};

const ButtonInfo TopRow[] = {
    { "Cannon",             ToolbarButton::Cannon },
    { "Object",             ToolbarButton::Object },
    { "Light Source",       ToolbarButton::Source },
    { "Clear All",          ToolbarButton::ClearAll },
};

const ButtonInfo BottomRow[] = {
    { "Lens",               ToolbarButton::Lens },
    { "Mirror",             ToolbarButton::Mirror },
    { "Clear",              ToolbarButton::ClearLast },
    { "Images",             ToolbarButton::Images },
    { "Exit",               ToolbarButton::Exit },
};

constexpr ImGuiWindowFlags BarFlags =
    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing;

/* One bar across the area, its top left corner at `topLeft`, with a row of buttons
   centred in it. */
ToolbarButton DrawBar(const char* name, std::span<const ButtonInfo> row, ImVec2 topLeft, float height, int clientWidth)
{
    ToolbarButton clicked = ToolbarButton::None;
    const std::size_t N = row.size();

    ImGui::SetNextWindowPos(topLeft);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(clientWidth), height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    if (ImGui::Begin(name, nullptr, BarFlags))
    {
        const int left = (clientWidth - static_cast<int>(N) * ButtonPitch) / 2;
        for (std::size_t i = 0; i < N; ++i)
        {
            ImGui::SetCursorPos(ImVec2(static_cast<float>(left + static_cast<int>(i) * ButtonPitch),
                                       static_cast<float>(ButtonTopMargin)));
            if (ImGui::Button(row[i].title, ImVec2(static_cast<float>(ButtonWidth), static_cast<float>(ButtonHeight))))
                clicked = row[i].button;
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    return clicked;
}

} // namespace

ToolbarButton DrawToolbar(ImVec2 origin, int clientWidth, int clientHeight, bool withExit)
{
    std::span<const ButtonInfo> bottomRow(BottomRow);
    if (!withExit)
        bottomRow = bottomRow.first(bottomRow.size() - 1);       /* Exit is the last one */

    const ToolbarButton top = DrawBar("##top", TopRow, origin, static_cast<float>(ToolbarHeight), clientWidth);
    const ToolbarButton bottom = DrawBar("##bottom", bottomRow,
                                         ImVec2(origin.x, origin.y + clientHeight - BottomBarHeight),
                                         static_cast<float>(BottomBarHeight), clientWidth);
    return top != ToolbarButton::None ? top : bottom;
}
