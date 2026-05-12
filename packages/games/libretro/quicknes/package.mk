# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="quicknes"
PKG_VERSION="71782569078f29214017a966b0f992b9e512bf19"
PKG_SHA256="37700bc255f319eceb4a94fb168053b32d8940852bc20450549c1194582e6817"
PKG_LICENSE="LGPLv2.1+"
PKG_SITE="https://github.com/libretro/QuickNES_Core"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="The QuickNES core library, originally by Shay Green, heavily modified"
PKG_TOOLCHAIN="make"

make_target() {
  VERSION='GIT_VERSION ?= '
  VERSION+=${PKG_VERSION:0:7}
  sed -i "s/GIT_VERSION ?= \" \$(shell git describe --dirty --always --tags)\"/${VERSION}/g" ${PKG_BUILD}/Makefile
  make
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp quicknes_libretro.so ${INSTALL}/usr/lib/libretro/
}
