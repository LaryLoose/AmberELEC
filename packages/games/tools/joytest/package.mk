# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="joytest"
PKG_VERSION="1.0"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/AmberELEC"
PKG_DEPENDS_TARGET="toolchain SDL2 SDL2_gfx SDL2_ttf"
PKG_LONGDESC="SDL2 joystick / gamepad test tool (buttons, axes, hats, stick drift)"
PKG_TOOLCHAIN="manual"

unpack() {
  mkdir -p ${PKG_BUILD}
  cp -f ${PKG_DIR}/sources/joytest.c ${PKG_BUILD}/
  cp -f ${PKG_DIR}/sources/DejaVuSansMono.ttf ${PKG_BUILD}/
}

make_target() {
  ${CC} ${TARGET_CFLAGS} -I${SYSROOT_PREFIX}/usr/include/SDL2 -D_REENTRANT \
    joytest.c -o joytest \
    ${TARGET_LDFLAGS} -lSDL2 -lSDL2_gfx -lSDL2_ttf -lm
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
  cp -v joytest ${INSTALL}/usr/bin/joytest
  mkdir -p ${INSTALL}/usr/share/fonts/dejavu
  cp -v DejaVuSansMono.ttf ${INSTALL}/usr/share/fonts/dejavu/DejaVuSansMono.ttf
}
