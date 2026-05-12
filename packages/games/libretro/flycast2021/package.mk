# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2021-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="flycast2021"
PKG_VERSION="b897744e27c730c7519784b2aef12ba7f658de31"
PKG_SHA256="677a9b4f4870e59dd896cb14852af6bfdcee661823f0b87a25f1430f0bae0972"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/libretro/flycast"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain ${OPENGLES}"
PKG_LONGDESC="Flycast is a multiplatform Sega Dreamcast emulator"
PKG_TOOLCHAIN="make"

pre_configure_target() {
  sed -i 's/\-O[23]//' ${PKG_BUILD}/Makefile
  sed -i 's/CFLAGS   :=//' ${PKG_BUILD}/Makefile
  sed -i 's/CXXFLAGS   :=//' ${PKG_BUILD}/Makefile
  sed -i 's/LDFLAGS   :=//' ${PKG_BUILD}/Makefile

  sed -i 's/^ASFLAGS.*/ASFLAGS :=/g' ${PKG_BUILD}/Makefile

  sed -i 's/define CORE_OPTION_NAME "reicast"/define CORE_OPTION_NAME "flycast2021"/g' core/libretro/libretro_core_option_defines.h
  sed -i 's/"Flycast"/"Flycast 2021"/g' core/libretro/libretro.cpp
  sed -i 's/RETRO_PIXEL_FORMAT_XRGB8888/RETRO_PIXEL_FORMAT_RGB565/g' core/libretro/libretro.cpp

  PKG_MAKE_OPTS_TARGET="GIT_VERSION=${PKG_VERSION:0:7}"
}

pre_make_target() {
  export BUILD_SYSROOT=${SYSROOT_PREFIX}
  export ASFLAGS=""
  PKG_MAKE_OPTS_TARGET+=" ARCH=arm64 platform=arm64"
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp flycast_libretro.so ${INSTALL}/usr/lib/libretro/flycast2021_libretro.so
}
