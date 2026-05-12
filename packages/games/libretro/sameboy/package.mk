# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="sameboy"
PKG_VERSION="208ba4afabffab9edde416f2dbb8ae459e34adb8"
PKG_LICENSE="MIT"
PKG_SITE="https://github.com/LIJI32/SameBoy"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain rgbds:host"
PKG_LONGDESC="Gameboy and Gameboy Color emulator written in C"
PKG_TOOLCHAIN="make"
PKG_GIT_CLONE_BRANCH="libretro"

make_target() {
  PATH=/usr/bin:$PATH 
  make libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp ${PKG_BUILD}/build/bin/sameboy_libretro.so ${INSTALL}/usr/lib/libretro/
}
