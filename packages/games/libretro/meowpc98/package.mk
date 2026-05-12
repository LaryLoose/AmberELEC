# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="meowpc98"
PKG_VERSION="9b298bf92adc6ce93859421bf45fa2df9690d699"
PKG_SHA256="19967b8f50359fa01b7e3b974e1cf1573bf6a7e19cd6bf50d4d9c7a1f18b1374"
PKG_LICENSE="Unknown"
PKG_SITE="https://github.com/libretro/libretro-meowPC98"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Neko Project 2 (PC98 emulator) port for libretro/RetroArch"
PKG_TOOLCHAIN="make"

make_target() {
  make -C libretro -f Makefile.libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp libretro/nekop2_libretro.so ${INSTALL}/usr/lib/libretro/
}
