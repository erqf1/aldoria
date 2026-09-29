# Paketierung

Aldoria wird unter Windows entwickelt; hier ist kein Linux-Compiler verfügbar. Die Linux-Pakete unten bauen deshalb lokal
aus dem Quellcode mit dem Toolchain des jeweiligen Systems, statt ein unter Windows quer-kompiliertes Binary zu verteilen —
das ist für Arch/Debian auch der übliche Weg (PKGBUILDs und .deb-Quellpakete bauen praktisch immer selbst) und garantiert,
dass die Binärdatei zu den installierten Bibliotheken (X11, Mesa/OpenGL, ALSA) passt.

Alle drei Wege installieren dieselbe CMake-`install()`-Regel aus der Repo-Wurzel: Binärdatei und `data/`-Ordner landen
zusammen unter `/opt/aldoria`, ein kleines Wrapper-Skript (`packaging/linux/aldoria-wrapper.sh`) kommt nach `/usr/bin/aldoria`
und sorgt dafür, dass Spielstände pro Benutzer unter `~/.local/share/aldoria/saves` landen (kein Schreibzugriff auf `/opt`
nötig).

## Arch Linux — `packaging/arch/PKGBUILD`

```
makepkg -si
```

Lädt den Quellcode vom in der `PKGBUILD` verlinkten Release-Tag, baut mit `cmake`+`ninja` und installiert als reguläres
pacman-Paket. `pacman -Qi aldoria` zeigt es danach an, `pacman -R aldoria` entfernt es wieder.

## Debian/Ubuntu — `packaging/debian/build_deb.sh`

```
bash packaging/debian/build_deb.sh [Version]
```

Baut aus dem Quellcode im Repo (kein Download nötig, läuft im Checkout) und erzeugt `aldoria_<Version>_amd64.deb` per
`dpkg-deb` direkt in der Repo-Wurzel. Installieren mit `sudo apt install ./aldoria_*.deb`.

## Generischer Linux-Build — `build_linux.sh` (Repo-Wurzel)

```
bash build_linux.sh && bash play.sh
```

Baut nur `build/aldoria`, ohne etwas zu installieren oder zu paketieren — für schnelles Testen aus dem Checkout heraus,
auf Arch, Debian/Ubuntu und den meisten anderen Distributionen mit `cmake`, `g++ >= 13` und den X11/GL/ALSA-Entwicklungspaketen.
