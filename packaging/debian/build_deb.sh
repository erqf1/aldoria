#!/usr/bin/env bash
# Baut ein .deb-Paket auf Debian/Ubuntu (und Ableitungen). Braucht cmake, g++ >= 13, ninja-build (oder make),
# die X11/GL/ALSA-Entwicklungspakete und dpkg-dev. Läuft NICHT auf Arch (dafür siehe packaging/arch/PKGBUILD).
# Aufruf, aus dem Repo-Wurzelverzeichnis:  bash packaging/debian/build_deb.sh [Version]
set -euo pipefail
cd "$(dirname "$0")/../.."
root="$PWD"
version="${1:-1.0.0}"

need=(cmake g++ ninja-build dpkg-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev
      libxi-dev libxext-dev libxfixes-dev libxrender-dev libgl1-mesa-dev libasound2-dev)
if command -v apt >/dev/null 2>&1; then
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

echo "== Baue Aldoria $version =="
cmake -S . -B build-deb -G Ninja -DCMAKE_BUILD_TYPE=Release -DALDORIA_BUILD_TESTS=OFF
cmake --build build-deb -j"$(nproc)"

echo "== Staging =="
stage="$root/build-deb/stage"
rm -rf "$stage"
mkdir -p "$stage/DEBIAN" "$stage/usr/bin"
DESTDIR="$stage" cmake --install build-deb --prefix /opt/aldoria
install -Dm755 packaging/linux/aldoria-wrapper.sh "$stage/usr/bin/aldoria"
sed "s/VERSION_PLACEHOLDER/$version/" packaging/debian/control > "$stage/DEBIAN/control"
size_kb=$(du -sk "$stage" | cut -f1)
sed -i "/^Description:/i Installed-Size: $size_kb" "$stage/DEBIAN/control"

echo "== .deb bauen =="
out="$root/aldoria_${version}_amd64.deb"
dpkg-deb --root-owner-group --build "$stage" "$out"
echo
echo "Fertig: $out"
echo "Installieren mit:  sudo apt install ./$(basename "$out")"
