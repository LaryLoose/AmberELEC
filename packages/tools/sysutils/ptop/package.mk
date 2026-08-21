# SPDX-License-Identifier: GPL-2.0-or-later

PKG_NAME="ptop"
PKG_VERSION="1.0"
PKG_ARCH="any"
PKG_LICENSE="GPL-2.0-or-later"
PKG_SITE="https://amberelec.org"
PKG_URL=""
PKG_DEPENDS_TARGET="toolchain"
PKG_SECTION="sysutils"
PKG_SHORTDESC="Live battery power monitor for the RG351MP"
PKG_LONGDESC="ptop displays and logs battery power, learned coulomb state of charge, voltage state of charge, and learned full charge capacity."
PKG_TOOLCHAIN="make"

make_target() {
  ${CXX} ${CXXFLAGS} -O2 -std=c++17 -o ptop ptop.cpp ${LDFLAGS}
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
  cp ptop ${INSTALL}/usr/bin/
}