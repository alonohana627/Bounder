#pragma once

// What Bounder keeps between sessions, in bounder.ini in the user data
// folder (BOUNDER_DATA_DIR moves it): GameLib's standard settings, the
// campaign (levels cleared, the star coins found and the best score per
// level, as core::Campaign records), where the hero stands on the
// overworld, the run's lives, coins, score and power (so quitting from the
// map continues the run), and the best score of a whole run.

#include "session.hpp"

#include <gamelib/app/arcade_profile.hpp>
#include <gamelib/core/campaign.hpp>
#include <gamelib/storage/ini.hpp>

#include <string_view>

namespace bounder::app {

[[nodiscard]] gamelib::core::Campaign makeCampaign();
[[nodiscard]] std::string_view levelId(int level);

struct Progress {
    gamelib::core::Campaign campaign = makeCampaign();
    int world = 0; // the overworld shown (0-based)
    int node = 0;  // the hero's node on it
    game::RunState run;
    bool finished = false; // the castle was beaten once

    void read(const gamelib::storage::IniFile& ini);
    void write(gamelib::storage::IniFile& ini) const;
    friend bool operator==(const Progress&, const Progress&) = default;
};

using ProfileFile = gamelib::app::ArcadeProfileFileOf<Progress>;

[[nodiscard]] ProfileFile openProfile();

// Star coins found over the whole campaign, and how many there are.
[[nodiscard]] int starCoinsFound(const gamelib::core::Campaign& campaign);
inline constexpr int kStarCoinsTotal = game::kLevelCount * 3;

} // namespace bounder::app
