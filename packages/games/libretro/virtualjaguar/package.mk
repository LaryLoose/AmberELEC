# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="virtualjaguar"
PKG_VERSION="dd44259f8dca0ba87068eb2264367c01e131c263"
PKG_SHA256="7ceb77261f6552cffdc6f7bea3680d90ef5cafc7e85f0d43856dfce1388237c0"
PKG_LICENSE="GPLv3"
PKG_SITE="https://github.com/libretro/virtualjaguar-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Port of Virtual Jaguar to Libretro"
PKG_TOOLCHAIN="make"

pre_configure_target() {
  sed -i 's/\-O[23]//' ${PKG_BUILD}/Makefile
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp virtualjaguar_libretro.so ${INSTALL}/usr/lib/libretro/
}
