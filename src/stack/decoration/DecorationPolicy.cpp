#include "DecorationPolicy.hpp"
#include "TitlebarDecoration.hpp"
#include "../StackMode.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../desktop/rule/windowRule/WindowRuleApplicator.hpp"
#include "../../desktop/view/WLSurface.hpp"
#include "../../desktop/view/window/Window.hpp"
#include "../../desktop/view/window/WindowPresentation.hpp"
#include "../../protocols/ServerDecorationKDE.hpp"
#include "../../protocols/XDGDecoration.hpp"
#include "../../protocols/core/Compositor.hpp"

bool Stack::negotiatedServerSide(PHLWINDOW w) {
    if (!w || w->backend().isX11())
        return false;

    const auto SURFACE = w->backend().surface();
    return SURFACE && (PROTO::xdgDecoration->hasDecorationFor(SURFACE) || PROTO::serverDecorationKDE->hasServerDecorationFor(SURFACE));
}

bool Stack::isClientDecorated(PHLWINDOW w) {
    return w && !w->backend().isX11() && windowIsStacking(w) && !negotiatedServerSide(w);
}

void Stack::applyDecorationPolicy(PHLWINDOW w) {
    static auto PDECORATECSD = CConfigValue<Config::INTEGER>("stacking:decorate_csd");

    if (*PDECORATECSD || !isClientDecorated(w))
        return;

    // lowest priority: window rules still win
    w->m_ruleApplicator->decorate().set(false, Desktop::Types::PRIORITY_LAYOUT);
    w->m_ruleApplicator->rounding().set(0, Desktop::Types::PRIORITY_LAYOUT);
}

Vector2D Stack::mainSurfaceOrigin(PHLWINDOW w) {
    const auto OVERFLOW = surfaceOverflow(w);
    return w->position(Desktop::View::IGeometric::GEOMETRIC_CURRENT) - (OVERFLOW ? OVERFLOW->topLeft : Vector2D{});
}

CBox Stack::resizeFrame(PHLWINDOW w, const CBox& windowBox) {
    static auto PHEIGHT = CConfigValue<Config::INTEGER>("stacking:titlebar:height");

    if (!wantsTitlebar(w))
        return windowBox;

    return CBox{windowBox.x, windowBox.y - *PHEIGHT, windowBox.w, windowBox.h + *PHEIGHT};
}

bool Stack::inResizeCorner(PHLWINDOW w, const Vector2D& pos) {
    if (!w->presentation().isInCurvedCorner(pos.x, pos.y))
        return false;

    return !wantsTitlebar(w) || pos.y > w->getWindowMainSurfaceBox().middle().y;
}

std::optional<SBoxExtents> Stack::surfaceOverflow(PHLWINDOW w) {
    if (!w || w->backend().isX11() || !windowIsStacking(w) || !w->wlSurface() || !w->wlSurface()->resource())
        return std::nullopt;

    const auto GEOMETRY = w->backend().geometry().box;
    const auto SURFACE  = w->wlSurface()->resource()->m_current.size;

    if (GEOMETRY.w < 1 || GEOMETRY.h < 1 || SURFACE.x < 1 || SURFACE.y < 1)
        return std::nullopt;

    if (GEOMETRY.pos() == Vector2D{} && GEOMETRY.size() == SURFACE)
        return std::nullopt;

    // during size animations the frame is scaled, so is everything around it
    const auto RATIO = w->size(Desktop::View::IGeometric::GEOMETRIC_CURRENT) / GEOMETRY.size();

    return SBoxExtents{
        .topLeft     = (GEOMETRY.pos() * RATIO).clamp(Vector2D{}, Vector2D{1e6, 1e6}),
        .bottomRight = ((SURFACE - GEOMETRY.pos() - GEOMETRY.size()) * RATIO).clamp(Vector2D{}, Vector2D{1e6, 1e6}),
    };
}
