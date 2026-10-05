# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="batterymonitor"
PKG_VERSION="1.0"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/AmberELEC"
PKG_DEPENDS_TARGET="toolchain SDL2 SDL2_gfx SDL2_ttf"
PKG_LONGDESC="SDL2 battery monitor (Android-style charge-over-time graph)"
PKG_TOOLCHAIN="manual"

unpack() {
  mkdir -p ${PKG_BUILD}
  cp -f ${PKG_DIR}/sources/batterymonitor.c ${PKG_BUILD}/
  cp -f ${PKG_DIR}/sources/DejaVuSans.ttf ${PKG_BUILD}/
}

make_target() {
  ${CC} ${TARGET_CFLAGS} -I${SYSROOT_PREFIX}/usr/include/SDL2 -D_REENTRANT \
    batterymonitor.c -o batterymonitor \
    ${TARGET_LDFLAGS} -lSDL2 -lSDL2_gfx -lSDL2_ttf -lm
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
  cp -v batterymonitor ${INSTALL}/usr/bin/batterymonitor
  mkdir -p ${INSTALL}/usr/share/fonts/dejavu
  cp -v DejaVuSans.ttf ${INSTALL}/usr/share/fonts/dejavu/DejaVuSans.ttf
}
