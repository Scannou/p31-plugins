#!/usr/bin/env bash
# Fetch SDK submodules and dev tools. Safe to re-run.
set -euo pipefail
cd "$(dirname "$0")/.."

git submodule update --init
# The VST3 SDK's own submodules include vstgui, docs and tutorials we never
# build; fetch only what clap-wrapper and the validator need.
git -C libs/vst3sdk submodule update --init --depth 1 base pluginterfaces public.sdk cmake

# Our fixes to submodules, until they are upstream. Each patch is applied
# once: a patch that already reverses cleanly is in place.
for patch in patches/*/*.patch; do
    [[ -e "$patch" ]] || continue
    module="libs/$(basename "$(dirname "$patch")")"
    if git -C "$module" apply --reverse --check "$PWD/$patch" 2>/dev/null; then
        echo "already applied: $patch"
    else
        git -C "$module" apply "$PWD/$patch"
        echo "applied: $patch"
    fi
done

CLAP_VALIDATOR_VERSION=0.4.1
CLAP_VALIDATOR_ASSET=clap-validator-0.4.1-127-g152b982-macos-universal.zip
if [[ ! -x tools/clap-validator ]]; then
    mkdir -p tools
    tmp=$(mktemp -d)
    curl -fsSL -o "$tmp/cv.zip" \
        "https://github.com/free-audio/clap-validator/releases/download/${CLAP_VALIDATOR_VERSION}/${CLAP_VALIDATOR_ASSET}"
    # The zip wraps a tarball that holds binaries/clap-validator.
    unzip -q "$tmp/cv.zip" -d "$tmp/cv"
    tar xzf "$tmp"/cv/*.tar.gz -C "$tmp/cv"
    cp "$tmp/cv/binaries/clap-validator" tools/clap-validator
    chmod +x tools/clap-validator
    xattr -d com.apple.quarantine tools/clap-validator 2>/dev/null || true
    rm -rf "$tmp"
fi
tools/clap-validator --version
