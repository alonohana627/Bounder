add_subdirectory(game)
add_subdirectory(app)

# The executable is only main.cpp; everything else lives in the libraries.
add_executable(bounder main.cpp)
target_link_libraries(bounder PRIVATE bounder_app)
gamelib_target_warnings(bounder)
gamelib_copy_runtime(bounder)