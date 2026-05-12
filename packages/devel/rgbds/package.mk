# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="rgbds"
PKG_VERSION="0.7.0"
PKG_SHA256="b3e2bae43e679492efd6f128dc6e951dd4b1b9ef75905df937a9b9fa67bcfaf2"
PKG_LICENSE="MIT"
PKG_SITE="https://rgbds.gbdev.io"
PKG_URL="https://github.com/gbdev/rgbds/releases/download/v${PKG_VERSION}/rgbds-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_HOST="toolchain bison:host zlib:host libpng17:host"
PKG_LONGDESC="Rednex Game Boy Development System - Assembler/linker for Game Boy and Game Boy Color"
PKG_TOOLCHAIN="cmake"

PKG_CMAKE_OPTS_HOST="-DCMAKE_BUILD_TYPE=Release"


makeinstall_host() {
  mkdir -p ${TOOLCHAIN}/bin
  cp -P ${PKG_BUILD}/.${HOST_NAME}/src/rgbasm ${TOOLCHAIN}/bin/
  cp -P ${PKG_BUILD}/.${HOST_NAME}/src/rgblink ${TOOLCHAIN}/bin/
  cp -P ${PKG_BUILD}/.${HOST_NAME}/src/rgbfix ${TOOLCHAIN}/bin/
  cp -P ${PKG_BUILD}/.${HOST_NAME}/src/rgbgfx ${TOOLCHAIN}/bin/
}