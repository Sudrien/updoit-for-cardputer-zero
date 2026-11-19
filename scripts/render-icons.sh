#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Renders the launcher/store icons and the update-count badge variants from
# assets/images/updoit-icon-skull.svg. Needs rsvg-convert (librsvg2-bin).
# The PNGs are committed, so a normal build does not need this script.
#
#   scripts/render-icons.sh

set -eu
cd "$(dirname "$0")/.."
src=assets/images/updoit-icon-skull.svg
out=assets/images
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

rsvg-convert -w 100 -h 100 "$src" -o "$out/updoit-for-cardputer-zero_100.png"
rsvg-convert -w 80 -h 80 "$src" -o "$out/updoit-for-cardputer-zero_80.png"
cp "$out/updoit-for-cardputer-zero_100.png" "$out/updoit-for-cardputer-zero.png"
rsvg-convert -w 256 -h 256 "$src" -o "$out/updoit-store-icon.png"

# Badge variants for the launcher: 0 (no badge), 1..9, and 9+.
badge() { # $1 = label, $2 = output name
    size=56
    [ "${#1}" -gt 1 ] && size=44
    sed "s#</svg>#<g><circle cx=\"198\" cy=\"198\" r=\"42\" fill=\"\#e5383b\" stroke=\"\#ffffff\" stroke-width=\"7\"/><text x=\"198\" y=\"203\" font-family=\"DejaVu Sans, sans-serif\" font-weight=\"bold\" font-size=\"$size\" fill=\"\#ffffff\" text-anchor=\"middle\" dominant-baseline=\"central\">$1</text></g></svg>#" \
        "$src" >"$tmp/$2.svg"
    rsvg-convert -w 100 -h 100 "$tmp/$2.svg" -o "$out/badge/$2.png"
}
mkdir -p "$out/badge"
cp "$out/updoit-for-cardputer-zero_100.png" "$out/badge/updoit-badge-0.png"
for n in 1 2 3 4 5 6 7 8 9; do
    badge "$n" "updoit-badge-$n"
done
badge "9+" "updoit-badge-9plus"
echo "rendered icons into $out"
