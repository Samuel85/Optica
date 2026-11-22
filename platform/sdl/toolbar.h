#pragma once

// The two button bars, drawn with Dear ImGui: the same buttons, sizes and positions as
// the Windows toolbar (layout.h), each row centred in the area they are drawn in.

#include "imgui.h"

enum class ToolbarButton {
    None,
    Cannon, Object, Source, ClearAll,                 // top row
    Lens, Mirror, ClearLast, Images, Exit             // bottom row
};

// Draws both bars at the top and bottom of an area of this size whose top left corner is
// `origin`; returns the button clicked in this frame. Without `withExit` the bottom row
// has no Exit button (iOS apps do not quit themselves).
// Call between ImGui::NewFrame() and ImGui::Render().
ToolbarButton DrawToolbar(ImVec2 origin, int clientWidth, int clientHeight, bool withExit);
