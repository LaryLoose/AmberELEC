#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026-present AmberELEC (https://github.com/AmberELEC)
#
# Moonlight launcher for the EmulationStation "moonlight" category.
# Provides pairing and streaming for the Moonlight Embedded CLI.

. /etc/profile

export SDL_GAMECONTROLLERCONFIG_FILE="/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt"
export TERM=xterm-color
export DIALOGRC=/etc/amberelec.dialogrc

CONF="/storage/.config/moonlight/moonlight.conf"
GPTK="/usr/config/gptokeyb/settime.gptk"
CONFDIST="/storage/.config/distribution/configs/distribution.conf"
PLATFORM="moonlight"
BASE_DB="/storage/.config/SDL-GameControllerDB/gamecontrollerdb.txt"
MAP_DB="/storage/.config/moonlight/gamecontrollerdb.txt"
MAPPING_ARG=()

mkdir -p /storage/.config/moonlight
[ -f "${CONF}" ] || cp /usr/config/moonlight/moonlight.conf "${CONF}"

start_keys() { gptokeyb Moonlight.sh -c "${GPTK}" & }
stop_keys()  { kill -9 "$(pidof gptokeyb)" 2>/dev/null; }

console_on()  { echo -e '\033[?25h\033[?16;224;238c' > /dev/console; clear > /dev/console; }
console_off() { echo -e '\033[?25l' > /dev/console; clear > /dev/console; }

get_conf()  { sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*//p" "${CONF}" 2>/dev/null | tail -1; }
host_arg()  { local h; h="$(get_conf address)"; [ -n "${h}" ] && printf '%s' "${h}"; }

# Controller layout set via the ES options menu (moonlight.controller_layout).
get_setting() {
  local key="$1" val
  val=$(sed -n "s|^${PLATFORM}[\.-]${key}=\(.*\)|\1|p" "${CONFDIST}" 2>/dev/null | head -1)
  [ -z "${val}" ] && val=$(sed -n "s|^global[\.-].*${key}=\(.*\)|\1|p" "${CONFDIST}" 2>/dev/null | head -1)
  printf '%s' "${val}"
}

# Rewrite the SDL controller DB, swapping face buttons (and optionally
# shoulders/triggers), so Moonlight sends the chosen layout to the host.
gen_map() {
  [ -f "${BASE_DB}" ] || BASE_DB="/usr/share/moonlight/gamecontrollerdb.txt"
  [ -f "${BASE_DB}" ] || return 1
  awk -v swaplr="$1" 'BEGIN{FS=OFS=","}
    /^#/ || /^$/ { print; next }
    {
      out=$1","$2
      for(i=3;i<=NF;i++){
        tok=$i; p=index(tok,":")
        if(p==0){ if(tok!=""){out=out","tok}; continue }
        k=substr(tok,1,p-1); v=substr(tok,p+1)
        if(k=="a")k="b"; else if(k=="b")k="a"; else if(k=="x")k="y"; else if(k=="y")k="x"
        if(swaplr=="1"){
          if(k=="leftshoulder")k="lefttrigger"; else if(k=="lefttrigger")k="leftshoulder"
          else if(k=="rightshoulder")k="righttrigger"; else if(k=="righttrigger")k="rightshoulder"
        }
        out=out","k":"v
      }
      print out
    }' "${BASE_DB}" > "${MAP_DB}" 2>/dev/null && MAPPING_ARG=(-mapping "${MAP_DB}")
}

apply_controller_layout() {
  MAPPING_ARG=()
  case "$(get_setting controller_layout)" in
    xbox)                         gen_map 0 ;;
    xbox_swap_shoulders_triggers) gen_map 1 ;;
    *) : ;;  # nintendo / unset = native mapping
  esac
}

quit() { stop_keys; console_off; exit 0; }

# Run a network-only CLI action (pair/list/unpair); keep gptokeyb so A = Enter.
run_text() {
  clear > /dev/console
  { "$@"; echo; echo "Done - press A to return to the menu."; } > /dev/console 2>&1
  read -r _ < /dev/console
}

# Stream: fetch the host's app list, let the user pick one, then hand the
# gamepad to Moonlight. Avoids the CLI default app ("Steam"), which rarely
# matches the real app names ("Steam Big Picture", "Desktop", ...).
run_stream() {
  clear > /dev/console
  apply_controller_layout

  local host listing line
  host="$(host_arg)"
  echo "Searching for host and apps..." > /dev/console
  listing="$(moonlight list ${host} 2>&1)"
  [ -z "${host}" ] && host="$(printf '%s\n' "${listing}" | sed -n 's/^Connecting to \(.*\)\.\.\.$/\1/p' | head -1)"

  local -a apps=()
  while IFS= read -r line; do
    case "${line}" in
      [0-9]*'. '*) apps+=("$(( ${#apps[@]} / 2 + 1 ))" "${line#*. }") ;;
    esac
  done <<< "${listing}"

  if [ "${#apps[@]}" -eq 0 ]; then
    { echo; echo "No apps found, or the host is unreachable / not paired."; echo "Press A to return."; } > /dev/console
    read -r _ < /dev/console
    return
  fi

  local choice
  choice="$(dialog --title " Stream " --clear --cancel-label "Back" \
    --menu "Select a game/app to stream" 0 0 8 "${apps[@]}" 2>&1 > /dev/console)" || return

  local app="${apps[$(( (choice - 1) * 2 + 1 ))]}"
  stop_keys
  clear > /dev/console
  moonlight stream "${MAPPING_ARG[@]}" -app "${app}" ${host} > /dev/console 2>&1
  start_keys
}

console_on
start_keys

while true; do
  choice="$(dialog --title " Moonlight " --clear --no-cancel \
    --menu "NVIDIA GameStream / Sunshine client" 0 0 5 \
    1 "Stream" \
    2 "Pair with host" \
    3 "List apps on host" \
    4 "Unpair host" \
    5 "Quit" \
    2>&1 > /dev/console)" || quit

  case "${choice}" in
    1) run_stream ;;
    2) run_text moonlight pair $(host_arg) ;;
    3) run_text moonlight list $(host_arg) ;;
    4) run_text moonlight unpair $(host_arg) ;;
    5) quit ;;
  esac
done
