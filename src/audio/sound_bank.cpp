#include "audio/sound_bank.h"

#include <algorithm>
#include <cmath>

#include "audio/synth.h"

namespace aldoria {
namespace {

using namespace synth;

std::vector<float> silence(float duration) { return std::vector<float>((size_t)(duration * (float)kSampleRate), 0.0f); }

// Ein Ton mit Startzeit, für Arpeggien und Fanfaren
void addTone(std::vector<float>& out, float at, float f0, float f1, float dur, float vol, Wave w = Wave::Triangle, float decay = 1.6f) {
    Tone t;
    t.freqStart = f0;
    t.freqEnd = f1;
    t.duration = dur;
    t.volume = vol;
    t.wave = w;
    t.decayPower = decay;
    mixInto(out, tone(t), at, 1.0f);
}

void addNoise(std::vector<float>& out, float at, float dur, float vol, float smoothing, float decay = 1.5f, unsigned seed = 1) {
    mixInto(out, noise(dur, vol, smoothing, decay, seed), at, 1.0f);
}

}  // namespace

float sfxBaseVolume(Sfx sfx) {
    switch (sfx) {
        case Sfx::Jump: return 0.5f;
        case Sfx::Land: return 0.6f;
        case Sfx::HitEnemy: return 0.9f;
        case Sfx::PlayerHurt: return 1.0f;
        case Sfx::BossRoar: return 1.0f;
        case Sfx::BossStomp: return 1.0f;
        case Sfx::UiMove: return 0.5f;
        default: return 0.8f;
    }
}

std::vector<float> renderSfx(Sfx sfx) {
    std::vector<float> out;
    switch (sfx) {
        case Sfx::Jump:
            out = silence(0.16f);
            addTone(out, 0, 320, 660, 0.14f, 0.5f, Wave::Sine, 1.2f);
            break;
        case Sfx::AirJump:
            out = silence(0.2f);
            addNoise(out, 0, 0.12f, 0.3f, 0.6f, 1.0f, 41);
            addTone(out, 0, 520, 1100, 0.16f, 0.32f, Wave::Triangle, 1.1f);
            addTone(out, 0.03f, 780, 1500, 0.12f, 0.16f, Wave::Sine, 1.3f);
            break;
        case Sfx::Land:
            out = silence(0.14f);
            addNoise(out, 0, 0.1f, 0.5f, 0.15f, 1.2f, 3);
            addTone(out, 0, 150, 60, 0.12f, 0.6f, Wave::Sine, 1.5f);
            break;
        case Sfx::Swing1:
            out = silence(0.2f);
            addNoise(out, 0, 0.17f, 0.45f, 0.3f, 1.0f, 11);
            break;
        case Sfx::Swing2:
            out = silence(0.2f);
            addNoise(out, 0, 0.17f, 0.45f, 0.38f, 1.0f, 12);
            addTone(out, 0, 500, 300, 0.12f, 0.08f, Wave::Sine, 1.0f);
            break;
        case Sfx::Swing3:
            out = silence(0.36f);
            addNoise(out, 0, 0.33f, 0.5f, 0.22f, 0.8f, 13);
            addTone(out, 0, 260, 120, 0.3f, 0.12f, Wave::Sine, 1.0f);
            break;
        case Sfx::HitEnemy:
            out = silence(0.16f);
            addNoise(out, 0, 0.08f, 0.6f, 0.7f, 1.3f, 21);
            addTone(out, 0, 230, 90, 0.12f, 0.4f, Wave::Square, 1.6f);
            break;
        case Sfx::EnemyDie:
            out = silence(0.34f);
            addTone(out, 0, 340, 50, 0.3f, 0.4f, Wave::Saw, 1.3f);
            addNoise(out, 0, 0.2f, 0.3f, 0.3f, 1.5f, 22);
            break;
        case Sfx::PlayerHurt:
            out = silence(0.3f);
            addTone(out, 0, 260, 90, 0.26f, 0.5f, Wave::Saw, 1.3f);
            addNoise(out, 0, 0.12f, 0.4f, 0.6f, 1.6f, 23);
            break;
        case Sfx::PlayerDie:
            out = silence(0.9f);
            {
                Tone t;
                t.freqStart = 200;
                t.freqEnd = 38;
                t.duration = 0.85f;
                t.volume = 0.5f;
                t.wave = Wave::Saw;
                t.vibratoHz = 7;
                t.vibratoDepth = 0.04f;
                t.decayPower = 1.2f;
                mixInto(out, tone(t), 0);
            }
            addNoise(out, 0, 0.3f, 0.3f, 0.3f, 1.4f, 24);
            break;
        case Sfx::Dodge:
            out = silence(0.24f);
            addNoise(out, 0, 0.22f, 0.4f, 0.2f, 1.1f, 31);
            addTone(out, 0, 520, 200, 0.2f, 0.15f, Wave::Sine, 1.2f);
            break;
        case Sfx::Dash:
            out = silence(0.22f);
            addNoise(out, 0, 0.2f, 0.42f, 0.5f, 1.0f, 32);
            addTone(out, 0, 400, 1300, 0.18f, 0.2f, Wave::Sine, 1.0f);
            break;
        case Sfx::Spark:
            out = silence(0.2f);
            addTone(out, 0, 700, 1800, 0.16f, 0.3f, Wave::Sine, 1.0f);
            addTone(out, 0.02f, 1400, 2300, 0.12f, 0.14f, Wave::Triangle, 1.4f);
            break;
        case Sfx::SparkHit:
            out = silence(0.14f);
            addNoise(out, 0, 0.1f, 0.4f, 0.8f, 1.4f, 33);
            addTone(out, 0, 900, 300, 0.1f, 0.25f, Wave::Sine, 1.4f);
            break;
        case Sfx::PickupKey:
            out = silence(0.5f);
            addTone(out, 0.00f, 659, 659, 0.2f, 0.3f);
            addTone(out, 0.07f, 831, 831, 0.2f, 0.3f);
            addTone(out, 0.14f, 988, 988, 0.2f, 0.3f);
            addTone(out, 0.21f, 1319, 1319, 0.28f, 0.3f);
            break;
        case Sfx::PickupShard:
            out = silence(0.16f);
            addTone(out, 0, 1300, 1900, 0.1f, 0.3f, Wave::Triangle, 1.2f);
            addTone(out, 0.04f, 2600, 2600, 0.08f, 0.1f, Wave::Sine, 1.5f);
            break;
        case Sfx::PickupHeart:
            out = silence(0.4f);
            addTone(out, 0, 520, 780, 0.2f, 0.35f, Wave::Sine, 1.0f);
            addTone(out, 0.12f, 780, 1040, 0.25f, 0.35f, Wave::Sine, 1.2f);
            break;
        case Sfx::PickupItem:
            out = silence(1.0f);
            addTone(out, 0.00f, 523, 523, 0.3f, 0.3f);
            addTone(out, 0.12f, 659, 659, 0.3f, 0.3f);
            addTone(out, 0.24f, 784, 784, 0.3f, 0.3f);
            addTone(out, 0.36f, 1047, 1047, 0.6f, 0.34f, Wave::Triangle, 1.0f);
            addTone(out, 0.36f, 1319, 1319, 0.6f, 0.18f, Wave::Sine, 1.0f);
            break;
        case Sfx::DoorOpen:
            out = silence(0.6f);
            addNoise(out, 0, 0.55f, 0.45f, 0.1f, 0.8f, 41);
            addTone(out, 0, 70, 50, 0.55f, 0.5f, Wave::Sine, 0.8f);
            break;
        case Sfx::DoorUnlock:
            out = silence(0.4f);
            addNoise(out, 0, 0.03f, 0.5f, 0.9f, 1.0f, 42);
            addTone(out, 0.03f, 880, 880, 0.22f, 0.3f);
            addTone(out, 0.1f, 1320, 1320, 0.25f, 0.3f);
            break;
        case Sfx::Switch:
            out = silence(0.3f);
            addTone(out, 0, 480, 960, 0.14f, 0.3f, Wave::Sine, 1.0f);
            addTone(out, 0.08f, 960, 960, 0.2f, 0.25f, Wave::Triangle, 1.4f);
            break;
        case Sfx::Crumble:
            out = silence(0.45f);
            addNoise(out, 0, 0.4f, 0.5f, 0.55f, 0.9f, 43);
            addNoise(out, 0.1f, 0.3f, 0.3f, 0.3f, 1.2f, 44);
            break;
        case Sfx::Potion:
            out = silence(0.5f);
            for (int i = 0; i < 5; i++) addTone(out, 0.07f * (float)i, 500.0f + 130.0f * (float)i, 800.0f + 160.0f * (float)i, 0.1f, 0.25f, Wave::Sine, 1.2f);
            break;
        case Sfx::Heal:
            out = silence(0.55f);
            for (int i = 0; i < 5; i++) addTone(out, 0.06f * (float)i, 660.0f * std::pow(1.25f, (float)i), 660.0f * std::pow(1.25f, (float)i), 0.22f, 0.2f);
            break;
        case Sfx::Checkpoint:
            out = silence(1.2f);
            addTone(out, 0, 880, 880, 1.1f, 0.3f, Wave::Sine, 3.0f);
            addTone(out, 0, 1320, 1320, 0.9f, 0.15f, Wave::Sine, 3.0f);
            addTone(out, 0, 1760, 1760, 0.7f, 0.08f, Wave::Sine, 3.0f);
            break;
        case Sfx::EncounterStart: {
            out = silence(0.9f);
            Tone t;
            t.freqStart = 165;
            t.freqEnd = 150;
            t.duration = 0.8f;
            t.volume = 0.32f;
            t.wave = Wave::Saw;
            t.attack = 0.15f;
            t.decayPower = 0.8f;
            mixInto(out, tone(t), 0);
            addNoise(out, 0, 0.1f, 0.2f, 0.5f, 1.5f, 51);
            break;
        }
        case Sfx::WaveStart:
            out = silence(0.3f);
            addTone(out, 0.00f, 440, 440, 0.1f, 0.25f, Wave::Square, 1.0f);
            addTone(out, 0.12f, 587, 587, 0.14f, 0.25f, Wave::Square, 1.0f);
            break;
        case Sfx::EncounterClear:
            out = silence(0.9f);
            addTone(out, 0.00f, 523, 523, 0.3f, 0.3f);
            addTone(out, 0.10f, 659, 659, 0.3f, 0.3f);
            addTone(out, 0.20f, 784, 784, 0.3f, 0.3f);
            addTone(out, 0.32f, 1047, 1047, 0.55f, 0.34f, Wave::Triangle, 1.0f);
            break;
        case Sfx::BossRoar: {
            out = silence(1.0f);
            Tone t;
            t.freqStart = 110;
            t.freqEnd = 60;
            t.duration = 0.95f;
            t.volume = 0.5f;
            t.wave = Wave::Saw;
            t.vibratoHz = 12;
            t.vibratoDepth = 0.06f;
            t.attack = 0.05f;
            t.decayPower = 0.9f;
            mixInto(out, tone(t), 0);
            addNoise(out, 0, 0.9f, 0.35f, 0.1f, 1.0f, 52);
            break;
        }
        case Sfx::BossStomp:
            out = silence(0.6f);
            addTone(out, 0, 90, 35, 0.55f, 0.75f, Wave::Sine, 1.4f);
            addNoise(out, 0, 0.3f, 0.5f, 0.12f, 1.3f, 53);
            break;
        case Sfx::BossShot:
            out = silence(0.35f);
            addTone(out, 0, 300, 120, 0.3f, 0.35f, Wave::Saw, 1.3f);
            addNoise(out, 0, 0.2f, 0.3f, 0.4f, 1.3f, 54);
            break;
        case Sfx::EnemyShot:
            out = silence(0.24f);
            addTone(out, 0, 520, 240, 0.2f, 0.3f, Wave::Triangle, 1.4f);
            addNoise(out, 0, 0.05f, 0.2f, 0.8f, 1.5f, 55);
            break;
        case Sfx::Segen:
            out = silence(1.1f);
            for (float f : {523.0f, 659.0f, 784.0f, 988.0f}) addTone(out, 0, f, f, 1.0f, 0.16f, Wave::Triangle, 1.6f);
            addEcho(out, 0.12f, 0.4f, 0.4f, false);
            break;
        case Sfx::UiMove:
            out = silence(0.05f);
            addTone(out, 0, 700, 700, 0.045f, 0.2f, Wave::Triangle, 1.0f);
            break;
        case Sfx::UiSelect:
            out = silence(0.1f);
            addTone(out, 0, 900, 1200, 0.09f, 0.25f, Wave::Triangle, 1.0f);
            break;
        case Sfx::UiBack:
            out = silence(0.1f);
            addTone(out, 0, 600, 400, 0.09f, 0.25f, Wave::Triangle, 1.0f);
            break;
        case Sfx::Spawn:
            out = silence(0.45f);
            addNoise(out, 0, 0.4f, 0.3f, 0.15f, 0.7f, 56);
            addTone(out, 0, 100, 200, 0.4f, 0.3f, Wave::Sine, 0.9f);
            break;
        case Sfx::Count: break;
    }
    fadeEdges(out, 0.003f);
    return out;
}

// ---------------------------------------------------------------- Musik

namespace {

struct Chord {
    float bass;
    float notes[3];
};

// Ein Dur-/Moll-Viererzug: pro Akkord acht Schläge (zwei Takte)
void arrangeExplore(std::vector<float>& out, float beat) {
    const Chord chords[4] = {{45, {57, 60, 64}}, {41, {53, 57, 60}}, {48, {60, 64, 67}}, {43, {55, 59, 62}}};
    const int arp[8] = {0, 1, 2, 1, 0, 2, 1, 2};
    for (int c = 0; c < 4; c++) {
        float t0 = (float)(c * 8) * beat;
        for (float n : chords[c].notes) {
            Tone t;
            t.freqStart = t.freqEnd = midiToHz(n);
            t.duration = 8.6f * beat;
            t.volume = 0.11f;
            t.wave = Wave::Triangle;
            t.attack = 0.9f;
            t.decayPower = 0.5f;
            mixInto(out, tone(t), t0, 1.0f, true);
        }
        for (int b = 0; b < 2; b++) {
            Tone t;
            t.freqStart = t.freqEnd = midiToHz(chords[c].bass);
            t.duration = 3.6f * beat;
            t.volume = 0.24f;
            t.wave = Wave::Sine;
            t.decayPower = 1.0f;
            mixInto(out, tone(t), t0 + (float)(b * 4) * beat, 1.0f, true);
        }
        for (int b = 0; b < 8; b++) {
            Tone t;
            t.freqStart = t.freqEnd = midiToHz(chords[c].notes[arp[b]] + 12);
            t.duration = 0.7f * beat;
            t.volume = 0.07f;
            t.wave = Wave::Triangle;
            t.decayPower = 2.2f;
            mixInto(out, tone(t), t0 + (float)b * beat, 1.0f, true);
        }
    }
    addEcho(out, beat * 0.75f, 0.45f, 0.35f, true);
}

void arrangeCombat(std::vector<float>& out, float beat, const Chord (&chords)[4], float bassWave, bool heavy) {
    for (int b = 0; b < 32; b++) {
        float t = (float)b * beat;
        Tone kick;
        kick.freqStart = 140;
        kick.freqEnd = 45;
        kick.duration = 0.24f;
        kick.volume = heavy ? 0.9f : 0.8f;
        kick.decayPower = 2.0f;
        mixInto(out, tone(kick), t, 1.0f, true);
        if (b % 2 == 1) mixInto(out, noise(0.16f, heavy ? 0.4f : 0.3f, 0.5f, 1.4f, (unsigned)(60 + b)), t, 1.0f, true);
        mixInto(out, noise(0.04f, 0.12f, 0.95f, 1.5f, (unsigned)(100 + b)), t + beat * 0.5f, 1.0f, true);
        if (heavy && b % 4 == 2) {
            Tone k2 = kick;
            k2.volume = 0.6f;
            mixInto(out, tone(k2), t + beat * 0.5f, 1.0f, true);
        }
    }
    for (int c = 0; c < 4; c++) {
        float t0 = (float)(c * 8) * beat;
        int steps = heavy ? 32 : 16;
        float stepLen = 8.0f * beat / (float)steps;
        for (int s = 0; s < steps; s++) {
            Tone t;
            float octave = (s % 2 == 0) ? 0.0f : 12.0f;
            t.freqStart = t.freqEnd = midiToHz(chords[c].bass + octave);
            t.duration = stepLen * 0.9f;
            t.volume = 0.13f;
            t.wave = bassWave > 0.5f ? Wave::Saw : Wave::Square;
            t.decayPower = 1.6f;
            mixInto(out, tone(t), t0 + (float)s * stepLen, 1.0f, true);
        }
        for (int stab = 0; stab < 4; stab++) {
            float at = t0 + ((float)stab * 2.0f + 1.5f) * beat;
            for (float n : chords[c].notes) {
                Tone t;
                t.freqStart = t.freqEnd = midiToHz(n);
                t.duration = 0.3f * beat;
                t.volume = heavy ? 0.06f : 0.05f;
                t.wave = Wave::Saw;
                t.decayPower = 2.0f;
                mixInto(out, tone(t), at, 1.0f, true);
            }
        }
        if (heavy) {
            // Hohe Arpeggio-Linie
            for (int s = 0; s < 16; s++) {
                Tone t;
                t.freqStart = t.freqEnd = midiToHz(chords[c].notes[s % 3] + 24);
                t.duration = 0.5f * beat * 0.5f;
                t.volume = 0.05f;
                t.wave = Wave::Triangle;
                t.decayPower = 2.0f;
                mixInto(out, tone(t), t0 + (float)s * beat * 0.5f, 1.0f, true);
            }
        }
    }
    addEcho(out, beat * 0.75f, 0.35f, 0.2f, true);
}

}  // namespace

std::vector<float> renderMusic(MusicKind kind) {
    std::vector<float> out;
    switch (kind) {
        case MusicKind::Explore: {
            float beat = 60.0f / 76.0f;
            out = silence(32.0f * beat);
            arrangeExplore(out, beat);
            break;
        }
        case MusicKind::Combat: {
            float beat = 60.0f / 132.0f;
            out = silence(32.0f * beat);
            const Chord chords[4] = {{45, {57, 60, 64}}, {41, {53, 57, 60}}, {48, {60, 64, 67}}, {43, {55, 59, 62}}};
            arrangeCombat(out, beat, chords, 1.0f, false);
            break;
        }
        case MusicKind::Boss: {
            float beat = 60.0f / 148.0f;
            out = silence(32.0f * beat);
            const Chord chords[4] = {{38, {62, 65, 69}}, {34, {58, 62, 65}}, {31, {55, 58, 62}}, {33, {61, 64, 69}}};
            arrangeCombat(out, beat, chords, 1.0f, true);
            break;
        }
        case MusicKind::Count: break;
    }
    normalize(out, 0.6f);
    return out;
}

}  // namespace aldoria
