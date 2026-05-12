# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2020-present Shanti Gilbert (https://github.com/shantigilbert)

PKG_NAME="hydracastlelabyrinth"
PKG_VERSION="56efcf605f99f8bbe5ac4b2e0705aadfba3cb8ac"
PKG_SHA256="0a508e1ec195de7bf325c821eab21b69f8d089ac7dbbb0a13b9ce5276eacf12c"
PKG_LICENSE="GPL2"
PKG_SITE="https://github.com/ptitSeb/hydracastlelabyrinth"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain SDL2"
PKG_LONGDESC="A port to Linux (and OpenPandora / DragonBox Pyra / ODroid / PocketCHIP / GameShell / AmigaOS4 / MorphOS / Emscripten) of Hydra Castle Labyrinth (a "metroidvania" kind of game). Status: Working."
PKG_TOOLCHAIN="cmake-make"

pre_configure_target() {
  export CFLAGS="${CFLAGS} -fcommon"
  PKG_CMAKE_OPTS_TARGET=" -DUSE_SDL2=ON -DSDL2_INCLUDE_DIRS=${SYSROOT_PREFIX}/usr/include/SDL2 -DSDL2_LIBRARIES=${SYSROOT_PREFIX}/usr/lib"
  LDFLAGS="${LDFLAGS} -lSDL2"
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/local/bin
  cp ${PKG_BUILD}/.${TARGET_NAME}/hcl ${INSTALL}/usr/local/bin
  mkdir -p ${INSTALL}/usr/config/ports/hcl
  cp -rf ${PKG_BUILD}/data ${INSTALL}/usr/config/ports/hcl/
}
