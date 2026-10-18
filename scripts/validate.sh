#!/usr/bin/env bash
# Run every plugin validator against a build. Usage: scripts/validate.sh [build-dir]
# AU validation needs the .component installed in ~/Library/Audio/Plug-Ins/Components
# (P31_COPY_AFTER_BUILD=ON does that).
set -uo pipefail
cd "$(dirname "$0")/.."

BUILD=${1:-build/dev}
OUT="$BUILD/plugins"
PLUGINVAL=${PLUGINVAL:-/Applications/pluginval.app/Contents/MacOS/pluginval}
PLUGINS=("P31 Spectrum:P31S" "P31 Scope:P31V")
AU_MANUFACTURER=Scnu

failures=()
run() {
    local name=$1; shift
    echo "=== $name"
    if "$@"; then echo "--- PASS: $name"; else echo "--- FAIL: $name"; failures+=("$name"); fi
}

cmake --build "$BUILD" --target vst3_validator >/dev/null
VST3_VALIDATOR=$(find "$BUILD/validator-build" -type f -name validator -perm -u+x | head -1)

# Make the AU registrar pick up freshly copied components.
killall -9 AudioComponentRegistrar 2>/dev/null || true

for entry in "${PLUGINS[@]}"; do
    name=${entry%%:*}
    subtype=${entry#*:}
    run "clap-validator $name" tools/clap-validator validate "$OUT/$name.clap"
    run "vst3 validator $name" "$VST3_VALIDATOR" "$OUT/$name.vst3"
    run "auval $name" auval -strict -v aufx "$subtype" "$AU_MANUFACTURER"
    run "pluginval VST3 $name" "$PLUGINVAL" --strictness-level 10 --skip-gui-tests \
        --validate-in-process --validate "$OUT/$name.vst3"
    run "pluginval AU $name" "$PLUGINVAL" --strictness-level 10 --skip-gui-tests \
        --validate-in-process --validate "$HOME/Library/Audio/Plug-Ins/Components/$name.component"
done

echo
if [[ ${#failures[@]} -eq 0 ]]; then
    echo "All validators passed."
else
    printf 'FAILED: %s\n' "${failures[@]}"
    exit 1
fi
