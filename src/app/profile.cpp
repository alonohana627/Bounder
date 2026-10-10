#include "profile.hpp"

#include <gamelib/app/campaign_file.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace bounder::app {

namespace {

// helpers

constexpr std::string_view kAppName = "Bounder";
constexpr std::string_view kDataDirVariable = "BOUNDER_DATA_DIR";

int _readInt(const gamelib::storage::IniFile& ini, const std::string_view key, const int fallback, const int low,
             const int high)
{
    if (const auto text = ini.get("run", key)) {
        if (const auto value = gamelib::storage::parseInt(*text)) {
            return static_cast<int>(std::clamp<long long>(*value, low, high));
        }
    }
    return fallback;
}

} // namespace

std::string_view levelId(const int level)
{
    return game::level(level).id;
}

gamelib::core::Campaign makeCampaign()
{
    std::vector<std::string> ids;
    for (int i = 0; i < game::kLevelCount; ++i) {
        ids.emplace_back(levelId(i));
    }
    gamelib::core::Campaign campaign{std::move(ids)};
    campaign.requireInOrder();
    return campaign;
}

void Progress::read(const gamelib::storage::IniFile& ini)
{
    gamelib::app::readCampaign(ini, campaign);
    world = _readInt(ini, "world", 1, 1, game::kWorlds) - 1;
    node = _readInt(ini, "node", 0, 0, 16);
    run.lives = _readInt(ini, "lives", game::kStartLives, 1, game::kMaxLives);
    run.coins = _readInt(ini, "coins", 0, 0, game::kCoinsPerLife - 1);
    run.power = static_cast<game::Power>(_readInt(ini, "power", 0, 0, 2));
    if (const auto text = ini.get("run", "score")) {
        if (const auto value = gamelib::storage::parseInt(*text)) {
            run.score = std::max<long long>(0, *value);
        }
    }
    if (const auto text = ini.get("run", "finished")) {
        finished = gamelib::storage::parseBool(*text).value_or(false);
    }
}

void Progress::write(gamelib::storage::IniFile& ini) const
{
    ini.set("run", "world", std::to_string(world + 1));
    ini.set("run", "node", std::to_string(node));
    ini.set("run", "lives", std::to_string(run.lives));
    ini.set("run", "coins", std::to_string(run.coins));
    ini.set("run", "score", std::to_string(run.score));
    ini.set("run", "power", std::to_string(static_cast<int>(run.power)));
    ini.set("run", "finished", finished ? "true" : "false");
    gamelib::app::writeCampaign(campaign, ini);
}

ProfileFile openProfile()
{
    return gamelib::app::openArcadeProfile<Progress>(kAppName, kDataDirVariable);
}

int starCoinsFound(const gamelib::core::Campaign& campaign)
{
    return campaign.foundCount();
}

} // namespace bounder::app
