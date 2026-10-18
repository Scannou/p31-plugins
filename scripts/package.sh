#!/usr/bin/env bash
# Build the macOS installer: dist/P31-Plugins-<version>.pkg with CLAP, VST3,
# AU and the standalone apps as separate choices.
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
PLUGINS=("P31 Spectrum" "P31 Scope")
DIST=dist
STAGE=$DIST/stage
PKG=$DIST/P31-Plugins-$VERSION.pkg

step() { printf '\n== %s\n' "$*"; }

if [[ $SKIP_BUILD == 0 ]]; then
    step "Build (Release, universal)"
    cmake --preset release
    cmake --build --preset release
fi

step "Stage bundles"
rm -rf "$DIST"
mkdir -p "$STAGE"/{CLAP,VST3,Components,Applications} "$DIST/pkgs" "$DIST/resources"
for p in "${PLUGINS[@]}"; do
    ditto "$BUILD_DIR/plugins/$p.clap" "$STAGE/CLAP/$p.clap"
    ditto "$BUILD_DIR/plugins/$p.vst3" "$STAGE/VST3/$p.vst3"
    ditto "$BUILD_DIR/plugins/$p.component" "$STAGE/Components/$p.component"
    ditto "$BUILD_DIR/plugins/$p.app" "$STAGE/Applications/$p.app"
done
# Every bundle carries the licenses.
for b in "$STAGE"/*/*.{clap,vst3,component,app}; do
    mkdir -p "$b/Contents/Resources"
    cp LICENSE "$b/Contents/Resources/LICENSE.txt"
    cp THIRD_PARTY_NOTICES.txt "$b/Contents/Resources/"
done

step "Sign bundles (${SIGN_ID:-ad-hoc})"
for b in "$STAGE"/*/*.{clap,vst3,component,app}; do
    args=(--force --sign "${SIGN_ID:--}")
    if [[ -n "$SIGN_ID" ]]; then args+=(--timestamp --options runtime); fi
    # The standalone apps record from the input device chosen in them.
    if [[ "$b" == *.app ]]; then args+=(--entitlements scripts/standalone.entitlements); fi
    codesign "${args[@]}" "$b"
    codesign --verify --strict "$b"
done

step "Component packages"
cat > "$DIST/postinstall-au" <<'SH'
#!/bin/sh
# Make hosts see new or updated Audio Units without a log out.
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
SH
chmod +x "$DIST/postinstall-au"
mkdir -p "$DIST/scripts-au"
mv "$DIST/postinstall-au" "$DIST/scripts-au/postinstall"

component() {  # id, stage subdir, install location, [scripts dir]
    local id=$1 dir=$2 location=$3 scripts=${4:-}
    local plist="$DIST/pkgs/$id.plist"
    pkgbuild --analyze --root "$STAGE/$dir" "$plist" >/dev/null
    # Installed bundles must land where we say, not "relocate" onto a copy
    # the installer finds elsewhere on the disk (e.g. a build folder).
    local count
    count=$(/usr/libexec/PlistBuddy -c 'Print' "$plist" | grep -c 'RootRelativeBundlePath' || true)
    for ((i = 0; i < count; i++)); do
        /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" 2>/dev/null || true
        /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    done
    local args=(--root "$STAGE/$dir" --component-plist "$plist" --identifier "com.scannou.p31.$id"
                --version "$VERSION" --install-location "$location")
    if [[ -n "$scripts" ]]; then args+=(--scripts "$scripts"); fi
    pkgbuild "${args[@]}" "$DIST/pkgs/$id.pkg" >/dev/null
    echo "$id -> $location"
}
component clap CLAP /Library/Audio/Plug-Ins/CLAP
component vst3 VST3 /Library/Audio/Plug-Ins/VST3
component au Components /Library/Audio/Plug-Ins/Components "$DIST/scripts-au"
component apps Applications /Applications

step "Installer"
{
    cat LICENSE
    printf '\n\n'
    cat THIRD_PARTY_NOTICES.txt
} > "$DIST/resources/license.txt"
cat > "$DIST/resources/welcome.txt" <<EOF
P31 Plugins $VERSION

P31 Spectrum: real-time spectrum analyzer.
P31 Scope: vectorscope with tri-band phase correlation.

Both pass audio through untouched. Choose "Customize" to pick formats: CLAP,
VST3 and Audio Unit plugins, and standalone apps that analyse an input device.
EOF

cat > "$DIST/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>P31 Plugins $VERSION</title>
    <welcome file="welcome.txt" mime-type="text/plain"/>
    <license file="license.txt" mime-type="text/plain"/>
    <options customize="allow" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <volume-check>
        <allowed-os-versions><os-version min="12.0"/></allowed-os-versions>
    </volume-check>
    <choices-outline>
        <line choice="clap"/>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="apps"/>
    </choices-outline>
    <choice id="clap" title="CLAP plugins" description="/Library/Audio/Plug-Ins/CLAP">
        <pkg-ref id="com.scannou.p31.clap"/>
    </choice>
    <choice id="vst3" title="VST3 plugins" description="/Library/Audio/Plug-Ins/VST3">
        <pkg-ref id="com.scannou.p31.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit plugins" description="/Library/Audio/Plug-Ins/Components (Logic, GarageBand, Live and others)">
        <pkg-ref id="com.scannou.p31.au"/>
    </choice>
    <choice id="apps" title="Standalone apps" description="/Applications: analyse any input device without a DAW">
        <pkg-ref id="com.scannou.p31.apps"/>
    </choice>
    <pkg-ref id="com.scannou.p31.clap" version="$VERSION">clap.pkg</pkg-ref>
    <pkg-ref id="com.scannou.p31.vst3" version="$VERSION">vst3.pkg</pkg-ref>
    <pkg-ref id="com.scannou.p31.au" version="$VERSION">au.pkg</pkg-ref>
    <pkg-ref id="com.scannou.p31.apps" version="$VERSION">apps.pkg</pkg-ref>
</installer-gui-script>
EOF

args=(--distribution "$DIST/distribution.xml" --resources "$DIST/resources" --package-path "$DIST/pkgs")
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
