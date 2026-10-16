#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"

#include <map>
#include <optional>
#include <vector>

namespace Desktop {
    enum eFocusReason : uint32_t;
}

namespace Stack {

    enum eStackLayer : uint8_t {
        LAYER_BELOW = 0,
        LAYER_NORMAL,
        LAYER_ABOVE,
    };

    enum eMaximizeMode : uint8_t {
        MAXIMIZE_NONE       = 0,
        MAXIMIZE_HORIZONTAL = (1 << 0),
        MAXIMIZE_VERTICAL   = (1 << 1),
        MAXIMIZE_FULL       = MAXIMIZE_HORIZONTAL | MAXIMIZE_VERTICAL,
    };

    struct SWindowStackData {
        eStackLayer layer     = LAYER_NORMAL;
        uint8_t     maximized = MAXIMIZE_NONE;

        // window box (global, without decorations) to go back to when leaving maximize
        std::optional<CBox> restoreBox;
    };

    // Hyprstack: per-window state of the stacking window manager — maximize, minimize,
    // stacking layers (keep above / below) and z-order policy (transients above parents).
    class CWindowStack {
      public:
        CWindowStack();
        ~CWindowStack() = default;

        // maximize
        void    setMaximized(PHLWINDOW w, uint8_t mode);
        void    toggleMaximized(PHLWINDOW w, uint8_t mode = MAXIMIZE_FULL);
        uint8_t maximized(PHLWINDOW w) const;
        CBox    maximizedBox(PHLWINDOW w, uint8_t mode);

        // re-apply maximized geometry, e.g. after the work area changed
        void refitMaximized(PHLWINDOW w);

        // forget the maximized state without touching the geometry (manual move / resize)
        void dropMaximized(PHLWINDOW w);

        // minimize
        void setMinimized(PHLWINDOW w, bool minimized);
        bool minimized(PHLWINDOW w) const;

        // Minimize request from a client / foreign-toplevel. In non-stacking workspaces this only
        // forwards the request to IPC/event listeners, as upstream Hyprland does.
        void requestMinimize(PHLWINDOW w, bool minimized);

        // layers
        void        setLayer(PHLWINDOW w, eStackLayer layer);
        eStackLayer layer(PHLWINDOW w) const;

        // Called by Desktop::CWindowState after every z-order change. Enforces layers
        // and keeps transients (dialogs) above their parents. Returns true if anything moved.
        bool restack(std::vector<PHLWINDOW>& windows);

        // Intercepts fullscreen-controller requests: in stacking workspaces "maximized" is not a
        // fullscreen mode but a geometry state handled here. Returns true if handled.
        bool interceptFullscreenRequest(PHLWINDOW w, std::optional<uint8_t> internal, std::optional<uint8_t> client);

      private:
        SWindowStackData&                        data(PHLWINDOW w);
        const SWindowStackData*                  find(PHLWINDOW w) const;
        void                                     purgeExpired();
        void                                     postMaximizeState(PHLWINDOW w);
        void                                     focusNextAfter(PHLWINDOW w);

        std::map<PHLWINDOWREF, SWindowStackData> m_data;

        struct {
            CHyprSignalListener windowDestroy;
            CHyprSignalListener windowActive;
        } m_listeners;
    };

    UP<CWindowStack>& windowStack();
}
