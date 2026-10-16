#include "StackMode.hpp"

#include "layout/StackingTiledAlgorithm.hpp"

#include "../desktop/view/window/Window.hpp"
#include "../config/ConfigValue.hpp"
#include "../layout/algorithm/Algorithm.hpp"
#include "../layout/space/Space.hpp"
#include "../workspace/HLWorkspace.hpp"

bool Stack::workspaceIsStacking(const PHLWORKSPACE& ws) {
    if (!ws || !ws->space() || !ws->space()->algorithm())
        return false;

    return dc<Layout::Tiled::CStackingTiledAlgorithm*>(ws->space()->algorithm()->tiledAlgo().get()) != nullptr;
}

bool Stack::stackingIsDefault() {
    static auto PLAYOUT = CConfigValue<Config::STRING>("general:layout");
    return *PLAYOUT == LAYOUT_NAME;
}

bool Stack::windowIsStacking(const PHLWINDOW& w) {
    if (!w)
        return false;

    return workspaceIsStacking(w->m_workspace);
}
