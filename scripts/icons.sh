#!/usr/bin/env bash
# Render the standalone apps' icons: assets/brand/app-icons/<name>.svg ->
# <name>.icns next to it. The .icns files are committed, so this only runs
# after an icon's SVG changed. Needs Inkscape (brew install --cask inkscape).
set -euo pipefail
cd "$(dirname "$0")/../assets/brand/app-icons"

INKSCAPE=${INKSCAPE:-inkscape}
command -v "$INKSCAPE" >/dev/null || { echo "inkscape not found" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

for svg in *.svg; do
    name=${svg%.svg}
    set_dir="$tmp/$name.iconset"
    mkdir -p "$set_dir"
    # Every size is rendered from the SVG, not scaled down from the largest.
    for size in 16 32 128 256 512; do
        "$INKSCAPE" "$svg" --export-type=png -w "$size" -h "$size" \
            --export-filename="$set_dir/icon_${size}x${size}.png" 2>/dev/null
        "$INKSCAPE" "$svg" --export-type=png -w $((size * 2)) -h $((size * 2)) \
            --export-filename="$set_dir/icon_${size}x${size}@2x.png" 2>/dev/null
    done
    iconutil --convert icns --output "$name.icns" "$set_dir"
    echo "$name.icns"
done
