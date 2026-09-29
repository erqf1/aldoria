# Aldoria

Ein 3D-Action-Adventure in C++ (raylib) aus der Schulterperspektive: Räume voller Sprungpassagen, Rätsel, Kampfarenen und Bosse.
Alles wird im Code gezeichnet und vertont, es gibt keine Bild- oder Klangdateien.

Drei Dungeons nacheinander: **Die Wurzelhallen** (Kobold-König), **Die Glutschmiede** (Lava, Schmiedegolem) und **Der Himmelsturm** (Wolken, Sturmwächter). Dazu Endlos-Arena und Boss-Rush.

## Herunterladen

Fertige Versionen gibt es bei den [GitHub Releases](https://github.com/erqf1/aldoria/releases):

- **Windows:** `aldoria-windows.zip` entpacken, `aldoria.exe` starten. Keine Installation nötig.
- **Linux, Quellcode (tar.gz):** `aldoria-linux-source.tar.gz` entpacken, `bash build_linux.sh` (baut mit CMake, ein paar Minuten,
  braucht Internet beim ersten Mal), dann `bash play.sh`. Läuft auf Arch, Debian/Ubuntu und den meisten anderen Distributionen.
- **Arch Linux (pacman):** `PKGBUILD` herunterladen, `makepkg -si` im selben Ordner ausführen. Lädt den Quellcode selbst
  herunter und installiert `aldoria` als echtes Paket (`pacman -Qi aldoria` danach sichtbar, `pacman -R aldoria` deinstalliert).
- **Debian/Ubuntu (.deb):** dasselbe `aldoria-linux-source.tar.gz` entpacken, darin `bash packaging/debian/build_deb.sh`
  ausführen — baut ein echtes `aldoria_<Version>_amd64.deb`, danach `sudo apt install ./aldoria_*.deb`.

Für Arch und Debian gibt es (noch) keine vorkompilierten Binärpakete zum Herunterladen, weil dieses Repo unter Windows
gepflegt wird und kein Linux-Compiler zur Verfügung steht — `PKGBUILD` und `build_deb.sh` bauen stattdessen lokal aus dem
Quellcode mit dem eigenen Toolchain des jeweiligen Systems (üblich und zuverlässiger als ein fremd kompiliertes Binary).
Details stehen in `packaging/README.md`.

## Starten (aus dem Quellcode)

```
build\aldoria.exe
```

Bauen (Visual Studio Build Tools, CMake und Ninja sind schon eingerichtet):

```
build.bat
```

`build.bat` baut das Spiel und die Tests und führt die Tests aus. Fertige Weitergabe-Stände liegen in `compiled\`: `windows\` (exe + data, dazu `aldoria-windows.zip`) und `linux\` (Quellpaket mit `build_arch.sh`, noch nicht auf Linux kompiliert). `package.bat` legt danach einen Ordner `dist\Aldoria` an (exe + data), den man an Freunde weitergeben kann; die exe braucht keine zusätzliche Installation. Beim ersten Mal lädt CMake raylib, nlohmann/json und doctest herunter.

Auf Linux baut `bash build_linux.sh` (dann `bash play.sh`) aus dem Quellcode; `packaging/` enthält fertige Paketrezepte für Arch (`pacman`) und Debian/Ubuntu (`.deb`), siehe oben und `packaging/README.md`.

## Grafik

Texturen mit Normalkarten (CC0 von ambientCG.com, siehe `data/textures/LIZENZ.txt`), Schatten der Sonne, Punktlichter von Fackeln, Feuerschalen und Lava, HDR mit Bloom, Kantenglättung und ein Himmel mit Wolken. In den Einstellungen gibt es „Grafik: Niedrig / Mittel / Hoch“. Figuren haben Gelenke: Held und Gegner laufen, springen, schlagen und rollen mit echten Beinen und Armen. Der Held steht aufrecht (nur im Angriff und Dash neigt er sich) und trägt Tunika, Lederstulpen, Schulterstücke, Stiefel und ein Schwert mit Hohlkehle. Sein Umhang ist simulierter Stoff mit Falten und goldenem Saum; er stößt an Rumpf, Rock, Schultern und Beine und geht nie hindurch (Test „Umhang: bleibt außerhalb des Körpers“). Kobolde, Schützen, Springer und Brecher haben Schuppen- bzw. Rauhaut, Hörner, Zähne, Krallen, Schwanz, Lederzeug, Kapuze und Köcher, Metallschulterpanzer und Stachelkeulen. Fahrende Plattformen nehmen ihre Textur mit. Gegner laufen nie über eine Kante ins Leere und springen nur, wenn der Landeplatz fester Boden ist; fällt doch einer, kehrt er zum letzten sicheren Punkt zurück.

## Steuerung

| Aktion | Taste |
|---|---|
| Laufen / Umsehen | WASD / Maus |
| Springen (länger halten = höher) | Leertaste |
| Sprinten | Shift |
| Schwert (bis zu drei Schläge, dritter ist ein Rundumschlag) | Linksklick |
| Funkenwurf (Fernkampf, aktiviert auch Kristalle) | Rechtsklick |
| Ausweichen (kurz unverwundbar) | Strg |
| Dash (nach dem Fund der Windstiefel) | Alt oder V |
| Doppelsprung (nach dem Fund der Sturmfedern) | Leertaste in der Luft |
| Heiltrank | R |
| Benutzen (Ruheplatz: ausruhen) | E |
| Segen für Splitter kaufen (am Ruheplatz) | Q |
| Karte (besuchte Räume) | M |
| Menü | Esc |
| Vollbild (randlos) | F11 |

Auch Controller werden unterstützt. Menüs: Pfeile oder WASD, Enter.
Alle Tasten lassen sich unter **Einstellungen → Tasten belegen** ändern: Aktion wählen, dann eine Zeile („Taste 1“, „Taste 2“ …) auswählen und die neue Taste oder Maustaste drücken (Esc bricht ab, Entf löscht die Taste). Jede Aktion kann bis zu vier Tasten haben („Weitere Taste hinzufügen“), und dieselbe Taste darf mehreren Aktionen gehören (die Seite zeigt dann „Auch belegt bei …“). „Diese Aktion auf Standard“ und „Alle Tasten auf Standard“ stellen die Vorgaben wieder her. Die Belegung wird in `saves/settings.json` gespeichert, die Hilfezeile im Spiel zeigt immer die aktuellen Tasten.

## Spielablauf

Der Dungeon besteht aus Räumen, die durch Türen verbunden sind:

- **Plattform-Räume:** Abgründe, bewegliche und bröckelnde Plattformen. Ein Sturz kostet Leben, tötet aber nie.
- **Kampfarenen:** Die Türen schließen sich, mehrere Wellen kommen. Danach wählst du 1 von 3 Segen (Upgrades).
- **Rätsel:** Kristalle (mit Schwert oder Funken aktivieren), Schalter, Schlüsseltüren.
- **Ruheplätze:** Leben und Tränke auffüllen, Fortschritt speichern. Nach dem Tod wachst du dort wieder auf. Mit gesammelten Splittern kannst du dort auch einen Segen kaufen (Q; der Preis steigt mit jedem Segen).
- **Bossangriffe:** Hieb, Stampfen, gezielte Feuerbälle in Brusthöhe (Strg zum Ausweichen), Ansturm mit sichtbarer Bahn (gegen die Wand gerannt ist er benommen), Meteoreinschläge mit rotem Warnkreis, Spiralfeuer, Rufen von Dienern und in Phase 3 ein Sprung. Funken (Rechtsklick) richten an Bossen nur etwa die Hälfte an, und wer nur aus der Ferne schießt, wird angerannt oder mit Meteoren belegt.
- **Boss:** Der Kobold-König (Wurzelhallen) und der Schmiedegolem (Glutschmiede), je mit drei Phasen. Nach schweren Angriffen sind sie erschöpft und verwundbar.
- **Lava:** In der Glutschmiede kostet Lava Leben; Fähren und bröckelnde Plattformen führen darüber. Mit dem Dash (Windstiefel aus den Wurzelhallen) geht es auch direkt.
- **Dungeon-Folge:** Nach jedem Boss erscheint im Abschlussbild „Weiter zu: …“. Fähigkeiten, Segen, Herzen und Splitter bleiben erhalten.
- **Himmelsturm:** Lücken bis ca. 6 m, die man nur mit den Sturmfedern (Doppelsprung, in der Federkammer) überwindet. Ein verstecktes Herz liegt in der Wolkenkammer; der Schlüssel dafür auf einer Nebeninsel im Wolkenpfad.
- **Boss-Rush:** Im Hauptmenü. Ein Boss nach dem anderen (Kobold-König, Schmiedegolem, Sturmwächter, wieder von vorn), nach jedem ein Segen und etwas Leben zurück. Der Rekord wird gespeichert.
- **Endlos-Arena:** Im Hauptmenü. Immer stärkere Wellen, alle drei Wellen ein Segen, alle zehn Wellen ein Boss (reihum Kobold-König, Schmiedegolem, Sturmwächter). Ab Welle 8 mischen sich Glutschmiede-Gegner darunter, ab Welle 16 Sturmgegner.

Spielstände und Einstellungen liegen in `build\saves\`.

## Ordner

```
src/        core (App, Szenen, Speichern), render, world (Räume, Kollision), entities, combat, components, audio, ui, debug
data/       alle Spielwerte und Inhalte als JSON: rooms/, dungeons/, enemies/, weapons/, config/, segen.json, shaders/
tests/      Unit-Tests (doctest), inklusive Prüfung aller Raumdaten
scripts/    Testskripte für den Autopiloten (siehe unten)
legacy_2d/  der erste 2D-Prototyp
docs/       älterer Plan
```

## Inhalte ändern (ohne Neukompilieren)

Werte und Räume sind JSON. Im Spiel lädt F5 den aktuellen Raum neu.

- `data/config/player.json`: Bewegung, Sprung, Ausweichen, Dash, Heiltrank
- `data/weapons/sword.json`: Schläge der Kombo
- `data/enemies/*.json`: Gegner (Nahkampf, Fernkampf, Springer) und Boss
- `data/segen.json`: Upgrades
- `data/rooms/*.json`: Räume. Eine `shell` erzeugt Wände, `doors` mit `side` schneiden die Türen hinein. Weitere Abschnitte:
  `boxes`, `ramps`, `platforms`, `switches`, `hazards`, `pickups`, `enemies`, `encounter`, `boss`, `checkpoint`, `decor`, `scatter`.
  Die Tests (`tests/test_dungeon_data.cpp`) prüfen Türverbindungen, Erreichbarkeit, Gegnertypen und Schlüsselzahl.
- `data/dungeons/*.json`: Start- und Endraum und der Folge-Dungeon (`next_dungeon`)

## Automatische Tests

```
build\aldoria_tests.exe
```

Zusätzlich kann sich das Spiel von einem Skript steuern lassen, ohne dass Tasten ans System geschickt werden:

```
build\aldoria.exe --script scripts\flow_doors.txt --shots build\shots
```

Skriptbefehle: `wait`, `hold <Aktion+Aktion> <Sekunden>`, `press <Aktion>`, `look`, `shot <Name>`, `quit` und Testbefehle
(`new_game`, `room <id>`, `teleport x y z yaw`, `god`, `give`, `kill_all`, `status`, ...). Fertige Abläufe liegen in `scripts/`:
Türen und Rätsel, Arena, Boss, Tod und Weiterspielen, Menüs, Dash, Endlos-Arena, Boss-Rush, Dungeon 2 und 3, Zufallstest.
`scripts\run_script.ps1` startet ein Skript mit eigenem Spielstand-Ordner und zeigt das Log.
`scripts\run_flows.ps1` führt alle `flow_*.txt` nacheinander aus und meldet Exitcode und Fehlerzeilen im Log. Die Texturen
`scaly_hide`, `rough_skin`, `weave_linen` und `bone_ivory` (Gegner und Held) erzeugt `python scripts/gen_creature_textures.py` (prozedural, ohne Download).

## Lizenz

Apache License 2.0, siehe `LICENSE`. Eigener Code und eigene Inhalte (Code, Level-Daten, prozedurale Texturen) stehen unter
dieser Lizenz. Gebündelte Fremdkomponenten haben ihre eigene Lizenz (raylib: zlib/libpng, nlohmann/json: MIT, doctest: MIT,
ambientCG-Texturen: CC0 1.0) — Details in `NOTICE` und `data/textures/LIZENZ.txt`.
