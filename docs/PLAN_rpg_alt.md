# Aldoria 3D – Plan (nur C++)

Stand: 24.09.2026 · Start der Umsetzung: 25.09.2026

## 0. Ausgangslage

- Der 2D-Prototyp „Legende von Aldoria“ (C++ + raylib) läuft: `src/main.cpp`, gebaut mit `build.bat`.
- Neue Vorgabe: die große Spezifikation (Lobby, 4 Welten, Force/Dash/Time, Waffen, Skill-Tree, Sammelobjekte, Bosse, Speichern) – **nur in C++**, nicht in Godot.
- Zur Spezifikation: Das Skript erzeugt „3000 Zeilen“, aber nur ca. 250 sind echte Regeln. Der Rest sind wiederholte „REVIEW PASS“-Zeilen. Die echten Regeln stecken in Abschnitt 5 dieses Plans.
- Ehrliche Einordnung: Die volle Spezifikation ist für ein Team ein Projekt über Monate. Deshalb bauen wir zuerst einen **Vertical Slice** (Lobby + komplette Welt 1). Das verlangt die Spezifikation selbst („Do not build four empty worlds before one world is fun“). Erst danach kommen Welt 2–4.

## 1. Technik-Entscheidungen

| Thema | Entscheidung | Warum |
|---|---|---|
| Sprache/Build | C++20, MSVC Build Tools, CMake + Ninja | läuft bei dir schon |
| Engine-Ersatz | **raylib 5.5** (3D-Kamera, Modelle, glTF-Import, Shader, Audio, Input) | schon eingebunden, kein Editor nötig, statisch gelinkt |
| Look | Stylisiertes Low-/Mid-Poly, eigene GLSL-Shader (Licht, Rim-Light, Nebel, Shadow-Map) | „modern, aber nicht hyper-modern“ ohne Riesen-Assets |
| Kollision/Physik | Eigenbau: Kapsel gegen Boxen/Rampen/Terrain, Raycasts. **Keine** Physik-Engine | Force/Time laufen über kontrollierte Bewegung, das ist stabil und soft-lock-sicher |
| Daten | JSON (nlohmann/json über CMake FetchContent) für Waffen, Fähigkeiten, Gegner, Sammelobjekte, Upgrades, Welten, Dialoge | Balancing ohne Neu-Kompilieren, Hot-Reload im Dev-Build |
| Levels | JSON-Levels (Boxen, Rampen, Props, Spawns, Trigger, Checkpoints) + **Dev-Editor-Overlay im Spiel** (platzieren, verschieben, speichern) | ohne Editor kann man keine 3D-Welten bauen |
| Grafik-Assets | Erst prozedural aus Primitiven (Platzhalter), später optional CC0-glTF-Modelle | Regel „Placeholder zuerst“; Austausch ohne Code-Umbau |
| Animation | Erst prozedural (Schwingen, Squash, Gliedmaßen), später glTF-Animationen | keine Animationstools nötig |
| Audio | raylib-Audio + eigene Busse (Master/Musik/SFX/Ambient/Dialog), SFX zuerst generiert | Spezifikation verlangt Busse und Crossfade |
| Speichern | JSON, Versionsfeld, atomar (Temp-Datei + Rename), Backup-Datei | Spezifikation: nie gültigen Spielstand überschreiben |
| UI | eigenes kleines UI-Modul auf raylib (HUD, Menüs, Skill-Tree); raygui nur für Debug-Tools | gestyltes Fantasy-UI, sauber getrennt von der Logik |
| Tests | doctest + CTest für Speichern, Freischaltungen, Kosten, Sammelobjekte | Spezifikation nennt genau diese Punkte |

## 2. Architektur

```
src/
  core/        App, Spielzustände (Titel, Lobby, Welt, Pause), Input-Aktionen, Event-Bus, Zeit/Hitstop
  render/      Kamera-Rig, Shader, Licht, Nebel, Partikel, Debug-Zeichnen
  world/       Level-Loader, Kollisionswelt, Trigger, Checkpoints, Schreine, Tore
  entities/    Player, Enemy (Zustandsautomat), Boss, Interactables, Pickups
  components/  Health, Combat (Hitbox/Hurtbox), AbilityManager, Interaction
  abilities/   Force, Dash, Time (gemeinsame Basisklasse, datengetrieben)
  systems/     Progression, Inventory, Collectibles, Save, Audio, Notifications, Quests/Dialog
  ui/          HUD, Pause, Skill-Tree, Waffen, Sammelobjekte, Einstellungen, Titel
  debug/       Dev-Menü, Level-Editor-Overlay, Overlays (FPS, Kollision, Koordinaten)
data/          weapons/ abilities/ enemies/ collectibles/ upgrades/ worlds/ levels/ dialogue/
tests/
legacy_2d/     der aktuelle 2D-Prototyp (wird verschoben, nichts wird gelöscht)
```

Regeln:
- Komposition statt riesiger `Player.cpp`. Jedes System hat genau einen Besitzer (Progression besitzt Fortschritt, Save besitzt Speichern, UI besitzt nichts).
- Managers gehören der `Game`-Klasse. Es gibt keine globalen Singletons und keinen „Gott-Manager“.
- Ereignisse laufen über einen Event-Bus (entspricht den „Signals“ der Spezifikation).
- Alle stabilen IDs (Welt, Waffe, Fähigkeit, Sammelobjekt) sind Strings und bleiben über Save-Versionen gleich.
- Fähigkeitsgesperrte Inhalte nutzen Tags (`requires: ["force","dash"]`). Im Player-Code steht keine Sammelobjekt-ID.

## 3. Meilensteine

Eine „Sitzung“ ist ein Arbeitstag mit mir. Die Schätzungen sind grob.

| # | Meilenstein | Inhalt | Fertig, wenn … | Sitzungen |
|---|---|---|---|---|
| M0 | Fundament | Neue CMake-Struktur, 3D-Fenster, Kamera, Input-Aktionen, Debug-Overlay, JSON-Laden, Event-Bus | man in einem 3D-Testlevel herumfliegen kann, FPS sichtbar | 1 |
| M1 | Bewegung | Third-Person-Spieler: Laufen, Sprinten, Springen, Coyote-Time, Jump-Buffer, Kamera mit Kollision | sich Rennen/Springen gut anfühlt, kein Durchfallen | 1–2 |
| M2 | Kampf | Schwert-Combo, Hitbox/Hurtbox, Hit-Stop, Health-Komponente, erster Gegner (Zustandsautomat, Telegraph), Ausweichen | ein Kampf lesbar und fair ist | 2 |
| M3 | Fortschritt & Speichern | Münzen, Skill-Punkte, Waffen-Upgrades, SaveManager (atomar, versioniert), Tests | Speichern/Laden übersteht Absturz und alte Versionen | 1 |
| M4 | UI | HUD, Pause, Skill-Tree, Waffen-/Sammelobjekt-Menü, Benachrichtigungen, Einstellungen | alles mit Tastatur und Maus bedienbar | 2 |
| M5 | Titel & Lobby | Titelbildschirm, Lobby-Szene, 4 Tore, Stationen, Teleport-Schrein | Titel → Lobby → Welt 1 → zurück funktioniert | 1–2 |
| **M6** | **Vertical Slice: Welt 1** | Wald-Level, Force (schieben/ziehen), Sammelobjekte, Geheimnis, Mini-Boss, Boss, Checkpoints, Abschlussbildschirm | Welt 1 komplett spielbar, Lobby verändert sich danach | 4–5 |
| M7 | Welt 2: Ruinen | Dash, Fallen, Druckplatten, Ruinen-Gegner, Mini-Boss/Boss, Rückkehr-Inhalte in Welt 1 | Dash-Geheimnisse in Welt 1 erreichbar | 2–3 |
| M8 | Welt 3: Void | Time (Verlangsamen/Einfrieren markierter Objekte), Energie-Anzeige, Phasen-Gegner, Boss | Dash+Time-Rätsel spielbar | 2–3 |
| M9 | Welt 4: Lava | Lava-Gefahren (Geysire, Hitze, einstürzende Brücken), alle Fähigkeiten kombiniert, Endboss, Ende | Kampagne von Titel bis Abspann durchspielbar | 3–4 |
| M10 | Politur & QA | Relikte, Kosmetik, Herausforderungen, Erfolge, Audio-Schichten, Grafik-Presets, Soft-Lock-Tests, Balance | QA-Checkliste (Abschnitt 6) komplett grün | 2–3 |

Gesamt: ca. 21–28 Sitzungen. **Nach M6** entscheiden wir gemeinsam, ob Welt 2–4 im vollen Umfang gebaut werden oder in einer kleineren Fassung.

### Stand

- **M0 Fundament: fertig.** Neue CMake-Struktur (Spiel + `aldoria_core` + Tests), 3D-Fenster mit Licht-Shader und Nebel, Input-Aktionen (Tastatur, Maus, Gamepad), Event-Bus, Zeitsteuerung (Hit-Stop), Logging mit Kategorien, JSON-Level und JSON-Spielerwerte, Debug-Overlay, Flugkamera, Level-Neuladen mit F5. 26 Unit-Tests laufen grün.
- **M1 Bewegung: fertig bis auf Feintuning.** Kamerarelative Bewegung, Sprung mit Coyote-Time, Sprungpuffer und kürzbarem Sprung, Kollision mit Wänden, Kisten, Stufen und **Rampen**, Kamera mit Wand- und Bodenkollision. Alle Werte stehen in `data/config/player.json`. Das Bewegungsgefühl braucht dein Feedback.
- **M2 Kampf: erster Durchstich fertig.** Dreistufige Schwert-Kombo (`data/weapons/sword.json`, letzte Stufe ist ein Rundumschlag), Ausweichrolle mit kurzer Unverwundbarkeit, Zielhilfe auf nahe Gegner, Hit-Stop, Kamerawackeln (einstellbar über `Settings::cameraShake`), Schadenszahlen, Lebensbalken. Gegner „Waldkobold“ (`data/enemies/forest_imp.json`) mit Zustandsautomat: warten, verfolgen, **Vorwarnung mit „!“**, Ausfall, Erholung, Betäubung (ohne Dauer-Stun), Tod. Spieler-Tod mit Respawn nach 2,6 s. Der Kampf wurde im laufenden Spiel getestet (Kobold besiegt).
- **Noch offen aus M2:** Feintuning der Kampfwerte (dein Feedback), weitere Gegnertypen (Fernkämpfer, Hinterhalt), optionale Zielerfassung, Schwerangriff.
- **Tests:** 78 Unit-Tests (Gesundheit, Nahkampf-Trefferprüfung, Spielerzustände, Gegner-Zustände, Rampen, Level- und Datenformate). `build.bat` baut alles und startet sie.
- **Neu seit M0:** `legacy_2d/` enthält den alten 2D-Prototyp; `src/` ist nach `core|render|world|entities|combat|components|ui|debug` gegliedert.
- **Noch nicht drin:** echte Schatten (bisher nur ein Schattenfleck unter dem Spieler), Shadow-Map kommt später; Töne; Gamepad ist angebunden, aber ungetestet.

## 4. Was wir bewusst weglassen oder verschieben

- Weglassen: Multiplayer, Crafting, Hunger, große Dialogbäume, Open World, prozedural generierte Kampagne (steht auch so in der Spezifikation).
- Verschieben: Controller-Unterstützung (Input-Aktionen werden aber von Anfang an geräteunabhängig gebaut), Level-Streaming/LOD/Occlusion (Welten sind klein genug, um komplett zu laden), Erfolge, Herausforderungen, Post-Game, Kosmetik.
- Ersetzt: „Godot-Ressourcen“ und „Autoloads“ der Spezifikation → JSON-Daten und `Game`-eigene Manager.

## 5. Spezifikation → wo sie umgesetzt wird

| Bereich der Spezifikation | Meilenstein |
|---|---|
| Bewegung, Kamera, Coyote/Buffer | M1 |
| Kampf, Telegraphen, Hit-Stop, Gegner-Zustände, Health, Ausweichen | M2 |
| Münzen, Skill-Punkte, Upgrade-Kosten, Speichern | M3 |
| HUD, Menüs, Skill-Tree (verzweigt, Sperrgründe sichtbar), Inventar, Einstellungen | M4 |
| Lobby, Weltentore, Teleport-Schrein (gesperrt im Kampf/Fall/Boss), Titelbildschirm | M5 |
| Welt 1, Force, erste Sammelobjekte/Geheimnisse, Bosse, Abschluss, Lobby-Wandel | M6 |
| Dash, Ruinen, Rückkehr nach Welt 1 | M7 |
| Time, Void | M8 |
| Lava, Endboss, Ende | M9 |
| Relikte, Kosmetik, Herausforderungen, Erfolge, Audio-Layer, Presets, Barrierefreiheit | M10 (Barrierefreiheit-Optionen wie Shake-Reduktion schon ab M4) |

## 6. Qualitätsregeln (aus der Spezifikation)

- Kein Soft-Lock: jedes Rätsel hat einen Reset, Objekte können nie unerreichbar verschoben werden.
- Kein Teleport im Kampf, im Fall oder im Bosskampf.
- Dauerhafte Belohnungen sind nicht doppelt einsammelbar.
- Keine Fähigkeit und keine Welt vor der geplanten Freischaltung.
- Speichern beschädigt nie einen gültigen Spielstand.
- Werte (Schaden, Kosten, Cooldowns) stehen in JSON, nicht im Code.
- Fähigkeiten sind mehr als Kampf-Tasten: jede hat eine Erkundungs-Funktion.

## 7. Risiken

- **3D ohne Editor**: Levels bauen ist mühsam. Gegenmittel: Dev-Editor-Overlay in M0/M6 früh bauen.
- **Kollision selbst bauen**: Fehler zeigen sich als Durchfallen oder Hängenbleiben. Gegenmittel: einfache Geometrie (Boxen/Rampen), früh testen, Debug-Ansicht der Kollision.
- **Optik**: Ohne Künstler wirkt Low-Poly schnell leer. Gegenmittel: Licht, Nebel und Farbpalette pro Welt priorisieren, später CC0-Modelle einbauen.
- **Umfang**: Der größte Risikofaktor. Gegenmittel: Vertical Slice zuerst und Entscheidung nach M6.

## 8. Morgen (Sitzung 1 = M0)

1. Den 2D-Prototyp nach `legacy_2d/` verschieben (nichts löschen).
2. Neue `CMakeLists.txt` mit Modulen; nlohmann/json und doctest per FetchContent.
3. 3D-Fenster mit Boden, Licht-Shader und freier Kamera.
4. Input-Aktionen (Bewegen, Blick, Springen, Angriff, Interagieren, Pause).
5. Debug-Overlay (FPS, Koordinaten) und Event-Bus.
6. Ergebnis: ein 3D-Testlevel, in dem man schon mit der Kapsel-Figur herumlaufen kann. Direkt danach M1.

## 9. Festgelegte Standardwerte (ohne Rückfrage)

- Third-Person-3D wie in der Spezifikation, Low-/Mid-Poly-Optik aus selbst erzeugten Formen.
- Neue Downloads beim Bauen: raylib (schon da), nlohmann/json, doctest – jeweils die offiziellen GitHub-Repos.
- Code und Bezeichner auf Englisch, Spieltexte auf Deutsch.
- Der 2D-Prototyp bleibt als `legacy_2d/` erhalten.
