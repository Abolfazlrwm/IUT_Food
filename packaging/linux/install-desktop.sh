#!/bin/sh
# Optional desktop integration for the portable Linux tarball:
#   cd desktop && ./install-desktop.sh
# Copies the brand icon into the user's hicolor icons and installs a
# menu entry ("IUT-Food") pointing at the client inside this package.
# Nothing system-wide is touched; undo = delete the two installed files.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"          # .../IUT-Food-*/desktop
ROOT="$(dirname "$HERE")"                      # .../IUT-Food-*
[ -x "$ROOT/iut-food-client" ] || {
    echo "error: run me from the extracted package (desktop/ next to iut-food-client)" >&2
    exit 1
}

DATA="${XDG_DATA_HOME:-$HOME/.local/share}"
ICONS="$DATA/icons/hicolor"
mkdir -p "$DATA/applications" "$ICONS/256x256/apps" "$ICONS/512x512/apps"

cp "$HERE/iut-food-icon.png" "$ICONS/512x512/apps/iut-food.png"
cp "$HERE/iut-food-icon.png" "$ICONS/256x256/apps/iut-food.png"
sed "s|^Exec=.*|Exec=$ROOT/iut-food-client|" "$HERE/iut-food-client.desktop" \
    > "$DATA/applications/iut-food.desktop"

command -v update-desktop-database >/dev/null 2>&1 && \
    update-desktop-database "$DATA/applications" || true

echo "Installed the 'IUT-Food' menu entry -> $ROOT/iut-food-client"
