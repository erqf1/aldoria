#pragma once

#include <vector>

#include "audio/sound_bank.h"
#include "core/event_bus.h"
#include "core/settings.h"
#include "raylib.h"

namespace aldoria {

// Spielt Effekte und Musik. Alle Klänge werden beim Start berechnet (siehe sound_bank). Ist kein
// Audiogerät vorhanden oder ist der Ton aus (Testmodus), tun alle Aufrufe nichts.
class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    bool init();
    void shutdown();
    bool ready() const { return ready_; }

    // Hört auf Spielereignisse und spielt die passenden Effekte
    void attach(EventBus& events);

    // Jeden Frame: Musikpuffer füllen, Lautstärken und Überblendung anwenden
    void update(float dt, const Settings& settings);

    void setMusic(MusicKind kind) { target_ = kind; }
    MusicKind music() const { return target_; }
    void play(Sfx sfx, float volume = 1.0f);
    // 0 = Auswahl bewegt, 1 = bestätigt, 2 = zurück
    void playUi(int kind);

private:
    struct Voices {
        std::vector<Sound> sounds;  // erste ist das Original, weitere sind Aliase (überlappende Wiedergabe)
        size_t next = 0;
    };

    bool ready_ = false;
    EventBus* events_ = nullptr;
    std::vector<SubscriptionId> subscriptions_;
    std::vector<Voices> voices_;
    Music music_[(int)MusicKind::Count]{};
    bool musicLoaded_[(int)MusicKind::Count]{};
    float mix_[(int)MusicKind::Count]{};
    MusicKind target_ = MusicKind::Explore;
    float sfxVolume_ = 0.8f;
};

}  // namespace aldoria
