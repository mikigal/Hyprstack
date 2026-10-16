#include "WindowStack.hpp"
#include "StackMode.hpp"

#include "../config/ConfigValue.hpp"
#include "../desktop/history/WindowHistoryTracker.hpp"
#include "../desktop/state/FocusState.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../desktop/view/window/WindowPresentation.hpp"
#include "../event/EventBus.hpp"
#include "../ipc/s2/S2.hpp"
#include "../layout/LayoutManager.hpp"
#include "../layout/space/Space.hpp"
#include "../layout/target/Target.hpp"
#include "../managers/fullscreen/FullscreenController.hpp"
#include "../render/Renderer.hpp"
#include "../workspace/HLWorkspace.hpp"

#include <algorithm>
#include <format>
#include <ranges>

using namespace Stack;

UP<CWindowStack>& Stack::windowStack() {
    static UP<CWindowStack> p = makeUnique<CWindowStack>();
    return p;
}

CWindowStack::CWindowStack() {
    m_listeners.windowDestroy = Event::bus()->m_events.window.destroy.listen([this](PHLWINDOWREF) { purgeExpired(); });

    m_listeners.windowActive = Event::bus()->m_events.window.active.listen([](PHLWINDOW w, Desktop::eFocusReason reason) {
        static auto PRAISE = CConfigValue<Config::INTEGER>("stacking:raise_on_focus");

        // click-to-focus raises on its own (InputManager), hover focus must not raise
        if (!w || !*PRAISE || reason == Desktop::FOCUS_REASON_FFM || !windowIsStacking(w))
            return;

        Desktop::windowState()->raise(w);
    });
}

SWindowStackData& CWindowStack::data(PHLWINDOW w) {
    return m_data[w];
}

const SWindowStackData* CWindowStack::find(PHLWINDOW w) const {
    const auto IT = m_data.find(w);
    return IT == m_data.end() ? nullptr : &IT->second;
}

void CWindowStack::purgeExpired() {
    std::erase_if(m_data, [](const auto& e) { return e.first.expired(); });
}

//
// maximize
//

uint8_t CWindowStack::maximized(PHLWINDOW w) const {
    const auto DATA = find(w);
    return DATA ? DATA->maximized : MAXIMIZE_NONE;
}

CBox CWindowStack::maximizedBox(PHLWINDOW w, uint8_t mode) {
    const auto TARGET = w->layoutTarget();
    const auto SPACE  = TARGET ? TARGET->space() : nullptr;
    if (!SPACE)
        return TARGET ? TARGET->position() : CBox{};

    const auto EXTENTS   = w->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS);
    const auto WORK_AREA = SPACE->workArea(true);
    const CBox FULL      = WORK_AREA.copy().addExtents(SBoxExtents{.topLeft = -EXTENTS.topLeft, .bottomRight = -EXTENTS.bottomRight});

    // partial maximize keeps the other axis from the restore geometry
    const auto& DATA = data(w);
    CBox        box  = DATA.restoreBox.value_or(TARGET->position());

    if (mode & MAXIMIZE_HORIZONTAL) {
        box.x = FULL.x;
        box.w = FULL.w;
    }

    if (mode & MAXIMIZE_VERTICAL) {
        box.y = FULL.y;
        box.h = FULL.h;
    }

    return box;
}

void CWindowStack::setMaximized(PHLWINDOW w, uint8_t mode) {
    if (!w || !w->layoutTarget() || !windowIsStacking(w))
        return;

    auto& DATA = data(w);
    if (DATA.maximized == mode)
        return;

    const auto TARGET = w->layoutTarget();

    if (DATA.maximized == MAXIMIZE_NONE)
        DATA.restoreBox = TARGET->position();

    DATA.maximized = mode;

    if (mode == MAXIMIZE_NONE) {
        if (DATA.restoreBox)
            g_layoutManager->setTargetGeom(*DATA.restoreBox, TARGET);
        DATA.restoreBox.reset();
    } else
        g_layoutManager->setTargetGeom(maximizedBox(w, mode), TARGET);

    w->backend().setMaximized(mode == MAXIMIZE_FULL);
    w->presentation().updateDecorations();

    postMaximizeState(w);
}

void CWindowStack::toggleMaximized(PHLWINDOW w, uint8_t mode) {
    if (!w)
        return;

    setMaximized(w, maximized(w) == mode ? MAXIMIZE_NONE : mode);
}

void CWindowStack::refitMaximized(PHLWINDOW w) {
    const auto MODE = maximized(w);
    if (MODE == MAXIMIZE_NONE || !w->layoutTarget())
        return;

    const auto BOX = maximizedBox(w, MODE);
    if (BOX == w->layoutTarget()->position())
        return;

    g_layoutManager->setTargetGeom(BOX, w->layoutTarget());
}

void CWindowStack::dropMaximized(PHLWINDOW w) {
    const auto IT = m_data.find(w);
    if (IT == m_data.end() || IT->second.maximized == MAXIMIZE_NONE)
        return;

    IT->second.maximized = MAXIMIZE_NONE;
    IT->second.restoreBox.reset();

    w->backend().setMaximized(false);
    w->presentation().updateDecorations();

    postMaximizeState(w);
}

std::optional<Vector2D> CWindowStack::restoreForDrag(PHLWINDOW w, const Vector2D& hotspot) {
    if (!w || !w->layoutTarget() || maximized(w) == MAXIMIZE_NONE)
        return std::nullopt;

    const auto CURRENT = w->layoutTarget()->position();
    const auto RESTORE = data(w).restoreBox.value_or(CURRENT);

    dropMaximized(w);

    const Vector2D NEW_HOTSPOT = {CURRENT.w > 0 ? hotspot.x / CURRENT.w * RESTORE.w : hotspot.x, hotspot.y};
    w->layoutTarget()->setPositionGlobal(CBox{CURRENT.pos() + hotspot - NEW_HOTSPOT, RESTORE.size()});

    return NEW_HOTSPOT;
}

void CWindowStack::postMaximizeState(PHLWINDOW w) {
    IPC::Socket2::sock()->postEvent({.event = "maximized", .data = std::format("{:x},{}", rc<uintptr_t>(w.get()), maximized(w))});

    // foreign-toplevel & friends refresh their state on this event
    Event::bus()->m_events.window.fullscreen.emit(w);
}

bool CWindowStack::interceptFullscreenRequest(PHLWINDOW w, std::optional<uint8_t> internal, std::optional<uint8_t> client) {
    if (!w || !windowIsStacking(w))
        return false;

    // the internal mode decides, the client mode only matters when internal is unspecified
    const auto REQUESTED = internal.has_value() ? internal : client;
    if (!REQUESTED.has_value())
        return false;

    if (*REQUESTED == Fullscreen::FSMODE_MAXIMIZED) {
        if (Fullscreen::controller()->isFullscreen(w))
            Fullscreen::controller()->setFullscreenMode(w, Fullscreen::FSMODE_NONE, Fullscreen::FSMODE_NONE);

        setMaximized(w, MAXIMIZE_FULL);
        return true;
    }

    // leaving "maximized": only ours to handle if the window isn't really fullscreen
    if (*REQUESTED == Fullscreen::FSMODE_NONE && maximized(w) != MAXIMIZE_NONE && !Fullscreen::controller()->isFullscreen(w)) {
        setMaximized(w, MAXIMIZE_NONE);
        return true;
    }

    return false;
}

//
// minimize
//

bool CWindowStack::minimized(PHLWINDOW w) const {
    return w && w->isMinimized();
}

void CWindowStack::setMinimized(PHLWINDOW w, bool minimize) {
    if (!w || !w->mapped() || !windowIsStacking(w) || w->isMinimized() == minimize)
        return;

    const bool WAS_FOCUSED = Desktop::focusState()->window() == w;

    w->setMinimized(minimize);
    w->backend().setMinimized(minimize);
    g_pHyprRenderer->damageWindow(w);

    if (minimize) {
        if (WAS_FOCUSED)
            focusNextAfter(w);
    } else {
        Desktop::windowState()->raise(w);
        Desktop::focusState()->fullWindowFocus(w, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
    }

    IPC::Socket2::sock()->postEvent({.event = "minimized", .data = std::format("{:x},{}", rc<uintptr_t>(w.get()), minimize ? 1 : 0)});
    Event::bus()->m_events.window.minimize.emit(w, minimize);
}

void CWindowStack::focusNextAfter(PHLWINDOW w) {
    // most recently used visible window, preferring the same workspace
    PHLWINDOW fallback;

    for (const auto& ref : Desktop::History::windowTracker()->fullHistory() | std::views::reverse) {
        const auto CANDIDATE = ref.lock();
        if (!CANDIDATE || CANDIDATE == w || !CANDIDATE->mapped() || CANDIDATE->isHidden() || !CANDIDATE->m_workspace || !CANDIDATE->m_workspace->visible())
            continue;

        if (CANDIDATE->m_workspace == w->m_workspace) {
            Desktop::focusState()->fullWindowFocus(CANDIDATE, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
            return;
        }

        if (!fallback)
            fallback = CANDIDATE;
    }

    if (fallback)
        Desktop::focusState()->fullWindowFocus(fallback, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
    else
        Desktop::focusState()->rawWindowFocus(nullptr, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
}

void CWindowStack::requestMinimize(PHLWINDOW w, bool minimize) {
    if (!w || !w->mapped())
        return;

    if (windowIsStacking(w)) {
        setMinimized(w, minimize);
        return;
    }

    // upstream behavior: let a shell / plugin decide
    IPC::Socket2::sock()->postEvent({.event = "minimized", .data = std::format("{:x},{}", rc<uintptr_t>(w.get()), minimize ? 1 : 0)});
    Event::bus()->m_events.window.minimize.emit(w, minimize);
}

//
// layers & z-order
//

eStackLayer CWindowStack::layer(PHLWINDOW w) const {
    const auto DATA = find(w);
    return DATA ? DATA->layer : LAYER_NORMAL;
}

void CWindowStack::setLayer(PHLWINDOW w, eStackLayer layer) {
    if (!w)
        return;

    data(w).layer = layer;

    // restack around the new layer
    if (layer == LAYER_BELOW)
        Desktop::windowState()->lower(w);
    else
        Desktop::windowState()->raise(w);

    IPC::Socket2::sock()->postEvent({.event = "stacklayer", .data = std::format("{:x},{}", rc<uintptr_t>(w.get()), sc<int>(layer))});
}

static PHLWINDOW waylandParent(const PHLWINDOW& w) {
    if (w->backend().isX11())
        return nullptr; // X11 transient stacks are handled by CWindowState itself
    return w->backend().parent();
}

bool CWindowStack::restack(std::vector<PHLWINDOW>& windows) {
    // effective layer: transients inherit their parent's layer if it is higher
    const auto effectiveLayer = [this](PHLWINDOW w) {
        uint8_t result = layer(w);
        for (auto p = waylandParent(w); p; p = waylandParent(p))
            result = std::max<uint8_t>(result, layer(p));
        return result;
    };

    bool       changed = false;

    const auto BEFORE = windows;
    std::ranges::stable_sort(windows, [&](const PHLWINDOW& a, const PHLWINDOW& b) {
        // non-stacking windows keep their relative order (their layer is always normal)
        return effectiveLayer(a) < effectiveLayer(b);
    });

    // transients go right above their parent if they ended up below it
    for (size_t pass = 0; pass < windows.size(); ++pass) {
        bool moved = false;

        for (size_t i = 0; i < windows.size(); ++i) {
            const auto W = windows[i];
            if (!windowIsStacking(W))
                continue;

            const auto PARENT = waylandParent(W);
            if (!PARENT)
                continue;

            const auto PARENT_IT = std::ranges::find(windows, PARENT);
            if (PARENT_IT == windows.end() || std::distance(windows.begin(), PARENT_IT) < sc<ptrdiff_t>(i))
                continue;

            windows.erase(windows.begin() + i);
            const auto NEW_PARENT_IT = std::ranges::find(windows, PARENT);
            windows.insert(NEW_PARENT_IT + 1, W);
            moved = true;
            break;
        }

        if (!moved)
            break;
    }

    changed = BEFORE != windows;

    return changed;
}
