# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="nestopia"
PKG_VERSION="b0fd87dd07e3c52903435d302b04e5e97796f127"
PKG_SHA256="32c602b65090615c6b1b6867772c3b7f261127d56d28b48ab6c05afc68d781e0"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/libretro/nestopia"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="This project is a fork of the original Nestopia source code, plus the Linux port. The purpose of the project is to enhance the original, and ensure it continues to work on modern operating systems."
PKG_TOOLCHAIN="make"

make_target() {
  cd ${PKG_BUILD}
  make -C libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp libretro/nestopia_libretro.so ${INSTALL}/usr/lib/libretro/
}
