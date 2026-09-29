#include <cmath>

#include "audio/sound_bank.h"
#include "audio/synth.h"
#include "doctest.h"

using namespace aldoria;
using namespace aldoria::synth;

namespace {

bool allFinite(const std::vector<float>& v) {
    for (float x : v) {
        if (!std::isfinite(x)) return false;
    }
    return true;
}

float peak(const std::vector<float>& v) {
    float m = 0;
    for (float x : v) m = std::max(m, std::fabs(x));
    return m;
}

}  // namespace

TEST_CASE("Ein Ton hat die gewünschte Länge und bleibt im Wertebereich") {
    Tone t;
    t.duration = 0.25f;
    t.volume = 0.6f;
    auto s = tone(t);
    CHECK(s.size() == (size_t)(0.25f * kSampleRate));
    CHECK(allFinite(s));
    CHECK(peak(s) <= 0.6f + 1e-4f);
    CHECK(peak(s) > 0.3f);
}

TEST_CASE("Ein Ton mit Frequenzverlauf ändert seine Schwingungsrate") {
    Tone t;
    t.freqStart = 200;
    t.freqEnd = 800;
    t.duration = 0.5f;
    t.decayPower = 0.0f;
    t.attack = 0.0f;
    auto s = tone(t);
    auto zeroCrossings = [&](size_t from, size_t to) {
        int c = 0;
        for (size_t i = from + 1; i < to; i++) c += (s[i - 1] < 0) != (s[i] < 0);
        return c;
    };
    size_t q = s.size() / 4;
    CHECK(zeroCrossings(s.size() - q, s.size()) > zeroCrossings(0, q) * 2);
}

TEST_CASE("Rauschen ist deterministisch und klingt aus") {
    auto a = noise(0.2f, 0.5f, 0.4f, 1.5f, 7);
    auto b = noise(0.2f, 0.5f, 0.4f, 1.5f, 7);
    auto c = noise(0.2f, 0.5f, 0.4f, 1.5f, 8);
    CHECK(a == b);
    CHECK(a != c);
    float head = 0, tail = 0;
    for (size_t i = 0; i < 500; i++) head = std::max(head, std::fabs(a[i]));
    for (size_t i = a.size() - 500; i < a.size(); i++) tail = std::max(tail, std::fabs(a[i]));
    CHECK(tail < head * 0.2f);
}

TEST_CASE("Mischen mit Überlauf: ohne wrap wird abgeschnitten, mit wrap läuft es vorn weiter") {
    std::vector<float> dst(1000, 0.0f), src(400, 1.0f);
    mixInto(dst, src, 800.0f / kSampleRate, 1.0f, false);
    CHECK(dst[799] == 0.0f);
    CHECK(dst[800] == 1.0f);
    CHECK(dst[0] == 0.0f);

    std::vector<float> wrapped(1000, 0.0f);
    mixInto(wrapped, src, 800.0f / kSampleRate, 1.0f, true);
    CHECK(wrapped[100] == 1.0f);   // Überhang landet am Anfang
    CHECK(wrapped[250] == 0.0f);
}

TEST_CASE("Normalisieren und PCM-Umwandlung") {
    std::vector<float> v = {0.0f, 0.25f, -0.5f, 0.1f};
    normalize(v, 1.0f);
    CHECK(peak(v) == doctest::Approx(1.0f));
    auto pcm = toPcm16({-2.0f, -1.0f, 0.0f, 0.5f, 2.0f});
    CHECK(pcm[0] == -32767);
    CHECK(pcm[2] == 0);
    CHECK(pcm[4] == 32767);
}

TEST_CASE("Echo verändert das Signal, ohne es zu sprengen") {
    std::vector<float> v(20000, 0.0f);
    v[0] = 1.0f;
    addEcho(v, 0.1f, 0.5f, 0.5f, false);
    size_t d = (size_t)(0.1f * kSampleRate);
    CHECK(v[d] > 0.1f);
    CHECK(allFinite(v));
    CHECK(peak(v) <= 1.5f);
}

TEST_CASE("Jeder Effekt ist nicht leer, endlich und nicht übersteuert") {
    for (size_t i = 0; i < (size_t)Sfx::Count; i++) {
        INFO("Effekt Nummer " << i);
        auto s = renderSfx((Sfx)i);
        REQUIRE(!s.empty());
        CHECK(allFinite(s));
        CHECK(peak(s) > 0.02f);
        CHECK(peak(s) <= 1.5f);
        CHECK(s.size() < (size_t)(2.0f * kSampleRate));
        CHECK(sfxBaseVolume((Sfx)i) > 0.0f);
        // Anfang und Ende leise, damit nichts knackt
        CHECK(std::fabs(s.front()) < 0.05f);
        CHECK(std::fabs(s.back()) < 0.05f);
    }
}

TEST_CASE("Musikstücke haben die erwartete Länge, sind endlich und laufen nahtlos") {
    struct Expected {
        MusicKind kind;
        float seconds;
    };
    for (Expected e : {Expected{MusicKind::Explore, 32.0f * 60.0f / 76.0f}, Expected{MusicKind::Combat, 32.0f * 60.0f / 132.0f},
                       Expected{MusicKind::Boss, 32.0f * 60.0f / 148.0f}}) {
        auto m = renderMusic(e.kind);
        REQUIRE(!m.empty());
        CHECK(allFinite(m));
        CHECK(m.size() == (size_t)(e.seconds * kSampleRate));
        CHECK(peak(m) == doctest::Approx(0.6f).epsilon(0.01));
        // Nahtstelle: der Sprung vom letzten zum ersten Wert ist klein
        CHECK(std::fabs(m.back() - m.front()) < 0.25f);
    }
}
