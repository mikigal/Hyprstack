#pragma once

#include <hyprutils/memory/UniquePtr.hpp>
#include <hyprutils/os/Process.hpp>

#include <string>
#include <vector>

// Helpers shared by the stacking tests. Windows are identified by their class (foot --app-id).
namespace StackTest {
    struct SBox {
        int  x = 0, y = 0, w = 0, h = 0;

        bool overlaps(const SBox& o) const {
            return x < o.x + o.w && o.x < x + w && y < o.y + o.h && o.y < y + h;
        }
    };

    struct SWindowInfo {
        bool        found = false;
        std::string address;
        SBox        box;
        bool        floating  = false;
        bool        hidden    = false;
        bool        minimized = false;
        int         maximized = 0;
        int         layer     = 1;
        int         tiled     = 0;
        bool        titlebar  = false;
    };

    // switch the running compositor to stacking mode / restore the test config
    void                                                       enterStacking();
    void                                                       leaveStacking();

    Hyprutils::Memory::CUniquePointer<Hyprutils::OS::CProcess> spawnFoot(const std::string& cls, int cols = 50, int rows = 12, const std::vector<std::string>& extraArgs = {});

    // any client; waits until the window count grows
    Hyprutils::Memory::CUniquePointer<Hyprutils::OS::CProcess> spawnClient(const std::string& binary, const std::vector<std::string>& args, int timeoutMs = 15000);

    SWindowInfo                                                window(const std::string& cls);
    std::string                                                activeClass();

    // classes of all windows, bottom-most first
    std::vector<std::string> zOrder();
    bool                     isAbove(const std::string& upper, const std::string& lower);

    // geometry of the monitor the given window is on (logical, minus reserved areas)
    SBox workAreaOf(const std::string& cls);

    // name of the monitor the window is on
    std::string monitorNameOf(const std::string& cls);

    // create an extra monitor right of the window's monitor (removed by leaveStacking())
    void placeMonitorRightOf(const std::string& cls);
}

#define SPAWN_FOOT(var, cls, ...)                                                                                                                                                  \
    auto var = StackTest::spawnFoot(cls __VA_OPT__(, ) __VA_ARGS__);                                                                                                               \
    if (!var)                                                                                                                                                                      \
    FAIL_TEST("Could not spawn foot ({})", cls)
