# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="same_cdi"
PKG_VERSION="2184aa6d87a31fb6c64534b9b7b2d26e36bae757"
PKG_LICENSE="GPL"
PKG_SITE="https://github.com/libretro/same_cdi"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain"
PKG_LONGDESC="SAME_CDI is a Single Arcade/Machine Emulator for libretro"
PKG_TOOLCHAIN="make"

PKG_MAKE_OPTS_TARGET="REGENIE=1 \
                      VERBOSE=1 \
                      NOWERROR=1 \
                      OPENMP=1 \
                      CROSS_BUILD=1 \
                      TOOLS=0 \
                      RETRO=1 \
                      PTR64=0 \
                      NOASM=0 \
                      PYTHON_EXECUTABLE=python3 \
                      CONFIG=libretro \
                      LIBRETRO_OS=unix \
                      LIBRETRO_CPU= \
                      PLATFORM=arm64 \
                      ARCH= \
                      TARGET=mame \
                      OSD=retro \
                      USE_SYSTEM_LIB_EXPAT=1 \
                      USE_SYSTEM_LIB_ZLIB=1 \
                      USE_SYSTEM_LIB_FLAC=1 \
                      USE_SYSTEM_LIB_SQLITE3=1"

pre_configure_target() {
  sed -i "s/-static-libstdc++//g" scripts/genie.lua
  # Build the host-side genie tool with the real system compiler (no target sysroot),
  # else host-gcc pulls the aarch64 math-vector.h ("unknown type name __Float32x4_t").
  sed -i 's|^\([ \t]*CC[ \t]*=\).*|\1 /usr/bin/gcc|g' 3rdparty/genie/build/gmake.linux/genie.make
  sed -i 's|^\([ \t]*CXX[ \t]*=\).*|\1 /usr/bin/g++|g' 3rdparty/genie/build/gmake.linux/genie.make
}

make_target() {
  unset ARCH
  unset DISTRO
  unset PROJECT
  # GCC 15 type-checks template bodies before instantiation; bundled sol2's
  # optional<T&>::emplace references a non-existent construct() -> downgrade.
  # (<cstdint> for uint8_t is handled by the gcc15-cstdint patch, not a force-include,
  #  which would break MAME's precompiled header.)
  export ARCHOPTS="-D__aarch64__ -DASMJIT_BUILD_X86"
  export ARCHOPTS_CXX="-Wno-template-body"

  env -u CFLAGS -u LDFLAGS -u CPPFLAGS make -C 3rdparty/genie/build/gmake.linux -f genie.make \
      CC="/usr/bin/gcc" CXX="/usr/bin/g++" CFLAGS="" CPPFLAGS="" LDFLAGS=""

  make -f Makefile.libretro ${PKG_MAKE_OPTS_TARGET} OVERRIDE_CC=${CC} OVERRIDE_CXX=${CXX} OVERRIDE_LD=${LD} AR=${AR} ${MAKEFLAGS}
}

makeinstall_target() {
  mkdir -p ${INSTALL}/usr/lib/libretro
  cp same_cdi_libretro.so ${INSTALL}/usr/lib/libretro/
}
