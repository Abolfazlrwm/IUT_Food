#!/usr/bin/env bash
# Build a self-contained Debian package for IUT-Food.
#
#   scripts/package_deb.sh <version> <build-dir> [output-dir]
#
# The client and server binaries (plus their Qt libraries and the Qt
# platform/imageformat/sqlite plugins) are bundled under /opt/iut-food,
# with $ORIGIN rpaths, so the package installs on any distro with a
# compatible glibc - no system Qt required.  Core system libraries
# (glibc, libstdc++, the Mesa/GL stack) stay external and are declared
# as package dependencies instead: bundling them is unsafe.
set -euo pipefail

VERSION="${1:?usage: package_deb.sh <version> <build-dir> [output-dir]}"
BUILD_DIR="${2:?}"
OUT_DIR="${3:-$PWD}"

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STAGE="$(mktemp -d /tmp/iut-deb.XXXXXX)"
PKG_ROOT="$STAGE/iut-food_${VERSION}_amd64"

cleanup() { rm -rf "$STAGE"; }
trap cleanup EXIT

# ---- staged layout -------------------------------------------------------
OPT="$PKG_ROOT/opt/iut-food"
BIN="$OPT/bin"; LIB="$OPT/lib"; PLUG="$OPT/plugins"
mkdir -p "$BIN" "$LIB" "$PLUG" "$PKG_ROOT/DEBIAN" \
         "$PKG_ROOT/usr/share/applications" "$PKG_ROOT/usr/share/icons/hicolor/256x256/apps"

cp "$BUILD_DIR/iut-food-client" "$BUILD_DIR/iut-food-server" "$BIN/"
cp "$REPO/README.md" "$REPO/LICENSE" "$REPO/CHANGELOG.md" "$OPT/"

# ---- recursive ldd bundling ---------------------------------------------
# Copies every external shared object the binaries (and the copied
# libraries themselves) need, except the system-core exclusions below.
# Bundles everything the binaries need, including the GL/EGL stack
# (a missing libEGL kills the client at load time on slim systems).
# Only glibc/libstdc++ and the hardware driver internals stay external.
EXCLUDE_RE='(lib(bsd|c|dl|m|pthread|resolv|rt|gcc_s|stdc\+\+|glapi|gbm|wayland-client|wayland-cursor|wayland-egl|drm)\.so|ld-linux|libstdc\+\+|libLLVM|libgallium|libglapi|libdrm_radeon|libz\.so|libzstd\.so|liblzma\.so|libbz2|libcap\.so|libselinux\.so|libpcre2-8\.so|libsystemd\.so|liblz4\.so|libtinfo|libX11-xcb|libXau\.so|libXdmcp\.so|libwayland-server|libffi\.so|libmd\.so|libbsd\.so)'

declare -A COPIED=()
# If the host is missing a dependency ("=> not found"), fall back to the
# Qt SDK's own lib dir or the build-time GL stub dir.
QT_LIB_DIR=""
FALLBACK_DIRS=()

bundle_lib() {
    local src="$1"
    local want="$(basename "$src")"
    if [ ! -f "$src" ]; then
        for d in "${FALLBACK_DIRS[@]}"; do
            [ -n "$d" ] && [ -f "$d/$want" ] && { src="$d/$want"; break; }
        done
        if [ ! -f "$src" ]; then
            echo "  WARNING: cannot bundle $want - not on host or in fallback dirs"
            return 0
        fi
    fi
    [[ "$want" =~ $EXCLUDE_RE ]] && return 0
    local dest="$LIB/$want"
    if [ -z "${COPIED[$src]:-}" ] && [ ! -f "$dest" ]; then
        COPIED[$src]=1
        cp -L "$src" "$dest"
        # follow the freshly copied library's own dependencies too
        walk_ldd "$dest"
    fi
}

walk_ldd() {
    local obj="$1"
    # process substitution keeps COPIED updates in this shell (a pipe
    # would run the loop in a subshell and forget the dedupe map)
    while read -r dep; do bundle_lib "$dep"; done \
        < <(ldd "$obj" 2>/dev/null \
            | awk '/=> \//{print $3} /=> not found/{print $1} /^\//{print $1}')
}

# Qt plugin dirs from the SDK the build used.  Detection order:
#   1. qmake on PATH          2. $QT_ROOT_DIR (install-qt-action)
#   3. CMAKE_PREFIX_PATH in the build dir's CMakeCache
#   4. the default aqt layout (~/Qt/<ver>/<toolchain>)
QT_PLUGINS=""
if command -v qmake6 >/dev/null 2>&1; then
    QT_PLUGINS="$(qmake6 -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
elif command -v qmake >/dev/null 2>&1; then
    QT_PLUGINS="$(qmake -query QT_INSTALL_PLUGINS 2>/dev/null || true)"
fi
if [ -z "$QT_PLUGINS" ] && [ -n "${QT_ROOT_DIR:-}" ] && [ -d "$QT_ROOT_DIR/plugins" ]; then
    QT_PLUGINS="$QT_ROOT_DIR/plugins"
fi
if [ -z "$QT_PLUGINS" ]; then
    PREFIXES="$(grep -m1 '^CMAKE_PREFIX_PATH' "$BUILD_DIR/CMakeCache.txt" 2>/dev/null \
        | sed 's/^[^=]*=//; s/;//' || true)"
    for pfx in ${PREFIXES:-}; do
        [ -d "$pfx/plugins" ] && { QT_PLUGINS="$pfx/plugins"; break; }
    done
fi
if [ -z "$QT_PLUGINS" ]; then
    for cand in /home/z/Qt/*/gcc_64/plugins /opt/Qt/*/gcc_64/plugins; do
        [ -d "$cand" ] && { QT_PLUGINS="$cand"; break; }
    done
fi
[ -z "$QT_PLUGINS" ] && { echo "cannot locate the Qt plugins dir"; exit 1; }
echo "bundling Qt plugins from: $QT_PLUGINS"
QT_LIB_DIR="$(cd "$(dirname "$QT_PLUGINS")/lib" 2>/dev/null && pwd)"
FALLBACK_DIRS=("$QT_LIB_DIR" /home/z/gl-stub/lib "${QT_ROOT_DIR:-/nonexistent}/lib")

for obj in "$BIN/iut-food-client" "$BIN/iut-food-server"; do
    walk_ldd "$obj"
done
for p in platforms/libqxcb.so platforms/libqoffscreen.so platforms/libqeglfs.so \
         imageformats/*.so sqldrivers/libqsqlite.so iconengines/libqsvgicon.so; do
    if [ -f "$QT_PLUGINS/$p" ]; then
        mkdir -p "$PLUG/$(dirname "$p")"
        cp "$QT_PLUGINS/$p" "$PLUG/$p"
        walk_ldd "$PLUG/$p"
    fi
done

# rpaths: binaries look in ../lib, plugins in ../../lib
command -v patchelf >/dev/null || { echo "patchelf is required (apt install patchelf)"; exit 1; }
patchelf --set-rpath '$ORIGIN/../lib' "$BIN/iut-food-client" "$BIN/iut-food-server"
find "$PLUG" -name '*.so' -exec patchelf --set-rpath '$ORIGIN/../../lib' {} \;

# plugin discovery via env instead of hard-coded qt.conf paths
mv "$BIN/iut-food-client" "$BIN/iut-food-client.bin"
cat > "$BIN/iut-food-client" <<'EOF'
#!/bin/sh
HERE="$(cd "$(dirname "$0")" && pwd)"
export QT_PLUGIN_PATH="$HERE/../plugins"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
exec "$HERE/iut-food-client.bin" "$@"
EOF
chmod +x "$BIN/iut-food-client" "$BIN/iut-food-client.bin"

# ---- desktop integration --------------------------------------------------
cp "$REPO/packaging/linux/iut-food-client.bin.png" \
   "$PKG_ROOT/usr/share/icons/hicolor/256x256/apps/iut-food.png"
cat > "$PKG_ROOT/usr/share/applications/iut-food.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=IUT-Food
GenericName=Campus food ordering
Comment=Browse restaurants, order food, pay from your wallet
Exec=/opt/iut-food/bin/iut-food-client
Icon=iut-food
Terminal=false
Categories=Network;Utility;
EOF

# ---- control --------------------------------------------------------------
cat > "$PKG_ROOT/DEBIAN/control" <<EOF
Package: iut-food
Version: ${VERSION}
Section: net
Priority: optional
Architecture: amd64
Maintainer: Abolfazl Ranjbaran <206173650+Abolfazlrwm@users.noreply.github.com>
Depends: libc6 (>= 2.31), libgcc-s1, libstdc++6, libgl1, libegl1, libfontconfig1, libfreetype6, libdbus-1-3, libglib2.0-0 | libglib2.0-0t64, libxkbcommon0, libxkbcommon-x11-0, libxcb-cursor0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-render-util0, libxcb-xkb1
Homepage: https://github.com/Abolfazlrwm/IUT_Food
Description: Campus food-ordering system (IUT) - Qt 6 client + TCP server
 IUT-Food is a two-app campus food-ordering platform: a Qt Widgets
 desktop client (restaurants, wallet, coupons, live order tracking,
 reviews, admin console) and a TCP JSON server with TLS, sessions,
 Prometheus metrics, backups and an audit trail.  Qt libraries are
 bundled under /opt/iut-food - no system Qt needed.
EOF

dpkg-deb --root-owner-group --build "$PKG_ROOT" "$OUT_DIR/iut-food_${VERSION}_amd64.deb"
echo "wrote $OUT_DIR/iut-food_${VERSION}_amd64.deb"
