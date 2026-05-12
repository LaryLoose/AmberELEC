# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="snes9x2002"
PKG_VERSION="15826a2afc1474b30c310502b5dbe25c639be59d"
PKG_SHA256="38929ff36a44730757eeb88a355e5859142c5fc54b12b72a8d319b738c0b25a4"
PKG_LICENSE="Non-commercial"
PKG_SITE="https://github.com/libretro/snes9x2002"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Snes9x 2002. Port of SNES9x 1.39 for libretro (was previously called PocketSNES). Heavily optimized for ARM."
PKG_TOOLCHAIN="make"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp snes9x2002_libretro.so ${INSTALL}/usr/lib/libretro/
}
