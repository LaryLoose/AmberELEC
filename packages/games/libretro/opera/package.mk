# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="opera"
PKG_VERSION="4c4ca6bf741c40715723a8b8dae4b6187ff6ac30"
PKG_SHA256="1487bc56bfba97f648d54e864e1b138ab929e885b70170b8f1323e0f0ac97b2f"
PKG_LICENSE="LGPL with additional notes"
PKG_SITE="https://github.com/libretro/opera-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Port of 4DO/libfreedo to libretro."
PKG_TOOLCHAIN="make"

pre_configure_target() {
  sed -i 's/\-O[23]//' ${PKG_BUILD}/Makefile
}

make_target() {
  make CC=${CC} CXX=${CXX} AR=${AR}
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp opera_libretro.so ${INSTALL}/usr/lib/libretro/
}
