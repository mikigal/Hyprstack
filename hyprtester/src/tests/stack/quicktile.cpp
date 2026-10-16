#include "tests.hpp"
#include "helpers.hpp"

#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <chrono>
#include <format>
#include <thread>

using namespace StackTest;

static constexpr int TITLEBAR = 32;
static constexpr int BORDER   = 1;

// eTileEdges
static constexpr int LEFT = 1, RIGHT = 2, TOP = 4, BOTTOM = 8;

static void          moveCursor(int x, int y) {
    getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x, y));
}

// drag a window by its titlebar so that the cursor ends up at (x, y)
static void dragTitlebarTo(const std::string& cls, int x, int y) {
    const auto BOX = window(cls).box;
    const int  SX = BOX.x + 40, SY = BOX.y - TITLEBAR / 2;
    moveCursor(SX, SY);
    getFromSocket("/eval hl.plugin.test.click(272, 1)");
    moveCursor((SX + x) / 2, (SY + y) / 2);
    moveCursor(x, y);
    getFromSocket("/eval hl.plugin.test.click(272, 0)");
    Tests::sync();
}

static void snap(const std::string& cls, const std::string& dir) {
    getFromSocket(std::format("/dispatch hl.dsp.window.snap({{ direction = '{}', window = 'class:{}' }})", dir, cls));
    Tests::sync();
}

TEST_CASE(stackQuickTileDragToLeftEdge) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto AREA = workAreaOf("stack-a");
    dragTitlebarTo("stack-a", AREA.x + 1, AREA.y + AREA.h / 2);

    const auto WIN = window("stack-a");
    EXPECT(WIN.tiled, LEFT);
    EXPECT(WIN.box.x, AREA.x + BORDER);
    EXPECT(WIN.box.y, AREA.y + BORDER + TITLEBAR);
    EXPECT(WIN.box.w, AREA.w / 2 - 2 * BORDER);
    EXPECT(WIN.box.h, AREA.h - 2 * BORDER - TITLEBAR);

    leaveStacking();
}

TEST_CASE(stackQuickTileDragToCorner) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto AREA = workAreaOf("stack-a");
    dragTitlebarTo("stack-a", AREA.x + 1, AREA.y + 1);

    const auto WIN = window("stack-a");
    EXPECT(WIN.tiled, LEFT | TOP);
    EXPECT(WIN.box.w, AREA.w / 2 - 2 * BORDER);
    EXPECT(WIN.box.h, AREA.h / 2 - 2 * BORDER - TITLEBAR);

    leaveStacking();
}

TEST_CASE(stackQuickTileDragToTopMaximizes) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto AREA = workAreaOf("stack-a");
    dragTitlebarTo("stack-a", AREA.x + AREA.w / 2, AREA.y + 1);

    EXPECT(window("stack-a").maximized, 3);
    EXPECT(window("stack-a").tiled, 0);

    leaveStacking();
}

TEST_CASE(stackQuickTileIgnoresSharedEdges) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    // the monitor's right edge borders the next monitor in the test setup: no tiling there
    const auto AREA = workAreaOf("stack-a");
    placeMonitorRightOf("stack-a");

    dragTitlebarTo("stack-a", AREA.x + AREA.w - 2, AREA.y + AREA.h / 2);
    EXPECT(window("stack-a").tiled, 0);
    EXPECT(window("stack-a").maximized, 0);

    leaveStacking();
}

TEST_CASE(stackQuickTileDragOutRestores) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto ORIGINAL = window("stack-a").box;
    OK(getFromSocket("/dispatch hl.dsp.window.tile({ edges = 'right', window = 'class:stack-a' })"));
    Tests::sync();
    ASSERT(window("stack-a").tiled, RIGHT);

    const auto AREA = workAreaOf("stack-a");
    dragTitlebarTo("stack-a", AREA.x + AREA.w / 2, AREA.y + AREA.h / 2);

    const auto WIN = window("stack-a");
    EXPECT(WIN.tiled, 0);
    EXPECT(WIN.box.w, ORIGINAL.w);
    EXPECT(WIN.box.h, ORIGINAL.h);

    leaveStacking();
}

TEST_CASE(stackSnapKeyboardCycle) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto ORIGINAL = window("stack-a").box;

    snap("stack-a", "left");
    EXPECT(window("stack-a").tiled, LEFT);
    snap("stack-a", "up");
    EXPECT(window("stack-a").tiled, LEFT | TOP);
    snap("stack-a", "down");
    EXPECT(window("stack-a").tiled, LEFT);
    snap("stack-a", "down");
    EXPECT(window("stack-a").tiled, LEFT | BOTTOM);
    snap("stack-a", "up");
    EXPECT(window("stack-a").tiled, LEFT);

    // the opposite direction restores the original geometry
    snap("stack-a", "right");
    {
        const auto WIN = window("stack-a");
        EXPECT(WIN.tiled, 0);
        EXPECT(WIN.box.x, ORIGINAL.x);
        EXPECT(WIN.box.y, ORIGINAL.y);
        EXPECT(WIN.box.w, ORIGINAL.w);
        EXPECT(WIN.box.h, ORIGINAL.h);
    }

    // up maximizes a normal window, down restores, down again minimizes
    snap("stack-a", "up");
    EXPECT(window("stack-a").maximized, 3);
    snap("stack-a", "down");
    EXPECT(window("stack-a").maximized, 0);
    snap("stack-a", "down");
    EXPECT(window("stack-a").minimized, true);

    leaveStacking();
}

TEST_CASE(stackSnapAcrossMonitors) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto AREA = workAreaOf("stack-a");
    placeMonitorRightOf("stack-a");

    // right half, then on to the left half of the monitor to the right
    snap("stack-a", "right");
    EXPECT(window("stack-a").tiled, RIGHT);
    snap("stack-a", "right");

    const auto WIN = window("stack-a");
    EXPECT(WIN.tiled, LEFT);
    EXPECT(WIN.box.x, AREA.x + AREA.w + BORDER);

    leaveStacking();
}

TEST_CASE(stackTiledRefitsOnWorkAreaChange) {
    enterStacking();
    SPAWN_FOOT(a, "stack-a", 60, 14);

    snap("stack-a", "left");
    const auto BEFORE = window("stack-a").box;

    // a panel reserving space on the left shrinks the work area: the tile follows
    const auto MONITOR = monitorNameOf("stack-a");
    OK(getFromSocket(std::format("/eval hl.monitor({{ output = '{}', reserved = {{ top = 0, right = 0, bottom = 0, left = 100 }} }})", MONITOR)));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Tests::sync();

    const auto AFTER = window("stack-a").box;
    EXPECT(AFTER.x, BEFORE.x + 100);
    EXPECT(AFTER.w, BEFORE.w - 50);

    leaveStacking();
}
