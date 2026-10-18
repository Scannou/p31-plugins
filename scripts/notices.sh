#!/usr/bin/env bash
# Regenerate THIRD_PARTY_NOTICES.txt from the license files of the bundled
# submodules. Run after updating a submodule; package.sh copies the result
# into every bundle and the installer.
set -euo pipefail
cd "$(dirname "$0")/.."

out=THIRD_PARTY_NOTICES.txt
section() {  # name, used in, version, license text file or "-" for stdin
    printf '\n%s\n' "================================================================================"
    printf '%s%s\nUsed in: %s\n' "$1" "${3:+ $3}" "$2"
    printf '%s\n\n' "================================================================================"
    if [[ "$4" == "-" ]]; then cat; else cat "$4"; fi
}
version() { git -C "libs/$1" describe --tags 2>/dev/null || git -C "libs/$1" rev-parse --short HEAD; }

{
    echo "P31 plugins include the following third-party software."
    section "CLAP" "all formats" "$(version clap)" libs/clap/LICENSE
    section "clap-wrapper" "VST3, AU and standalone builds" "$(version clap-wrapper)" libs/clap-wrapper/LICENSE
    section "pffft" "all formats" "$(version pffft)" libs/pffft/LICENSE.txt
    section "VST3 SDK" "VST3" "$(version vst3sdk)" libs/vst3sdk/LICENSE.txt
    sed -n '1,31p' libs/clap-wrapper/libs/fmt/fmt/format.h | sed -e '1d' -e '$d' -e 's/^  //' |
        section "{fmt} (bundled with clap-wrapper)" "VST3" "" -
    section "Audio Unit SDK" "AU" "$(version AudioUnitSDK)" libs/AudioUnitSDK/LICENSE.txt
    section "RtAudio" "standalone apps" "$(version rtaudio)" libs/rtaudio/LICENSE
    section "RtMidi" "standalone apps" "$(version rtmidi)" libs/rtmidi/LICENSE
} > "$out"
echo "wrote $out ($(wc -l < "$out") lines)"
