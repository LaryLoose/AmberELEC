#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later

. /etc/profile

# redream derives its data directory from the executable location (/proc/self/exe),
# so it must run from writable storage rather than the read-only /usr.
APPDIR="/storage/.config/redream"
mkdir -p "${APPDIR}"

IMAGE_VERSION="$(</usr/share/redream/.version)"
LOCAL_VERSION=""
[[ -r "${APPDIR}/.version" ]] && LOCAL_VERSION="$(<"${APPDIR}/.version")"

if [[ ! -x "${APPDIR}/redream" || "${IMAGE_VERSION}" != "${LOCAL_VERSION}" ]]; then
  cp -f /usr/share/redream/redream "${APPDIR}/redream"
  cp -f /usr/share/redream/.version "${APPDIR}/.version"
  chmod +x "${APPDIR}/redream"
fi

if [[ -f "/storage/roms/bios/dc/dc_boot.bin" && ! -f "${APPDIR}/boot.bin" ]]; then
  cp -f "/storage/roms/bios/dc/dc_boot.bin" "${APPDIR}/boot.bin"
fi

if [[ -f "/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt" ]]; then
  export SDL_GAMECONTROLLERCONFIG_FILE="/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt"
fi

exec "${APPDIR}/redream" "$1"