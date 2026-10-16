#pragma once

#include "../desktop/DesktopTypes.hpp"
#include "../helpers/math/Direction.hpp"
#include "../helpers/math/Math.hpp"
#include "../helpers/memory/Memory.hpp"
#include "../helpers/signal/Signal.hpp"

#include <optional>

namespace Layout {
    class ITarget;
}

namespace Render {
    class CRenderContext;
}

namespace Stack {

    // what dropping a window at the current cursor position would do
    struct SQuickTileTarget {
        PHLMONITOR monitor;
        uint8_t    edges    = 0; // eTileEdges
        bool       maximize = false;

        bool       operator==(const SQuickTileTarget&) const = default;
    };

    // Hyprstack: KDE-like quick-tiling by dragging a window against a screen edge (half) or
    // corner (quarter), or the top edge (maximize). Shows a preview while dragging. Only outer
    // screen edges trigger, so windows can still be moved between monitors.
    class CQuickTileController {
      public:
        CQuickTileController();
        ~CQuickTileController() = default;

        // hooked into the drag controller (move drags only)
        void                            onDragMotion(SP<Layout::ITarget> target, const Vector2D& mousePos);

        std::optional<SQuickTileTarget> targetAt(const Vector2D& pos) const;
        std::optional<SQuickTileTarget> currentTarget() const;

        // Windows-like keyboard snapping (Super + arrows), see the dispatcher docs
        void snap(PHLWINDOW w, Math::eDirection dir);

      private:
        void                            onDragEnded();
        void                            setPreview(std::optional<SQuickTileTarget> target);
        CBox                            previewBox(const SQuickTileTarget& target) const;
        void                            damagePreview();
        void                            renderPreview(Render::CRenderContext& ctx, PHLMONITOR monitor);

        WP<Layout::ITarget>             m_dragTarget;
        std::optional<SQuickTileTarget> m_target;
        bool                            m_previewDrawn = false;

        struct {
            CHyprSignalListener dragEnded;
            CHyprSignalListener render;
        } m_listeners;
    };

    UP<CQuickTileController>& quickTile();
}
