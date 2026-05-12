#!/bin/bash

# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2019-present Shanti Gilbert (https://github.com/shantigilbert)

# Source predefined functions and variables
. /etc/profile

# Time logging helper
TIMELOG_FILE="/tmp/autostart-times.log"
log_time() {
  [ -z "${TIMELOG_FILE}" ] && return
  echo "$(date +'%Y-%m-%d %H:%M:%S') $*" >> "${TIMELOG_FILE}"
}
: > "${TIMELOG_FILE}"
log_time "autostart.sh:start"

# Hint file helper functions
hint_should_run() {
  local key=$1
  # If postupdate is triggered, always run to ensure everything is up to date
  if [ "UPDATE" == "$(cat /storage/.config/boot.hint 2>/dev/null)" ]; then
    return 0
  fi
  local file="/storage/.config/${key}.hint"
  [ ! -f "$file" ]
}

mark_hint_done() {
  local key=$1
  local file="/storage/.config/${key}.hint"
  local desc
  case "$key" in
    "game_data") desc="game data migration" ;;
    "distribution") desc="distribution sync" ;;
    *) desc="$key" ;;
  esac
  echo "delete this file to trigger $desc at the next start" > "$file"
}

# Show splash logo
log_time "show_splash:start"
/usr/bin/show_splash.sh &
log_time "show_splash:started"

DEVICE=$(tr -d '\0' < /sys/firmware/devicetree/base/model)

# Set performance mode to start the boot
log_time "performance:start"
performance
log_time "performance:done"

# write logs to tmpfs not the sdcard
log_time "logdir_setup:start"
mkdir /tmp/logs
mkdir -p /storage/.config/emulationstation/logs/
if [ ! -L "/tmp/logs/retroarch" ]; then
  ln -s /storage/roms/gamedata/retroarch/logs/ /tmp/logs/retroarch
fi
if [ ! -L "/tmp/logs/emulationstation" ]; then
  ln -s /storage/.config/emulationstation/logs/ /tmp/logs/emulationstation
fi
log_time "logdir_setup:done"

# Apply some kernel tuning
log_time "kernel_tuning:start"
sysctl vm.swappiness=1
log_time "kernel_tuning:done"

# Restore config if backup exists
log_time "restore_backup:check"
BPATH="/storage/roms/backup/"
BACKUPFILE="${BPATH}/AmberELEC_BACKUP.zip"

if [ -e "${BPATH}/.restore" ]
then
  if [ -f "${BACKUPFILE}" ]; then
    echo -en '\e[0;0H\e[37mRestoring backup and rebooting...\e[0m' >/dev/console
    unzip -o ${BACKUPFILE} -d /
    rm ${BACKUPFILE}
    log_time "restore_backup:done"
    systemctl reboot
  fi
fi
log_time "restore_backup:done"

# Restore identity if it exists from a factory reset
log_time "restore_identity:check"
IDENTITYFILE="${BPATH}/identity.tar.gz"

if [ -e "${IDENTITYFILE}" ]
then
  cd /
  tar -xvzf ${IDENTITYFILE} >${BPATH}/restore.log
  rm ${IDENTITYFILE}
  echo -en '\e[0;0H\e[37mIdentity restored, rebooting...\e[0m' >/dev/console
  log_time "restore_identity:done"
  systemctl reboot
fi
log_time "restore_identity:done"

if [ ! -e "/storage/.newcfg" ]
then
  log_time "init_message:show"
  echo -en '\e[0;0H\e[37mPlease wait, initializing system...\e[0m' >/dev/console
fi
log_time "init_message:done"

# It seems some slow SDcards have a problem creating the symlink on time :/
CONFIG_DIR="/storage/.emulationstation"
CONFIG_DIR2="/storage/.config/emulationstation"

log_time "config_symlink:start"
if [ ! -L "$CONFIG_DIR" ]; then
  ln -sf $CONFIG_DIR2 $CONFIG_DIR
fi
log_time "config_symlink:done"

# Setup default artbook symlink for use in ES
log_time "theme_symlink:start"
DEFAULT_THEME_USR=/usr/config/emulationstation/themes/es-theme-art-book-next/
DEFAULT_THEME_STORAGE=/storage/.config/emulationstation/themes/es-theme-art-book-next-default
if [ ! -e "$DEFAULT_THEME_STORAGE" ]; then
  ln -s $DEFAULT_THEME_USR $DEFAULT_THEME_STORAGE
fi
log_time "theme_symlink:done"

log_time "distribution_sync:check"
if hint_should_run "distribution"; then
  log_time "distribution_sync:start"
  # Create the distribution directory if it doesn't exist, sync it if it does
  if [ ! -d "/storage/.config/distribution" ]
  then
    log_time "rsync:distribution:create:start"
    rsync -a /usr/config/distribution/ /storage/.config/distribution/ &
  else
    log_time "rsync:distribution:update:start"
    rsync -a --delete --exclude=custom_start.sh --exclude=configs --exclude=lzdoom.ini --exclude=gzdoom.ini --exclude=raze.ini --exclude=ecwolf.cfg /usr/config/distribution/ /storage/.config/distribution/ &
  fi

  # Clean cache garbage when boot up.
  log_time "rsync:cache_cleanup:start"
  rsync -a --delete /tmp/cache/ /storage/.cache/cores/ &

  # Copy in build metadata
  log_time "rsync:build_metadata:start"
  rsync /usr/config/.OS* /storage/.config &

  # Copy remappings
  log_time "rsync:remappings:start"
  rsync --ignore-existing -raz /usr/config/remappings/* /storage/remappings/ &

  # Move ports to the GAMES volume
  #rsync -a --exclude gamelist.xml /usr/config/ports/* /storage/roms/homebrew &

  # Sync ES locale if missing
  if [ ! -d "/storage/.config/emulationstation/locale" ]
  then
    log_time "rsync:locale:start"
    rsync -a /usr/config/locale/ /storage/.config/emulationstation/locale/ &
  fi

  # Wait for the rsync processes to finish.
  wait
  log_time "distribution_sync:done"
  mark_hint_done "distribution"
fi
log_time "distribution_sync:check_done"

#if [ ! -e "/storage/roms/homebrew/gamelist.xml" ]
#then
#  cp -f /usr/config/ports/gamelist.xml /storage/roms/homebrew
#fi

# End Automatic updates

# restart volume control service
log_time "volume_service:start"
systemctl stop volume; systemctl start volume &
log_time "volume_service:done"

# start services
log_time "startservices:start"
/usr/bin/startservices.sh &
log_time "startservices:started"

# Migrate game data to the games partition
log_time "game_data_migration:check"
if hint_should_run "game_data"; then
  log_time "game_data_migration:start"
  GAMEDATA="/storage/roms/gamedata"

  if [ ! -d "${GAMEDATA}" ]; then mkdir -p "${GAMEDATA}"; fi

  for GAME in ppsspp dosbox retroarch hatari
  do
    # Migrate or copy fresh data
    if [ ! -d "${GAMEDATA}/${GAME}" ]; then
      if [ -d "/storage/.config/${GAME}" ]; then
        mv "/storage/.config/${GAME}" "${GAMEDATA}/${GAME}"
      else
        log_time "rsync:game_data:${GAME}:start"
        rsync -a "/usr/config/${GAME}/" "${GAMEDATA}/${GAME}/"
        log_time "rsync:game_data:${GAME}:done"
      fi
    fi

    # Link the original location to the new data location
    if [ ! -L "/storage/.config/${GAME}" ]
    then
      rm -rf "/storage/.config/${GAME}" 2>/dev/null
      ln -sf "${GAMEDATA}/${GAME}" "/storage/.config/${GAME}"
    fi
  done
  # Sync ppsspp assets
  log_time "ppsspp_assets:start"
  if [ -d "${GAMEDATA}/ppsspp" ]
  then
    log_time "rsync:ppsspp_assets:start"
    rsync -a "/usr/config/ppsspp/assets" "${GAMEDATA}/ppsspp/"
    log_time "rsync:ppsspp_assets:done"
  fi
  log_time "ppsspp_assets:done"

  log_time "drastic_dir:start"
  # Create drastic gamedata folder
  if [ ! -d "${GAMEDATA}/drastic" ]
  then
    mkdir -p "${GAMEDATA}/drastic"
    ln -sf "${GAMEDATA}/drastic" "/storage/drastic"
  fi
  log_time "drastic_dir:done"

  log_time "remappings_migration:start"
  # Controller remaps
  if [ ! -d "${GAMEDATA}/remappings" ]
  then
    if [ -d "/storage/remappings" ]
    then
      mv "/storage/remappings" "${GAMEDATA}/remappings"
    else
      mkdir -p "${GAMEDATA}/remappings"
    fi
  fi

  if [ ! -L "/storage/remappings" ]
  then
     rm -rf "/storage/remappings" 2>/dev/null
     ln -sf "${GAMEDATA}/remappings" "/storage/remappings"
  fi
  log_time "remappings_migration:done"
  mark_hint_done "game_data"
  log_time "game_data_migration:done"
fi
log_time "game_data_migration:check_done"

## Only call postupdate once after an UPDATE
log_time "postupdate:check"
if [ "UPDATE" == "$(cat /storage/.config/boot.hint)" ]; then
	echo -en '\e[0;0H\e[37mExecuting postupdate...\e[0m' >/dev/console
	log_time "postupdate:start"
	/usr/bin/postupdate.sh

  log_time "hide_tools:start"
  # hide tools entries
  if [ "$EE_DEVICE" == "RG351MP" ]; then
    if [ "$DEVICE" == "PowKiddy Magicx XU10" ]  || [ "$DEVICE" == "SZDiiER D007 Plus" ]; then
      xmlstarlet ed -L -u "//game[path='./display_fix.sh']/hidden" -v "true" /storage/.config/distribution/modules/gamelist.xml
      xmlstarlet ed -L -u "//game[path='./joyleds_conf.sh']/hidden" -v "false" /storage/.config/distribution/modules/gamelist.xml
    else
      xmlstarlet ed -L -u "//game[path='./display_fix.sh']/hidden" -v "false" /storage/.config/distribution/modules/gamelist.xml
      xmlstarlet ed -L -u "//game[path='./joyleds_conf.sh']/hidden" -v "true" /storage/.config/distribution/modules/gamelist.xml
    fi
  fi
  log_time "hide_tools:done"
  
	echo "OK" > /storage/.config/boot.hint
	log_time "postupdate:done"
fi
log_time "postupdate:check_done"

log_time "sync:start"
sync &
log_time "sync:started"

log_time "custom_start_before:start"
# run custom_start before FE scripts
/storage/.config/custom_start.sh before
log_time "custom_start_before:done"

log_time "brightness_restore:start"
# Restore last saved brightness
BRIGHTNESS=$(get_ee_setting system.brightness)
if [[ ! "${BRIGHTNESS}" =~ [0-9] ]]; then BRIGHTNESS=100; fi

# Ensure user doesn't get "locked out" with super low brightness
if [[ "${BRIGHTNESS}" -lt "3" ]]; then BRIGHTNESS=3; fi

BRIGHTNESS=$(printf "%.0f" ${BRIGHTNESS})
echo ${BRIGHTNESS} > /sys/class/backlight/backlight/brightness
set_ee_setting system.brightness ${BRIGHTNESS}
log_time "brightness_restore:done"

log_time "wifi_disable:start"
# If the WIFI adapter isn't enabled, disable it on startup
# to soft block the radio and save a bit of power.
if [ "$(get_ee_setting wifi.enabled)" == "0" ]
then
  connmanctl disable wifi
  # Power down the WIFI device
  if [ "$DEVICE" == "Anbernic RG552" ]; then
    echo 0 > /sys/class/gpio/gpio113/value
  elif [ "$DEVICE" == "Anbernic RG351P" ]; then
    echo 0 > /sys/class/gpio/gpio110/value
  else
    echo 0 > /sys/class/gpio/gpio5/value
  fi
fi
log_time "wifi_disable:done"

log_time "wifi_internal:start"
if [ "$(get_ee_setting wifi.internal.disabled)" == "1" ]
then
  /usr/bin/batocera-internal-wifi disable-no-refresh
fi
log_time "wifi_internal:done"

rm -f "/storage/.config/device" 2>/dev/null
if [ "$DEVICE" == "Anbernic RG351MP" ]; then
  VOLT1=$(cat /sys/bus/iio/devices/iio:device0/in_voltage1_raw)
  VOLT2=$(cat /sys/bus/iio/devices/iio:device0/in_voltage2_raw)
  if (( ${VOLT2} < 500 )); then
    if ((${VOLT1} >= 450 && ${VOLT1} <= 800)); then
      echo "R3xS" > /storage/.config/device
    elif ((${VOLT1} >= 950 && ${VOLT1} <= 1035)); then
      echo "R33S" > /storage/.config/device
    else
      echo "Unknown" > /storage/.config/device
    fi
  fi
fi

log_time "speaker_path:start"
if [ "$DEVICE" == "Anbernic RG351MP" ] || [ "$DEVICE" == "PowKiddy Magicx XU10" ]; then
	amixer -c 0 cset iface=MIXER,name='Playback Path' SPK_HP
fi
log_time "speaker_path:done"

# Initialize audio so the softvol mixer is created and audio is allowed to be changed
# - This is the shortest, totally silent .wav I could create with audacity - duration is .001 seconds
log_time "audio_init:start"
aplay /usr/bin/emustation-config-init.wav

if [ "$EE_DEVICE" == "RG552" ] || [[ "$EE_DEVICE" =~ RG351 ]]; then
  # For some reason the audio is being reseted to 100 at boot, so we reapply the saved settings here
  /usr/bin/odroidgoa_utils.sh vol $(get_ee_setting "audio.volume")
fi
log_time "audio_init:done"

# restore last played game
log_time "restore_lastgame:check"
timeout=60 #ms
elapsed=0
if [ -f /storage/.config/lastgame ]; then
  echo -en '\e[0;0H\e[37mRestoring the last running game...\e[0m' >/dev/console
  if [ "$(get_ee_setting retroachievements)" = "1" ]; then
    if [[ $(ls /sys/class/net | grep -E '^(wlan|eth)[0-9]+$') ]]; then
      while [[ ! $(ip route | grep default) ]]; do
          if [[ $elapsed -ge 10 ]]; then
            if [[ $elapsed -ge $timeout ]]; then
              break
            else
              echo -en '\e[0;0H\e[37m\nRetroachievements are enabled...\nWaiting for network to become available...\e[0m' >/dev/console
            fi
          fi
          sleep 0.1
          ((elapsed++))
      done
    fi
  fi
  command=`cat /storage/.config/lastgame`
  rm -rf /storage/.config/lastgame
  if [[ $command == *"autosave 0"* ]]; then
    sh -c -- "$command" &
    sleep 8
    /usr/bin/retroarch --command LOAD_STATE
  else
    sh -c -- "$command"
  fi
fi
log_time "restore_lastgame:done"

# What to start at boot?
DEFE=$(get_ee_setting ee_boot)

log_time "boot_target:start"
case "$DEFE" in
"Retroarch")
        rm -rf /var/lock/start.retro
        touch /var/lock/start.retro
        systemctl start retroarch
        ;;
*)
        rm /var/lock/start.games
        touch /var/lock/start.games
        systemctl start emustation
        ;;
esac
log_time "boot_target:done"

# run custom_start ending scripts
log_time "custom_start_after:start"
/storage/.config/custom_start.sh after
log_time "custom_start_after:done"

# default to ondemand/powersave in EmulationStation
POWERSAVE_ES=$(get_ee_setting powersave_es)
if [ "${POWERSAVE_ES}" == "1" ]; then
  es_powersave &
else
  es_ondemand &
fi
log_time "powersave_setting:done"

clear > /dev/console
log_time "autostart.sh:done"
