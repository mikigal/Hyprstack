#pragma once

#include "../../layout/algorithm/floating/default/DefaultFloatingAlgorithm.hpp"

namespace Layout::Floating {

    // Hyprstack: floating algorithm used by stacking workspaces.
    // Extends the default one with KWin-like placement of newly mapped windows.
    class CStackingFloatingAlgorithm : public CDefaultFloatingAlgorithm {
      public:
        CStackingFloatingAlgorithm()          = default;
        virtual ~CStackingFloatingAlgorithm() = default;

        virtual void newTarget(SP<ITarget> target);
        virtual void movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void moveTarget(const Vector2D& Δ, SP<ITarget> target);
        virtual void recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN);

      private:
        bool shouldPlace(SP<ITarget> target);

        // all boxes below are "full" boxes: window box + reserved extents (decorations)
        CBox placeSmart(SP<ITarget> target, const CBox& box, const CBox& workArea);
        CBox placeCascade(SP<ITarget> target, const CBox& box, const CBox& workArea);
        CBox placeUnderMouse(const CBox& box, const CBox& workArea);
        CBox placeCentered(const CBox& box, const CBox& workArea);
        CBox placeTransient(SP<ITarget> target, const CBox& box, const CBox& workArea);

        CBox clampIntoWorkArea(CBox box, const CBox& workArea);

        // full boxes of other visible windows in this space
        std::vector<CBox> otherWindowBoxes(SP<ITarget> target);
    };
};
