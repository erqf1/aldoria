#pragma once

#include <cstdint>
#include <vector>

namespace aldoria::synth {

// Einfache Klangerzeugung im Code: Töne mit Frequenzverlauf, Rauschen und Mischen.
// Alle Puffer sind mono, Werte im Bereich -1..1.
constexpr int kSampleRate = 22050;

enum class Wave { Sine, Triangle, Saw, Square };

struct Tone {
    float freqStart = 440.0f;
    float freqEnd = 440.0f;      // Verlauf von freqStart nach freqEnd (exponentiell)
    float duration = 0.2f;
    float volume = 0.5f;
    Wave wave = Wave::Sine;
    float attack = 0.005f;       // Einblendzeit
    float decayPower = 1.6f;     // größer = schnelleres Ausklingen
    float vibratoHz = 0.0f;
    float vibratoDepth = 0.0f;   // Anteil der Frequenz (0,02 = 2 %)
};

std::vector<float> tone(const Tone& t);

// Weißes Rauschen mit Tiefpass (`smoothing` 0..1: klein = dumpf) und Ausklingen
std::vector<float> noise(float duration, float volume, float smoothing, float decayPower = 1.5f, unsigned seed = 1);

// Addiert `src` ab `offsetSeconds` in `dst`. Mit `wrap` läuft der Überhang am Ende vorn weiter (nahtlose Schleifen).
void mixInto(std::vector<float>& dst, const std::vector<float>& src, float offsetSeconds, float gain = 1.0f, bool wrap = false);

// Echo für mehr Raumgefühl
void addEcho(std::vector<float>& buffer, float delaySeconds, float feedback, float mix, bool wrapAround);

// Skaliert so, dass der größte Ausschlag `peak` beträgt
void normalize(std::vector<float>& buffer, float peak);

// Blendet Anfang und Ende kurz aus (gegen Knacken)
void fadeEdges(std::vector<float>& buffer, float seconds);

std::vector<std::int16_t> toPcm16(const std::vector<float>& buffer);

float midiToHz(float midi);

}  // namespace aldoria::synth
