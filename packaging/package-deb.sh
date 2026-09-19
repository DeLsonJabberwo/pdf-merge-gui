#!/usr/bin/env bash
set -euo pipefail

if (($# != 3)); then
    echo "usage: $0 APPDIR VERSION OUTPUT" >&2
    exit 2
fi
appdir=$(realpath "$1")
version=$2
output=$(realpath -m "$3")
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT

mkdir -p "$root/DEBIAN" "$root/opt/pdf-merge" "$root/usr/bin" \
    "$root/usr/share/applications" "$root/usr/share/icons/hicolor/scalable/apps"
cp -a "$appdir/." "$root/opt/pdf-merge/"
cat > "$root/usr/bin/pdf-merge" <<'EOF'
#!/bin/sh
exec /opt/pdf-merge/AppRun "$@"
EOF
chmod 0755 "$root/usr/bin/pdf-merge"
sed 's|^Exec=.*|Exec=pdf-merge %F|' \
    "$appdir/usr/share/applications/org.pdfmerge.desktop" \
    > "$root/usr/share/applications/org.pdfmerge.desktop"
cp "$appdir/usr/share/icons/hicolor/scalable/apps/pdf-merge.svg" \
    "$root/usr/share/icons/hicolor/scalable/apps/pdf-merge.svg"

size=$(du -sk "$root/opt/pdf-merge" | cut -f1)
cat > "$root/DEBIAN/control" <<EOF
Package: pdf-merge
Version: $version
Section: utils
Priority: optional
Architecture: amd64
Installed-Size: $size
Maintainer: PDF Merge contributors
Description: Arrange PDF pages and export one document
 PDF Merge is a native Qt application. PDF processing stays on the local machine.
EOF
mkdir -p "$(dirname "$output")"
dpkg-deb --root-owner-group --build "$root" "$output"
