# Desktop build of the card faces: renders frames and checks that text fits,
# with the same drawing code the watch runs. Needs SDL2 headers (brew install sdl2).
# tools/import_decks.py builds and runs it; by hand:
#
#   cmake -S sim -B sim/build && cmake --build sim/build -j
#   sim/build/card_sim render packs/a2-start sim/out
cmake_minimum_required(VERSION 3.16)
project(card_sim C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(GFX ${ROOT}/managed_components/m5stack__m5gfx)
file(GLOB_RECURSE GFX_SRC ${GFX}/src/*.cpp ${GFX}/src/*.c)

find_package(SDL2 REQUIRED)

add_executable(card_sim sim_main.cpp
    ${ROOT}/main/character.cpp ${ROOT}/main/knoedel.cpp ${ROOT}/main/knoedel_bubbles.cpp ${ROOT}/main/katze.cpp ${ROOT}/main/dinge.cpp ${ROOT}/main/scenery.cpp ${ROOT}/main/card_ui.cpp ${ROOT}/main/deck.cpp ${ROOT}/main/progress.cpp ${GFX_SRC})
target_include_directories(card_sim PRIVATE ${GFX}/src ${ROOT}/main ${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS}/..)
target_compile_definitions(card_sim PRIVATE LGFX_SDL)
target_compile_options(card_sim PRIVATE -w)
target_link_libraries(card_sim PRIVATE SDL2::SDL2)
