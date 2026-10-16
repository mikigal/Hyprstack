#pragma once

#include "../../layout/algorithm/TiledAlgorithm.hpp"

namespace Layout::Tiled {

    // Hyprstack: the "stacking" layout. There is no tiling in stacking mode: every target
    // that reaches the tiled side is immediately handed over to the floating algorithm.
    class CStackingTiledAlgorithm : public ITiledAlgorithm {
      public:
        CStackingTiledAlgorithm()          = default;
        virtual ~CStackingTiledAlgorithm() = default;

        virtual void                       newTarget(SP<ITarget> target);
        virtual void                       movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint = std::nullopt);
        virtual void                       removeTarget(SP<ITarget> target);

        virtual void                       resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner = CORNER_NONE);
        virtual void                       recalculate(eRecalculateReason reason = RECALCULATE_REASON_UNKNOWN);

        virtual SP<ITarget>                getNextCandidate(SP<ITarget> old);

        virtual void                       swapTargets(SP<ITarget> a, SP<ITarget> b);
        virtual void                       moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent);

        virtual std::optional<std::string> layoutName() const;

      private:
        void handOver(SP<ITarget> target);
    };
};
