#!/bin/sh
# The Windows x86-64 release: openscreamer.exe built with MinGW-w64 on Linux,
# SDL2.dll beside it, a README and the licences, as
#   dist/openscreamer-<version>-windows-x86_64.zip
# usage: tools/release_windows.sh      (needs x86_64-w64-mingw32-gcc, cmake, curl)
# SDL2's own Windows package for MinGW (headers, import library, SDL2.dll) is
# fetched into build/release-windows.
set -e
cd "$(dirname "$0")/.."
ROOT=$PWD
V=$(sh tools/version.sh)
NAME=openscreamer-$V-windows-x86_64
SDL=SDL2-2.32.10
W=build/release-windows

rm -rf $W/game "dist/$NAME"
mkdir -p $W/dl $W/game "dist/$NAME"
[ -f $W/dl/$SDL-mingw.tar.gz ] || curl -fsSL -o $W/dl/$SDL-mingw.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz
[ -d $W/$SDL ] || tar -xzf $W/dl/$SDL-mingw.tar.gz -C $W
P=$ROOT/$W/$SDL/x86_64-w64-mingw32
# (its pkg-config file names the place it was built for)
sed "s|^prefix=.*|prefix=$P|" "$P/lib/pkgconfig/sdl2.pc" > $W/sdl2.pc
cat > $W/toolchain.cmake <<EOT
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32 $P)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
EOT
PKG_CONFIG_LIBDIR=$ROOT/$W PKG_CONFIG_PATH= cmake -S port -B $W/game -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$ROOT/$W/toolchain.cmake > $W/cmake.log
cmake --build $W/game -j"$(nproc)" 2>&1 | grep -E ' error|warning: implicit|Built target openscreamer'
x86_64-w64-mingw32-strip $W/game/openscreamer.exe

cp $W/game/openscreamer.exe "$P/bin/SDL2.dll" "dist/$NAME/"
cp LICENSE "dist/$NAME/LICENSE.txt"
cp port/src/nuked/opl3.h "dist/$NAME/LICENSE-nuked-opl3.txt"
cp "$ROOT/$W/$SDL/LICENSE.txt" "dist/$NAME/LICENSE-SDL2.txt"
sed "s/@VERSION@/$V/" tools/release_windows_readme.txt | sed 's/$/\r/' > "dist/$NAME/README.txt"
rm -f "dist/$NAME.zip"
(cd dist && python3 -m zipfile -c "$NAME.zip" "$NAME")
echo "what it needs:"
x86_64-w64-mingw32-objdump -p "dist/$NAME/openscreamer.exe" | grep "DLL Name"
ls -la "dist/$NAME.zip"
