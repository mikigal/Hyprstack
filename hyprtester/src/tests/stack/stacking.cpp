#include "tests.hpp"
#include "helpers.hpp"

#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <format>

using namespace StackTest;

// border_size is 1 in enterStacking(): window boxes are inset by it from the work area
static constexpr int BORDER = 1;

TEST_CASE(stackWindowsAlwaysFloat) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");

    EXPECT(window("stack-a").floating, true);
    EXPECT(window("stack-b").floating, true);

    // toggling floating is a no-op in stacking mode
    OK(getFromSocket("/dispatch hl.dsp.window.float({ action = 'toggle', window = 'class:stack-a' })"));
    Tests::sync();
    EXPECT(window("stack-a").floating, true);

    leaveStacking();
}

TEST_CASE(stackPlacementFirstCentered) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");

    const auto WIN  = window("stack-a");
    const auto AREA = workAreaOf("stack-a");

    EXPECT_MAX_DELTA(WIN.box.x + WIN.box.w / 2, AREA.x + AREA.w / 2, 2);
    EXPECT_MAX_DELTA(WIN.box.y + WIN.box.h / 2, AREA.y + AREA.h / 2, 2);

    leaveStacking();
}

TEST_CASE(stackPlacementSmartAvoidsOverlap) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");
    SPAWN_FOOT(c, "stack-c");

    const auto A = window("stack-a").box, B = window("stack-b").box, C = window("stack-c").box;
    const auto AREA = workAreaOf("stack-a");

    EXPECT(A.overlaps(B), false);
    EXPECT(A.overlaps(C), false);
    EXPECT(B.overlaps(C), false);

    for (const auto& box : {A, B, C}) {
        EXPECT(box.x >= AREA.x && box.y >= AREA.y, true);
        EXPECT(box.x + box.w <= AREA.x + AREA.w && box.y + box.h <= AREA.y + AREA.h, true);
    }

    leaveStacking();
}

TEST_CASE(stackPlacementCascadeWhenFull) {
    enterStacking();
    OK(getFromSocket("/eval hl.config({ stacking = { placement = 'cascade', cascade_offset = 40 } })"));

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");

    const auto A = window("stack-a").box, B = window("stack-b").box;
    EXPECT_MAX_DELTA(B.x - A.x, 40, 2);
    EXPECT_MAX_DELTA(B.y - A.y, 40, 2);

    leaveStacking();
}

TEST_CASE(stackMaximizeToggle) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");

    const auto BEFORE = window("stack-a").box;
    const auto AREA   = workAreaOf("stack-a");

    OK(getFromSocket("/dispatch hl.dsp.window.maximize({ window = 'class:stack-a' })"));
    Tests::sync();

    {
        const auto WIN = window("stack-a");
        EXPECT(WIN.maximized, 3);
        EXPECT(WIN.box.x, AREA.x + BORDER);
        EXPECT(WIN.box.y, AREA.y + BORDER);
        EXPECT(WIN.box.w, AREA.w - 2 * BORDER);
        EXPECT(WIN.box.h, AREA.h - 2 * BORDER);
    }

    OK(getFromSocket("/dispatch hl.dsp.window.maximize({ window = 'class:stack-a' })"));
    Tests::sync();

    {
        const auto WIN = window("stack-a");
        EXPECT(WIN.maximized, 0);
        EXPECT(WIN.box.x, BEFORE.x);
        EXPECT(WIN.box.y, BEFORE.y);
        EXPECT(WIN.box.w, BEFORE.w);
        EXPECT(WIN.box.h, BEFORE.h);
    }

    leaveStacking();
}

TEST_CASE(stackMaximizePartial) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");

    const auto BEFORE = window("stack-a").box;
    const auto AREA   = workAreaOf("stack-a");

    OK(getFromSocket("/dispatch hl.dsp.window.maximize({ mode = 'vertical', window = 'class:stack-a' })"));
    Tests::sync();

    const auto WIN = window("stack-a");
    EXPECT(WIN.maximized, 2);
    EXPECT(WIN.box.x, BEFORE.x);
    EXPECT(WIN.box.w, BEFORE.w);
    EXPECT(WIN.box.y, AREA.y + BORDER);
    EXPECT(WIN.box.h, AREA.h - 2 * BORDER);

    leaveStacking();
}

TEST_CASE(stackMultipleWindowsMaximized) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");

    OK(getFromSocket("/dispatch hl.dsp.window.maximize({ action = 'enable', window = 'class:stack-a' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.maximize({ action = 'enable', window = 'class:stack-b' })"));
    Tests::sync();

    // unlike fullscreen, any number of windows can be maximized at once
    EXPECT(window("stack-a").maximized, 3);
    EXPECT(window("stack-b").maximized, 3);
    EXPECT_NOT(Tests::getAttribute(getFromSocket("/activewindow"), "fullscreen"), "1");

    leaveStacking();
}

TEST_CASE(stackMaximizeViaFullscreenDispatcher) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");

    // the upstream "maximized" fullscreen mode maps to a real maximize in stacking mode
    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', window = 'class:stack-a' })"));
    Tests::sync();
    EXPECT(window("stack-a").maximized, 3);
    EXPECT(Tests::getAttribute(getFromSocket("/activewindow"), "fullscreen"), "0");

    OK(getFromSocket("/dispatch hl.dsp.window.fullscreen({ mode = 'maximized', window = 'class:stack-a' })"));
    Tests::sync();
    EXPECT(window("stack-a").maximized, 0);

    leaveStacking();
}

TEST_CASE(stackMinimizeAndRestore) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:stack-b' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.minimize({ window = 'class:stack-b' })"));
    Tests::sync();

    {
        const auto B = window("stack-b");
        EXPECT(B.minimized, true);
        EXPECT(B.hidden, true);
        // focus falls back to the most recently used visible window
        EXPECT(activeClass(), "stack-a");
    }

    OK(getFromSocket("/dispatch hl.dsp.window.minimize({ action = 'disable', window = 'class:stack-b' })"));
    Tests::sync();

    {
        const auto B = window("stack-b");
        EXPECT(B.minimized, false);
        EXPECT(B.hidden, false);
        EXPECT(activeClass(), "stack-b");
        EXPECT(isAbove("stack-b", "stack-a"), true);
    }

    leaveStacking();
}

TEST_CASE(stackFocusRaises) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");

    EXPECT(isAbove("stack-b", "stack-a"), true);

    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:stack-a' })"));
    Tests::sync();
    EXPECT(isAbove("stack-a", "stack-b"), true);

    leaveStacking();
}

TEST_CASE(stackClickRaises) {
    enterStacking();
    OK(getFromSocket("/eval hl.config({ stacking = { placement = 'center' } })"));

    // both centered: b covers the middle of a, a sticks out at the bottom
    SPAWN_FOOT(a, "stack-a", 50, 20);
    SPAWN_FOOT(b, "stack-b", 50, 12);

    EXPECT(isAbove("stack-b", "stack-a"), true);

    const auto A = window("stack-a").box;
    OK(getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", A.x + A.w / 2, A.y + A.h - 5)));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 1)"));
    OK(getFromSocket("/eval hl.plugin.test.click(272, 0)"));
    Tests::sync();

    EXPECT(activeClass(), "stack-a");
    EXPECT(isAbove("stack-a", "stack-b"), true);

    leaveStacking();
}

TEST_CASE(stackKeepAboveAndBelow) {
    enterStacking();

    SPAWN_FOOT(a, "stack-a");
    SPAWN_FOOT(b, "stack-b");
    SPAWN_FOOT(c, "stack-c");

    OK(getFromSocket("/dispatch hl.dsp.window.keep_above({ action = 'enable', window = 'class:stack-a' })"));
    OK(getFromSocket("/dispatch hl.dsp.window.keep_below({ action = 'enable', window = 'class:stack-c' })"));
    Tests::sync();

    EXPECT(window("stack-a").layer, 2);
    EXPECT(window("stack-c").layer, 0);

    // focusing raises, but never past the layers
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:stack-c' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:stack-b' })"));
    Tests::sync();

    const auto ORDER = zOrder();
    ASSERT(ORDER.size(), 3);
    EXPECT(ORDER[0], "stack-c");
    EXPECT(ORDER[1], "stack-b");
    EXPECT(ORDER[2], "stack-a");

    OK(getFromSocket("/dispatch hl.dsp.window.keep_above({ action = 'disable', window = 'class:stack-a' })"));
    OK(getFromSocket("/dispatch hl.dsp.focus({ window = 'class:stack-b' })"));
    Tests::sync();
    EXPECT(isAbove("stack-b", "stack-a"), true);

    leaveStacking();
}
