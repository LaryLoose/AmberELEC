#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)

source /usr/bin/env.sh
source /etc/profile

jslisten set "killall batterymonitor"

/usr/bin/batterymonitor

clear > /dev/console
