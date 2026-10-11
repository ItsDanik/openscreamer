cmake_minimum_required(VERSION 3.16)
project(openscreamer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -O2)

# SDL2 rather than SDL3: the MiSTer ARM toolchain and the hybrid-core
# scaffolding are built around SDL2.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

add_executable(openscreamer
  src/main.c
  src/mesh.c
  src/unr.c
  src/scene.c
  src/drive.c
  src/sound.c
  src/music.c
  src/port.c
  src/controls.c
)

# the version (../VERSION + commits since, see tools/version.sh), looked up at every build
add_custom_target(os_version
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/os_version.h
          -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/os_version.h)
add_dependencies(openscreamer os_version)

target_include_directories(openscreamer PRIVATE ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
target_link_directories(openscreamer PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(openscreamer PRIVATE ${SDL2_LIBRARIES} m)
