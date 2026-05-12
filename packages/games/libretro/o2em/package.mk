# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="o2em"
PKG_VERSION="dee1076eb70c728d4ff47186aea9cd1c11ce7638"
PKG_SHA256="8d7202424f8168d797aa69ef69b837862f8c4b44316baac7a2c9286db1d43a71"
PKG_LICENSE="Artistic License"
PKG_SITE="https://github.com/libretro/libretro-o2em"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Port of O2EM to the libretro API, an Odyssey 2 / VideoPac emulator."
PKG_TOOLCHAIN="make"

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp o2em_libretro.so ${INSTALL}/usr/lib/libretro/
}
