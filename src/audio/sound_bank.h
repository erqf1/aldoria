#pragma once

#include <vector>

namespace aldoria {

// Alle Effekte des Spiels. Sie werden beim Start aus einfachen Wellenformen berechnet, es gibt keine Klangdateien.
enum class Sfx {
    Jump,
    Land,
    Swing1,
    Swing2,
    Swing3,
    HitEnemy,
    EnemyDie,
    PlayerHurt,
    PlayerDie,
    Dodge,
    Dash,
    Spark,
    SparkHit,
    PickupKey,
    PickupShard,
    PickupHeart,
    PickupItem,
    DoorOpen,
    DoorUnlock,
    Switch,
    Crumble,
    Potion,
    Heal,
    Checkpoint,
    EncounterStart,
    WaveStart,
    EncounterClear,
    BossRoar,
    BossStomp,
    BossShot,
    EnemyShot,
    Segen,
    UiMove,
    UiSelect,
    UiBack,
    Spawn,
    AirJump,
    Count
};

enum class MusicKind { Explore, Combat, Boss, Count };

// Berechnet einen Effekt (mono, 22050 Hz, Werte -1..1)
std::vector<float> renderSfx(Sfx sfx);

// Berechnet ein nahtlos wiederholbares Musikstück
std::vector<float> renderMusic(MusicKind kind);

// Wie laut ein Effekt im Verhältnis zu den anderen sein soll
float sfxBaseVolume(Sfx sfx);

}  // namespace aldoria
