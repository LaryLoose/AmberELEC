# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2012 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2020-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="glsl-shaders"
PKG_VERSION="ebe123c55fa8538ec6abfa3eaa22d87f91e36679"
PKG_SHA256="fa08ae608493496592a001ade52baf2dc90fca8e0a06de9cbd1726befa5a8e90"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/libretro/glsl-shaders"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain common-shaders"
PKG_LONGDESC="Common GSLS shaders for RetroArch"
PKG_TOOLCHAIN="make"

configure_target() {
  cd ${PKG_BUILD}
}

#makeinstall_target() {
#  make install INSTALLDIR="${INSTALL}/usr/share/common-shaders"
#  cp -rf ${PKG_DIR}/shaders/* ${INSTALL}/usr/share/common-shaders
#}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/share/common-shaders

  rsync -a \
    --exclude='.install_pkg' \
    ${PKG_BUILD}/ \
    ${INSTALL}/usr/share/common-shaders/

  cp -rf ${PKG_DIR}/shaders/* ${INSTALL}/usr/share/common-shaders
}

post_makeinstall_target() {
  cp -f ${PKG_DIR}/removeshaders.sh .
  chmod 755 removeshaders.sh
  /bin/sh removeshaders.sh ${INSTALL}
}
