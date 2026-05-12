# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="libretro-database"
PKG_VERSION="1179bfce43764c72ba79dc05d254af63c97a13f1"
PKG_SHA256="8a730e286537b53e447599a283bd5933de3846045b9a1c707eda085f91010da4"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/libretro/libretro-database"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Repository containing cheatcode files, content data files, etc."
PKG_TOOLCHAIN="make"

configure_target() {
  cd ${PKG_BUILD}
}

makeinstall_target() {
  make install INSTALLDIR="${INSTALL}/usr/share/libretro-database"
}
