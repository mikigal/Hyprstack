#include "tests.hpp"
#include "helpers.hpp"

#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <chrono>
#include <format>
#include <thread>

using namespace StackTest;

// must match enterStacking() / stacking:titlebar defaults
static constexpr int TITLEBAR_HEIGHT = 32;
static constexpr int BUTTON_WIDTH    = 46;

static void          moveCursor(int x, int y) {
    getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x, y));
}

static void press() {
    getFromSocket("/eval hl.plugin.test.click(272, 1)");
}

static void release() {
    getFromSocket("/eval hl.plugin.test.click(272, 0)");
}

static void clickAt(int x, int y) {
    moveCursor(x, y);
    press();
    release();
}

// center of the n-th button from the right (0 = close, 1 = maximize, 2 = minimize)
static std::pair<int, int> buttonCenter(const SBox& win, int fromRight) {
    return {win.x + win.w - BUTTON_WIDTH * fromRight - BUTTON_WIDTH / 2, win.y - TITLEBAR_HEIGHT / 2};
}

TEST_CASE(stackTitlebarMaximizeButton) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto [X, Y] = buttonCenter(window("stack-a").box, 1);
    clickAt(X, Y);
    Tests::sync();
    EXPECT(window("stack-a").maximized, 3);

    // the titlebar follows the maximized window: click restore
    const auto [X2, Y2] = buttonCenter(window("stack-a").box, 1);
    clickAt(X2, Y2);
    Tests::sync();
    EXPECT(window("stack-a").maximized, 0);

    leaveStacking();
}

TEST_CASE(stackTitlebarMinimizeButton) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto [X, Y] = buttonCenter(window("stack-a").box, 2);
    clickAt(X, Y);
    Tests::sync();
    EXPECT(window("stack-a").minimized, true);

    leaveStacking();
}

TEST_CASE(stackTitlebarCloseButton) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);
    SPAWN_FOOT(b, "stack-b", 40, 8);
    ASSERT(Tests::windowCount(), 2);

    const auto [X, Y] = buttonCenter(window("stack-b").box, 0);
    clickAt(X, Y);
    Tests::waitUntilWindowsN(1);
    EXPECT(window("stack-b").found, false);
    EXPECT(window("stack-a").found, true);

    leaveStacking();
}

TEST_CASE(stackTitlebarButtonCancelledByMovingAway) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);

    // press on close, release elsewhere: nothing happens (like on Windows)
    const auto WIN    = window("stack-a").box;
    const auto [X, Y] = buttonCenter(WIN, 0);
    moveCursor(X, Y);
    press();
    moveCursor(WIN.x + WIN.w / 2, WIN.y + WIN.h / 2);
    release();
    Tests::sync();

    EXPECT(window("stack-a").found, true);

    leaveStacking();
}

TEST_CASE(stackTitlebarDoubleClickMaximizes) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto WIN = window("stack-a").box;
    const int  X = WIN.x + 40, Y = WIN.y - TITLEBAR_HEIGHT / 2;
    clickAt(X, Y);
    clickAt(X, Y);
    Tests::sync();
    EXPECT(window("stack-a").maximized, 3);

    leaveStacking();
}

TEST_CASE(stackTitlebarDragMoves) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);
    SPAWN_FOOT(b, "stack-b", 40, 8);

    const auto BEFORE = window("stack-a").box;
    const int  X = BEFORE.x + 40, Y = BEFORE.y - TITLEBAR_HEIGHT / 2;

    moveCursor(X, Y);
    press();
    // a click alone must focus and raise
    EXPECT(activeClass(), "stack-a");
    EXPECT(isAbove("stack-a", "stack-b"), true);

    moveCursor(X + 10, Y + 5);
    moveCursor(X + 60, Y + 40);
    release();
    Tests::sync();

    const auto AFTER = window("stack-a").box;
    EXPECT_MAX_DELTA(AFTER.x - BEFORE.x, 60, 2);
    EXPECT_MAX_DELTA(AFTER.y - BEFORE.y, 40, 2);
    EXPECT(AFTER.w, BEFORE.w);
    EXPECT(AFTER.h, BEFORE.h);

    leaveStacking();
}

TEST_CASE(stackPlacementKeepsTitlebarOnScreen) {
    enterStacking();

    // smart placement starts at the work area's corner: the titlebar must fit above the window
    SPAWN_FOOT(a, "stack-a", 50, 10);
    SPAWN_FOOT(b, "stack-b", 50, 10);

    const auto AREA = workAreaOf("stack-b");
    for (const auto& cls : {"stack-a", "stack-b"}) {
        const auto BOX = window(cls).box;
        EXPECT(BOX.y - TITLEBAR_HEIGHT >= AREA.y, true);
    }

    leaveStacking();
}

TEST_CASE(stackDragMaximizedRestores) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a", 60, 14);

    const auto ORIGINAL = window("stack-a").box;
    OK(getFromSocket("/dispatch hl.dsp.window.maximize({ window = 'class:stack-a' })"));
    Tests::sync();

    // grab the titlebar at a quarter of its width and pull the window down
    const auto MAX = window("stack-a").box;
    const int  X = MAX.x + MAX.w / 4, Y = MAX.y - TITLEBAR_HEIGHT / 2;
    moveCursor(X, Y);
    press();
    moveCursor(X, Y + 20);
    moveCursor(X, Y + 120);
    release();
    Tests::sync();

    const auto AFTER = window("stack-a");
    EXPECT(AFTER.maximized, 0);
    EXPECT(AFTER.box.w, ORIGINAL.w);
    EXPECT(AFTER.box.h, ORIGINAL.h);
    // the cursor is still at a quarter of the (now smaller) titlebar
    EXPECT_MAX_DELTA(X - AFTER.box.x, AFTER.box.w / 4, 3);
    EXPECT_MAX_DELTA(Y + 120 - AFTER.box.y, -TITLEBAR_HEIGHT / 2, 3);

    leaveStacking();
}

// resize drags are throttled to the monitor refresh rate, give each motion a frame
static void moveCursorSlow(int x, int y) {
    moveCursor(x, y);
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
}

TEST_CASE(stackResizeOnBorderAroundTitlebar) {
    enterStacking();
    OK(getFromSocket("/eval hl.config({ general = { resize_on_border = true, extend_border_grab_area = 10 } })"));

    SPAWN_FOOT(a, "stack-a", 60, 14);

    // top edge: the grab area sits above the titlebar, not inside it
    {
        const auto BEFORE = window("stack-a").box;
        const int  X = BEFORE.x + BEFORE.w / 2, Y = BEFORE.y - TITLEBAR_HEIGHT - 4;
        moveCursorSlow(X, Y);
        press();
        moveCursorSlow(X, Y - 10);
        moveCursorSlow(X, Y - 50);
        release();
        Tests::sync();

        const auto AFTER = window("stack-a").box;
        EXPECT_MAX_DELTA(AFTER.h - BEFORE.h, 50, 20); // terminals snap to their cell grid
        EXPECT_MAX_DELTA(AFTER.y + AFTER.h, BEFORE.y + BEFORE.h, 2);
    }

    // right edge
    {
        const auto BEFORE = window("stack-a").box;
        const int  X = BEFORE.x + BEFORE.w + 4, Y = BEFORE.y + BEFORE.h / 2;
        moveCursorSlow(X, Y);
        press();
        moveCursorSlow(X + 10, Y);
        moveCursorSlow(X + 80, Y);
        release();
        Tests::sync();

        const auto AFTER = window("stack-a").box;
        EXPECT_MAX_DELTA(AFTER.w - BEFORE.w, 80, 20);
        EXPECT(AFTER.x, BEFORE.x);
    }

    leaveStacking();
}
