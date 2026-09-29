#!/bin/sh
# Von den Linux-Paketen (PKGBUILD, .deb) nach /usr/bin/aldoria installiert.
# Das eigentliche Spiel liegt unter /opt/aldoria (Binärdatei + data-Ordner nebeneinander,
# genau wie im Windows- und Quellpaket). Spielstände landen pro Benutzer unter
# $XDG_DATA_HOME (Standard: ~/.local/share), damit /opt/aldoria nicht beschreibbar sein muss.
set -e
ALDORIA_HOME="/opt/aldoria"
export ALDORIA_DATA="$ALDORIA_HOME/data"
export ALDORIA_SAVES="${XDG_DATA_HOME:-$HOME/.local/share}/aldoria/saves"
mkdir -p "$ALDORIA_SAVES"
exec "$ALDORIA_HOME/aldoria" "$@"
