# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2024-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="doublecherrygb"
PKG_VERSION="33c09c13856a8d36c8922a1a1c319c0b1259100f"
PKG_SHA256="b0d4c22e18814acc658324883553a2ca7224ed806ab4fa5308fecb3545475cc0"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/TimOelrichs/doublecherryGB-libretro"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="libretro gameboy core with up to 16 players support"
PKG_TOOLCHAIN="make"

makeinstall_target() {
echo "PWD: $(pwd)"
  find . -name "*.so" -print
  
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp ../DoubleCherryGB_libretro.so ${INSTALL}/usr/lib/libretro/
}

make_target() {
  make -C $PKG_BUILD
}