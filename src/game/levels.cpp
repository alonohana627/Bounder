#include "levels.hpp"

#include "level_text.hpp"

#include <stdexcept>
#include <string>

namespace bounder::game {

const Segment kGoalSegment{
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(......XX......................)",
    R"(.....XXX......................)",
    R"(....XXXX......................)",
    R"(...XXXXX......................)",
    R"(..XXXXXX...G..................)",
    R"(##############################)",
    R"(##############################)",
};

const Segment kGoalTallSegment{
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(..............................)",
    R"(......XX......................)",
    R"(.....XXX......................)",
    R"(....XXXX......................)",
    R"(...XXXXX......................)",
    R"(..XXXXXX...G..................)",
    R"(##############################)",
    R"(##############################)",
};

const Segment kArenaSegment{
    R"(##############################)",
    R"(##############################)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z.....XX..............XX.....)",
    R"(.z............................)",
    R"(.z............................)",
    R"(.z......................Z.....)",
    R"(##############################)",
    R"(##############################)",
};

AreaDef joinSegments(const std::string_view levelId, const Theme theme,
                     const std::vector<std::vector<std::string_view>>& segments)
{
    AreaDef area{.theme = theme, .rows = {}};
    for (std::size_t s = 0; s < segments.size(); ++s) {
        const std::vector<std::string_view>& segment = segments[s];
        const auto fail = [&](const std::string& what) {
            throw std::logic_error{"Bounder level " + std::string{levelId} + ", segment " + std::to_string(s) + ": " + what};
        };
        if (segment.empty()) {
            fail("no rows");
        }
        if (area.rows.empty()) {
            area.rows.resize(segment.size());
        } else if (segment.size() != area.rows.size()) {
            fail(std::to_string(segment.size()) + " rows, expected " + std::to_string(area.rows.size()));
        }
        for (std::size_t r = 0; r < segment.size(); ++r) {
            if (segment[r].size() != segment.front().size()) {
                fail("row " + std::to_string(r) + " is " + std::to_string(segment[r].size()) + " wide, expected "
                     + std::to_string(segment.front().size()));
            }
            for (const char c : segment[r]) {
                area.rows[r] += c == '.' ? ' ' : c;
            }
        }
    }
    return area;
}

std::vector<LevelDef> buildLevels()
{
    std::vector<LevelDef> all;
    for (std::vector<LevelDef> world : {worldOneLevels(), worldTwoLevels(), worldThreeLevels(), worldFourLevels()}) {
        if (world.size() != static_cast<std::size_t>(kLevelsPerWorld)) {
            throw std::logic_error{"Bounder: a world needs 4 levels and a fortress"};
        }
        for (LevelDef& def : world) {
            all.push_back(std::move(def));
        }
    }
    all.push_back(castleLevel());
    return all;
}

const std::vector<LevelDef>& levels()
{
    static const std::vector<LevelDef> all = buildLevels();
    return all;
}

const LevelDef& level(const int index)
{
    return levels().at(static_cast<std::size_t>(index));
}

std::string_view worldName(const int world)
{
    switch (world) {
    case 0: return "GRASSLAND";
    case 1: return "DUNE DESERT";
    case 2: return "FROST PEAKS";
    case 3: return "SKY REALM";
    default: return "ROYAL CASTLE";
    }
}

Theme worldTheme(const int world)
{
    switch (world) {
    case 0: return Theme::Grass;
    case 1: return Theme::Desert;
    case 2: return Theme::Ice;
    default: return Theme::Sky;
    }
}

} // namespace bounder::game
