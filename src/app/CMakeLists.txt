# bounder_app: everything around the rules - the GameLib main-loop glue, the
# profile file, the level and map views and their shaders, sounds, music and
# effects. Menus, settings, sound banks, transitions, effects and the attract
# mode come from GameLib (app, ui, audio, fx, ai).
add_library(bounder_app STATIC
    bounder_game.cpp
    effects.cpp
    map_view.cpp
    palette.cpp
    profile.cpp
    sounds.cpp
    view.cpp)
target_include_directories(bounder_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(bounder_app PUBLIC bounder_game GameLib::app GameLib::ui GameLib::fx GameLib::ai)
gamelib_target_warnings(bounder_app)
# Bounder's own procedural shapes, embedded as SPIR-V (see view.hpp, Shapes).
gamelib_add_shaders(bounder_app SHADERS
    ${PROJECT_SOURCE_DIR}/shaders/bushes.frag
    ${PROJECT_SOURCE_DIR}/shaders/enemy.frag
    ${PROJECT_SOURCE_DIR}/shaders/far.frag
    ${PROJECT_SOURCE_DIR}/shaders/hero.frag
    ${PROJECT_SOURCE_DIR}/shaders/item.frag
    ${PROJECT_SOURCE_DIR}/shaders/map.frag
    ${PROJECT_SOURCE_DIR}/shaders/scenery.frag
    ${PROJECT_SOURCE_DIR}/shaders/sky.frag
    ${PROJECT_SOURCE_DIR}/shaders/tile.frag)