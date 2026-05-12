PKG_NAME="libpcap"
PKG_VERSION="1.10.4"
PKG_LICENSE="BSD"
PKG_SITE="https://www.tcpdump.org/"
PKG_URL="https://www.tcpdump.org/release/libpcap-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="Packet capture library"
PKG_TOOLCHAIN="autotools"

pre_configure_target() {
  export ac_cv_linux_vers=2
  PKG_CONFIGURE_OPTS_TARGET+=" --disable-dbus --without-libnl"
}

makeinstall_target() {
  mkdir -p ${SYSROOT_PREFIX}/usr/lib
  
  cp -av ${PKG_BUILD}/.${TARGET_NAME}/libpcap.so* ${SYSROOT_PREFIX}/usr/lib/
  cp -av ${PKG_BUILD}/.${TARGET_NAME}/libpcap.a ${SYSROOT_PREFIX}/usr/lib/

  mkdir -p ${SYSROOT_PREFIX}/usr/include
  cp -av ${PKG_BUILD}/pcap.h ${SYSROOT_PREFIX}/usr/include/
  cp -av ${PKG_BUILD}/pcap ${SYSROOT_PREFIX}/usr/include/ -r
}