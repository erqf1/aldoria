# Aldoria – neuer Plan: Dungeon-Action-Adventure (nur C++)

Stand: 25.09.2026 · Der alte RPG-Plan liegt unverändert in `docs/PLAN_rpg_alt.md`.

> **Aktueller Stand:** M3 bis M10 sowie **Dungeon 2 (Die Glutschmiede)**, **Dungeon 3 (Der Himmelsturm)** mit neuer Fähigkeit Doppelsprung und die Endlos-Arena sind gebaut und getestet.
> Außerdem fertig: neue Grafik (CC0-Texturen, Schatten, Punktlichter, Bloom), Figuren mit Gelenken und Animationen, neue Bossangriffe, Tastenbelegung in eigenem Menü (mehrere Tasten pro Aktion, Standard zurücksetzen), Umhang mit Falten und Körperkollision, neue Held- und Gegnertexturen, Gegner ohne Absturz in Abgründe, Karte (M), Boss-Rush, Vollbild (F11), Weitergabe-Paket (`package.bat`). Offen: Level-Editor (M5, bewusst ausgelassen, Räume sind JSON), Dungeon 4, Feinschliff nach deinem Feedback zu Balance und Bewegungsgefühl.
> Den Abschnitt „Nächster Schritt“ unten gibt es nicht mehr; die Meilensteintabelle zeigt den Ist-Zustand.

## 1. Idee

Aus deiner Auswahl (1 Arena-Roguelite, 2 Bosskämpfe, 3 Plattformer, 5 Zelda-artiges Abenteuer) wird **ein** Spiel mit gemeinsamem Rahmen: **Räume**.

Ein Dungeon besteht aus handgebauten Räumen, die durch Türen verbunden sind. Jeder Raum hat einen Typ:

| Raumtyp | Was passiert | Kommt aus |
|---|---|---|
| Plattform-Raum | Springen, Rampen, bewegliche Plattformen, Abgründe, Sammel-Splitter | 3 |
| Kampf-Arena | Türen schließen sich, 2–3 Wellen Gegner, danach Wahl von 1 aus 3 Upgrades („Segen“) | 1 |
| Schlüssel-/Schalter-Raum | Schalter, Druckplatten, Schlüssel, verschlossene Türen | 5 |
| Boss-Raum | Boss mit Phasen und Mustern, belohnt mit einer neuen Fähigkeit (z. B. Dash oder Doppelsprung) | 2 |
| Ruheraum | Checkpoint, Heilung, Speichern | 5 |

Später kommen zwei Zusatzmodi ohne großen Mehraufwand: **Endlos-Arena** (Score) und **Boss-Rush**, beide nutzen dieselben Räume.

Festgelegte Standardwerte (ohne Rückfrage, jederzeit änderbar):
- 3D aus der Schulterperspektive bleibt. Tempo mittel bis schnell, aber immer lesbar (Vorwarnung bei Angriffen).
- Dungeon-Aufbau ist handgebaut. Zufall gibt es nur bei Upgrade-Angeboten und der Zusammensetzung der Wellen.
- Tod bringt dich zum letzten Ruheraum. Gesäuberte Räume bleiben gesäubert.
- Spieltexte Deutsch, Code und Bezeichner Englisch. Arbeitstitel „Aldoria“.

## 2. Was bleibt, was wegfällt

Bleibt (schon gebaut): Bewegung mit Coyote-Time und Sprungpuffer, Rampen, Kamera, Schwert-Kombo, Ausweichrolle, Gegner mit Vorwarnung, Hit-Stop, Kamerawackeln, Level-JSON, Neuladen mit F5, Event-Bus, Tests.

Fällt weg aus dem RPG-Plan: Lobby, vier Welten, Skill-Tree, Münzen und Waffen-Upgrades, Relikte, Kosmetik, Quests, Erfolge, große Sammelsysteme.

## 3. Neue Bausteine

- **Dungeon-System:** `data/dungeons/*.json` listet Räume und ihre Türverbindungen. Es ist immer nur ein Raum geladen, Übergang mit Überblendung. `DungeonState` merkt sich gesäuberte Räume, Schlüssel, Splitter und den letzten Ruheraum.
- **Level-Elemente in JSON:** Türen (offen, verschlossen, Arena-gesperrt), Schlüssel, Schalter, Druckplatten, Checkpoints, bewegliche und bröckelnde Plattformen, Gefahrenzonen mit Respawn, Trigger („wenn alle Gegner tot“).
- **Kampf:** Wellen-Spawner, weitere Gegnertypen (Schütze, Brecher, Springer), Boss-Grundgerüst mit Phasen, Heiltrank, **Segen-System** (daten-getriebene Modifikatoren auf Spieler und Waffe).
- **Werkzeug:** Level-Editor-Overlay im Spiel (Boxen, Rampen, Türen, Spawns platzieren, speichern). Ohne ihn sind Räume in JSON von Hand zu mühsam.
- **Rest:** HUD mit Schlüsseln und Splittern, Boss-Leiste, Pause-Menü, Titel, Speichern (atomar, mit Version), Töne, echte Schatten für bessere Tiefenwirkung beim Springen.

## 4. Meilensteine

Eine „Sitzung“ ist ein Arbeitstag mit mir; die Schätzungen sind grob.

| # | Meilenstein | Fertig, wenn … | Sitzungen |
|---|---|---|---|
| M0–M2 | Fundament, Bewegung, Kampf-Durchstich | **erledigt** | – |
| M3 | Räume, Türen, Schlüssel, Schalter | **erledigt** | – |
| M4 | Plattform-Elemente | **erledigt** | – |
| M5 | Level-Editor-Overlay | **ausgelassen** (JSON + F5-Neuladen reicht bisher) | – |
| M6 | Arena-Kämpfe und Segen | **erledigt**, inkl. Endlos-Arena | – |
| M7 | Bosse | **erledigt**: Kobold-König und Schmiedegolem, je 3 Phasen | – |
| M8 | **Dungeon 1 (Vertical Slice)** | **erledigt**: 13 Räume, Wurzelhallen | – |
| M9 | Menüs und Speichern | **erledigt** | – |
| M10 | Ton und Optik | **erledigt** (Ton komplett synthetisiert; einfache Schatten als Blob, keine Schattenkarte) | – |
| M11 | Dungeon 2 und 3 | Dungeon 2 (11 Räume, Lava, Schmiedegolem) und Dungeon 3 (14 Räume, Himmelsturm, Doppelsprung, Sturmwächter) **erledigt** | – |
| M12 | Zusatzmodi | Endlos-Arena und Boss-Rush **erledigt** (Rekorde werden gespeichert) | – |

Gesamt: ca. 24–30 Sitzungen. **Nach M8** (Dungeon 1) entscheiden wir, ob Dungeon 2 und 3 in voller Größe entstehen. Nach M6 gibt es schon eine spielbare Arena, um früh zu prüfen, ob es Spaß macht.

## 5. Risiken

- **Vier Genres in einem Spiel** können unfokussiert wirken. Gegenmittel: der Raum-Rahmen und der Vertical Slice.
- **Level bauen ist mühsam.** Gegenmittel: Editor-Overlay in M5, bevor viele Räume entstehen.
- **Bewegung auf beweglichen Plattformen** (Mitnehmen des Spielers) ist heikel. Gegenmittel: früh in M4 bauen und mit Tests absichern.
- **Optik ohne Künstler.** Gegenmittel: Licht, Nebel und Farbpalette pro Dungeon, später optional CC0-Modelle.

## 6. Ideen für danach

1. Feedback zum Spielgefühl einarbeiten (Sprunghöhe, Kamera, Kampftempo, Schwierigkeit der Bosse).
2. Dungeon 4 mit neuer Fähigkeit (z. B. Enterhaken) und Boss.
3. Karte im Pausenmenü (besuchte Räume).
4. Weitere Bosse für den Boss-Rush (bisher drei).
5. Level-Editor-Overlay, falls viele weitere Räume entstehen sollen.
