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

    // The box resize-on-border grabs around: the window plus its titlebar, if it has one.
    CBox resizeFrame(PHLWINDOW w, const CBox& windowBox);

    // A point inside the window that still counts as its border (rounded corners). The top
    // corners of a window with a titlebar are covered by the titlebar.
    bool inResizeCorner(PHLWINDOW w, const Vector2D& pos);
}
