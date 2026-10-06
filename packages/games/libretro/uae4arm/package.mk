# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="uae4arm"
PKG_VERSION="fc1cb90afd6b5c6d9bb933d111c4c99c90f37688"
PKG_SHA256="e9d9626e1af3f046adb8495a3ac79f69154e77065d593463e0a1f3faf63ca9df"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/Chips-fr/uae4arm-rpi"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain flac mpg123"
PKG_LONGDESC="Port of uae4arm for libretro (rpi/android)"
PKG_TOOLCHAIN="make"

make_target() {
  if [ "${DEVICE}" = "RG552" ]; then
    make -f Makefile.libretro platform=rpi4_aarch64
  else
    make -f Makefile.libretro platform=rpi3_aarch64
  fi
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp uae4arm_libretro.so ${INSTALL}/usr/lib/libretro/
}
