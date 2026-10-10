# Features

What Bounder does today, verified by running it. Updated with each feature.

## The campaign

- 21 levels: four worlds (GRASSLAND, DUNE DESERT, FROST PEAKS, SKY REALM)
  of four levels and a fortress, then the ROYAL CASTLE. Levels are text
  segments joined left to right (`src/game/levels_*.cpp`) and read with
  GameLib's `core::TileMap::fromText`; up to 202 tiles wide; 2-2, 3-3 and 4-3
  are 30 rows tall with vertical climbs (4-3 ends in the sky).
- Bonus rooms behind pipes (hold down on an enterable pipe; the hero sinks
  in and rises out of the room's pipe, and back again).
- A checkpoint flag in every level (it also turns the small hero big) and a
  goal pole (more points the higher it is touched; the hero slides down and
  walks into the castle) or, in fortresses and the castle, a boss arena whose
  gate closes behind the hero and locks the camera.
- Every level's goal and every star coin proven reachable by a headless
  beam search over the real rules (outside the repository).

## Overworld

- One map per world (`src/game/overworld.cpp`): nodes for the levels, the
  fortress, the castle and the gates between worlds, joined by paths. Paths
  open as levels are cleared (GameLib's `floodFill` from the start; uncleared
  nodes stop it); the hero walks node to node along the paths; jump enters
  the level or takes a gate.
- Cleared levels are flagged, closed paths faint, and the star coins found
  show as dots under each node; the panel names the node and its best score.

## Movement (GameLib's platformer controller, Bounder's tuning, 60 ticks a second)

- Walk and run (hold the run button): speed builds over half a second; a
  turn skids; the jump rises 4.25 tiles standing and up to 5.25 at full
  running speed.
- Variable jump height, coyote time and a jump buffer (6 ticks each), corner
  correction, a gentler apex while jump is held.
- 45- and 22.5-degree slopes (running downhill is 30 % faster), one-way
  platforms jumped up through, moving lifts on rails (horizontal and
  vertical) and falling platforms that carry the hero.
- Ice in world 3: slow to stop and to turn round (lifts are not icy).
- Keyboard and gamepad.

## Blocks, items and the run

- "?" blocks (a coin, a power-up, a star) that bump when hit; bricks that
  break under a big hero and bump under a small one; multi-coin bricks;
  hidden blocks that appear when hit from below (a coin or a 1-up); used
  blocks.
- Power-ups: the mushroom (big: one extra hit), the fire flower (bouncing
  fireballs, two at a time) and the star (ten seconds of invincibility that
  knocks enemies away, with its own music). A hit shrinks the hero with a
  moment of flicker.
- Coins (100 give a life), lives, score with popups, a level timer (in
  units of 0.4 s; the music speeds up below 100 and time out costs a life),
  a time bonus at the goal.
- Three star coins per level (on high routes, behind hidden blocks, in bonus
  rooms, in brick vaults); found ones show as ghosts on later visits.

## Enemies and bosses

- Walkers (turn at walls), shell walkers (green walk off ledges, red turn at
  them) whose shells can be kicked to slide and bounce between walls,
  knocking out enemies and breaking bricks; hoppers that jump at the hero;
  fliers on sine paths (across or up and down); plants that rise from pipes
  (never while the hero stands beside the pipe); spikies that cannot be
  stomped. Enemies wake when the hero comes near.
- Stomping (with a bounce, higher while jump is held; chains of stomps
  score more, up to lives), fireballs, sliding shells, the star and bumped
  blocks defeat them; side contact hurts.
- The Brute in every fortress (walks at the hero and hops, faster in later
  worlds; three stomps or six fireballs) and the King in the castle (walks,
  leaps at the hero, throws fans of fireballs, more of them as he weakens;
  four stomps or twelve fireballs), with a health bar.

## Screens

- Title (GameLib's `ArcadeMenus`) with the demo bot playing world 1 behind
  it; `--demo` shows the demo full screen.
- A card with the level's name and the lives left before each attempt
  (an iris from the map, a fade after a death).
- Pause (resume, settings, restart the level, exit to the map at no cost,
  main menu, quit), the game over menu (back to the map with fresh lives;
  progress kept) and, after the King, the ending with fireworks, the run's
  score, levels cleared, star coins and the best score.
- Teaching lines written into 1-1's sky.

## Look

- A bright cartoon palette per theme (GameLib `core::Palette`): grass,
  sand, snow, cloud, fortress and castle stone, and the blue bonus rooms.
- One tile shader: neighbour-aware ground with grass, sand, snow or cloud
  caps, slopes with the same caps, bevelled hard blocks, bricks, shining "?"
  blocks, pipes, one-way planks (clouds in the sky), coins that spin by
  scaling, spikes, bubbling lava and dotted rails.
- Parallax: a sky (sun, snowfall in world 3, stars high up, stone halls with
  lit windows indoors), far hills from noise, clouds and near bushes (cacti,
  snowy pines, cloud puffs, pillars by theme).
- A cartoon hero with a walk cycle, skid, jump, throw and death poses, squash
  and stretch; rounded outlined enemies that squash when stomped and flip
  when knocked out; mushrooms, flowers, stars and fireballs; goal pole,
  checkpoint flag and castle.
- Juice: dust, brick shards, coins popping from blocks, rings and sparks,
  score popups, a flash and screen shake (switchable), hit stop on stomps and
  boss hits.

## Sound

- Synthesized effects: jump (small and big), stomp (rising with the chain),
  kick, coin, sprout, power-up, 1-up, bump, brick, pipe, shrink, fireball,
  skid, checkpoint, goal pole, course clear, death, hurry-up, star coin, boss
  hit, boss defeated, boss landing, platform drop, map and menu sounds, game
  over.
- Music: a theme per world, fortress, bonus room, map, boss, star, title and
  ending; the level themes have a faster take when the timer runs low.

## Saves and settings

- `bounder.ini` (GameLib `ArcadeProfile` plus Bounder's keys): sound, music,
  screen shake, fullscreen (F11), vsync; the campaign (cleared levels, star
  coins and best score per level); the map position; the run in progress
  (lives, coins, score, power); the best run score.
