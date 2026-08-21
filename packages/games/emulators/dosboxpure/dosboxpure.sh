#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

. /etc/profile

DATA_DIR="/storage/roms/gamedata/dosboxpure"
mkdir -p "${DATA_DIR}/saves" "${DATA_DIR}/system"
cd "${DATA_DIR}"

exec /usr/bin/DOSBoxPure "$@"