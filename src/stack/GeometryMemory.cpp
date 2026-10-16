#include "GeometryMemory.hpp"
#include "StackMode.hpp"
#include "WindowStack.hpp"

#include "../config/ConfigValue.hpp"
#include "../debug/log/Logger.hpp"
#include "../desktop/state/WindowState.hpp"
#include "../desktop/view/window/Window.hpp"
#include "../event/EventBus.hpp"
#include "../layout/space/Space.hpp"
#include "../layout/target/Target.hpp"
#include "../output/Monitor.hpp"
#include "../workspace/HLWorkspace.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using namespace Stack;

UP<CGeometryMemory>& Stack::geometryMemory() {
    static UP<CGeometryMemory> p = makeUnique<CGeometryMemory>();
    return p;
}

CGeometryMemory::CGeometryMemory() {
    m_listeners.windowClose = Event::bus()->m_events.window.close.listen([this](PHLWINDOW w) { remember(w); });
}

std::string CGeometryMemory::path() const {
    static auto PFILE = CConfigValue<Config::STRING>("stacking:remember_geometry_file");
    if (!(*PFILE).empty())
        return *PFILE;

    const char* STATE = std::getenv("XDG_STATE_HOME");
    const char* HOME  = std::getenv("HOME");
    const auto  BASE  = STATE && *STATE ? std::string{STATE} : std::string{HOME ? HOME : "/tmp"} + "/.local/state";
    return BASE + "/hyprstack/window-geometry";
}

void CGeometryMemory::load() {
    static std::string loadedFrom;

    const auto         PATH = path();
    if (m_loaded && loadedFrom == PATH)
        return;

    m_loaded   = true;
    loadedFrom = PATH;
    m_entries.clear();

    std::ifstream file(PATH);
    std::string   line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string        appID, monitor;
        SEntry             e;
        if (!std::getline(ss, appID, '\t') || !std::getline(ss, monitor, '\t'))
            continue;
        if (!(ss >> e.offset.x >> e.offset.y >> e.size.x >> e.size.y))
            continue;
        e.monitor        = monitor;
        m_entries[appID] = e;
    }
}

void CGeometryMemory::save() const {
    const auto      PATH = path();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(PATH).parent_path(), ec);

    std::ofstream file(PATH, std::ios::trunc);
    if (!file) {
        LOG(Log::ERR, "[stack] couldn't write window geometry to {}", PATH);
        return;
    }

    for (const auto& [appID, e] : m_entries) {
        file << appID << '\t' << e.monitor << '\t' << e.offset.x << ' ' << e.offset.y << ' ' << e.size.x << ' ' << e.size.y << '\n';
    }
}

std::optional<CGeometryMemory::SEntry> CGeometryMemory::get(const std::string& appID) const {
    const auto IT = m_entries.find(appID);
    return IT == m_entries.end() ? std::nullopt : std::optional<SEntry>{IT->second};
}

void CGeometryMemory::remember(PHLWINDOW w) {
    static auto PMODE = CConfigValue<Config::STRING>("stacking:remember_geometry");

    if (*PMODE == "none" || !w || !windowIsStacking(w) || !w->layoutTarget() || w->backend().parent() || w->backend().traits().overrideRedirect)
        return;

    const auto APPID   = w->metadata().appID();
    const auto MONITOR = w->m_monitor.lock();
    if (APPID.empty() || !MONITOR || !MONITOR->m_activeWorkspace || !MONITOR->m_activeWorkspace->space())
        return;

    load();

    // maximized / tiled windows remember the geometry they go back to
    // the window box, not the frame: decorations may already be gone while the window closes
    const auto BOX = windowStack()->restoreBox(w).value_or(w->layoutTarget()->position());
    const auto WA  = MONITOR->m_activeWorkspace->space()->workArea(true);

    m_entries[APPID] = SEntry{
        .monitor = MONITOR->m_name,
        .offset  = BOX.pos() - WA.pos(),
        .size    = BOX.size(),
    };

    save();
}

CGeometryMemory::SRestore CGeometryMemory::restoreFor(PHLWINDOW w, const CBox& workArea) {
    static auto PMODE = CConfigValue<Config::STRING>("stacking:remember_geometry");

    if (*PMODE == "none" || !w || w->backend().parent())
        return {};

    load();

    const auto ENTRY = get(w->metadata().appID());
    if (!ENTRY || ENTRY->size.x < 1 || ENTRY->size.y < 1)
        return {};

    SRestore   result{.size = ENTRY->size};

    const auto MONITOR = w->m_monitor.lock();
    if (*PMODE != "all" || !MONITOR || MONITOR->m_name != ENTRY->monitor)
        return result;

    const auto WINDOW_POS = workArea.pos() + ENTRY->offset;

    // a second instance of the app shouldn't open exactly on top of the first one
    for (const auto& other : Desktop::windowState()->windows()) {
        if (other == w || !other->mapped() || other->isHidden() || !other->layoutTarget() || other->metadata().appID() != w->metadata().appID())
            continue;

        if (other->layoutTarget()->position().pos().distance(WINDOW_POS) < 24)
            return result;
    }

    result.windowPos = WINDOW_POS;
    return result;
}
