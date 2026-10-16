#include "StackingFloatingAlgorithm.hpp"
#include "../WindowStack.hpp"

#include "../../layout/algorithm/Algorithm.hpp"
#include "../../layout/space/Space.hpp"
#include "../../layout/target/WindowTarget.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/history/WindowHistoryTracker.hpp"
#include "../../desktop/rule/windowRule/WindowRuleApplicator.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../managers/input/InputManager.hpp"

#include <algorithm>
#include <limits>
#include <ranges>

using namespace Layout;
using namespace Layout::Floating;

static SBoxExtents extentsFor(SP<ITarget> t) {
    return t->window() ? t->window()->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS) : SBoxExtents{};
}

void CStackingFloatingAlgorithm::newTarget(SP<ITarget> target) {
    const bool FIRST_MAP = target->window() && (target->window()->m_state & Desktop::View::WINDOW_STATE_FIRST_MAP);

    CDefaultFloatingAlgorithm::newTarget(target);

    if (!FIRST_MAP || !shouldPlace(target))
        return;

    static auto PPLACEMENT  = CConfigValue<Config::STRING>("stacking:placement");
    static auto PTRANSIENTS = CConfigValue<Config::INTEGER>("stacking:center_transients");
    static auto PCLAMP      = CConfigValue<Config::INTEGER>("stacking:clamp_new_to_workarea");

    const auto  WORK_AREA = m_parent->space()->workArea(true);
    const auto  EXTENTS   = extentsFor(target);

    CBox        box = target->position().copy().addExtents(EXTENTS);

    if (*PCLAMP) {
        box.w = std::min(box.w, WORK_AREA.w);
        box.h = std::min(box.h, WORK_AREA.h);
    }

    const auto PARENT = target->window()->backend().parent();

    if (*PTRANSIENTS && PARENT && PARENT->m_workspace == target->window()->m_workspace)
        box = placeTransient(target, box, WORK_AREA);
    else if (*PPLACEMENT == "center")
        box = placeCentered(box, WORK_AREA);
    else if (*PPLACEMENT == "cascade")
        box = placeCascade(target, box, WORK_AREA);
    else if (*PPLACEMENT == "mouse")
        box = placeUnderMouse(box, WORK_AREA);
    else
        box = placeSmart(target, box, WORK_AREA);

    box = clampIntoWorkArea(box, WORK_AREA);

    target->setPositionGlobal(box.addExtents(SBoxExtents{.topLeft = -EXTENTS.topLeft, .bottomRight = -EXTENTS.bottomRight}));
    updateTarget(target);
}

void CStackingFloatingAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> focalPoint) {
    CDefaultFloatingAlgorithm::movedTarget(target, focalPoint);

    // e.g. moved to another monitor: maximized windows follow the new work area
    if (target->window())
        Stack::windowStack()->refitMaximized(target->window());
}

void CStackingFloatingAlgorithm::resizeTarget(const Vector2D& Δ, SP<ITarget> target, eRectCorner corner) {
    // a manual resize ends maximize, keeping the current geometry
    if (target->window())
        Stack::windowStack()->dropMaximized(target->window());

    CDefaultFloatingAlgorithm::resizeTarget(Δ, target, corner);
}

void CStackingFloatingAlgorithm::moveTarget(const Vector2D& Δ, SP<ITarget> target) {
    if (target->window())
        Stack::windowStack()->dropMaximized(target->window());

    CDefaultFloatingAlgorithm::moveTarget(Δ, target);
}

void CStackingFloatingAlgorithm::recalculate(eRecalculateReason reason) {
    CDefaultFloatingAlgorithm::recalculate(reason);

    // the work area may have changed (monitor mode, reserved areas of panels)
    for (const auto& [weakTarget, data] : m_datas) {
        const auto TARGET = weakTarget.lock();
        if (TARGET && TARGET->window())
            Stack::windowStack()->refitMaximized(TARGET->window());
    }
}

bool CStackingFloatingAlgorithm::shouldPlace(SP<ITarget> target) {
    const auto WINDOW = target->window();
    if (!WINDOW || WINDOW->isHidden())
        return false;

    const auto TRAITS = WINDOW->backend().traits();
    if (TRAITS.overrideRedirect)
        return false;

    // explicit placement from window rules wins
    if (WINDOW->m_ruleApplicator->static_.position || WINDOW->m_ruleApplicator->static_.center.value_or(false))
        return false;

    // X11 clients may position themselves
    const auto DESIRED = target->desiredGeometry();
    if (DESIRED && DESIRED->pos && WINDOW->backend().isX11())
        return false;

    return true;
}

std::vector<CBox> CStackingFloatingAlgorithm::otherWindowBoxes(SP<ITarget> target) {
    std::vector<CBox> boxes;

    const auto        WORKSPACE = m_parent->space()->workspace();

    for (const auto& w : Desktop::windowState()->windows()) {
        if (w == target->window() || w->m_workspace != WORKSPACE || !w->mapped() || w->isHidden() || !w->layoutTarget())
            continue;

        boxes.emplace_back(w->layoutTarget()->position().copy().addExtents(w->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS)));
    }

    return boxes;
}

CBox CStackingFloatingAlgorithm::placeCentered(const CBox& box, const CBox& workArea) {
    return CBox{workArea.middle() - box.size() / 2.F, box.size()};
}

CBox CStackingFloatingAlgorithm::placeUnderMouse(const CBox& box, const CBox& workArea) {
    return CBox{g_pInputManager->getMouseCoordsInternal() - box.size() / 2.F, box.size()};
}

CBox CStackingFloatingAlgorithm::placeTransient(SP<ITarget> target, const CBox& box, const CBox& workArea) {
    const auto PARENT = target->window()->backend().parent();
    if (!PARENT || !PARENT->layoutTarget())
        return placeCentered(box, workArea);

    const auto PARENT_BOX = PARENT->layoutTarget()->position();
    return CBox{PARENT_BOX.middle() - box.size() / 2.F, box.size()};
}

CBox CStackingFloatingAlgorithm::placeCascade(SP<ITarget> target, const CBox& box, const CBox& workArea) {
    static auto PCASCADE = CConfigValue<Config::INTEGER>("stacking:cascade_offset");

    const auto  WORKSPACE = m_parent->space()->workspace();

    // cascade off the most recently focused window in this space
    for (const auto& ref : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
        const auto w = ref.lock();
        if (!w || w == target->window() || w->m_workspace != WORKSPACE || !w->mapped() || w->isHidden() || !w->layoutTarget())
            continue;

        const auto LAST = w->layoutTarget()->position().copy().addExtents(w->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS));
        CBox       next = {LAST.pos() + Vector2D{sc<double>(*PCASCADE), sc<double>(*PCASCADE)}, box.size()};

        // wrap around to the top-left corner when we fall off the work area
        if (next.x + next.w > workArea.x + workArea.w || next.y + next.h > workArea.y + workArea.h)
            next = {workArea.pos(), box.size()};

        return next;
    }

    return {workArea.pos(), box.size()};
}

CBox CStackingFloatingAlgorithm::placeSmart(SP<ITarget> target, const CBox& box, const CBox& workArea) {
    // Minimal-overlap placement: try positions aligned with the work area and other
    // windows' edges, pick the one overlapping least. Ties go to the top-most, then left-most.
    const auto OTHERS = otherWindowBoxes(target);

    if (OTHERS.empty())
        return placeCentered(box, workArea);

    std::vector<double> xs = {workArea.x, workArea.x + workArea.w - box.w};
    std::vector<double> ys = {workArea.y, workArea.y + workArea.h - box.h};

    for (const auto& o : OTHERS) {
        xs.emplace_back(o.x + o.w);
        xs.emplace_back(o.x - box.w);
        ys.emplace_back(o.y + o.h);
        ys.emplace_back(o.y - box.h);
    }

    const auto inRange = [](double v, double lo, double hi) { return v >= lo - 0.5 && v <= hi + 0.5; };

    double     bestOverlap = std::numeric_limits<double>::max();
    CBox       best        = placeCentered(box, workArea);

    for (const auto y : ys) {
        if (!inRange(y, workArea.y, workArea.y + workArea.h - box.h))
            continue;

        for (const auto x : xs) {
            if (!inRange(x, workArea.x, workArea.x + workArea.w - box.w))
                continue;

            const CBox CANDIDATE = {x, y, box.w, box.h};
            double     overlap   = 0;
            for (const auto& o : OTHERS) {
                const auto INTERSECTION = CANDIDATE.intersection(o);
                overlap += INTERSECTION.w * INTERSECTION.h;
            }

            const bool BETTER = overlap < bestOverlap - 0.5 || (std::abs(overlap - bestOverlap) <= 0.5 && (y < best.y - 0.5 || (std::abs(y - best.y) <= 0.5 && x < best.x)));

            if (!BETTER)
                continue;

            bestOverlap = overlap;
            best        = CANDIDATE;
        }
    }

    // nothing free: fall back to cascading so the new window is at least distinguishable
    if (bestOverlap > 0.5)
        return placeCascade(target, box, workArea);

    return best;
}

CBox CStackingFloatingAlgorithm::clampIntoWorkArea(CBox box, const CBox& workArea) {
    box.x = std::clamp(box.x, workArea.x, std::max(workArea.x, workArea.x + workArea.w - box.w));
    box.y = std::clamp(box.y, workArea.y, std::max(workArea.y, workArea.y + workArea.h - box.h));
    return box;
}
