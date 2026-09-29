#!/usr/bin/env bash
# Baut Aldoria aus dem Quellcode (Arch, Debian/Ubuntu und die meisten anderen Distributionen mit
# cmake, g++ >= 13 und den X11/GL/ALSA-Entwicklungspaketen). Für Arch gibt es zusätzlich ein
# fertiges Paketrezept (packaging/arch/PKGBUILD), für Debian/Ubuntu eines für ein echtes .deb
# (packaging/debian/build_deb.sh). Dieses Skript hier baut einfach nur build/aldoria.
# Aufruf:  bash build_linux.sh
set -euo pipefail
cd "$(dirname "$0")"

if command -v pacman >/dev/null 2>&1; then
    need=(cmake gcc make git libx11 libxrandr libxinerama libxcursor libxi libxext libxfixes libxrender xorgproto mesa libglvnd alsa-lib)
    missing=()
    for p in "${need[@]}"; do
        pacman -Qi "$p" >/dev/null 2>&1 || missing+=("$p")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        echo "Es fehlen Pakete: ${missing[*]}"
        echo "Installieren mit:  sudo pacman -S --needed ${missing[*]}"
        exit 1
    fi
elif command -v apt >/dev/null 2>&1; then
    need=(cmake g++ git libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
          libxext-dev libxfixes-dev libxrender-dev libgl1-mesa-dev libasound2-dev)
    missing=()
    for p in "${need[@]}"; do
        dpkg -s "$p" >/dev/null 2>&1 || missing+=("$p")
    done
    if [ ${#missing[@]} -gt 0 ]; then
        echo "Es fehlen Pakete: ${missing[*]}"
        echo "Installieren mit:  sudo apt install ${missing[*]}"
        exit 1
    fi
fi

gcc_major=$(g++ -dumpversion | cut -d. -f1)
if [ "$gcc_major" -lt 13 ]; then
    echo "g++ $gcc_major ist zu alt, gebraucht wird Version 13 oder neuer (wegen <format>)."
    exit 1
fi

# Beim ersten Mal lädt CMake raylib und nlohmann/json aus dem Internet.
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DALDORIA_BUILD_TESTS=OFF
cmake --build build -j"$(nproc)"

echo
echo "Fertig. Starten mit:  bash play.sh"
