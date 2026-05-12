# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="gw-libretro"
PKG_VERSION="f8750d0f37db5f1f779437710f2653e8b1651ded"
PKG_SHA256="387f346cb4de7ede3e2273d44d583e4a097bd5cdbc9d3427e71ca32c97802de3"
PKG_LICENSE="GPLv3"
PKG_SITE="https://github.com/libretro/gw-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="A libretro core for Game & Watch simulators "
PKG_TOOLCHAIN="make"

make_target() {
  make -f Makefile.libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp gw_libretro.so ${INSTALL}/usr/lib/libretro/
}
