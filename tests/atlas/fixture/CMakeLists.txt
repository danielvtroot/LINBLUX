cmake_minimum_required(VERSION 3.16)
project(atlas_fixture LANGUAGES CXX)

add_library(fixture_core STATIC)
target_sources(fixture_core PRIVATE src/core/Widget.cpp)
target_include_directories(fixture_core PUBLIC src)

add_executable(fixture_app src/app/Main.cpp src/app/Service.cpp)
target_link_libraries(fixture_app PRIVATE fixture_core)
add_test(NAME fixture_app_smoke COMMAND fixture_app)

add_executable(fixture_tests tests/widget_test.cpp)
target_link_libraries(fixture_tests PRIVATE fixture_core)
add_test(NAME widget_test COMMAND fixture_tests)
