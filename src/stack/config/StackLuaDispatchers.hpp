#pragma once

struct lua_State;

namespace Stack::Lua {
    // Adds hyprstack dispatchers to the hl.dsp.window table, which must be on top of the stack.
    void registerWindowDispatchers(lua_State* L);

    // Adds hyprstack dispatchers to the hl.dsp table, which must be on top of the stack.
    void registerDispatchers(lua_State* L);
}
