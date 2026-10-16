#include "StackingTiledAlgorithm.hpp"

#include "../StackMode.hpp"

#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/target/Target.hpp"
#include "../../managers/eventLoop/EventLoopManager.hpp"

using namespace Layout;
using namespace Layout::Tiled;

void CStackingTiledAlgorithm::newTarget(SP<ITarget> target) {
    handOver(target);
}

void CStackingTiledAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    handOver(target);
}

void CStackingTiledAlgorithm::removeTarget(SP<ITarget> target) {
    ;
}

void CStackingTiledAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    ;
}

void CStackingTiledAlgorithm::recalculate(eRecalculateReason reason) {
    ;
}

SP<ITarget> CStackingTiledAlgorithm::getNextCandidate(SP<ITarget> old) {
    return nullptr;
}

void CStackingTiledAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    ;
}

void CStackingTiledAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    ;
}

std::optional<std::string> CStackingTiledAlgorithm::layoutName() const {
    return Stack::LAYOUT_NAME;
}

void CStackingTiledAlgorithm::handOver(SP<ITarget> target) {
    // New windows on stacking workspaces are made floating before they reach the layout
    // (see CWindow's map path), so this is only a safety net for layout switches, rules and
    // plugins. Deferred, because we may be called while CAlgorithm iterates its target lists.
    g_pEventLoopManager->doLater([weakTarget = WP<ITarget>{target}, weakParent = m_parent] {
        const auto TARGET = weakTarget.lock();
        const auto PARENT = weakParent.lock();
        if (!TARGET || !PARENT || TARGET->floating() || !TARGET->space() || TARGET->space()->algorithm() != PARENT)
            return;

        // reposition = true: let the floating algorithm place it like a new window
        PARENT->setFloating(TARGET, true, true);
    });
}
