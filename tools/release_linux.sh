#!/bin/sh
# The Linux x86-64 release: openscreamer built in a Debian 11 container, so
# that it runs on distributions back to about 2018 (it ends up asking for
# glibc 2.27 and nothing of SDL2 newer than 2.0.9; SDL2 is the
# system's own libSDL2-2.0.so.0), with a README and the licences, as
#   dist/openscreamer-<version>-linux-x86_64.tar.gz
# usage: tools/release_linux.sh        (needs docker and curl)
#
# The container is any image of Debian 11 with gcc and cmake: by default the
# hybrid cores' toolchain image (mister-hybrid-tc, from Hybrid_MiSTer's
# toolchain/), or name one in OS_RELEASE_IMAGE.  Debian 11's own package
# servers are gone, so SDL2 is not installed from them: its source is fetched
# and built here as a bare shared library, which is only there to link
# against -- the program runs with the SDL2 of the machine it is on.
set -e
cd "$(dirname "$0")/.."
ROOT=$PWD
V=$(sh tools/version.sh)
NAME=openscreamer-$V-linux-x86_64
IMAGE=${OS_RELEASE_IMAGE:-mister-hybrid-tc}
SDL=SDL2-2.32.10
W=build/release-linux

rm -rf $W/game "dist/$NAME"
mkdir -p $W/dl $W/game "dist/$NAME"
[ -f $W/dl/$SDL.tar.gz ] || curl -fsSL -o $W/dl/$SDL.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/$SDL.tar.gz
[ -d $W/$SDL ] || tar -xzf $W/dl/$SDL.tar.gz -C $W
# (the container has no git: the version is handed in)
docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e OS_VERSION_IS="$V" -v "$ROOT":"$ROOT" -w "$ROOT" $IMAGE sh -c "
  set -e
  if [ ! -f $W/prefix/lib/pkgconfig/sdl2.pc ]; then
    cmake -S $W/$SDL -B $W/sdl -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$ROOT/$W/prefix \
      -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF \
      -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF -DSDL_ALSA=OFF -DSDL_PULSEAUDIO=OFF \
      -DSDL_PIPEWIRE=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_DBUS=OFF -DSDL_IBUS=OFF -DSDL_LIBUDEV=OFF > /dev/null
    cmake --build $W/sdl -j\$(nproc) > /dev/null
    cmake --install $W/sdl > /dev/null
  fi
  PKG_CONFIG_PATH=$ROOT/$W/prefix/lib/pkgconfig cmake -S port -B $W/game -DCMAKE_BUILD_TYPE=Release -DCMAKE_SKIP_RPATH=ON -DOS_GLIBC_227=ON > /dev/null
  cmake --build $W/game -j\$(nproc) 2>&1 | grep -E ' error|Built target openscreamer'
  strip $W/game/openscreamer"

cp $W/game/openscreamer "dist/$NAME/"
cp LICENSE "dist/$NAME/LICENSE.txt"
cp port/src/nuked/opl3.h "dist/$NAME/LICENSE-nuked-opl3.txt"
sed "s/@VERSION@/$V/" tools/release_linux_readme.txt > "dist/$NAME/README.txt"
tar -C dist -czf "dist/$NAME.tar.gz" "$NAME"
echo "what it needs:"
objdump -p "dist/$NAME/openscreamer" | grep NEEDED
objdump -T "dist/$NAME/openscreamer" | grep -o 'GLIBC_[0-9.]*' | sort -V | tail -1
ls -la "dist/$NAME.tar.gz"
