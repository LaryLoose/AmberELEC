# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="stella-2014"
PKG_VERSION="eed47e154d1bbda3305e9ef2d486b6710c8973f4"
PKG_SHA256="0cfde939c99b7fd925d2c0243f15094635c8302a24d9b576bea101ac623b2f21"
PKG_LICENSE="GPL2"
PKG_SITE="https://github.com/libretro/stella2014-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Stella is a multi-platform Atari 2600 VCS emulator released under the GNU General Public License (GPL)."
PKG_TOOLCHAIN="make"

pre_configure_target() {
PKG_MAKE_OPTS_TARGET=" -C ${PKG_BUILD}/ -f Makefile platform=emuelec-arm64"
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp ${PKG_BUILD}/stella2014_libretro.so ${INSTALL}/usr/lib/libretro/
}
