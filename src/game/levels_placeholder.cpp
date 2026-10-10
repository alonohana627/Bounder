// Temporary levels while the campaign is written: world 1's first level
// re-themed, and a short fortress with a boss arena.

#include "level_text.hpp"

#include <string>

namespace bounder::game {

namespace {

// helpers

const Segment kFortStart{
    R"(####################)",
    R"(####################)",
    R"(....................)",
    R"(....................)",
    R"(....................)",
    R"(....................)",
    R"(....................)",
    R"(....................)",
    R"(.........P..........)",
    R"(....................)",
    R"(....................)",
    R"(....................)",
    R"(...S..........s.....)",
    R"(#########~~~~#######)",
    R"(####################)",
};


std::vector<LevelDef> _world(const int world)
{
    std::vector<LevelDef> levels;
    for (int i = 1; i <= 4; ++i) {
        LevelDef def = worldOneLevels().front();
        static const std::string ids[4][4] = {{"1-1", "1-2", "1-3", "1-4"},
                                              {"2-1", "2-2", "2-3", "2-4"},
                                              {"3-1", "3-2", "3-3", "3-4"},
                                              {"4-1", "4-2", "4-3", "4-4"}};
        def.id = ids[world][i - 1];
        def.theme = worldTheme(world);
        def.areas.front().theme = worldTheme(world);
        levels.push_back(std::move(def));
    }
    static const std::string forts[4] = {"1-F", "2-F", "3-F", "4-F"};
    levels.push_back(placeholderFortress(forts[world], world));
    return levels;
}

} // namespace

LevelDef placeholderFortress(const std::string_view id, const int world)
{
    (void)world;
    return {.id = id, .name = "STONE KEEP", .theme = Theme::Fortress, .time = 300,
            .areas = {joinSegments(id, Theme::Fortress, {kFortStart, kArenaSegment})}};
}

std::vector<LevelDef> worldTwoLevels() { return _world(1); }
std::vector<LevelDef> worldThreeLevels() { return _world(2); }
std::vector<LevelDef> worldFourLevels() { return _world(3); }

LevelDef castleLevel()
{
    LevelDef def = placeholderFortress("castle", 3);
    def.name = "ROYAL CASTLE";
    def.theme = Theme::Castle;
    def.areas.front().theme = Theme::Castle;
    return def;
}

} // namespace bounder::game
