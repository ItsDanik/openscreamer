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
  src/cdaudio.c
  src/third_party/decoders.c
  src/music.c
  src/fmchip.c
  src/nuked/opl3.c
  src/port.c
  src/controls.c
  src/s2_hooks.c
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

# the decoders of the CD's songs as files (src/third_party: stb_vorbis, dr_mp3, dr_wav) are
# others' code, taken as it is: their warnings are not ours to mend
set_source_files_properties(src/third_party/decoders.c PROPERTIES COMPILE_OPTIONS "-w")
# Screamer 2's race and menu routines: the game's own, written by hand (src/s2r_car.h, docs/NOTES2.md
# S2-8), the sums done on the x87 as the game's are.  S2_REFERENCE (the developer's): instead the
# routines converted to C from the game's program (src/s2_gen.inc, made by tools/s2c.py -- NOT in
# the repository, never published), the reference the hand-written ones are checked against.
option(S2_REFERENCE "Screamer 2 from the converted routines (src/s2_gen.inc, the developer's reference)" OFF)
target_compile_definitions(openscreamer PRIVATE S2_CONVERTED)
if(S2_REFERENCE)
  if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/s2_gen.inc")
    message(FATAL_ERROR "S2_REFERENCE needs src/s2_gen.inc (tools/s2c.py)")
  endif()
  message(STATUS "Screamer 2: the reference -- the routines converted to C (src/s2_gen.inc)")
  target_compile_definitions(openscreamer PRIVATE S2_REFERENCE)
  target_sources(openscreamer PRIVATE src/s2_exe.c)   # (the menu's data out of the player's STARTL.EXE: the reference's alone)
endif()
# the Linux release: pow / exp / log in their first versions, for glibc 2.27 (src/glibc_227.h)
option(OS_GLIBC_227 "Link pow, exp and log to their oldest glibc versions (the Linux release)" OFF)
if(OS_GLIBC_227)
  target_compile_options(openscreamer PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/src/glibc_227.h)
endif()
# the game's words are read both as floats and as whole numbers (F32 / I32 of one address): the
# compiler must not assume the two never meet (it moved a read of a word before the write of it)
set(S2_OPTS "-fno-strict-aliasing")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i[3-6]86")
  list(APPEND S2_OPTS "-mfpmath=387" "-fexcess-precision=standard")
endif()
set_source_files_properties(src/s2_hooks.c PROPERTIES COMPILE_OPTIONS "${S2_OPTS}")
