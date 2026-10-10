# bounder_game: the rules - levels, the hero (GameLib's platformer controller
# with Bounder's walk/run tuning), blocks, power-ups, enemies, bosses, pipes,
# platforms, the overworld's paths and the demo bot. Depends only on
# GameLib::core: no window, GPU, audio or clock, deterministic for the
# per-tick input.
add_library(bounder_game STATIC
    boss.cpp
    demo_bot.cpp
    enemies.cpp
    levels.cpp
    levels_placeholder.cpp
    levels_w1.cpp
    overworld.cpp
    rules.cpp
    session.cpp)
target_include_directories(bounder_game PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bounder_game PUBLIC GameLib::core)
gamelib_target_warnings(bounder_game)