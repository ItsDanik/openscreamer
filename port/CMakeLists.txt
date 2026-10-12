cmake_minimum_required(VERSION 3.16)
project(openscreamer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# -fno-math-errno: sqrtf() and the like as the processor's own instruction
add_compile_options(-Wall -Wextra -O2 -fno-math-errno)

# SDL2 rather than SDL3: the MiSTer ARM toolchain and the hybrid-core
# scaffolding are built around SDL2.
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(OS_SOURCES
  src/main.c
  src/mesh.c
  src/unr.c
  src/scene.c
  src/drive.c
  src/sound.c
  src/music.c
  src/fmchip.c
  src/nuked/opl3.c
  src/port.c
  src/controls.c
)

# MISTER (branch mister): the hybrid core.  SDL is the framework's, linked
# statically with its "mister" drivers.  The port puts its own statics back
# between the parts of the program (port.c), which must not touch a library
# linked into the same program: so the port's objects are joined into one and
# their .data and .bss renamed, and port.c saves those two sections only.
option(MISTER_HYBRID "Build for the MiSTer hybrid core (needs SDL2 with the MiSTer drivers)" OFF)
if(MISTER_HYBRID)
  add_library(os_objects OBJECT ${OS_SOURCES} src/mister.c)
  target_compile_definitions(os_objects PRIVATE MISTER_HYBRID=1 OS_OWN_SECTIONS=1)
  target_include_directories(os_objects PRIVATE ${SDL2_STATIC_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_options(os_objects PRIVATE ${SDL2_STATIC_CFLAGS_OTHER} -fno-common)
  # the MiSTer's ARM: its own instruction set (not Thumb), and arithmetic
  # without the checks nothing here relies on -- one picture in twenty
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    target_compile_options(os_objects PRIVATE -O3 -marm -ffast-math)
  endif()
  add_custom_command(OUTPUT os_own.o
    COMMAND ${CMAKE_LINKER} -r -o os_all.o $<TARGET_OBJECTS:os_objects>
    COMMAND ${CMAKE_OBJCOPY} --rename-section .data=osdata --rename-section .data.rel.local=osdata --rename-section .data.rel=osdata --rename-section .bss=osbss os_all.o os_own.o
    DEPENDS os_objects $<TARGET_OBJECTS:os_objects> COMMAND_EXPAND_LISTS VERBATIM)
  add_executable(openscreamer ${CMAKE_CURRENT_BINARY_DIR}/os_own.o)
  set_target_properties(openscreamer PROPERTIES LINKER_LANGUAGE C)
  set(OS_VERSION_FOR os_objects)
  set(SDL2_LIBRARIES ${SDL2_STATIC_LDFLAGS})
  set(SDL2_LIBRARY_DIRS ${SDL2_STATIC_LIBRARY_DIRS})
else()
  add_executable(openscreamer ${OS_SOURCES})
  set(OS_VERSION_FOR openscreamer)
endif()

# the version (../VERSION + commits since, see tools/version.sh), looked up at every build
add_custom_target(os_version
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DOUT=${CMAKE_CURRENT_BINARY_DIR}/os_version.h
          -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
  BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/os_version.h)
add_dependencies(${OS_VERSION_FOR} os_version)

if(NOT MISTER_HYBRID)
  target_include_directories(openscreamer PRIVATE ${SDL2_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})
endif()
target_link_directories(openscreamer PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(openscreamer PRIVATE ${SDL2_LIBRARIES} m)
if(WIN32)
  # WINDOWS (MinGW-w64, tools/release_windows.sh): no console window, and
  # nothing of the compiler's own to ship beside SDL2.dll.  SDL2 must stay a
  # DLL: linked in, its statics would be among those port.c puts back
  target_link_options(openscreamer PRIVATE -mwindows -static-libgcc)
  target_compile_options(openscreamer PRIVATE ${SDL2_CFLAGS_OTHER})
endif()
