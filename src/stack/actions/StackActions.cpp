#include "StackActions.hpp"
#include "../StackMode.hpp"

#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/view/window/Window.hpp"

using namespace Config::Actions;

static PHLWINDOW xtract(std::optional<PHLWINDOW> window) {
    return window.value_or(Desktop::focusState()->window());
}

static bool resolveToggle(eTogglableAction action, bool current) {
    switch (action) {
        case TOGGLE_ACTION_ENABLE: return true;
        case TOGGLE_ACTION_DISABLE: return false;
        default: return !current;
    }
}

static ActionResult noTarget() {
    return std::unexpected(SActionError{"no target window", eActionErrorLevel::WARNING, eActionErrorCode::NO_TARGET});
}

static ActionResult notStacking() {
    return std::unexpected(SActionError{"window is not on a stacking workspace", eActionErrorLevel::WARNING, eActionErrorCode::INVALID_STATE});
}

ActionResult Stack::Actions::minimize(eTogglableAction action, std::optional<PHLWINDOW> w) {
    const auto WINDOW = xtract(w);
    if (!WINDOW)
        return noTarget();
    if (!windowIsStacking(WINDOW))
        return notStacking();

    windowStack()->setMinimized(WINDOW, resolveToggle(action, WINDOW->isMinimized()));
    return {};
}

ActionResult Stack::Actions::maximize(eTogglableAction action, uint8_t mode, std::optional<PHLWINDOW> w) {
    const auto WINDOW = xtract(w);
    if (!WINDOW)
        return noTarget();
    if (!windowIsStacking(WINDOW))
        return notStacking();

    const bool ENABLE = resolveToggle(action, windowStack()->maximized(WINDOW) == mode);
    windowStack()->setMaximized(WINDOW, ENABLE ? mode : MAXIMIZE_NONE);
    return {};
}

static ActionResult setLayerToggle(eTogglableAction action, Stack::eStackLayer layer, std::optional<PHLWINDOW> w) {
    const auto WINDOW = xtract(w);
    if (!WINDOW)
        return noTarget();

    const bool ENABLE = resolveToggle(action, Stack::windowStack()->layer(WINDOW) == layer);
    Stack::windowStack()->setLayer(WINDOW, ENABLE ? layer : Stack::LAYER_NORMAL);
    return {};
}

ActionResult Stack::Actions::keepAbove(eTogglableAction action, std::optional<PHLWINDOW> w) {
    return setLayerToggle(action, LAYER_ABOVE, w);
}

ActionResult Stack::Actions::keepBelow(eTogglableAction action, std::optional<PHLWINDOW> w) {
    return setLayerToggle(action, LAYER_BELOW, w);
}
