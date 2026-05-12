# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="smsplus-gx"
PKG_VERSION="41212ee3309fcf84ef0c04317a0916f0e1252c00"
PKG_SHA256="aa8d138d1b74bf1aa588e1b5c541eb64f07a028db2e8e58e284c4d0f6beaadce"
PKG_LICENSE="Non-commercial"
PKG_SITE="https://github.com/libretro/smsplus-gx"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="SMS Plus is an open-source Sega Master System and Game Gear emulator written by Charles MacDonald."
PKG_TOOLCHAIN="make"

make_target() {
  if [ "${ARCH}" == "arm" ]; then
    CFLAGS="${CFLAGS} -DALIGN_LONG"
  fi
  make -f Makefile.libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp smsplus_libretro.so ${INSTALL}/usr/lib/libretro/
}
