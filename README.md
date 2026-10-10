# Bounder

A side-scrolling platformer campaign built on
[GameLib](https://github.com/alonohana627/GameLib), in the spirit of Super
Mario Bros. 3 and Super Mario World: four worlds of four levels and a
fortress on overworld maps, then the final castle and its King. Walk and run
(running builds speed and lengthens jumps), stomp walkers, kick shells, ride
lifts and falling platforms, break bricks, grab power-ups from "?" blocks
and find three hidden star coins in every level.

Everything on screen is drawn by shaders (tiles with neighbour-aware grass,
"?" blocks that bump, a cartoon hero, rounded enemies that squash, spinning
coins, three parallax layers); every sound and the music are synthesized at
startup. No image or audio files.

## Controls

| Action | Keyboard | Gamepad |
|---|---|---|
| Walk | Arrows or A / D | D-pad or left stick |
| Run (hold); throw a fireball (press, with the flower) | X, Shift or J | B or X |
| Jump (hold to jump higher; hold it on a stomp to bounce higher) | Z, Space, K or C | A |
| Enter a pipe | Down or S on top of it | D-pad down |
| Overworld: walk the paths, enter a level | Arrows, then Z / Space / Enter | D-pad, then A |
| Pause | Escape or P | Start |
| Fullscreen | F11 | |
| Debug overlay (hitboxes) | F3 | |

## The campaign

1. **Grassland**: the basics (1-1 teaches them), slopes and shells, treetops
   and lifts, a garden of pipes, the Stone Keep.
2. **Dune Desert**: dunes and hoppers, a cliff climb with a long slide down,
   quicksand lifts, a mirage pass, the Sandstone Fort.
3. **Frost Peaks**: slippery ice, a cave with spike floors, a glacier climb,
   snowdrifts, the Ice Keep.
4. **Sky Realm**: cloud hops, wind bridges, a beanstalk climb to a goal in
   the sky, the Starlight Way, the Sky Keep.
5. **The Royal Castle** and the King, then the ending with the run's score,
   levels cleared and star coins.

Small hero, big hero (one extra hit), fire hero (bouncing fireballs) and the
star (invincible for ten seconds). 100 coins give a life; every level has a
timer that speeds up the music when it runs low, a checkpoint flag and a goal
pole (touch it high for more points) or a boss. A game over sends you back to
the map with fresh lives; the cleared levels stay.

`bounder.ini` in the user data folder (`%APPDATA%\Bounder` on Windows;
`BOUNDER_DATA_DIR` moves it) keeps the settings, the cleared levels, the
star coins found and the best score per level, where you stand on the map,
the run in progress and the best score of a whole run.

## Build and run

Requirements are GameLib's (C++26 Clang, CMake 3.28+, Ninja, Vulkan 1.3,
glslc). CMake downloads GameLib at the commit pinned in `CMakeLists.txt`.

```powershell
cmake --preset debug
cmake --build --preset debug
build/debug/bin/bounder.exe          # --demo: the demo bot plays world 1 full screen
ctest --preset debug                 # smoke run with the validation layer
```
