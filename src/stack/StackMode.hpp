#pragma once

#include "../desktop/DesktopTypes.hpp"

// Hyprstack: stacking-mode helpers.
// A workspace is in stacking mode when its layout resolves to "stacking".
namespace Stack {
    inline constexpr const char* LAYOUT_NAME = "stacking";

    bool                         workspaceIsStacking(const PHLWORKSPACE& ws);
    bool                         windowIsStacking(const PHLWINDOW& w);

    // true when general:layout is "stacking", for code that runs before a window has a workspace
    bool stackingIsDefault();
}
