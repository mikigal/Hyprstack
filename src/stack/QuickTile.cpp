#include "QuickTile.hpp"
#include "StackMode.hpp"
#include "WindowStack.hpp"

#include "../config/ConfigValue.hpp"
#include "../config/shared/actions/ConfigActions.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../event/EventBus.hpp"
#include "../layout/LayoutManager.hpp"
#include "../layout/space/Space.hpp"
#include "../layout/supplementary/DragController.hpp"
#include "../layout/target/Target.hpp"
#include "../output/Monitor.hpp"
#include "../render/Renderer.hpp"
#include "../render/types.hpp"
#include "../render/pass/BorderPassElement.hpp"
#include "../render/pass/RectPassElement.hpp"
#include "../state/MonitorState.hpp"
#include "../workspace/HLWorkspace.hpp"

using namespace Stack;

UP<CQuickTileController>& Stack::quickTile() {
    static UP<CQuickTileController> p = makeUnique<CQuickTileController>();
    return p;
}

CQuickTileController::CQuickTileController() {
    m_listeners.dragEnded = g_layoutManager->dragController()->m_events.ended.listen([this] { onDragEnded(); });

    m_listeners.render = Event::bus()->m_events.render.stage.listen([this](const Event::SRenderStageEvent& ev) {
        if (!m_target || ev.monitor != m_target->monitor || !ev.context)
            return;

        // Drawn right before the dragged window: above everything else, below the window itself.
        // If the dragged window isn't on this monitor, after all windows.
        switch (ev.stage) {
            case RENDER_BEGIN: m_previewDrawn = false; return;
            case RENDER_PRE_WINDOW:
                if (m_previewDrawn || !m_dragTarget || ev.context->get().m_data.currentWindow.lock() != m_dragTarget->window())
                    return;
                break;
            case RENDER_POST_WINDOWS:
                if (m_previewDrawn)
                    return;
                break;
            default: return;
        }

        m_previewDrawn = true;
        renderPreview(ev.context->get(), ev.monitor);
    });
}

void CQuickTileController::renderPreview(Render::CRenderContext& ctx, PHLMONITOR monitor) {
    static auto                 PCOLOR   = CConfigValue<Config::INTEGER>("stacking:quick_tile:col.preview");
    static auto                 PBORDER  = CConfigValue<Config::INTEGER>("stacking:quick_tile:col.preview_border");
    static auto                 PROUND   = CConfigValue<Config::INTEGER>("decoration:rounding");
    static auto                 PROUNDPW = CConfigValue<Config::FLOAT>("decoration:rounding_power");

    const CBox                  BOX = previewBox(*m_target).translate(-monitor->m_position).scale(monitor->m_scale).round();

    CRectPassElement::SRectData rect;
    rect.box           = BOX;
    rect.color         = CHyprColor{sc<uint64_t>(*PCOLOR)};
    rect.round         = std::round(*PROUND * monitor->m_scale);
    rect.roundingPower = *PROUNDPW;
    rect.blur          = true;
    g_pHyprRenderer->addPassElement(ctx, makeUnique<CRectPassElement>(rect));

    CBorderPassElement::SBorderData border;
    border.box           = BOX;
    border.grad1         = Config::CGradientValueData{CHyprColor{sc<uint64_t>(*PBORDER)}};
    border.borderSize    = std::max(1, sc<int>(std::round(2 * monitor->m_scale)));
    border.round         = rect.round;
    border.roundingPower = *PROUNDPW;
    g_pHyprRenderer->addPassElement(ctx, makeUnique<CBorderPassElement>(border));
}

static bool insideAnyMonitor(const Vector2D& pos) {
    for (const auto& m : State::monitorState()->monitors()) {
        if (m->m_enabled && m->logicalBox().containsPoint(pos))
            return true;
    }
    return false;
}

std::optional<SQuickTileTarget> CQuickTileController::targetAt(const Vector2D& pos) const {
    static auto PENABLED   = CConfigValue<Config::INTEGER>("stacking:quick_tile:enabled");
    static auto PTHRESHOLD = CConfigValue<Config::INTEGER>("stacking:quick_tile:edge_threshold");
    static auto PCORNER    = CConfigValue<Config::INTEGER>("stacking:quick_tile:corner_size");
    static auto PTOP       = CConfigValue<Config::STRING>("stacking:quick_tile:top_action");

    if (!*PENABLED)
        return std::nullopt;

    PHLMONITOR monitor;
    for (const auto& m : State::monitorState()->monitors()) {
        if (m->m_enabled && m->logicalBox().containsPoint(pos)) {
            monitor = m;
            break;
        }
    }

    if (!monitor)
        return std::nullopt;

    const auto   BOX  = monitor->logicalBox();
    const double T    = *PTHRESHOLD;
    const double C    = *PCORNER;
    const double AWAY = T + 2; // probe this far past an edge for a neighboring monitor

    // only outer edges: an edge shared with another monitor is for moving windows across
    const bool       LEFT   = pos.x <= BOX.x + T && !insideAnyMonitor({BOX.x - AWAY, pos.y});
    const bool       RIGHT  = pos.x >= BOX.x + BOX.w - 1 - T && !insideAnyMonitor({BOX.x + BOX.w + AWAY, pos.y});
    const bool       TOP    = pos.y <= BOX.y + T && !insideAnyMonitor({pos.x, BOX.y - AWAY});
    const bool       BOTTOM = pos.y >= BOX.y + BOX.h - 1 - T && !insideAnyMonitor({pos.x, BOX.y + BOX.h + AWAY});

    const bool       NEAR_TOP    = pos.y <= BOX.y + C;
    const bool       NEAR_BOTTOM = pos.y >= BOX.y + BOX.h - C;
    const bool       NEAR_LEFT   = pos.x <= BOX.x + C;
    const bool       NEAR_RIGHT  = pos.x >= BOX.x + BOX.w - C;

    SQuickTileTarget target{.monitor = monitor};

    if (LEFT || RIGHT) {
        target.edges = LEFT ? TILE_LEFT : TILE_RIGHT;
        if (NEAR_TOP)
            target.edges |= TILE_TOP;
        else if (NEAR_BOTTOM)
            target.edges |= TILE_BOTTOM;
        return target;
    }

    if (TOP || BOTTOM) {
        if (NEAR_LEFT || NEAR_RIGHT) {
            target.edges = (TOP ? TILE_TOP : TILE_BOTTOM) | (NEAR_LEFT ? TILE_LEFT : TILE_RIGHT);
            return target;
        }

        if (BOTTOM)
            return std::nullopt;

        if (*PTOP == "maximize") {
            target.maximize = true;
            return target;
        }

        if (*PTOP == "tile") {
            target.edges = TILE_TOP;
            return target;
        }
    }

    return std::nullopt;
}

std::optional<SQuickTileTarget> CQuickTileController::currentTarget() const {
    return m_target;
}

CBox CQuickTileController::previewBox(const SQuickTileTarget& target) const {
    if (!target.monitor || !target.monitor->m_activeWorkspace || !target.monitor->m_activeWorkspace->space())
        return {};

    const auto WORK_AREA = target.monitor->m_activeWorkspace->space()->workArea(true);
    return target.maximize ? WORK_AREA : CWindowStack::tiledFrame(target.edges, WORK_AREA);
}

void CQuickTileController::damagePreview() {
    if (m_target && m_target->monitor)
        g_pHyprRenderer->damageBox(previewBox(*m_target).expand(4));
}

void CQuickTileController::setPreview(std::optional<SQuickTileTarget> target) {
    if (target == m_target)
        return;

    damagePreview();
    m_target = target;
    damagePreview();
}

void CQuickTileController::onDragMotion(SP<Layout::ITarget> target, const Vector2D& mousePos) {
    if (!target || !target->window() || !windowIsStacking(target->window())) {
        setPreview(std::nullopt);
        return;
    }

    m_dragTarget = target;
    setPreview(targetAt(mousePos));
}

// move a window onto a monitor's active workspace, if it isn't there already
static void ensureOnMonitor(PHLWINDOW w, PHLMONITOR monitor) {
    if (!monitor || !monitor->m_activeWorkspace || w->m_workspace == monitor->m_activeWorkspace)
        return;

    (void)Config::Actions::moveToWorkspace(monitor->m_activeWorkspace, true, w);
}

void CQuickTileController::onDragEnded() {
    const auto TARGET  = m_target;
    const auto DRAGGED = m_dragTarget.lock();

    setPreview(std::nullopt);
    m_dragTarget.reset();

    if (!TARGET || !DRAGGED || !DRAGGED->window())
        return;

    const auto W = DRAGGED->window();

    if (TARGET->maximize) {
        ensureOnMonitor(W, TARGET->monitor);
        windowStack()->setMaximized(W, MAXIMIZE_FULL);
    } else
        windowStack()->setTiled(W, TARGET->edges, TARGET->monitor);
}

void CQuickTileController::snap(PHLWINDOW w, Math::eDirection dir) {
    if (!w || !windowIsStacking(w))
        return;

    const auto T   = windowStack()->tiled(w);
    const bool MAX = windowStack()->maximized(w) == MAXIMIZE_FULL;
    const bool H   = T == TILE_LEFT || T == TILE_RIGHT; // a plain half

    switch (dir) {
        case Math::DIRECTION_LEFT:
        case Math::DIRECTION_RIGHT: {
            const uint8_t SAME     = dir == Math::DIRECTION_LEFT ? TILE_LEFT : TILE_RIGHT;
            const uint8_t OPPOSITE = dir == Math::DIRECTION_LEFT ? TILE_RIGHT : TILE_LEFT;

            if (T & OPPOSITE) {
                // snapped the other way: back to normal
                windowStack()->setTiled(w, TILE_NONE);
            } else if (T == SAME) {
                // already there: continue onto the next monitor, like on Windows
                const auto NEXT = State::monitorState()->query().inDirection(dir).relativeTo(w->m_monitor.lock()).run();
                if (NEXT && NEXT != w->m_monitor.lock())
                    windowStack()->setTiled(w, OPPOSITE, NEXT);
            } else
                windowStack()->setTiled(w, SAME);
            break;
        }
        case Math::DIRECTION_UP:
            if (MAX)
                break;
            if (H)
                windowStack()->setTiled(w, T | TILE_TOP);
            else if (T & TILE_BOTTOM)
                windowStack()->setTiled(w, T & ~TILE_BOTTOM);
            else
                windowStack()->setMaximized(w, MAXIMIZE_FULL);
            break;
        case Math::DIRECTION_DOWN:
            if (MAX)
                windowStack()->setMaximized(w, MAXIMIZE_NONE);
            else if (H)
                windowStack()->setTiled(w, T | TILE_BOTTOM);
            else if (T & TILE_TOP && T != TILE_TOP)
                windowStack()->setTiled(w, T & ~TILE_TOP);
            else if (T != TILE_NONE)
                windowStack()->setTiled(w, TILE_NONE);
            else
                windowStack()->setMinimized(w, true);
            break;
        default: break;
    }
}
