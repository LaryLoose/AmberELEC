# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2021-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="freej2me"
PKG_VERSION="6c534cb88d429ec638d84381185b9aa808c8b584"
PKG_SHA256="a8ffeaaed604e26d7ad4f1ac7a11aa05c827f6c25a0c77fcf0be4e202e57e8a1"
PKG_SITE="https://github.com/hex007/freej2me"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain apache-ant:host"
PKG_LONGDESC="A free J2ME emulator with libretro, awt and sdl2 frontends."
PKG_TOOLCHAIN="make"

pre_configure_target() {
  ${TOOLCHAIN}/bin/ant
}

make_target() {
  make -C ${PKG_BUILD}/src/libretro
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp ${PKG_BUILD}/src/libretro/freej2me_libretro.so ${INSTALL}/usr/lib/libretro/

  mkdir -p ${INSTALL}/usr/config/distribution/freej2me
  cp ${PKG_BUILD}/build/freej2me-lr.jar ${INSTALL}/usr/config/distribution/freej2me

  mkdir -p ${INSTALL}/usr/bin
  cp ${PKG_DIR}/freej2me.sh ${INSTALL}/usr/bin
}
