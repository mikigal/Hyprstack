#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace Stack {

    // Hyprstack: remembers where and how big application windows were when they were closed
    // (per app id), so they come back the same way — Wayland clients can't position themselves.
    // Stored in $XDG_STATE_HOME/hyprstack/window-geometry.
    class CGeometryMemory {
      public:
        CGeometryMemory();
        ~CGeometryMemory() = default;

        struct SEntry {
            std::string monitor;
            Vector2D    offset; // window position relative to the monitor's work area
            Vector2D    size;   // window (client) size
        };

        std::optional<SEntry> get(const std::string& appID) const;
        void                  remember(PHLWINDOW w);

        // what placement should use for a newly mapped window: size and/or position (window box)
        struct SRestore {
            std::optional<Vector2D> size;
            std::optional<Vector2D> windowPos;
        };
        SRestore restoreFor(PHLWINDOW w, const CBox& workArea);

      private:
        void                                    load();
        void                                    save() const;
        std::string                             path() const;

        std::unordered_map<std::string, SEntry> m_entries;
        bool                                    m_loaded = false;

        struct {
            CHyprSignalListener windowClose;
        } m_listeners;
    };

    UP<CGeometryMemory>& geometryMemory();
}
