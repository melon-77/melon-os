cmake_minimum_required(VERSION 3.16)
project(melon-pinball VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(PINBALL_TESTS "Build the headless physics test" OFF)
set(PINBALL_DATADIR "${CMAKE_INSTALL_PREFIX}/share/melon-pinball" CACHE STRING "Where the game's data files live")

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)

add_library(pinball-physics STATIC physics.cpp table.cpp)
target_include_directories(pinball-physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(melon-pinball main.cpp game.cpp render.cpp art.cpp raster.cpp text.cpp audio.cpp)
target_link_libraries(melon-pinball PRIVATE pinball-physics SDL3::SDL3 SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image)
target_compile_definitions(melon-pinball PRIVATE PINBALL_DATADIR="${PINBALL_DATADIR}"
                           PINBALL_VERSION="${PROJECT_VERSION}")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(melon-pinball PRIVATE -Wall -Wextra)
endif()
install(TARGETS melon-pinball RUNTIME DESTINATION bin)

if(PINBALL_TESTS)
  add_executable(melon-pinball-physics-test ../tests/physics_test.cpp)
  target_link_libraries(melon-pinball-physics-test PRIVATE pinball-physics)
endif()
