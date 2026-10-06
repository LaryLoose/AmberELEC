# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2021-present AmberELEC (https://github.com/AmberELEC)

PKG_NAME="SDL3"
PKG_VERSION="3.4.18"
PKG_LICENSE="GPL"
PKG_SITE="https://www.libsdl.org/"
PKG_URL="https://www.libsdl.org/release/SDL3-${PKG_VERSION}.tar.gz"
PKG_DEPENDS_TARGET="toolchain alsa-lib systemd dbus ${OPENGLES} pulseaudio libsamplerate libdrm"
PKG_LONGDESC="Simple DirectMedia Layer is a cross-platform development library designed to provide low level access to audio, keyboard, mouse, joystick, and graphics hardware. SDL3 installs side-by-side with SDL2 (libSDL3.so.0, /usr/include/SDL3, sdl3.pc)."
PKG_TOOLCHAIN="cmake-make"

pre_configure_target() {
  PKG_CMAKE_OPTS_TARGET="-DSDL_SHARED=ON \
                         -DSDL_STATIC=OFF \
                         -DSDL_UNIX_CONSOLE_BUILD=ON \
                         -DSDL_LIBC=ON \
                         -DSDL_GCC_ATOMICS=ON \
                         -DSDL_ALTIVEC=OFF \
                         -DSDL_OSS=OFF \
                         -DSDL_ALSA=ON \
                         -DSDL_ALSA_SHARED=ON \
                         -DSDL_JACK=OFF \
                         -DSDL_PIPEWIRE=OFF \
                         -DSDL_PULSEAUDIO=ON \
                         -DSDL_PULSEAUDIO_SHARED=ON \
                         -DSDL_SNDIO=OFF \
                         -DSDL_LIBSAMPLERATE=ON \
                         -DSDL_LIBSAMPLERATE_SHARED=OFF \
                         -DSDL_DISKAUDIO=OFF \
                         -DSDL_DUMMYAUDIO=OFF \
                         -DSDL_WAYLAND=OFF \
                         -DSDL_WAYLAND_SHARED=OFF \
                         -DSDL_X11=OFF \
                         -DSDL_X11_SHARED=OFF \
                         -DSDL_COCOA=OFF \
                         -DSDL_VIVANTE=OFF \
                         -DSDL_DUMMYVIDEO=OFF \
                         -DSDL_OFFSCREEN=OFF \
                         -DSDL_PTHREADS=ON \
                         -DSDL_PTHREADS_SEM=ON \
                         -DSDL_DIRECTX=OFF \
                         -DSDL_RPATH=OFF \
                         -DSDL_RENDER_D3D=OFF \
                         -DSDL_OPENGL=OFF \
                         -DSDL_OPENGLES=ON \
                         -DSDL_VULKAN=OFF \
                         -DSDL_KMSDRM=ON \
                         -DSDL_KMSDRM_SHARED=ON \
                         -DSDL_TESTS=OFF \
                         -DSDL_INSTALL_TESTS=OFF \
                         -DSDL_EXAMPLES=OFF"
}

post_makeinstall_target() {
  rm -rf ${INSTALL}/usr/bin
}
