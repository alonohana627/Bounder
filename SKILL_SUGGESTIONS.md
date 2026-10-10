# Skill suggestions

Skills that would have made work on this project go better. See `AGENTS.md` for
how this file is maintained.

## Already installed

In `~/.claude/skills/` (checked 2026-10-11):

- action-combat-frame-data
- action-rpg-progression-design
- arcade-action-physics
- articulated-2d-character-rigs
- chiptune-music-sequencing
- chiptune-sfx-synthesis
- ci-cross-platform
- cmake-build-developer
- cmake-library-consumption
- computer-graphics-developer
- cozy-sim-design
- cpp-debugging-profiling
- cpp-performance-developer
- cpp-testing
- cpp26-developer
- deterministic-replay-testing
- game-audio-developer
- game-engine-developer
- game-feel-designer
- game-library-extraction
- game-persistence
- game-ui-text-and-dialogue
- game-visual-design
- glfw-platform-developer
- glsl-shader-build-embedding
- grid-algorithms-for-games
- headless-balance-sweeps
- input-rebinding-ui
- learning-mentor
- logic-puzzle-demo-players
- metroidvania-world-design
- platformer-controller-feel
- platformer-demo-bots
- platformer-level-design
- pong-arcade-rules
- procedural-dungeon-generation
- procedural-landscape-shaders
- puzzle-game-design-undo
- retro-crt-screen-shader
- sdf-2d-shape-shaders
- shader-developer
- software-architecture-developer
- stroke-font-design
- technical-docs-writer
- tilemap-and-camera-2d
- vector-arcade-neon-look
- vulkan-app-smoke-testing
- vulkan-developer
- vulkan-offscreen-postprocess
- vulkan-text-and-ui
- win32-game-window-automation
- window-screenshot-bursts
- windows-release-packaging

## Suggested

### text-level-authoring-tools

Authoring large tile levels as text without losing alignment: splitting a
level into fixed-height segments joined at load (with a loader that names
the level, segment and row whose width is wrong), a legend kept next to the
data, markers for entities, rails and pipe pairs instead of hand-typed
coordinates, scripts that rewrite one segment from a list of rows and check
every row's width before saving, printing a level's reachability map from a
flood fill (support-aware: air counts only above something to stand on) to
find gaps the controller cannot cross, and a "start here" developer switch
(level and column) plus a replay switch for screenshots of any part of any
level.

*Why:* Bounder's 21 levels (about 3 000 columns of text) were written by
hand; most errors were rows one character too long, coordinates that moved
when a segment changed, and gaps or ledges a few units out of the
controller's reach, each found only by a crash at load, a failed search or a
screenshot.

### platformer-boss-design

Designing and verifying 2D platformer bosses on a fixed tick: an arena with
a closing gate and a locked camera, a health budget split between stomps and
projectiles, a state machine per boss (idle, approach, hop or leap at the
player, throw fans of arcing shots, stunned after a hit with invincibility
long enough to get clear), phases by health that add speed and shots
instead of new rules, telegraphs measured in ticks, fair contact rules
(touching a stunned boss is safe), a death sequence that hands control back,
and proving each fight winnable with a search that scores the boss's
remaining health.

*Why:* the Brute and the King were designed from scratch; a beam search only
beat the world 4 Brute once its score counted the boss's health and the
search started in the arena, and the hero first froze mid-air when the King
fell.
