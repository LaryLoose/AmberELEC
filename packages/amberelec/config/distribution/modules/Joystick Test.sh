#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

export SDL_GAMECONTROLLERCONFIG_FILE="/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt"
source /usr/bin/env.sh
source /etc/profile

jslisten set "killall joytest"

/usr/bin/joytest

clear > /dev/console

