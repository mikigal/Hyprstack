#pragma once

#include "../../config/shared/actions/ConfigActions.hpp"
#include "../WindowStack.hpp"

// Hyprstack: actions behind the stacking dispatchers (hl.dsp.window.minimize & co.)
namespace Stack::Actions {
    using Config::Actions::ActionResult;
    using Config::Actions::eTogglableAction;

    ActionResult minimize(eTogglableAction action, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult maximize(eTogglableAction action, uint8_t mode = MAXIMIZE_FULL, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult keepAbove(eTogglableAction action, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
    ActionResult keepBelow(eTogglableAction action, std::optional<PHLWINDOW> window = std::nullopt /* Active */);
}
