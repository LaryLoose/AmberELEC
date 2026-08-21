# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="dosboxpure"
PKG_VERSION="4a11412248ca4c862751a7d9e6818023795031e9"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/schellingb/dosbox-pure-unleashed"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain SDL2 ${OPENGLES}"
PKG_LONGDESC="DOSBox Pure Unleashed standalone DOS emulator"
PKG_TOOLCHAIN="make"

unpack() {
  mkdir -p ${PKG_BUILD}
  cp -rf ${SOURCES}/${PKG_NAME}/${PKG_NAME}-${PKG_VERSION}/. ${PKG_BUILD}/

  git clone -q --depth 1 --branch 1.0-preview6 https://github.com/schellingb/dosbox-pure.git ${PKG_BUILD}/dosbox-pure
  git clone -q --depth 1 https://github.com/schellingb/ZillaLib.git ${PKG_BUILD}/ZillaLib
  git -C ${PKG_BUILD}/ZillaLib reset -q --hard a2796bfe0faebe3e5de14b75d6b45866f1576f14
}

pre_patch() {
  find ${PKG_BUILD} -type f -exec dos2unix -q {} \;
}

pre_configure_target() {
  cat > ${PKG_BUILD}/ZillaLib/Linux/ZillaAppLocalConfig.mk << EOF
USE_EXTERNAL_SDL2 = 1
EXTERNAL_SDL2_INCLUDE = $(get_build_dir SDL2)/include
EXTERNAL_SDL2_SO = ${SYSROOT_PREFIX}/usr/lib/libSDL2.so
EOF
}

make_target() {
  make linux-release \
    CC=${CC} CXX=${CXX} AR=${AR} STRIP=${STRIP} UNAME=aarch64 \
    ZL_VIDEO_OPENGL_ES2=1
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
  cp ${PKG_BUILD}/Release-linux/DOSBoxPure_arm_64 ${INSTALL}/usr/bin/DOSBoxPure
  cp ${PKG_DIR}/dosboxpure.sh ${INSTALL}/usr/bin/

}