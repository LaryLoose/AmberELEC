# SPDX-License-Identifier: GPL-2.0-or-later

PKG_NAME="redream"
PKG_VERSION="1.5.0-1240-gc41f7f2"
PKG_ARCH="aarch64"
PKG_LICENSE="GPLv3"
PKG_SITE="https://redream.io/"
PKG_URL="https://redream.io/download/redream.universal-raspberry-linux-v${PKG_VERSION}.tar.gz"
PKG_LONGDESC="Redream Sega Dreamcast emulator"
PKG_TOOLCHAIN="manual"

unpack() {
  mkdir -p ${PKG_BUILD}
  tar -xf ${SOURCES}/redream/${PKG_NAME}-${PKG_VERSION}.tar.gz -C ${PKG_BUILD}
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/share/redream ${INSTALL}/usr/bin
  cp ${PKG_BUILD}/redream.aarch64.elf ${INSTALL}/usr/share/redream/redream
  echo "${PKG_VERSION}" > ${INSTALL}/usr/share/redream/.version
  cp ${PKG_DIR}/redream.sh ${INSTALL}/usr/bin/
}