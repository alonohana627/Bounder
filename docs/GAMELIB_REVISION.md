# GameLib revision

Bounder and GameLib are separate repositories, and commits across
repositories are not atomic. This file records which GameLib revision each
Bounder change was validated against (build with zero warnings, smoke run
with the validation layer). `CMakeLists.txt` pins GameLib by full commit hash
(`GIT_TAG`, `GIT_SHALLOW FALSE`), so checking out a Bounder commit reproduces
its combination by itself; CMake prints the revision it builds against
(`Bounder: using GameLib ...`).

| Bounder change | GameLib revision | Validated |
|---|---|---|
| Project from the new-game template (`build`) | `01232cd` (0.0.7 plus docs) | clean debug build fetching the commit, zero warnings, 1/1 smoke run |
| The rules, the 21 levels, the overworld, the demo bot, the shaders, menus, sound and music, docs (`feat(game)`, `feat(app)`, `feat`, `docs`) | `01232cd` | debug builds with zero compiler and shader warnings; 1/1 smoke run; `--demo` self-tests with zero validation messages; every level's goal and star coins proven by a headless search driver; the castle and the ending played from a recorded route |
| Re-pin: blocks hit from below, walking enemies, items and bosses, the shot tile test, bumped-tile offsets, rail and falling platforms, warp pipes from GameLib (`build`) | `043bb17` | debug build with zero warnings; 1/1 smoke run; the headless driver gives the same goal, star coin and demo bot results as on `01232cd` (tick counts within a few ticks where enemies turn at walls); pipes, lifts and block bumps checked on screen |
| Re-pin: dust puffs and rings, banners, score popups, the music bank, dev start switches, EXIT TO MAP as a pause action, the demo bot on `ai::bestPlan`, the overworld on `core::NodeMap`, hints with `drawWorldText`, `--keep-running`, the card faded with the cover (`build`, `feat`) | `3a5f6fc` | debug build with zero warnings; 1/1 smoke run; demo bot results on all 21 levels identical to `043bb17`; every overworld's open cells and routes identical to `01232cd` for six clear states; title, map, card, pause and exit to map checked on screen |
