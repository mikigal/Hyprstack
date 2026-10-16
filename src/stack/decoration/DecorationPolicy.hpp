#pragma once

#include "../../desktop/DesktopTypes.hpp"
#include "../../helpers/math/Math.hpp"

#include <optional>

namespace Stack {
    // the client negotiated server-side decorations (xdg-decoration / KDE server decoration)
    bool negotiatedServerSide(PHLWINDOW w);

    // Wayland window on a stacking workspace that draws its own decorations (GTK, Chromium, ...)
    bool isClientDecorated(PHLWINDOW w);

    // Called when a window is first placed: client-decorated windows don't get the compositor's
    // border, shadow and rounding (they draw their own), unless stacking:decorate_csd is set.
    void applyDecorationPolicy(PHLWINDOW w);

    // How far the main surface reaches outside its xdg window geometry (CSD shadows / resize
    // margins), in logical pixels, scaled to the window's current (possibly animated) size.
    std::optional<SBoxExtents> surfaceOverflow(PHLWINDOW w);
}
