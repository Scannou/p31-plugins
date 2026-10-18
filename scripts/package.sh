#!/usr/bin/env bash
# Build the macOS installer: dist/P31-Plugins-<version>.pkg. Each plugin is a
# group of choices with one box per format (CLAP, VST3, Audio Unit, standalone
# app), so users install only what they want.
#
#   scripts/package.sh [--skip-build] [--build-dir DIR]
#
# Signing and notarization are optional; without them the bundles are
# ad-hoc signed and the package is unsigned (fine for local testing only).
#
#   P31_SIGN_ID          "Developer ID Application: Name (TEAMID)" for the bundles
#   P31_INSTALLER_ID     "Developer ID Installer: Name (TEAMID)" for the .pkg
#   Notarization, either
#   P31_NOTARY_PROFILE   a notarytool keychain profile (xcrun notarytool store-credentials)
#   or an App Store Connect API key:
#   P31_NOTARY_KEY       path to the .p8 file
#   P31_NOTARY_KEY_ID    key ID
#   P31_NOTARY_ISSUER    issuer ID
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR=build/release
SKIP_BUILD=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=1 ;;
        --build-dir) BUILD_DIR="$2"; shift ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
    shift
done

SIGN_ID=${P31_SIGN_ID:-}
INSTALLER_ID=${P31_INSTALLER_ID:-}
NOTARIZE=0
if [[ -n "${P31_NOTARY_PROFILE:-}" || -n "${P31_NOTARY_KEY:-}" ]]; then NOTARIZE=1; fi
if [[ $NOTARIZE == 1 && ( -z "$SIGN_ID" || -z "$INSTALLER_ID" ) ]]; then
    echo "notarization needs P31_SIGN_ID and P31_INSTALLER_ID" >&2
    exit 2
fi

VERSION=$(sed -n 's/^project(p31-plugins VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
DIST=dist
STAGE=$DIST/stage
PKG=$DIST/P31-Plugins-$VERSION.pkg

# What the installer offers. Each plugin x format is its own component
# package (com.scannou.p31.<plugin>.<format>).
#   plugin: id | name | description
PLUGIN_ROWS=(
    "spectrum|P31 Spectrum|Real-time spectrum analyzer"
    "scope|P31 Scope|Vectorscope with tri-band phase correlation"
)
#   format: id | title | bundle extension | install location | hosts
FORMAT_ROWS=(
    "clap|CLAP|clap|/Library/Audio/Plug-Ins/CLAP|Bitwig, Reaper and other CLAP hosts"
    "vst3|VST3|vst3|/Library/Audio/Plug-Ins/VST3|Live, Cubase, Reaper and most other hosts"
    "au|Audio Unit|component|/Library/Audio/Plug-Ins/Components|Logic, GarageBand, Live and other AU hosts"
    "app|Standalone app|app|/Applications|analyse any input device without a DAW"
)
field() { echo "$1" | cut -d'|' -f"$2"; }

step() { printf '\n== %s\n' "$*"; }

if [[ $SKIP_BUILD == 0 ]]; then
    step "Build (Release, universal)"
    cmake --preset release
    cmake --build --preset release
fi

step "Stage bundles"
rm -rf "$DIST"
mkdir -p "$DIST/pkgs" "$DIST/resources"
for prow in "${PLUGIN_ROWS[@]}"; do
    pid=$(field "$prow" 1); pname=$(field "$prow" 2)
    for frow in "${FORMAT_ROWS[@]}"; do
        fid=$(field "$frow" 1); ext=$(field "$frow" 3)
        mkdir -p "$STAGE/$pid/$fid"
        ditto "$BUILD_DIR/plugins/$pname.$ext" "$STAGE/$pid/$fid/$pname.$ext"
    done
done
# Every bundle carries the licenses.
for b in "$STAGE"/*/*/*.{clap,vst3,component,app}; do
    mkdir -p "$b/Contents/Resources"
    cp LICENSE "$b/Contents/Resources/LICENSE.txt"
    cp THIRD_PARTY_NOTICES.txt "$b/Contents/Resources/"
done

step "Sign bundles (${SIGN_ID:-ad-hoc})"
for b in "$STAGE"/*/*/*.{clap,vst3,component,app}; do
    args=(--force --sign "${SIGN_ID:--}")
    if [[ -n "$SIGN_ID" ]]; then args+=(--timestamp --options runtime); fi
    # The standalone apps record from the input device chosen in them.
    if [[ "$b" == *.app ]]; then args+=(--entitlements scripts/standalone.entitlements); fi
    codesign "${args[@]}" "$b"
    codesign --verify --strict "$b"
done

step "Component packages"
mkdir -p "$DIST/scripts-au"
{
    echo '#!/bin/sh'
    echo '# Make hosts see new or updated Audio Units without a log out.'
    echo 'killall -9 AudioComponentRegistrar 2>/dev/null || true'
    echo 'exit 0'
} > "$DIST/scripts-au/postinstall"
chmod +x "$DIST/scripts-au/postinstall"

component() {  # plugin id, format id, install location, [scripts dir]
    local id="$1.$2" root="$STAGE/$1/$2" location=$3 scripts=${4:-}
    local plist="$DIST/pkgs/$id.plist"
    pkgbuild --analyze --root "$root" "$plist" >/dev/null
    # Installed bundles must land where we say, not "relocate" onto a copy
    # the installer finds elsewhere on the disk (e.g. a build folder).
    local count
    count=$(/usr/libexec/PlistBuddy -c 'Print' "$plist" | grep -c 'RootRelativeBundlePath' || true)
    for ((i = 0; i < count; i++)); do
        /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" 2>/dev/null || true
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    done
    local args=(--root "$root" --component-plist "$plist" --identifier "com.scannou.p31.$id"
                --version "$VERSION" --install-location "$location")
    if [[ -n "$scripts" ]]; then args+=(--scripts "$scripts"); fi
    pkgbuild "${args[@]}" "$DIST/pkgs/$id.pkg" >/dev/null
    echo "$id -> $location"
}
for prow in "${PLUGIN_ROWS[@]}"; do
    pid=$(field "$prow" 1)
    for frow in "${FORMAT_ROWS[@]}"; do
        fid=$(field "$frow" 1); location=$(field "$frow" 4)
        if [[ "$fid" == au ]]; then
            component "$pid" "$fid" "$location" "$DIST/scripts-au"
        else
            component "$pid" "$fid" "$location"
        fi
    done
done

step "Installer"
{
    cat LICENSE
    printf '\n\n'
    cat THIRD_PARTY_NOTICES.txt
} > "$DIST/resources/license.txt"
{
    echo "P31 Plugins $VERSION"
    echo
    echo "P31 Spectrum: real-time spectrum analyzer."
    echo "P31 Scope: vectorscope with tri-band phase correlation."
    echo
    echo "Both pass audio through untouched. The next steps let you pick which"
    echo "plugins to install and in which formats: CLAP, VST3, Audio Unit, and"
    echo "standalone apps that analyse an input device."
} > "$DIST/resources/welcome.txt"

# Choices: one group per plugin, one box per format inside it.
xml=$DIST/distribution.xml
{
    echo '<?xml version="1.0" encoding="utf-8"?>'
    echo '<installer-gui-script minSpecVersion="2">'
    echo "    <title>P31 Plugins $VERSION</title>"
    echo '    <welcome file="welcome.txt" mime-type="text/plain"/>'
    echo '    <license file="license.txt" mime-type="text/plain"/>'
    echo '    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>'
    echo '    <domains enable_localSystem="true"/>'
    echo '    <volume-check>'
    echo '        <allowed-os-versions><os-version min="12.0"/></allowed-os-versions>'
    echo '    </volume-check>'
    echo '    <choices-outline>'
    for prow in "${PLUGIN_ROWS[@]}"; do
        pid=$(field "$prow" 1)
        echo "        <line choice=\"$pid\">"
        for frow in "${FORMAT_ROWS[@]}"; do
            echo "            <line choice=\"$pid.$(field "$frow" 1)\"/>"
        done
        echo '        </line>'
    done
    echo '    </choices-outline>'
    for prow in "${PLUGIN_ROWS[@]}"; do
        pid=$(field "$prow" 1); pname=$(field "$prow" 2); pdesc=$(field "$prow" 3)
        echo "    <choice id=\"$pid\" title=\"$pname\" description=\"$pdesc\"/>"
        for frow in "${FORMAT_ROWS[@]}"; do
            fid=$(field "$frow" 1); ftitle=$(field "$frow" 2)
            location=$(field "$frow" 4); hosts=$(field "$frow" 5)
            echo "    <choice id=\"$pid.$fid\" title=\"$ftitle\" description=\"$location ($hosts)\">"
            echo "        <pkg-ref id=\"com.scannou.p31.$pid.$fid\"/>"
            echo '    </choice>'
        done
    done
    for prow in "${PLUGIN_ROWS[@]}"; do
        pid=$(field "$prow" 1)
        for frow in "${FORMAT_ROWS[@]}"; do
            fid=$(field "$frow" 1)
            echo "    <pkg-ref id=\"com.scannou.p31.$pid.$fid\" version=\"$VERSION\">$pid.$fid.pkg</pkg-ref>"
        done
    done
    echo '</installer-gui-script>'
} > "$xml"

args=(--distribution "$xml" --resources "$DIST/resources" --package-path "$DIST/pkgs")
if [[ -n "$INSTALLER_ID" ]]; then args+=(--sign "$INSTALLER_ID" --timestamp); fi
productbuild "${args[@]}" "$PKG"

if [[ $NOTARIZE == 1 ]]; then
    step "Notarize"
    if [[ -n "${P31_NOTARY_PROFILE:-}" ]]; then
        auth=(--keychain-profile "$P31_NOTARY_PROFILE")
    else
        auth=(--key "$P31_NOTARY_KEY" --key-id "$P31_NOTARY_KEY_ID" --issuer "$P31_NOTARY_ISSUER")
    fi
    xcrun notarytool submit "$PKG" "${auth[@]}" --wait
    xcrun stapler staple "$PKG"
    spctl --assess --type install -v "$PKG"
fi

step "Done"
ls -lh "$PKG"
if [[ -z "$INSTALLER_ID" ]]; then echo "unsigned package: for local testing only"; fi
