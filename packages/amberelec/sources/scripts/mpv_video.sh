#!/bin/bash

# Pick the fastest playback path per video codec (mpv cannot switch vo at runtime,
# so the codec is probed up front with ffprobe):
#   RK3326 HW-decodable (VDPU2 + HEVC block) -> vo=gpu + rkmpp (VPU, zero-copy)
#   everything else (vp9, av1, ...)          -> vo=sdl (software decode; RGA does CSC/scale)

FILE="${1}"

# Video codecs the RK3326 can decode in hardware (must match ffmpeg's *_rkmpp decoders).
HW_CODECS="h264 hevc vp8 mpeg2video mpeg4 h263 mjpeg"

codec=""
if command -v ffprobe >/dev/null 2>&1; then
  # read builtin grabs just the first output line (no external head)
  read -r codec < <(ffprobe -v error -select_streams v:0 -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 "${FILE}" 2>/dev/null)
fi

# Default is software (sdl); only a positively matched HW codec switches to gpu+rkmpp.
# So no ffprobe / probe failure / unknown codec all stay on the safe software path.
use_hw=0
for c in ${HW_CODECS}; do
  [[ "${codec}" == "${c}" ]] && use_hw=1 && break
done

if [[ "${use_hw}" == "1" ]]; then
  MPV_ARGS=(--vo=gpu --hwdec=rkmpp --gpu-context=drm --gpu-hwdec-interop=drmprime --hwdec-codecs="${HW_CODECS// /,}")
else
  MPV_ARGS=(--vo=sdl --hwdec=no)
fi

exec /usr/bin/mpv --input-ipc-server=/tmp/mpvsocket "${MPV_ARGS[@]}" "${FILE}"
