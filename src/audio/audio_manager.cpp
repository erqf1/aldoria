#include "audio/audio_manager.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "audio/synth.h"
#include "core/events.h"
#include "core/log.h"
#include "core/math_util.h"

namespace aldoria {
namespace {

constexpr int kVoicesPerSound = 3;

// Baut aus Rohwerten eine raylib-Wave (mono, 16 Bit)
Wave makeWave(const std::vector<float>& samples) {
    std::vector<std::int16_t> pcm = synth::toPcm16(samples);
    Wave wave{};
    wave.frameCount = (unsigned int)pcm.size();
    wave.sampleRate = synth::kSampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = MemAlloc((unsigned int)(pcm.size() * sizeof(std::int16_t)));
    std::memcpy(wave.data, pcm.data(), pcm.size() * sizeof(std::int16_t));
    return wave;
}

const char* musicName(MusicKind k) {
    switch (k) {
        case MusicKind::Explore: return "explore";
        case MusicKind::Combat: return "combat";
        case MusicKind::Boss: return "boss";
        default: return "unknown";
    }
}

}  // namespace

AudioManager::~AudioManager() { shutdown(); }

bool AudioManager::init() {
    if (ready_) return true;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        Log::warn(LogCategory::Core, "Kein Audiogerät verfügbar, das Spiel läuft ohne Ton");
        return false;
    }

    voices_.resize((size_t)Sfx::Count);
    for (size_t i = 0; i < (size_t)Sfx::Count; i++) {
        std::vector<float> samples = renderSfx((Sfx)i);
        if (samples.empty()) continue;
        Wave w = makeWave(samples);
        Sound first = LoadSoundFromWave(w);
        UnloadWave(w);
        voices_[i].sounds.push_back(first);
        for (int v = 1; v < kVoicesPerSound; v++) voices_[i].sounds.push_back(LoadSoundAlias(first));
    }

    // Musik: als WAV in einen Zwischenordner schreiben und von dort streamen
    std::filesystem::path dir = std::filesystem::path(GetApplicationDirectory()) / "audio_cache";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    for (int k = 0; k < (int)MusicKind::Count; k++) {
        std::filesystem::path path = dir / (std::string(musicName((MusicKind)k)) + ".wav");
        Wave w = makeWave(renderMusic((MusicKind)k));
        bool exported = ExportWave(w, path.string().c_str());
        UnloadWave(w);
        if (!exported) {
            Log::warn(LogCategory::Loading, "Musik '{}' konnte nicht geschrieben werden, sie fehlt", musicName((MusicKind)k));
            continue;
        }
        music_[k] = LoadMusicStream(path.string().c_str());
        if (music_[k].stream.buffer == nullptr) continue;
        music_[k].looping = true;
        musicLoaded_[k] = true;
        SetMusicVolume(music_[k], 0.0f);
        PlayMusicStream(music_[k]);
    }
    mix_[(int)MusicKind::Explore] = 1.0f;
    ready_ = true;
    Log::info(LogCategory::Loading, "Audio bereit ({} Effekte, Musik)", (int)Sfx::Count);
    return true;
}

void AudioManager::shutdown() {
    if (events_) {
        for (SubscriptionId id : subscriptions_) events_->unsubscribe(id);
        subscriptions_.clear();
        events_ = nullptr;
    }
    if (!ready_) return;
    for (int k = 0; k < (int)MusicKind::Count; k++) {
        if (musicLoaded_[k]) {
            StopMusicStream(music_[k]);
            UnloadMusicStream(music_[k]);
            musicLoaded_[k] = false;
        }
    }
    for (Voices& v : voices_) {
        for (size_t i = 1; i < v.sounds.size(); i++) UnloadSoundAlias(v.sounds[i]);
        if (!v.sounds.empty()) UnloadSound(v.sounds[0]);
    }
    voices_.clear();
    CloseAudioDevice();
    ready_ = false;
}

void AudioManager::update(float dt, const Settings& settings) {
    if (!ready_) return;
    SetMasterVolume(settings.masterVolume);
    sfxVolume_ = settings.sfxVolume;
    for (int k = 0; k < (int)MusicKind::Count; k++) {
        if (!musicLoaded_[k]) continue;
        UpdateMusicStream(music_[k]);
        float goal = target_ == (MusicKind)k ? 1.0f : 0.0f;
        mix_[k] = approach(mix_[k], goal, dt * 0.8f);
        SetMusicVolume(music_[k], settings.musicVolume * mix_[k] * 0.85f);
    }
}

void AudioManager::play(Sfx sfx, float volume) {
    if (!ready_ || (size_t)sfx >= voices_.size()) return;
    Voices& v = voices_[(size_t)sfx];
    if (v.sounds.empty()) return;
    Sound& s = v.sounds[v.next];
    v.next = (v.next + 1) % v.sounds.size();
    SetSoundVolume(s, std::clamp(volume * sfxBaseVolume(sfx) * sfxVolume_, 0.0f, 1.0f));
    // Leichte Tonhöhenschwankung, damit Wiederholungen nicht wie eine Maschine klingen
    SetSoundPitch(s, 1.0f + (float)GetRandomValue(-4, 4) / 100.0f);
    PlaySound(s);
}

void AudioManager::playUi(int kind) {
    play(kind == 0 ? Sfx::UiMove : (kind == 1 ? Sfx::UiSelect : Sfx::UiBack));
}

void AudioManager::attach(EventBus& events) {
    events_ = &events;
    auto& ev = events;
    auto sub = [this](SubscriptionId id) { subscriptions_.push_back(id); };

    sub(ev.subscribe<PlayerJumped>([this](const PlayerJumped&) { play(Sfx::Jump); }));
    sub(ev.subscribe<PlayerAirJumped>([this](const PlayerAirJumped&) { play(Sfx::AirJump); }));
    sub(ev.subscribe<PlayerLanded>([this](const PlayerLanded& e) {
        if (e.impactSpeed > 6.0f) play(Sfx::Land, std::min(1.0f, e.impactSpeed / 14.0f));
    }));
    sub(ev.subscribe<PlayerAttacked>([this](const PlayerAttacked& e) {
        play(e.comboStep == 0 ? Sfx::Swing1 : (e.comboStep == 1 ? Sfx::Swing2 : Sfx::Swing3));
    }));
    sub(ev.subscribe<PlayerDodged>([this](const PlayerDodged&) { play(Sfx::Dodge); }));
    sub(ev.subscribe<PlayerDashed>([this](const PlayerDashed&) { play(Sfx::Dash); }));
    sub(ev.subscribe<PlayerCast>([this](const PlayerCast&) { play(Sfx::Spark); }));
    sub(ev.subscribe<PlayerDamaged>([this](const PlayerDamaged&) { play(Sfx::PlayerHurt); }));
    sub(ev.subscribe<PlayerHealed>([this](const PlayerHealed&) { play(Sfx::Heal, 0.7f); }));
    sub(ev.subscribe<PlayerDied>([this](const PlayerDied&) { play(Sfx::PlayerDie); }));
    sub(ev.subscribe<EnemyDamaged>([this](const EnemyDamaged&) { play(Sfx::HitEnemy); }));
    sub(ev.subscribe<EnemyDied>([this](const EnemyDied&) { play(Sfx::EnemyDie); }));
    sub(ev.subscribe<EnemyShot>([this](const EnemyShot&) { play(Sfx::EnemyShot); }));
    sub(ev.subscribe<PickupCollected>([this](const PickupCollected& e) {
        if (e.type == "small_key" || e.type == "boss_key") play(Sfx::PickupKey);
        else if (e.type == "shard") play(Sfx::PickupShard);
        else if (e.type == "heart" || e.type == "health_drop") play(Sfx::PickupHeart);
        else if (e.type == "flask" || e.type == "potion") play(Sfx::Potion);
        else if (e.type == "item_dash") play(Sfx::PickupItem);
    }));
    sub(ev.subscribe<DoorOpened>([this](const DoorOpened&) { play(Sfx::DoorOpen); }));
    sub(ev.subscribe<DoorUnlocked>([this](const DoorUnlocked&) { play(Sfx::DoorUnlock); }));
    sub(ev.subscribe<SwitchToggled>([this](const SwitchToggled& e) {
        if (e.active) play(Sfx::Switch);
    }));
    sub(ev.subscribe<PlatformCrumbled>([this](const PlatformCrumbled&) { play(Sfx::Crumble); }));
    sub(ev.subscribe<EncounterStarted>([this](const EncounterStarted&) { play(Sfx::EncounterStart); }));
    sub(ev.subscribe<EncounterWaveStarted>([this](const EncounterWaveStarted&) { play(Sfx::WaveStart); }));
    sub(ev.subscribe<EncounterCleared>([this](const EncounterCleared&) { play(Sfx::EncounterClear); }));
    sub(ev.subscribe<BossRoar>([this](const BossRoar&) { play(Sfx::BossRoar); }));
    sub(ev.subscribe<BossStomp>([this](const BossStomp&) { play(Sfx::BossStomp); }));
    sub(ev.subscribe<BossVolleyFired>([this](const BossVolleyFired&) { play(Sfx::BossShot); }));
    sub(ev.subscribe<BossPhaseChanged>([this](const BossPhaseChanged&) { play(Sfx::BossRoar); }));
    sub(ev.subscribe<BossDefeated>([this](const BossDefeated&) { play(Sfx::EncounterClear); }));
    sub(ev.subscribe<CheckpointActivated>([this](const CheckpointActivated&) { play(Sfx::Checkpoint); }));
    sub(ev.subscribe<SegenChosen>([this](const SegenChosen&) { play(Sfx::Segen); }));
    sub(ev.subscribe<AbilityUnlocked>([this](const AbilityUnlocked&) { play(Sfx::PickupItem); }));
}

}  // namespace aldoria
