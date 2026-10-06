# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2021-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="SDL3_image"
PKG_VERSION="3.4.6"
PKG_LICENSE="GPL"
PKG_SITE="https://www.libsdl.org/"
PKG_URL="https://www.libsdl.org/projects/SDL_image/release/SDL3_image-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain SDL3 libpng libjpeg-turbo libwebp"
PKG_LONGDESC="SDL_image is an image file loading library for SDL3. Installs side-by-side with SDL2_image (libSDL3_image.so, SDL3_image CMake package)."
PKG_TOOLCHAIN="cmake-make"

PKG_CMAKE_OPTS_TARGET=" -DSDLIMAGE_VENDORED=OFF \
                        -DSDLIMAGE_SAMPLES=OFF \
                        -DSDLIMAGE_TESTS=OFF \
                        -DSDLIMAGE_DEPS_SHARED=OFF \
                        -DSDLIMAGE_AVIF=OFF \
                        -DSDLIMAGE_JXL=OFF \
                        -DSDLIMAGE_TIF=OFF \
                        -DSDLIMAGE_WEBP=ON"
