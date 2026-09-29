#include "audio/synth.h"

#include <algorithm>
#include <cmath>

namespace aldoria::synth {
namespace {

constexpr float kTwoPi = 6.28318530718f;

float shape(Wave w, float phase) {
    switch (w) {
        case Wave::Sine: return std::sin(phase);
        case Wave::Triangle: return 2.0f / 3.14159265f * std::asin(std::sin(phase));
        case Wave::Saw: {
            float x = phase / kTwoPi;
            return 2.0f * (x - std::floor(x + 0.5f));
        }
        case Wave::Square: return std::sin(phase) >= 0.0f ? 0.7f : -0.7f;
    }
    return 0.0f;
}

}  // namespace

float midiToHz(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

std::vector<float> tone(const Tone& t) {
    size_t n = (size_t)std::max(1.0f, t.duration * (float)kSampleRate);
    std::vector<float> out(n);
    float phase = 0.0f;
    float f0 = std::max(1.0f, t.freqStart), f1 = std::max(1.0f, t.freqEnd);
    for (size_t i = 0; i < n; i++) {
        float x = (float)i / (float)n;             // 0..1
        float time = (float)i / (float)kSampleRate;
        float freq = f0 * std::pow(f1 / f0, x);
        if (t.vibratoHz > 0.0f) freq *= 1.0f + t.vibratoDepth * std::sin(kTwoPi * t.vibratoHz * time);
        phase += kTwoPi * freq / (float)kSampleRate;
        float env = std::pow(std::max(0.0f, 1.0f - x), t.decayPower);
        if (t.attack > 0.0f) env *= std::min(1.0f, time / t.attack);
        out[i] = shape(t.wave, phase) * env * t.volume;
    }
    return out;
}

std::vector<float> noise(float duration, float volume, float smoothing, float decayPower, unsigned seed) {
    size_t n = (size_t)std::max(1.0f, duration * (float)kSampleRate);
    std::vector<float> out(n);
    unsigned state = seed * 2654435761u + 12345u;
    float y = 0.0f;
    smoothing = std::clamp(smoothing, 0.01f, 1.0f);
    for (size_t i = 0; i < n; i++) {
        // xorshift: deterministisch, damit Klänge bei jedem Start gleich sind
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        float white = (float)(state & 0xFFFF) / 32768.0f - 1.0f;
        y += smoothing * (white - y);
        float x = (float)i / (float)n;
        out[i] = y * std::pow(std::max(0.0f, 1.0f - x), decayPower) * volume;
    }
    return out;
}

void mixInto(std::vector<float>& dst, const std::vector<float>& src, float offsetSeconds, float gain, bool wrap) {
    if (dst.empty()) return;
    size_t start = (size_t)std::max(0.0f, offsetSeconds * (float)kSampleRate);
    for (size_t i = 0; i < src.size(); i++) {
        size_t idx = start + i;
        if (idx >= dst.size()) {
            if (!wrap) break;
            idx %= dst.size();
        }
        dst[idx] += src[i] * gain;
    }
}

void addEcho(std::vector<float>& buffer, float delaySeconds, float feedback, float mix, bool wrapAround) {
    size_t delay = (size_t)(delaySeconds * (float)kSampleRate);
    size_t n = buffer.size();
    if (delay == 0 || n <= delay) return;
    // Kammfilter mit Rückkopplung: y[i] = x[i] + feedback * y[i - delay]
    std::vector<float> y = buffer;
    for (int pass = 0; pass < (wrapAround ? 2 : 1); pass++) {
        for (size_t i = 0; i < n; i++) {
            size_t j;
            if (i >= delay) j = i - delay;
            else if (wrapAround) j = n + i - delay;  // Naht: Nachhall vom Ende der Schleife
            else continue;
            y[i] = buffer[i] + feedback * y[j];
        }
    }
    for (size_t i = 0; i < n; i++) buffer[i] += (y[i] - buffer[i]) * mix;
}

void normalize(std::vector<float>& buffer, float peak) {
    float m = 0.0f;
    for (float v : buffer) m = std::max(m, std::fabs(v));
    if (m < 1e-6f) return;
    float k = peak / m;
    for (float& v : buffer) v *= k;
}

void fadeEdges(std::vector<float>& buffer, float seconds) {
    size_t n = std::min((size_t)(seconds * (float)kSampleRate), buffer.size() / 2);
    for (size_t i = 0; i < n; i++) {
        float k = (float)i / (float)n;
        buffer[i] *= k;
        buffer[buffer.size() - 1 - i] *= k;
    }
}

std::vector<std::int16_t> toPcm16(const std::vector<float>& buffer) {
    std::vector<std::int16_t> out(buffer.size());
    for (size_t i = 0; i < buffer.size(); i++) {
        float v = std::clamp(buffer[i], -1.0f, 1.0f);
        out[i] = (std::int16_t)std::lround(v * 32767.0f);
    }
    return out;
}

}  // namespace aldoria::synth
