#include "tests.hpp"
#include "helpers.hpp"

#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <chrono>
#include <format>
#include <thread>

using namespace StackTest;

TEST_CASE(stackClientSideDecorationRequestHonored) {
    enterStacking();

    // foot negotiates xdg-decoration; asked for client side it draws its own and gets no titlebar
    SPAWN_FOOT(ssd, "stack-ssd");
    SPAWN_FOOT(csd, "stack-csd", 50, 12, std::vector<std::string>{"-o", "csd.preferred=client"});

    EXPECT(window("stack-ssd").titlebar, true);
    EXPECT(window("stack-csd").titlebar, false);

    leaveStacking();
}

static void clickAt(int x, int y) {
    getFromSocket(std::format("/dispatch hl.dsp.cursor.move({{ x = {}, y = {} }})", x, y));
    getFromSocket("/eval hl.plugin.test.click(272, 1)");
    getFromSocket("/eval hl.plugin.test.click(272, 0)");
}

// GTK draws shadows outside its xdg window geometry: pointer input has to be offset by it
TEST_CASE(stackCsdWindowGeometryInput) {
    enterStacking();

    auto gtk = spawnClient("gtk4-demo", {}, 20000);
    if (!gtk)
        FAIL_TEST("Could not spawn gtk4-demo");
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));

    const auto WIN = window("org.gtk.Demo4");
    ASSERT(WIN.found, true);
    EXPECT(WIN.titlebar, false);

    // double click on the header bar title, a few pixels below the frame's top edge. Without the
    // geometry offset this lands in GTK's shadow / resize margin.
    const int X = WIN.box.x + WIN.box.w / 2, Y = WIN.box.y + 8;
    clickAt(X, Y);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    clickAt(X, Y);

    for (int i = 0; i < 40 && window("org.gtk.Demo4").maximized != 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT(window("org.gtk.Demo4").maximized, 3);

    // and the client can leave maximize again (unset_maximized from the app), once it has
    // processed the maximized configure
    std::this_thread::sleep_for(std::chrono::milliseconds(800));
    const auto MAX = window("org.gtk.Demo4").box;
    clickAt(MAX.x + MAX.w / 2, MAX.y + 8);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    clickAt(MAX.x + MAX.w / 2, MAX.y + 8);

    for (int i = 0; i < 40 && window("org.gtk.Demo4").maximized != 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    EXPECT(window("org.gtk.Demo4").maximized, 0);

    leaveStacking();
}
