# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="moonlight-embedded"
PKG_VERSION="f32e415aea6797d261d6b470dcf8bf18727341c2"
PKG_LICENSE="GPLv3"
PKG_SITE="https://github.com/irtimmer/moonlight-embedded"
PKG_URL="${PKG_SITE}.git"
PKG_DEPENDS_TARGET="toolchain SDL2 ffmpeg opus libevdev rkmpp curl openssl expat avahi util-linux alsa-lib pulseaudio"
PKG_LONGDESC="Moonlight Embedded client for GameStream and Sunshine servers."
PKG_TOOLCHAIN="cmake-make"
PKG_BUILD_FLAGS="+lto-parallel"

PKG_CMAKE_OPTS_TARGET="-DENABLE_SDL=ON \
                       -DENABLE_FFMPEG=ON \
                       -DENABLE_X11=OFF \
                       -DENABLE_CEC=OFF \
                       -DENABLE_PULSE=ON"

post_makeinstall_target() {
    # Drop the upstream sample config; we ship our own template instead.
    rm -f ${INSTALL}/usr/etc/moonlight.conf
    rmdir ${INSTALL}/usr/etc 2>/dev/null || true

    # Config template + ES launcher, seeded to /storage on first boot.
    mkdir -p ${INSTALL}/usr/config/moonlight
    cp ${PKG_DIR}/sources/moonlight.conf ${INSTALL}/usr/config/moonlight/moonlight.conf
    cp ${PKG_DIR}/sources/Moonlight.sh   ${INSTALL}/usr/config/moonlight/Moonlight.sh
    chmod +x ${INSTALL}/usr/config/moonlight/Moonlight.sh
}