#pragma once

#include "../../config/values/ConfigValues.hpp"

namespace Stack::Config {
    // Hyprstack-specific config values, appended to the upstream list.
    std::vector<SP<::Config::Values::IValue>> getConfigValues();
}
