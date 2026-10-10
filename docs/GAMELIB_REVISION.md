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
