#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
output="$root/build-resources"
mkdir -p "$output/licenses" "$output/pdf-merge.iconset"
: "${QT_VERSION:?Set QT_VERSION to the release Qt version}"

# Keep notices in their original directory structure, including Chromium and
# PDFium's third-party notices. Qt PDF's sources are in qtwebengine.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
for module in qtbase qtsvg qtwayland qtwebengine; do
    archive="$module-everywhere-src-$QT_VERSION.tar.xz"
    url="https://download.qt.io/archive/qt/${QT_VERSION%.*}/$QT_VERSION/submodules/$archive"
    curl --fail --location --retry 3 "$url" -o "$work/$archive"
    curl --fail --location --retry 3 "$url.sha256" -o "$work/checksum"
    hash=$(awk 'NR == 1 {print $1}' "$work/checksum")
    (cd "$work" && printf '%s  %s\n' "$hash" "$archive" | sha256sum --check --strict)
    tar -tf "$work/$archive" | awk '
        tolower($0) ~ /(^|\/)([^\/]*(license|licence|copying|copyright|notice|authors)[^\/]*|qt_attribution\.json)$/ { print }
    ' > "$work/notices"
    test -s "$work/notices"
    tar -xf "$work/$archive" -C "$output/licenses" -T "$work/notices"
    printf '%s sources: %s\n' "$module" "$url" >> "$output/licenses/SOURCES.txt"
    rm "$work/$archive"
done

collect_dependency_notices()
{
    name=$1
    url=$2
    hash=$3
    archive="$work/$name.tar.gz"
    source="$work/$name"
    curl --fail --location --retry 3 "$url" -o "$archive"
    printf '%s  %s\n' "$hash" "$archive" | sha256sum --check --strict
    mkdir "$source"
    tar -xf "$archive" -C "$source" --strip-components=1
    cmake -DSOURCE_DIR="$source" -DDESTINATION="$output/licenses/$name" \
        -P "$root/packaging/collect-notices.cmake"
}
collect_dependency_notices qpdf \
    https://github.com/qpdf/qpdf/releases/download/v12.2.0/qpdf-12.2.0.tar.gz \
    b3d1575b2218badc3549d6977524bb0f8c468c6528eebc8967bbe3078cf2cace
collect_dependency_notices zlib \
    https://zlib.net/fossils/zlib-1.3.1.tar.gz \
    9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
collect_dependency_notices libjpeg-turbo \
    https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.1.0/libjpeg-turbo-3.1.0.tar.gz \
    9564c72b1dfd1d6fe6274c5f95a8d989b59854575d4bbee44ade7bc17aa9bc93

cat >> "$output/licenses/SOURCES.txt" <<'EOF'

Qt is dynamically linked under LGPLv3. You may replace the bundled Qt libraries
with compatible modified builds. PDF Merge's MIT license does not restrict
reverse engineering for debugging modifications to those libraries.

qpdf 12.2.0: https://github.com/qpdf/qpdf/releases/tag/v12.2.0
zlib 1.3.1: https://zlib.net/fossils/zlib-1.3.1.tar.gz
libjpeg-turbo 3.1.0: https://github.com/libjpeg-turbo/libjpeg-turbo/releases/tag/3.1.0
qpdf uses its native crypto provider and statically linked zlib/libjpeg-turbo.
The release dependency recipes are in packaging/dependencies/CMakeLists.txt.
EOF

for size in 16 32 128 256 512; do
    rsvg-convert -w "$size" -h "$size" "$root/packaging/pdf-merge.svg" \
        -o "$output/pdf-merge.iconset/icon_${size}x${size}.png"
    double=$((size * 2))
    rsvg-convert -w "$double" -h "$double" "$root/packaging/pdf-merge.svg" \
        -o "$output/pdf-merge.iconset/icon_${size}x${size}@2x.png"
done
convert "$output/pdf-merge.iconset/icon_256x256.png" \
    -define icon:auto-resize=256,128,64,48,32,16 "$output/pdf-merge.ico"
