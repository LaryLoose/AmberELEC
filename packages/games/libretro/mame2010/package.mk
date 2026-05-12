# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="mame2010"
PKG_VERSION="29095383c0281100fee2ee09f1438d8ae990c510"
PKG_SHA256="7994c34e3aca361ebb7d0aa9000e1a1107704512a06ee4e2cd07c87322c3cb03"
PKG_LICENSE="MAME"
PKG_SITE="https://github.com/libretro/mame2010-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Late 2010 version of MAME (0.139) for libretro. Compatible with MAME 0.139 romsets."
PKG_TOOLCHAIN="make"

make_target() {
  if [ "${ARCH}" == "arm" ]; then
    make CC="${CC}" LD="${CC}" PLATCFLAGS="${CFLAGS}" PTR64=0 ARM_ENABLED=1 LCPU=arm
  elif [ "${ARCH}" == "i386" ]; then
    make CC="${CC}" LD="${CC}" PLATCFLAGS="${CFLAGS}" PTR64=0 ARM_ENABLED=0 LCPU=x86
  elif [ "${ARCH}" == "x86_64" ]; then
    make CC="${CC}" LD="${CC}" PLATCFLAGS="${CFLAGS}" PTR64=1 ARM_ENABLED=0 LCPU=x86_64
  elif [ "${ARCH}" == "aarch64" ]; then
    make CC="${CC}" LD="${CC}" PLATCFLAGS="${CFLAGS}" PTR64=1 ARM_ENABLED=1 LCPU=arm64
  fi
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp mame2010_libretro.so ${INSTALL}/usr/lib/libretro/
}
