#include "helpers.hpp"

#include "../../hyprctlCompat.hpp"
#include "../../shared.hpp"
#include "../shared.hpp"

#include <chrono>
#include <format>
#include <sstream>
#include <thread>

using namespace Hyprutils::Memory;
using namespace Hyprutils::OS;

static constexpr const char* EXTRA_MONITOR_NAME = "HEADLESS-STACK";

void                         StackTest::enterStacking() {
    getFromSocket("/eval hl.config({ general = { layout = 'stacking', gaps_out = 0, gaps_in = 0, border_size = 1 }, animations = { enabled = false } })");
    getFromSocket("/eval hl.config({ stacking = { placement = 'smart' } })");
}

void StackTest::leaveStacking() {
    Tests::killAllWindows();
    if (getFromSocket("/monitors").contains(EXTRA_MONITOR_NAME))
        getFromSocket(std::string{"/output remove "} + EXTRA_MONITOR_NAME);
    getFromSocket("/reload");
}

CUniquePointer<CProcess> StackTest::spawnFoot(const std::string& cls, int cols, int rows) {
    const auto COUNT_BEFORE = Tests::windowCount();

    auto       foot = makeUnique<CProcess>("foot", std::vector<std::string>{"--app-id", cls, "-W", std::format("{}x{}", cols, rows)});
    foot->addEnv("WAYLAND_DISPLAY", WLDISPLAY);
    foot->runAsync();

    for (int i = 0; i < 100 && Tests::processAlive(foot->pid()) && Tests::windowCount() == COUNT_BEFORE; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!Tests::processAlive(foot->pid()) || Tests::windowCount() == COUNT_BEFORE)
        return nullptr;

    // let the first configure / commit round settle
    Tests::sync();
    return foot;
}

static std::vector<std::string> clientBlocks() {
    const auto               CLIENTS = getFromSocket("/clients");
    std::vector<std::string> blocks;

    size_t                   pos = 0;
    while ((pos = CLIENTS.find("Window ", pos)) != std::string::npos) {
        const auto END = CLIENTS.find("\n\n", pos);
        blocks.emplace_back(CLIENTS.substr(pos, END == std::string::npos ? std::string::npos : END - pos));
        if (END == std::string::npos)
            break;
        pos = END;
    }

    return blocks;
}

static std::pair<int, int> parsePair(const std::string& s) {
    const auto COMMA = s.find(',');
    if (COMMA == std::string::npos)
        return {0, 0};
    try {
        return {std::stoi(s.substr(0, COMMA)), std::stoi(s.substr(COMMA + 1))};
    } catch (...) { return {0, 0}; }
}

static int parseInt(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (...) { return -1; }
}

StackTest::SWindowInfo StackTest::window(const std::string& cls) {
    for (const auto& block : clientBlocks()) {
        if (Tests::getAttribute(block, "class") != cls)
            continue;

        SWindowInfo info;
        info.found = true;

        const auto ADDR_START = block.find("Window ") + 7;
        info.address          = "0x" + block.substr(ADDR_START, block.find(' ', ADDR_START) - ADDR_START);

        const auto [X, Y] = parsePair(Tests::getAttribute(block, "at"));
        const auto [W, H] = parsePair(Tests::getAttribute(block, "size"));
        info.box          = {X, Y, W, H};
        info.floating     = Tests::getAttribute(block, "floating") == "1";
        info.hidden       = Tests::getAttribute(block, "hidden") == "1";
        info.minimized    = Tests::getAttribute(block, "minimized") == "1";
        info.maximized    = parseInt(Tests::getAttribute(block, "maximized"));
        info.layer        = parseInt(Tests::getAttribute(block, "stackLayer"));
        info.tiled        = parseInt(Tests::getAttribute(block, "tiled"));
        return info;
    }

    return {};
}

std::string StackTest::activeClass() {
    return Tests::getAttribute(getFromSocket("/activewindow"), "class");
}

std::vector<std::string> StackTest::zOrder() {
    std::vector<std::string> order;
    for (const auto& block : clientBlocks()) {
        order.emplace_back(Tests::getAttribute(block, "class"));
    }
    return order;
}

bool StackTest::isAbove(const std::string& upper, const std::string& lower) {
    const auto ORDER = zOrder();
    const auto U     = std::ranges::find(ORDER, upper);
    const auto L     = std::ranges::find(ORDER, lower);
    return U != ORDER.end() && L != ORDER.end() && U > L;
}

StackTest::SBox StackTest::workAreaOf(const std::string& cls) {
    const auto WINDOW = window(cls);
    const auto BLOCK  = [&] {
        for (const auto& b : clientBlocks()) {
            if (Tests::getAttribute(b, "class") == cls)
                return b;
        }
        return std::string{};
    }();
    const auto MONITOR_ID = Tests::getAttribute(BLOCK, "monitor");

    const auto MONITORS = getFromSocket("/monitors");
    const auto HEADER   = std::format("(ID {}):", MONITOR_ID);
    const auto START    = MONITORS.find(HEADER);
    if (START == std::string::npos)
        return {};

    // "\tWxH@R at XxY"
    const auto        LINE_START = MONITORS.find('\t', START) + 1;
    const auto        LINE       = MONITORS.substr(LINE_START, MONITORS.find('\n', LINE_START) - LINE_START);

    int               w = 0, h = 0, x = 0, y = 0;
    float             refresh = 0;
    std::stringstream ss(LINE);
    char              sep = 0;
    std::string       at;
    ss >> w >> sep >> h >> sep >> refresh >> at >> x >> sep >> y;

    // "reserved: left top right bottom"
    const auto        BLOCK_END = MONITORS.find("\n\n", START);
    const auto        RESERVED  = Tests::getAttribute(MONITORS.substr(START, BLOCK_END - START), "reserved");
    int               rt = 0, rr = 0, rb = 0, rl = 0;
    std::stringstream rs(RESERVED);
    rs >> rl >> rt >> rr >> rb;

    return {x + rl, y + rt, w - rl - rr, h - rt - rb};
}

std::string StackTest::monitorNameOf(const std::string& cls) {
    std::string monitorID;
    for (const auto& b : clientBlocks()) {
        if (Tests::getAttribute(b, "class") == cls)
            monitorID = Tests::getAttribute(b, "monitor");
    }

    const auto MONITORS = getFromSocket("/monitors");
    const auto HEADER   = std::format("(ID {}):", monitorID);
    const auto POS      = MONITORS.find(HEADER);
    if (POS == std::string::npos)
        return "";

    const auto LINE_START = MONITORS.rfind("Monitor ", POS) + 8;
    return MONITORS.substr(LINE_START, POS - 1 - LINE_START);
}

static constexpr const char* EXTRA_MONITOR = EXTRA_MONITOR_NAME;

void                         StackTest::placeMonitorRightOf(const std::string& cls) {
    const auto AREA = workAreaOf(cls);

    // pin the window's monitor first: test.lua uses "auto-right", which would rearrange it
    getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '{}x{}@60', position = '{}x{}', scale = '1' }})", monitorNameOf(cls), AREA.w, AREA.h, AREA.x, AREA.y));
    getFromSocket(std::format("/output create headless {}", EXTRA_MONITOR));
    getFromSocket(std::format("/eval hl.monitor({{ output = '{}', mode = '1920x1080@60', position = '{}x{}', scale = '1' }})", EXTRA_MONITOR, AREA.x + AREA.w, AREA.y));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    Tests::sync();
}
