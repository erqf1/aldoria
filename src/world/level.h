#pragma once

#include <optional>
#include <string>
#include <vector>

#include "raylib.h"
#include "world/environment.h"

namespace aldoria {

// Achsenparallele Box: Sichtbares Level-Element und zugleich Kollisionskörper.
struct LevelBox {
    Vector3 center{0, 0, 0};
    Vector3 size{1, 1, 1};
    Color color{150, 150, 150, 255};
    bool solid = true;
    bool visible = true;
    std::string tag;

    float minX() const { return center.x - size.x * 0.5f; }
    float maxX() const { return center.x + size.x * 0.5f; }
    float minY() const { return center.y - size.y * 0.5f; }
    float maxY() const { return center.y + size.y * 0.5f; }
    float minZ() const { return center.z - size.z * 0.5f; }
    float maxZ() const { return center.z + size.z * 0.5f; }
};

// Richtung, in die eine Rampe ansteigt
enum class RampDir { PlusX, MinusX, PlusZ, MinusZ };

// Keilförmige Rampe: `size` ist die umschließende Box, die Höhe steigt entlang `rise` von 0 auf size.y.
struct LevelRamp {
    Vector3 center{0, 0, 0};
    Vector3 size{4, 1, 4};
    RampDir rise = RampDir::PlusZ;
    Color color{150, 150, 150, 255};

    float baseY() const { return center.y - size.y * 0.5f; }
    float minX() const { return center.x - size.x * 0.5f; }
    float maxX() const { return center.x + size.x * 0.5f; }
    float minZ() const { return center.z - size.z * 0.5f; }
    float maxZ() const { return center.z + size.z * 0.5f; }
    // Höhe der Rampenoberfläche an (x, z); Punkte außerhalb werden auf den Rand geklemmt
    float surfaceHeight(float x, float z) const;
};

struct EnemySpawn {
    std::string type;
    Vector3 position{0, 0, 0};
};

// ---------------------------------------------------------------- Raum-Elemente

enum class DoorKind {
    Open,          // immer offen
    Key,           // öffnet sich mit einem Schlüssel (key_type)
    Switch,        // offen, solange der Schalter `switchId` aktiv ist
    Arena,         // schließt sich während eines Arena-Kampfes
    BossDefeated,  // öffnet sich, wenn der Boss des Raums besiegt ist
    Sealed         // bleibt zu (nur zur Dekoration oder Vorbereitung)
};

struct LevelDoor {
    std::string id;
    DoorKind kind = DoorKind::Open;
    Vector3 center{0, 0, 0};  // Mitte der Türbox
    Vector3 size{4, 4.5f, 1};
    Color color{150, 110, 70, 255};
    std::string targetRoom;   // leer = führt nirgendwohin
    std::string targetDoor;
    std::string keyType = "small_key";
    std::string switchId;
    Vector3 arrival{0, 0, 0}; // wo man ankommt, wenn man durch diese Tür in den Raum kommt
    float arrivalYaw = 0.0f;
    int boxIndex = -1;        // Türbox in Level::boxes
};

enum class SwitchKind {
    Plate,   // Druckplatte: aktiv, solange jemand darauf steht
    Crystal  // Kristall: wird durch einen Schwertschlag oder Funken aktiviert
};

struct LevelSwitch {
    std::string id;
    SwitchKind kind = SwitchKind::Plate;
    Vector3 position{0, 0, 0};  // Fußpunkt (Plate: Mitte der Oberseite)
    Vector3 size{1.8f, 0.2f, 1.8f};
    bool latch = false;         // bleibt nach dem Auslösen an
    float holdTime = 0.0f;      // bleibt so viele Sekunden aktiv, nachdem niemand mehr darauf steht
    int boxIndex = -1;
};

enum class PlatformKind {
    Moving,  // fährt zwischen Wegpunkten hin und her
    Crumble  // bröckelt kurz nach dem Betreten weg und kommt später wieder
};

struct LevelPlatform {
    std::string id;
    PlatformKind kind = PlatformKind::Moving;
    Vector3 size{4, 0.6f, 4};
    Color color{150, 140, 120, 255};
    std::vector<Vector3> path;  // Mittelpunkte; path[0] ist die Startposition
    float speed = 2.0f;
    float pause = 0.6f;         // Wartezeit an den Endpunkten
    float crumbleDelay = 0.6f;
    float respawnTime = 3.0f;
    int boxIndex = -1;
};

enum class HazardKind { Spikes, Lava };

struct LevelHazard {
    HazardKind kind = HazardKind::Spikes;
    Vector3 center{0, 0, 0};
    Vector3 size{2, 0.4f, 2};
    float damage = 12.0f;
    int boxIndex = -1;
};

struct LevelPickup {
    std::string id;
    std::string type;  // small_key, boss_key, shard, heart, flask, item_dash, item_double_jump
    Vector3 position{0, 0, 0};
};

struct WaveEntry {
    std::string type;
    int count = 1;
};

struct LevelEncounter {
    std::vector<std::vector<WaveEntry>> waves;
    std::vector<Vector3> spawnPoints;
    std::string reward = "segen";  // "segen" = Wahl aus drei Upgrades, "" = keine Belohnung
};

struct LevelBossSpawn {
    std::string type;
    Vector3 position{0, 0, 0};
};

struct LevelDecor {
    std::string kind;  // tree, rock, bush, pillar, torch, mushroom, shrine
    Vector3 position{0, 0, 0};
    float scale = 1.0f;
    float yaw = 0.0f;
};

// Ein festes Punktlicht des Raums (Fackel, Feuerschale, Lava, Kristall). Farbe ist Farbe mal Helligkeit.
struct LevelLight {
    Vector3 position{0, 0, 0};
    Vector3 color{1, 1, 1};
    float radius = 8.0f;
    float flicker = 0.0f;  // 0 = ruhig, 1 = Kerzenflackern
};

class Level {
public:
    // Kanten bis zu dieser Höhe gelten als Stufe (begehbar), höhere als Wand
    static constexpr float kStepHeight = 0.4f;

    std::string id = "fallback";
    std::string name = "Notfall-Ebene";
    std::string type = "free";  // free, platform, arena, puzzle, boss, rest, treasure, hub
    std::string theme = "wurzel";  // Materialthema: wurzel, schmiede, himmel (siehe data/materials.json)
    std::vector<LevelLight> lights;
    Environment env;
    Vector3 playerSpawn{0, 0, 0};
    float playerSpawnYaw = 3.14159265f;  // Blickrichtung beim Start (PI = nach Norden, -Z)
    bool hasGround = true;
    float groundY = 0.0f;
    float killY = -30.0f;       // darunter gilt der Spieler als abgestürzt
    Vector2 groundSize{60, 60}; // zugleich die begehbare Fläche
    Color groundColor{96, 146, 84, 255};
    std::vector<LevelBox> boxes;
    std::vector<LevelRamp> ramps;
    std::vector<EnemySpawn> enemySpawns;
    std::vector<Vector3> arenaSpawnPoints;  // Startpunkte für den Endlos-Arena-Modus

    std::vector<LevelDoor> doors;
    std::vector<LevelSwitch> switches;
    std::vector<LevelPlatform> platforms;
    std::vector<LevelHazard> hazards;
    std::vector<LevelPickup> pickups;
    std::optional<Vector3> checkpoint;
    std::optional<LevelEncounter> encounter;
    std::optional<LevelBossSpawn> boss;
    std::vector<LevelDecor> decor;
    Vector2 shellSize{0, 0};    // Innenmaße der Raumhülle (0 = keine Hülle)
    Color wallCap{90, 150, 80, 255};  // Farbton der Wandkronen (Moos in den Wurzelhallen, Glut in der Schmiede)

    // Schiebt einen stehenden Zylinder (Radius, Höhe) aus Wänden und Levelgrenzen heraus
    void resolveHorizontal(Vector3& pos, float radius, float height) const;

    // Höhe der tragenden Fläche unter dem Zylinder. `feetY` ist die höchste Fußposition
    // des Frames (vorher/nachher), damit man auch schnell fallend nicht durch Kisten rutscht.
    float groundHeight(Vector3 pos, float radius, float feetY) const;

    // Strahl gegen alle festen Körper. Liefert die Entfernung zum nächsten Treffer.
    bool raycast(Vector3 origin, Vector3 dir, float maxDist, float& hitDist) const;

    // Steht ein Zylinder auf dieser Box (Fuß auf der Oberseite, Grundfläche überlappt)?
    static bool standsOn(const LevelBox& box, Vector3 feet, float radius);

    const LevelDoor* findDoor(const std::string& doorId) const;

    static Level makeFallback();
};

struct LevelLoadResult {
    std::optional<Level> level;
    std::string error;  // gefüllt, wenn level leer ist
};

LevelLoadResult loadLevelFromJson(const std::string& text);
LevelLoadResult loadLevelFromFile(const std::string& path);

}  // namespace aldoria
