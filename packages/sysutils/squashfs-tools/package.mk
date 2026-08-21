# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2009-2016 Stephan Raue (stephan@openelec.tv)
# Copyright (C) 2017-present Team LibreELEC (https://libreelec.tv)

PKG_NAME="squashfs-tools"
PKG_VERSION="4.5"
PKG_SHA256="b9e16188e6dc1857fe312633920f7d71cc36b0162eb50f3ecb1f0040f02edddd"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/plougher/squashfs-tools"
PKG_URL="https://github.com/plougher/squashfs-tools/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_HOST="ccache:host zlib:host lzo:host xz:host zstd:host"
PKG_DEPENDS_TARGET="toolchain zlib"
PKG_NEED_UNPACK="$(get_pkg_directory zlib) $(get_pkg_directory lzo) $(get_pkg_directory xz) $(get_pkg_directory zstd)"
PKG_LONGDESC="Tools for squashfs, a highly compressed read-only filesystem for Linux."
PKG_TOOLCHAIN="manual"

pre_build_host() {
  mkdir -p ${PKG_BUILD}/.${HOST_NAME}
  cp -r ${PKG_BUILD}/* ${PKG_BUILD}/.${HOST_NAME}
}

make_host() {
  make -C ${PKG_BUILD}/.${HOST_NAME}/squashfs-tools \
          mksquashfs \
          XZ_SUPPORT=1 \
          LZO_SUPPORT=1 \
          ZSTD_SUPPORT=1 \
          XATTR_SUPPORT=0 \
          XATTR_DEFAULT=0 \
          INCLUDEDIR="-I. -I${TOOLCHAIN}/include"
}

makeinstall_host() {
  mkdir -p ${TOOLCHAIN}/bin
  cp ${PKG_BUILD}/.${HOST_NAME}/squashfs-tools/mksquashfs ${TOOLCHAIN}/bin
}

pre_build_target() {
  mkdir -p ${PKG_BUILD}/.${TARGET_NAME}
  cp -r ${PKG_BUILD}/* ${PKG_BUILD}/.${TARGET_NAME}
}

make_target() {
  make -C ${PKG_BUILD}/.${TARGET_NAME}/squashfs-tools \
       CC="${CC}" \
       AR="${AR}" \
       RANLIB="${RANLIB}" \
       EXTRA_CFLAGS="${CFLAGS}" \
       LDFLAGS="${LDFLAGS}" \
       EXTRA_LDFLAGS="${LDFLAGS}"
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/bin
  cp ${PKG_BUILD}/.${TARGET_NAME}/squashfs-tools/mksquashfs ${INSTALL}/usr/bin
  cp ${PKG_BUILD}/.${TARGET_NAME}/squashfs-tools/unsquashfs ${INSTALL}/usr/bin
  ln -sf unsquashfs ${INSTALL}/usr/bin/sqfscat
  ln -sf mksquashfs ${INSTALL}/usr/bin/sqfstar
  ${STRIP} ${INSTALL}/usr/bin/{mksquashfs,unsquashfs}
}