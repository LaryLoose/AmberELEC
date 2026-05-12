# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="race"
PKG_VERSION="b629dc887401a95b2f7799692496993c168de514"
PKG_SHA256="9b5fa4897a125f145359c3e0d89d7739744c0160f50b7b3c427ce9285c5b82b4"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/libretro/RACE"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="This is the RACE NGPC emulator modified by theelf to run on the PSP."
PKG_TOOLCHAIN="make"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp race_libretro.so ${INSTALL}/usr/lib/libretro/
}