PKG_NAME="potator"
PKG_VERSION="ea88aa8c1df764d28bc13822d161a045b3747efc"
PKG_SHA256="113dccf656d58895c2b5bace911658aa5cd69bb015bccc9052ed31e7be33e81f"
PKG_LICENSE="GPLv2"
PKG_SITE="https://github.com/libretro/potator"
PKG_URL="${PKG_SITE}/archive/${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="A Watara Supervision Emulator based on Normmatt version."
PKG_TOOLCHAIN="make"


make_target() {
  make -C platform/libretro/ platform=aarch64
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp platform/libretro/potator_libretro.so ${INSTALL}/usr/lib/libretro/
}
