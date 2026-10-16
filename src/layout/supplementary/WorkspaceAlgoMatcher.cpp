#include "WorkspaceAlgoMatcher.hpp"

#include "../../config/ConfigValue.hpp"
#include "../../config/shared/workspace/WorkspaceRuleManager.hpp"

#include "../algorithm/Algorithm.hpp"
#include "../algorithm/TiledAlgorithm.hpp"
#include "../space/Space.hpp"

#include "../algorithm/floating/default/DefaultFloatingAlgorithm.hpp"
#include "../algorithm/tiled/dwindle/DwindleAlgorithm.hpp"
#include "../algorithm/tiled/master/MasterAlgorithm.hpp"
#include "../algorithm/tiled/scrolling/ScrollingAlgorithm.hpp"
#include "../algorithm/tiled/monocle/MonocleAlgorithm.hpp"

#include "../../state/WorkspaceState.hpp"

#include "../../stack/StackMode.hpp"
#include "../../stack/layout/StackingFloatingAlgorithm.hpp"
#include "../../stack/layout/StackingTiledAlgorithm.hpp"

using namespace Layout;
using namespace Layout::Supplementary;

constexpr const char*            DEFAULT_FLOATING_ALGO = "default";
constexpr const char*            DEFAULT_TILED_ALGO    = "dwindle";

const UP<CWorkspaceAlgoMatcher>& Supplementary::algoMatcher() {
    static UP<CWorkspaceAlgoMatcher> m = makeUnique<CWorkspaceAlgoMatcher>();
    return m;
}

CWorkspaceAlgoMatcher::CWorkspaceAlgoMatcher() {
    m_tiledAlgos = {
        {"dwindle", [] { return makeUnique<Tiled::CDwindleAlgorithm>(); }},
        {"master", [] { return makeUnique<Tiled::CMasterAlgorithm>(); }},
        {"scrolling", [] { return makeUnique<Tiled::CScrollingAlgorithm>(); }},
        {"monocle", [] { return makeUnique<Tiled::CMonocleAlgorithm>(); }},
        {Stack::LAYOUT_NAME, [] { return makeUnique<Tiled::CStackingTiledAlgorithm>(); }},
    };

    m_floatingAlgos = {
        {"default", [] { return makeUnique<Floating::CDefaultFloatingAlgorithm>(); }},
        {Stack::LAYOUT_NAME, [] { return makeUnique<Floating::CStackingFloatingAlgorithm>(); }},
    };

    m_algoNames = {
        {&typeid(Tiled::CDwindleAlgorithm), "dwindle"},
        {&typeid(Tiled::CMasterAlgorithm), "master"},
        {&typeid(Tiled::CScrollingAlgorithm), "scrolling"},
        {&typeid(Tiled::CMonocleAlgorithm), "monocle"},
        {&typeid(Floating::CDefaultFloatingAlgorithm), "default"},
        {&typeid(Tiled::CStackingTiledAlgorithm), Stack::LAYOUT_NAME},
        {&typeid(Floating::CStackingFloatingAlgorithm), Stack::LAYOUT_NAME},
    };
}

bool CWorkspaceAlgoMatcher::registerTiledAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<ITiledAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    m_tiledAlgos.emplace(name, std::move(factory));
    m_algoNames.emplace(typeInfo, name);

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::registerFloatingAlgo(const std::string& name, const std::type_info* typeInfo, std::function<UP<IFloatingAlgorithm>()>&& factory) {
    if (m_tiledAlgos.contains(name) || m_floatingAlgos.contains(name))
        return false;

    m_floatingAlgos.emplace(name, std::move(factory));
    m_algoNames.emplace(typeInfo, name);

    updateWorkspaceLayouts();

    return true;
}

bool CWorkspaceAlgoMatcher::unregisterAlgo(const std::string& name) {
    if (!m_tiledAlgos.contains(name) && !m_floatingAlgos.contains(name))
        return false;

    std::erase_if(m_algoNames, [&name](const auto& e) { return e.second == name; });

    if (m_floatingAlgos.contains(name))
        m_floatingAlgos.erase(name);
    else
        m_tiledAlgos.erase(name);

    // this is needed here to avoid situations where a plugin unloads and we still have a UP
    // to a plugin layout
    updateWorkspaceLayouts();

    return true;
}

UP<ITiledAlgorithm> CWorkspaceAlgoMatcher::algoForNameTiled(const std::string& s) {
    if (m_tiledAlgos.contains(s))
        return m_tiledAlgos.at(s)();
    return m_tiledAlgos.at(DEFAULT_TILED_ALGO)();
}

UP<IFloatingAlgorithm> CWorkspaceAlgoMatcher::algoForNameFloat(const std::string& s) {
    if (m_floatingAlgos.contains(s))
        return m_floatingAlgos.at(s)();
    return m_floatingAlgos.at(DEFAULT_FLOATING_ALGO)();
}

std::string CWorkspaceAlgoMatcher::tiledAlgoForWorkspace(const PHLWORKSPACE& w) {
    static auto PLAYOUT = CConfigValue<Config::STRING>("general:layout");

    auto        rule = Config::workspaceRuleMgr()->getWorkspaceRuleFor(w);
    return rule && rule->m_layout.has_value() ? rule->m_layout.value() : *PLAYOUT;
}

std::string CWorkspaceAlgoMatcher::floatingAlgoForTiled(const std::string& tiled) {
    return tiled == Stack::LAYOUT_NAME ? Stack::LAYOUT_NAME : DEFAULT_FLOATING_ALGO;
}

SP<CAlgorithm> CWorkspaceAlgoMatcher::createAlgorithmForWorkspace(PHLWORKSPACE w) {
    const auto TILED = tiledAlgoForWorkspace(w);
    return CAlgorithm::create(algoForNameTiled(TILED), algoForNameFloat(floatingAlgoForTiled(TILED)), w->space());
}

void CWorkspaceAlgoMatcher::updateWorkspaceLayouts() {
    const auto WORKSPACES = State::Workspace::state()->workspacesCopy();

    // TODO: make this ID-based, string comparison is slow
    for (const auto& WORKSPACE : WORKSPACES) {
        // Workspaces can briefly outlive usable layout state while monitor
        // changes move or destroy them.
        if (!WORKSPACE || !WORKSPACE->m_monitor || !WORKSPACE->space())
            continue;

        const auto& SPACE     = WORKSPACE->space();
        const auto  ALGORITHM = SPACE->algorithm();
        if (!ALGORITHM)
            continue;

        const auto& TILED_ALGO = ALGORITHM->tiledAlgo();

        if (!TILED_ALGO)
            continue;

        const auto LAYOUT_TO_USE = tiledAlgoForWorkspace(WORKSPACE);

        // Hyprstack: stacking workspaces use their own floating algorithm
        const auto FLOAT_TO_USE = floatingAlgoForTiled(LAYOUT_TO_USE);
        if (const auto& FLOAT_ALGO = ALGORITHM->floatingAlgo(); FLOAT_ALGO && getNameForTiledAlgo(&typeid(*FLOAT_ALGO)) != FLOAT_TO_USE)
            ALGORITHM->updateFloatingAlgo(algoForNameFloat(FLOAT_TO_USE));

        const auto CURRENT_LAYOUT = getNameForTiledAlgo(TILED_ALGO.get());

        if (CURRENT_LAYOUT == LAYOUT_TO_USE && m_tiledAlgos.contains(LAYOUT_TO_USE))
            continue;

        // needs a switchup
        ALGORITHM->updateTiledAlgo(algoForNameTiled(LAYOUT_TO_USE));
    }
}

std::string CWorkspaceAlgoMatcher::getNameForTiledAlgo(const ITiledAlgorithm* algo) {
    if (!algo)
        return "unknown";

    if (const auto name = algo->layoutName(); name.has_value())
        return *name;

    return getNameForTiledAlgo(&typeid(*algo));
}

std::string CWorkspaceAlgoMatcher::getNameForTiledAlgo(const std::type_info* type) {
    if (m_algoNames.contains(type))
        return m_algoNames.at(type);
    return "unknown";
}
