# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="beetle-supafaust"
PKG_VERSION="584ef2c5571f1ece95f6117aa04b7e8fee213fb1"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/libretro/supafaust"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Super Nintendo (Super Famicom) emulator"
PKG_TOOLCHAIN="make"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp ${PKG_BUILD}/mednafen_supafaust_libretro.so ${INSTALL}/usr/lib/libretro/beetle_supafaust_libretro.so
}
