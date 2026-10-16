#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Direction.hpp"
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

    // quick-tile: the screen edges a window is tiled against. One edge = half, two = quarter.
    enum eTileEdges : uint8_t {
        TILE_NONE   = 0,
        TILE_LEFT   = (1 << 0),
        TILE_RIGHT  = (1 << 1),
        TILE_TOP    = (1 << 2),
        TILE_BOTTOM = (1 << 3),
    };

    struct SWindowStackData {
        eStackLayer layer     = LAYER_NORMAL;
        uint8_t     maximized = MAXIMIZE_NONE;
        uint8_t     tiled     = TILE_NONE;

        // window box (global, without decorations) to go back to when leaving maximize / quick-tile
        std::optional<CBox> restoreBox;

        // the window's taskbar button (wlr-foreign-toplevel set_rectangle), minimize animates into it
        std::optional<CBox> iconRect;
        bool                minimizing = false;
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

        // quick-tile. Maximize and quick-tile are exclusive and share the restore geometry.
        // monitor: tile onto another monitor (default: the window's own)
        void    setTiled(PHLWINDOW w, uint8_t edges, PHLMONITOR monitor = nullptr);
        uint8_t tiled(PHLWINDOW w) const;
        CBox    tiledBox(PHLWINDOW w, uint8_t edges, PHLMONITOR monitor = nullptr);
        bool    hasGeometryState(PHLWINDOW w) const;

        // where a maximized / tiled window goes back to
        std::optional<CBox> restoreBox(PHLWINDOW w) const;

        // the part of a work area a tile covers, decorations included
        static CBox tiledFrame(uint8_t edges, const CBox& workArea);

        // re-apply maximized / tiled geometry, e.g. after the work area changed
        void refitGeometry(PHLWINDOW w);

        // forget maximize / quick-tile without touching the geometry (manual move / resize)
        void dropGeometryState(PHLWINDOW w);

        // A maximized / tiled window is being dragged: leave that state, shrink to the restore
        // size and return the new grab point (relative to the window) so the cursor keeps its
        // relative position along the titlebar. nullopt if the window had no such state.
        std::optional<Vector2D> restoreForDrag(PHLWINDOW w, const Vector2D& hotspot);

        // minimize
        void setMinimized(PHLWINDOW w, bool minimized);
        bool minimized(PHLWINDOW w) const;

        // Minimize request from a client / foreign-toplevel. In non-stacking workspaces this only
        // forwards the request to IPC/event listeners, as upstream Hyprland does.
        void requestMinimize(PHLWINDOW w, bool minimized);

        // taskbar button of a window, global coordinates (nullopt: unknown)
        void setIconRect(PHLWINDOW w, std::optional<CBox> rect);

        // Move a window to the neighboring monitor in a direction, keeping its relative position
        // and its maximize / quick-tile state (Super + Shift + arrows on Windows)
        void moveToMonitor(PHLWINDOW w, Math::eDirection dir);

        // Ask the shell to show the window menu for a window at a global position
        // (socket2 event windowmenu>>ADDR,X,Y)
        void requestWindowMenu(PHLWINDOW w, const Vector2D& pos);

        // "show desktop": minimize every visible window; toggling again restores them.
        // Restoring or opening any window ends the mode (like on Windows).
        void toggleShowDesktop();
        bool showingDesktop() const;

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
        void                                     postGeometryState(PHLWINDOW w);
        void                                     applyGeometryState(PHLWINDOW w, PHLMONITOR monitor = nullptr);
        void                                     focusNextAfter(PHLWINDOW w);
        void                                     finishMinimize(PHLWINDOW w);
        CBox                                     minimizeTargetFor(PHLWINDOW w);

        std::map<PHLWINDOWREF, SWindowStackData> m_data;

        bool                                     m_showingDesktop      = false;
        bool                                     m_changingShowDesktop = false;
        std::vector<PHLWINDOWREF>                m_hiddenByShowDesktop;
        void                                     endShowDesktop();

        struct {
            CHyprSignalListener windowDestroy;
            CHyprSignalListener windowActive;
            CHyprSignalListener windowOpen;
        } m_listeners;
    };

    UP<CWindowStack>& windowStack();
}
