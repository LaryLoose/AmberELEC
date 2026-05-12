# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2025-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="freej2me-plus"
PKG_VERSION="92d7c911feb72f289a52837520e9b03954cb8a2c"
PKG_SHA256="39f667dc1ac8eb58e45b9a0f3b30697cabba34b0aca8b01081c096e64c5b0d62"
PKG_SITE="https://github.com/TASEmulators/freej2me-plus"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain apache-ant:host"
PKG_LONGDESC="A free J2ME emulator with libretro, awt and sdl2 frontends."
PKG_TOOLCHAIN="make"

make_target() {
  sed -i 's/freej2me-lr.jar/freej2me-plus-lr.jar/' ${PKG_BUILD}/build.xml
  sed -i 's/"source.version" value="1.6"/"source.version" value="1.8"/' ${PKG_BUILD}/build.xml
  sed -i 's/"target.version" value="1.6"/"target.version" value="1.8"/' ${PKG_BUILD}/build.xml
  sed -i 's|${java.home}/lib/rt.jar||' ${PKG_BUILD}/build.xml
  make -C ${PKG_BUILD}/src/libretro platform=unix
  ${TOOLCHAIN}/bin/ant
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp ${PKG_BUILD}/src/libretro/freej2me-plus_libretro.so ${INSTALL}/usr/lib/libretro/
  cp ${PKG_BUILD}/src/libretro/freej2me_libretro.info ${INSTALL}/usr/lib/libretro/freej2me-plus_libretro.info

  mkdir -p ${INSTALL}/usr/config/distribution/freej2me
  cp ${PKG_BUILD}/build/freej2me-plus-lr.jar ${INSTALL}/usr/config/distribution/freej2me

  mkdir -p ${INSTALL}/usr/bin
  cp ${PKG_DIR}/freej2me-plus.sh ${INSTALL}/usr/bin
}
