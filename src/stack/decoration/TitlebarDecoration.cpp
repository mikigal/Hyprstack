#include "TitlebarDecoration.hpp"
#include "DecorationPolicy.hpp"

#include "../StackMode.hpp"
#include "../WindowStack.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/state/FocusState.hpp"
#include "../../desktop/state/WindowState.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../event/EventBus.hpp"
#include "../../helpers/varlist/VarList.hpp"
#include "../../ipc/s2/S2.hpp"
#include "../../layout/LayoutManager.hpp"
#include "../../layout/target/Target.hpp"
#include "../../managers/fullscreen/FullscreenController.hpp"
#include "../../managers/input/InputManager.hpp"
#include "../../output/Monitor.hpp"
#include "../../protocols/ServerDecorationKDE.hpp"
#include "../../protocols/XDGDecoration.hpp"
#include "../../render/Renderer.hpp"
#include "../../render/WindowRenderPresentation.hpp"
#include "../../render/pass/RectPassElement.hpp"
#include "../../render/pass/TexPassElement.hpp"

#include <cairo/cairo.h>
#include <linux/input-event-codes.h>
#include <chrono>
#include <cmath>
#include <format>
#include <unordered_map>

using namespace Stack;

constexpr int    DRAG_THRESHOLD   = 4;
constexpr auto   DOUBLE_CLICK_MAX = std::chrono::milliseconds(400);
constexpr double ICON_SIZE        = 10.0;

bool             Stack::wantsTitlebar(PHLWINDOW w) {
    static auto PENABLED = CConfigValue<Config::INTEGER>("stacking:titlebar:enabled");

    if (!*PENABLED || !w || !windowIsStacking(w))
        return false;

    if (!w->m_ruleApplicator->decorate().valueOrDefault() || Fullscreen::controller()->isFullscreen(w))
        return false;

    const auto TRAITS = w->backend().traits();
    if (TRAITS.overrideRedirect)
        return false;

    // X11: decorate unless the client asked not to (motif hints)
    if (w->backend().isX11())
        return !TRAITS.suggestsNoBorder;

    // Wayland: only clients that negotiate server-side decorations. Everything else (GTK & co.)
    // draws its own decorations.
    return negotiatedServerSide(w);
}

static CHyprColor configColor(const char* name) {
    return CHyprColor{sc<uint64_t>(*CConfigValue<Config::INTEGER>(name))};
}

// Button glyphs, drawn with cairo and cached per glyph / color / pixel size.
static SP<Render::ITexture> iconTexture(eTitlebarButton button, bool restore, const CHyprColor& color, int px) {
    static std::unordered_map<std::string, SP<Render::ITexture>> cache;

    const auto                                                   KEY = std::format("{}-{}-{:x}-{}", sc<int>(button), restore, color.getAsHex(), px);
    if (const auto IT = cache.find(KEY); IT != cache.end())
        return IT->second;

    const auto SURFACE = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, px, px);
    const auto CAIRO   = cairo_create(SURFACE);

    cairo_set_operator(CAIRO, CAIRO_OPERATOR_CLEAR);
    cairo_paint(CAIRO);
    cairo_set_operator(CAIRO, CAIRO_OPERATOR_OVER);

    const double LINE = std::max(1.0, std::round(px / ICON_SIZE));
    const double HALF = LINE / 2.0;
    cairo_set_source_rgba(CAIRO, color.r, color.g, color.b, color.a);
    cairo_set_line_width(CAIRO, LINE);

    switch (button) {
        case TITLEBAR_BUTTON_MINIMIZE:
            cairo_move_to(CAIRO, 0, std::round(px / 2.0) + HALF);
            cairo_line_to(CAIRO, px, std::round(px / 2.0) + HALF);
            break;
        case TITLEBAR_BUTTON_MAXIMIZE:
            if (restore) {
                // two overlapping frames
                const double OFF = std::round(px * 0.2);
                cairo_rectangle(CAIRO, HALF, OFF + HALF, px - OFF - LINE, px - OFF - LINE);
                cairo_move_to(CAIRO, OFF + HALF, OFF);
                cairo_line_to(CAIRO, OFF + HALF, HALF);
                cairo_line_to(CAIRO, px - HALF, HALF);
                cairo_line_to(CAIRO, px - HALF, px - OFF - HALF);
                cairo_line_to(CAIRO, px - OFF, px - OFF - HALF);
            } else
                cairo_rectangle(CAIRO, HALF, HALF, px - LINE, px - LINE);
            break;
        case TITLEBAR_BUTTON_CLOSE:
            cairo_set_line_cap(CAIRO, CAIRO_LINE_CAP_SQUARE);
            cairo_move_to(CAIRO, HALF, HALF);
            cairo_line_to(CAIRO, px - HALF, px - HALF);
            cairo_move_to(CAIRO, px - HALF, HALF);
            cairo_line_to(CAIRO, HALF, px - HALF);
            break;
    }

    cairo_stroke(CAIRO);
    cairo_surface_flush(SURFACE);

    auto tex = g_pHyprRenderer->createTexture(SURFACE);

    cairo_destroy(CAIRO);
    cairo_surface_destroy(SURFACE);

    cache[KEY] = tex;
    return tex;
}

CTitlebarDecoration::CTitlebarDecoration(PHLWINDOW pWindow) : IHyprWindowDecoration(pWindow), m_window(pWindow) {
    m_listeners.mouseMove   = Event::bus()->m_events.input.mouse.move.listen([this](Vector2D pos, Event::SCallbackInfo&) { onMouseMove(pos); });
    m_listeners.mouseButton = Event::bus()->m_events.input.mouse.button.listen([this](IPointer::SButtonEvent e, Event::SCallbackInfo&) { onGlobalButton(e); });
    m_listeners.fullscreen  = Event::bus()->m_events.window.fullscreen.listen([this](PHLWINDOW w) {
        if (w == m_window.lock())
            syncVisibility();
    });
}

bool CTitlebarDecoration::visible() const {
    return wantsTitlebar(m_window.lock());
}

void CTitlebarDecoration::syncVisibility() {
    const bool VISIBLE = visible();
    if (m_lastVisible == VISIBLE)
        return;

    // a titlebar that never showed up has nothing to reposition (keeps non-stacking windows untouched)
    const bool FIRST_SYNC = !m_lastVisible.has_value();
    m_lastVisible         = VISIBLE;
    if (FIRST_SYNC && !VISIBLE)
        return;
    g_pDecorationPositioner->repositionDeco(this);

    if (const auto W = m_window.lock(); W && W->layoutTarget())
        W->layoutTarget()->recalc();
}

SDecorationPositioningInfo CTitlebarDecoration::getPositioningInfo() {
    static auto                PHEIGHT = CConfigValue<Config::INTEGER>("stacking:titlebar:height");

    SDecorationPositioningInfo info;
    info.policy = DECORATION_POSITION_STICKY;
    info.edges  = DECORATION_EDGE_TOP;
    // above the border's priority: the border wraps both the titlebar and the window
    info.priority       = 10005;
    info.reserved       = true;
    info.desiredExtents = {.topLeft = Vector2D{0.0, visible() ? sc<double>(*PHEIGHT) : 0.0}, .bottomRight = Vector2D{}};
    return info;
}

void CTitlebarDecoration::onPositioningReply(const SDecorationPositioningReply& reply) {
    m_assignedBox = reply.assignedGeometry;
}

eDecorationType CTitlebarDecoration::getDecorationType() {
    return DECORATION_CUSTOM;
}

eDecorationLayer CTitlebarDecoration::getDecorationLayer() {
    return DECORATION_LAYER_UNDER;
}

uint64_t CTitlebarDecoration::getDecorationFlags() {
    return DECORATION_ALLOWS_MOUSE_INPUT | DECORATION_PART_OF_MAIN_WINDOW;
}

std::string CTitlebarDecoration::getDisplayName() {
    return "Titlebar";
}

void CTitlebarDecoration::updateWindow(PHLWINDOW) {
    syncVisibility();
    damageEntire();
}

CBox CTitlebarDecoration::assignedBoxGlobal() const {
    const auto W = m_window.lock();
    if (!W)
        return {};
    return assignedBoxGlobal(W->presentation().renderPresentation());
}

CBox CTitlebarDecoration::assignedBoxGlobal(const Render::SWindowRenderPresentation& presentation) const {
    CBox box = m_assignedBox;
    box.translate(g_pDecorationPositioner->getEdgeDefinedPoint(DECORATION_EDGE_TOP, m_window));
    box.translate(presentation.workspaceOffset);
    return box.round();
}

void CTitlebarDecoration::damageEntire() {
    const auto W = m_window.lock();
    if (!W)
        return;

    g_pHyprRenderer->damageBox(assignedBoxGlobal().translate(W->presentation().floatingOffset()));
}

std::vector<eTitlebarButton> CTitlebarDecoration::buttons() const {
    static auto                         PBUTTONS = CConfigValue<Config::STRING>("stacking:titlebar:buttons");

    static std::string                  lastConfig;
    static std::vector<eTitlebarButton> parsed;

    if (*PBUTTONS != lastConfig) {
        lastConfig = *PBUTTONS;
        parsed.clear();

        CVarList list(lastConfig, 0, ',', true);
        for (const auto& b : list) {
            if (b == "minimize")
                parsed.emplace_back(TITLEBAR_BUTTON_MINIMIZE);
            else if (b == "maximize")
                parsed.emplace_back(TITLEBAR_BUTTON_MAXIMIZE);
            else if (b == "close")
                parsed.emplace_back(TITLEBAR_BUTTON_CLOSE);
        }
    }

    const auto W = m_window.lock();
    if (!W)
        return parsed;

    // like KDE / Windows: dialogs only get a close button, fixed-size windows can't be maximized
    const bool TRANSIENT = W->backend().parent() != nullptr;
    const auto HINTS     = W->backend().geometryHints(Desktop::View::eBackendState::BACKEND_STATE_CURRENT);
    const bool FIXED     = HINTS.minSize && HINTS.maxSize && HINTS.maxSize->x > 0 && *HINTS.minSize == *HINTS.maxSize;

    if (!TRANSIENT && !FIXED)
        return parsed;

    std::vector<eTitlebarButton> filtered;
    for (const auto B : parsed) {
        if (B == TITLEBAR_BUTTON_CLOSE || (!TRANSIENT && B == TITLEBAR_BUTTON_MINIMIZE))
            filtered.emplace_back(B);
    }
    return filtered;
}

CBox CTitlebarDecoration::buttonBoxGlobal(size_t index) const {
    static auto PBUTTONWIDTH = CConfigValue<Config::INTEGER>("stacking:titlebar:button_width");

    const auto  BAR   = assignedBoxGlobal();
    const auto  COUNT = buttons().size();
    const auto  W     = sc<double>(*PBUTTONWIDTH);

    // buttons are right-aligned, in config order
    return CBox{BAR.x + BAR.w - W * sc<double>(COUNT - index), BAR.y, W, BAR.h};
}

std::optional<eTitlebarButton> CTitlebarDecoration::buttonAt(const Vector2D& pos) const {
    const auto BUTTONS = buttons();
    for (size_t i = 0; i < BUTTONS.size(); ++i) {
        if (buttonBoxGlobal(i).containsPoint(pos))
            return BUTTONS[i];
    }
    return std::nullopt;
}

//
// input
//

bool CTitlebarDecoration::onInputOnDeco(const eInputType type, const Vector2D& pos, std::any data) {
    if (!visible())
        return false;

    switch (type) {
        case INPUT_TYPE_BUTTON: {
            const auto* E = std::any_cast<IPointer::SButtonEvent>(&data);
            if (!E)
                return false;
            onButton(*E, pos);
            return true;
        }
        // let window dragging (e.g. Super + drag) start from the titlebar
        case INPUT_TYPE_DRAG_START: return false;
        case INPUT_TYPE_AXIS: return true;
        default: return false;
    }
}

void CTitlebarDecoration::onButton(const IPointer::SButtonEvent& e, const Vector2D& pos) {
    const auto W = m_window.lock();
    if (!W || e.state != WL_POINTER_BUTTON_STATE_PRESSED)
        return; // releases are handled in onGlobalButton, they may happen anywhere

    if (W->hasPopupAt(pos))
        return;

    // any click focuses and raises
    if (Desktop::focusState()->window() != W)
        Desktop::focusState()->fullWindowFocus(W, Desktop::FOCUS_REASON_CLICK);
    Desktop::windowState()->raise(W);

    switch (e.button) {
        case BTN_LEFT: break;
        case BTN_MIDDLE: Desktop::windowState()->lower(W); return;
        case BTN_RIGHT:
            // window menu is the shell's job
            IPC::Socket2::sock()->postEvent({.event = "titlebarmenu", .data = std::format("{:x},{},{}", rc<uintptr_t>(W.get()), sc<int>(pos.x), sc<int>(pos.y))});
            return;
        default: return;
    }

    if (const auto BUTTON = buttonAt(pos); BUTTON) {
        m_pressedButton = BUTTON;
        damageEntire();
        return;
    }

    static auto PDOUBLECLICK = CConfigValue<Config::STRING>("stacking:titlebar:double_click");

    const auto  NOW = Time::steadyNow();
    if (NOW - m_lastClick < DOUBLE_CLICK_MAX && pos.distance(m_pressPos) < DRAG_THRESHOLD) {
        m_lastClick = {};
        if (*PDOUBLECLICK == "maximize")
            windowStack()->toggleMaximized(W);
        else if (*PDOUBLECLICK == "minimize")
            windowStack()->setMinimized(W, true);
        return;
    }

    m_lastClick = NOW;
    m_pressPos  = pos;

    // start moving right away: the drag controller has its own threshold, and starting later
    // would lose the motion before it
    if (W->layoutTarget() && !g_layoutManager->dragController()->target()) {
        g_layoutManager->beginDragTarget(W->layoutTarget(), MBIND_MOVE);
        m_dragging = true;
    }
}

void CTitlebarDecoration::onGlobalButton(const IPointer::SButtonEvent& e) {
    if (e.state != WL_POINTER_BUTTON_STATE_RELEASED || e.button != BTN_LEFT)
        return;

    if (m_pressedButton) {
        const auto PRESSED = *m_pressedButton;
        const bool INSIDE  = buttonAt(g_pInputManager->getMouseCoordsInternal()) == PRESSED;
        m_pressedButton.reset();
        damageEntire();

        // like on Windows: the action fires on release, if still over the same button
        if (INSIDE)
            triggerButton(PRESSED);
        return;
    }

    endInteraction();
}

void CTitlebarDecoration::endInteraction() {
    if (m_dragging)
        g_layoutManager->endDragTarget();

    m_dragging = false;
}

void CTitlebarDecoration::onMouseMove(const Vector2D& pos) {
    if (!visible())
        return;

    const auto HOVERED = assignedBoxGlobal().containsPoint(pos) ? buttonAt(pos) : std::nullopt;
    if (HOVERED != m_hoveredButton) {
        m_hoveredButton = HOVERED;
        damageEntire();
    }
}

void CTitlebarDecoration::triggerButton(eTitlebarButton button) {
    const auto W = m_window.lock();
    if (!W)
        return;

    switch (button) {
        case TITLEBAR_BUTTON_MINIMIZE: windowStack()->setMinimized(W, true); break;
        case TITLEBAR_BUTTON_MAXIMIZE: windowStack()->toggleMaximized(W); break;
        case TITLEBAR_BUTTON_CLOSE: W->sendClose(); break;
    }
}

//
// rendering
//

void CTitlebarDecoration::renderTitle(PHLMONITOR monitor, float maxWidth, bool active) {
    static auto PFONT     = CConfigValue<Config::STRING>("stacking:titlebar:font_family");
    static auto PFALLBACK = CConfigValue<Config::STRING>("misc:font_family");
    static auto PFONTSIZE = CConfigValue<Config::INTEGER>("stacking:titlebar:font_size");

    const auto  W     = m_window.lock();
    const auto  TITLE = W->metadata().title();
    const auto  SCALE = monitor->m_scale;

    if (m_titleTex && TITLE == m_titleTexContent && SCALE == m_titleTexScale && maxWidth == m_titleTexWidth && active == m_titleTexActive)
        return;

    m_titleTexContent = TITLE;
    m_titleTexScale   = SCALE;
    m_titleTexWidth   = maxWidth;
    m_titleTexActive  = active;

    const auto COLOR = configColor(active ? "stacking:titlebar:col.text_active" : "stacking:titlebar:col.text_inactive");
    const auto FONT  = (*PFONT).empty() ? *PFALLBACK : *PFONT;

    m_titleTex = maxWidth > 1 ? g_pHyprRenderer->renderText(TITLE, COLOR, std::round(*PFONTSIZE * SCALE), false, FONT, sc<int>(maxWidth)) : nullptr;
}

void CTitlebarDecoration::draw(Render::CRenderContext& ctx, PHLMONITOR monitor, float const& a, const Render::SWindowRenderPresentation& presentation) {
    syncVisibility();

    const auto W = m_window.lock();
    if (!W || !m_lastVisible.value_or(false))
        return;

    static auto PPADDING = CConfigValue<Config::INTEGER>("stacking:titlebar:padding");
    static auto PALIGN   = CConfigValue<Config::STRING>("stacking:titlebar:text_align");

    const bool  ACTIVE   = Desktop::focusState()->window() == W;
    const auto  SCALE    = monitor->m_scale;
    const auto  ROUNDING = W->presentation().rounding();
    const auto  BAR      = assignedBoxGlobal(presentation);

    if (BAR.w < 1 || BAR.h < 1)
        return;

    // global logical -> monitor-local buffer coordinates
    const auto toLocal = [&](CBox box) { return box.translate(-monitor->m_position + presentation.floatingOffset).scale(SCALE).round(); };

    // background. Extends under the window, so the window's rounded top corners show the bar
    // color instead of whatever is behind the window. Only the top corners are rounded.
    {
        CBox  BAR_LOCAL = toLocal(BAR);
        float R         = ROUNDING * SCALE;

        auto  color = configColor(ACTIVE ? "stacking:titlebar:col.active" : "stacking:titlebar:col.inactive");
        color.a *= a;

        CRectPassElement::SRectData data;
        data.color                 = color;
        data.box                   = CBox{BAR_LOCAL.x, BAR_LOCAL.y, BAR_LOCAL.w, BAR_LOCAL.h + R * 2};
        data.clipBox               = CBox{BAR_LOCAL.x, BAR_LOCAL.y, BAR_LOCAL.w, BAR_LOCAL.h + R};
        data.round                 = std::round(R);
        data.roundingPower         = W->presentation().roundingPower();
        data.workspacePresentation = presentation;
        g_pHyprRenderer->addPassElement(ctx, makeUnique<CRectPassElement>(data));
    }

    // buttons
    const auto BUTTONS       = buttons();
    double     buttonsWidth  = 0;
    const bool MAXIMIZED     = windowStack()->maximized(W) == MAXIMIZE_FULL;
    const auto ICON_COLOR    = configColor(ACTIVE ? "stacking:titlebar:col.text_active" : "stacking:titlebar:col.text_inactive");
    const auto ICON_COLOR_HI = configColor("stacking:titlebar:col.text_active");

    for (size_t i = 0; i < BUTTONS.size(); ++i) {
        const auto BUTTON = BUTTONS[i];
        const auto BOX    = buttonBoxGlobal(i).translate(presentation.workspaceOffset - W->presentation().renderPresentation().workspaceOffset);
        buttonsWidth += BOX.w;

        const bool HOVERED = m_hoveredButton == BUTTON;
        const bool PRESSED = m_pressedButton == BUTTON && HOVERED;

        if (HOVERED || PRESSED) {
            auto color = BUTTON == TITLEBAR_BUTTON_CLOSE ? configColor("stacking:titlebar:col.close_hover") :
                                                           configColor(PRESSED ? "stacking:titlebar:col.button_pressed" : "stacking:titlebar:col.button_hover");
            color.a *= a;

            const auto                  LOCAL   = toLocal(BOX);
            const bool                  AT_EDGE = i == BUTTONS.size() - 1;
            const float                 R       = ROUNDING * SCALE;

            CRectPassElement::SRectData data;
            data.color = color;
            // the right-most button follows the bar's rounded top-right corner
            data.box                   = AT_EDGE && R > 0 ? CBox{LOCAL.x - R, LOCAL.y, LOCAL.w + R, LOCAL.h + R} : LOCAL;
            data.clipBox               = LOCAL;
            data.round                 = AT_EDGE ? std::round(R) : 0;
            data.roundingPower         = W->presentation().roundingPower();
            data.workspacePresentation = presentation;
            g_pHyprRenderer->addPassElement(ctx, makeUnique<CRectPassElement>(data));
        }

        const int  PX  = std::round(ICON_SIZE * SCALE);
        const auto TEX = iconTexture(BUTTON, BUTTON == TITLEBAR_BUTTON_MAXIMIZE && MAXIMIZED, (HOVERED && BUTTON == TITLEBAR_BUTTON_CLOSE) ? ICON_COLOR_HI : ICON_COLOR, PX);
        if (!TEX)
            continue;

        const auto                   LOCAL = toLocal(BOX);
        CTexPassElement::SRenderData data;
        data.tex = TEX;
        data.box = CBox{std::round(LOCAL.x + (LOCAL.w - PX) / 2.0), std::round(LOCAL.y + (LOCAL.h - PX) / 2.0), sc<double>(PX), sc<double>(PX)};
        data.a   = a;
        g_pHyprRenderer->addPassElement(ctx, makeUnique<CTexPassElement>(std::move(data)));
    }

    // title
    const bool  CENTER    = *PALIGN == "center";
    const float PADDING   = *PPADDING;
    const float MAX_WIDTH = (CENTER ? BAR.w - 2 * std::max<double>(buttonsWidth, PADDING) : BAR.w - buttonsWidth - 2 * PADDING) * SCALE;

    renderTitle(monitor, MAX_WIDTH, ACTIVE);

    if (m_titleTex) {
        const auto                   LOCAL = toLocal(BAR);
        const auto                   SIZE  = m_titleTex->m_size;
        const double                 X     = CENTER ? LOCAL.x + (LOCAL.w - SIZE.x) / 2.0 : LOCAL.x + PADDING * SCALE;

        CTexPassElement::SRenderData data;
        data.tex = m_titleTex;
        data.box = CBox{std::round(X), std::round(LOCAL.y + (LOCAL.h - SIZE.y) / 2.0), SIZE.x, SIZE.y};
        data.a   = a;
        g_pHyprRenderer->addPassElement(ctx, makeUnique<CTexPassElement>(std::move(data)));
    }
}
