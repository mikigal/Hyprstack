#include "StackLuaDispatchers.hpp"

#include "../actions/StackActions.hpp"

#include "../../config/lua/bindings/LuaBindingsInternal.hpp"

#include <lua.h>

#include <unordered_map>

using namespace Config::Lua::Bindings;
namespace CA = Config::Actions;

// upvalues: 1 = action, 2 = window
static int dsp_minimize(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::minimize(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

// upvalues: 1 = action, 2 = mode, 3 = window
static int dsp_maximize(lua_State* L) {
    return Internal::checkResult(L,
                                 Stack::Actions::maximize(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), sc<uint8_t>(lua_tonumber(L, lua_upvalueindex(2))),
                                                          Internal::windowFromUpval(L, 3)));
}

static int dsp_keepAbove(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::keepAbove(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

static int dsp_keepBelow(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::keepBelow(sc<CA::eTogglableAction>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

// hl.dsp.window.minimize({ action = "toggle"|"enable"|"disable", window = ... }), default: enable
static int hlWindowMinimize(lua_State* L) {
    auto action = CA::TOGGLE_ACTION_ENABLE;
    if (lua_istable(L, 1) && Internal::tableOptStr(L, 1, "action"))
        action = Internal::tableToggleAction(L, 1);

    lua_pushnumber(L, (int)action);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_minimize, 2);
    return 1;
}

// hl.dsp.window.maximize({ action = ..., mode = "full"|"horizontal"|"vertical", window = ... }), default: toggle full
static int hlWindowMaximize(lua_State* L) {
    const auto action = Internal::tableToggleAction(L, 1);
    uint8_t    mode   = Stack::MAXIMIZE_FULL;

    if (lua_istable(L, 1)) {
        if (const auto m = Internal::tableOptStr(L, 1, "mode"); m) {
            if (*m == "full")
                mode = Stack::MAXIMIZE_FULL;
            else if (*m == "horizontal")
                mode = Stack::MAXIMIZE_HORIZONTAL;
            else if (*m == "vertical")
                mode = Stack::MAXIMIZE_VERTICAL;
            else
                return Internal::configError(L, "hl.dsp.window.maximize: invalid mode \"{}\" (expected full/horizontal/vertical)", *m);
        }
    }

    lua_pushnumber(L, (int)action);
    lua_pushnumber(L, mode);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_maximize, 3);
    return 1;
}

static int hlWindowKeepAbove(lua_State* L) {
    lua_pushnumber(L, (int)Internal::tableToggleAction(L, 1));
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_keepAbove, 2);
    return 1;
}

static int hlWindowKeepBelow(lua_State* L) {
    lua_pushnumber(L, (int)Internal::tableToggleAction(L, 1));
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_keepBelow, 2);
    return 1;
}

// upvalues: 1 = edges, 2 = window
static int dsp_tile(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::tile(sc<uint8_t>(lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

// upvalues: 1 = direction, 2 = window
static int dsp_snap(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::snap(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

// hl.dsp.window.tile({ edges = "left"|"right"|"top"|"bottom"|"top_left"|"top_right"|"bottom_left"|"bottom_right"|"none", window = ... })
static int hlWindowTile(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.dsp.window.tile: expected a table { edges, window? }");

    static const std::unordered_map<std::string, uint8_t> EDGES = {
        {"none", Stack::TILE_NONE},
        {"left", Stack::TILE_LEFT},
        {"right", Stack::TILE_RIGHT},
        {"top", Stack::TILE_TOP},
        {"bottom", Stack::TILE_BOTTOM},
        {"top_left", Stack::TILE_TOP | Stack::TILE_LEFT},
        {"top_right", Stack::TILE_TOP | Stack::TILE_RIGHT},
        {"bottom_left", Stack::TILE_BOTTOM | Stack::TILE_LEFT},
        {"bottom_right", Stack::TILE_BOTTOM | Stack::TILE_RIGHT},
    };

    const auto NAME = Internal::tableOptStr(L, 1, "edges").value_or("");
    const auto IT   = EDGES.find(NAME);
    if (IT == EDGES.end())
        return Internal::configError(L, "hl.dsp.window.tile: invalid edges \"{}\"", NAME);

    lua_pushnumber(L, IT->second);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_tile, 2);
    return 1;
}

// hl.dsp.window.snap({ direction = "left"|"right"|"up"|"down", window = ... }): Windows-like Super + arrows
static int hlWindowSnap(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.dsp.window.snap: expected a table { direction, window? }");

    const auto DIR = Math::fromChar(Internal::tableOptStr(L, 1, "direction").value_or(" ")[0]);
    if (DIR == Math::DIRECTION_DEFAULT)
        return Internal::configError(L, "hl.dsp.window.snap: invalid direction (expected left/right/up/down)");

    lua_pushnumber(L, (int)DIR);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_snap, 2);
    return 1;
}

static int dsp_moveToMonitor(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::moveToMonitor(sc<Math::eDirection>((int)lua_tonumber(L, lua_upvalueindex(1))), Internal::windowFromUpval(L, 2)));
}

// hl.dsp.window.move_to_monitor({ direction = "left"|"right"|"up"|"down", window = ... })
static int hlWindowMoveToMonitor(lua_State* L) {
    if (!lua_istable(L, 1))
        return Internal::configError(L, "hl.dsp.window.move_to_monitor: expected a table { direction, window? }");

    const auto DIR = Math::fromChar(Internal::tableOptStr(L, 1, "direction").value_or(" ")[0]);
    if (DIR == Math::DIRECTION_DEFAULT)
        return Internal::configError(L, "hl.dsp.window.move_to_monitor: invalid direction (expected left/right/up/down)");

    lua_pushnumber(L, (int)DIR);
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_moveToMonitor, 2);
    return 1;
}

static int dsp_windowMenu(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::windowMenu(Internal::windowFromUpval(L, 1)));
}

// hl.dsp.window.menu({ window = ... }): ask the shell for the window menu (Alt+Space)
static int hlWindowMenu(lua_State* L) {
    Internal::pushWindowUpval(L, 1);
    lua_pushcclosure(L, dsp_windowMenu, 1);
    return 1;
}

void Stack::Lua::registerWindowDispatchers(lua_State* L) {
    Internal::setDispatcherFn(L, "minimize", hlWindowMinimize, 1);
    Internal::setDispatcherFn(L, "maximize", hlWindowMaximize, 1);
    Internal::setDispatcherFn(L, "keep_above", hlWindowKeepAbove, 1);
    Internal::setDispatcherFn(L, "keep_below", hlWindowKeepBelow, 1);
    Internal::setDispatcherFn(L, "tile", hlWindowTile, 1);
    Internal::setDispatcherFn(L, "snap", hlWindowSnap, 1);
    Internal::setDispatcherFn(L, "move_to_monitor", hlWindowMoveToMonitor, 1);
    Internal::setDispatcherFn(L, "menu", hlWindowMenu, 1);
}

static int dsp_showDesktop(lua_State* L) {
    return Internal::checkResult(L, Stack::Actions::showDesktop());
}

// hl.dsp.show_desktop(): toggle
static int hlShowDesktop(lua_State* L) {
    lua_pushcclosure(L, dsp_showDesktop, 0);
    return 1;
}

void Stack::Lua::registerDispatchers(lua_State* L) {
    Internal::setDispatcherFn(L, "show_desktop", hlShowDesktop, 0);
}
