#!/usr/bin/bash
#  SPDX-License-Identifier: MIT
#
#  ES-DE Frontend
#  create_AppImage_AArch64.sh
#
#  Runs the complete process of building a Linux AppImage for the ARM64/AArch64 ISA.
#  The SDL library is also built and included in the AppImage.
#
#  This script has only been tested on Ubuntu 24.04 LTS.
#
#  Dendencias posibles
sudo apt install appstream appstream-util
# How many CPU threads to use for the compilation.
JOBS=$(nproc 2>/dev/null || echo 8)

SDL_RELEASE_TAG=release-2.32.10
SDL_SHARED_LIBRARY=libSDL2-2.0.so.0.3200.10

echo "Building AppImage..."

if [ ! -f .clang-format ]; then
  echo "You need to run this script from the root of the repository."
  exit
fi

if [ ! -f appimagetool-aarch64.AppImage ]; then
  echo -e "Can't find appimagetool-aarch64.AppImage, downloading the latest version...\n"
  wget "https://github.com/pkgforge-dev/appimagetool-uruntime/releases/download/continuous/appimagetool-aarch64.AppImage"
fi

chmod a+x appimagetool-aarch64.AppImage

if [ ! -f linuxdeploy-aarch64.AppImage ]; then
  echo -e "Can't find linuxdeploy-aarch64.AppImage, downloading the latest version...\n"
  wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-aarch64.AppImage"
fi

chmod a+x linuxdeploy-aarch64.AppImage

if [ ! -f external/SDL/build/${SDL_SHARED_LIBRARY} ]; then
  echo
  echo "Building the SDL library..."
  cd external
  rm -rf SDL
  git clone https://github.com/libsdl-org/SDL.git
  cd SDL
  git checkout $SDL_RELEASE_TAG

  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release -S .. -B .

  if [ $(grep PKG_PIPEWIRE_VERSION:INTERNAL= CMakeCache.txt) = "PKG_PIPEWIRE_VERSION:INTERNAL=" ]; then
    echo
    echo -e "The SDL library is not configured with PipeWire support, aborting."
    exit
  fi

  make -j${JOBS}
  cd ../../..
else
  echo
  echo -e "The SDL library has already been built, skipping this step\n"
fi

rm -rf ./AppDir
mkdir AppDir

rm -f CMakeCache.txt
cmake -DAPPIMAGE_BUILD=on -DBUNDLED_CERTS=on .
make clean
make -j${JOBS}
make install DESTDIR=AppDir
cd AppDir
ln -s usr/bin/ESTACION-PRO AppRun
ln -s usr/share/pixmaps/org.estacion_pro.frontend.svg .
ln -s usr/share/applications/org.estacion_pro.frontend.desktop .
ln -s org.estacion_pro.frontend.svg .DirIcon
cd usr/bin
ln -s ../share/esstacion-pro/resources .
ln -s ../share/estacion-pro/themes .
cd ../../..

./linuxdeploy-aarch64.AppImage -l /lib/aarch64-linux-gnu/libOpenGL.so.0 -l /lib/aarch64-linux-gnu/libGLdispatch.so.0 -l /lib/aarch64-linux-gnu/libgio-2.0.so.0 --appdir AppDir
cp external/SDL/build/${SDL_SHARED_LIBRARY} AppDir/usr/lib/libSDL2-2.0.so.0
ARCH=aarch64 ./appimagetool-aarch64.AppImage AppDir

mv ESTACION-PRO-aarch64.AppImage ESTACION-PRO_aarch64.AppImage
echo -e "\nCreated AppImage ESTACION-PRO_aarch64.AppImage"
