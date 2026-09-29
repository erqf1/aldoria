#!/usr/bin/env bash
# Startet das mit build_linux.sh gebaute Spiel. Spielstände und Log liegen in build/ neben der Datei aldoria.
cd "$(dirname "$0")/build" || exit 1
exec ./aldoria "$@"
