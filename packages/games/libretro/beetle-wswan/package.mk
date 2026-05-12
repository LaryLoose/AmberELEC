# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="beetle-wswan"
PKG_VERSION="392db084316475411f3f24bd1ea54dba72ecbe51"
PKG_SHA256="a6fa50e12c928ab3e0c3c6626bf2021053bca1d2bdd6e19479e83a575d922dc2"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/libretro/beetle-wswan-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="libretro implementation of Mednafen wswan"
PKG_TOOLCHAIN="make"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp mednafen_wswan_libretro.so ${INSTALL}/usr/lib/libretro/beetle_wswan_libretro.so
}
