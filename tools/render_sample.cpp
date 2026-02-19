// Headless renderer: produces a sample WAV file using PseudoHarmonicEngine
// Build: g++ -std=c++17 -O2 -I../Source render_sample.cpp ../Source/DSP/PseudoHarmonicEngine.cpp -o render_sample

#include "DSP/PseudoHarmonicEngine.h"
#include <fstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cmath>

struct WavHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t fileSize;
    char wave[4] = {'W','A','V','E'};
    char fmt[4] = {'f','m','t',' '};
    uint32_t fmtSize = 16;
    uint16_t format = 1; // PCM
    uint16_t channels = 2;
    uint32_t sampleRate = 44100;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample = 16;
    char data[4] = {'d','a','t','a'};
    uint32_t dataSize;
};

void writeWav(const char* filename, const std::vector<float>& left, const std::vector<float>& right, int sr) {
    WavHeader h;
    h.sampleRate = sr;
    h.byteRate = sr * 2 * 2;
    h.blockAlign = 4;
    h.dataSize = left.size() * 4;
    h.fileSize = 36 + h.dataSize;

    std::ofstream f(filename, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));

    for (size_t i = 0; i < left.size(); ++i) {
        auto clamp16 = [](float v) -> int16_t {
            v = std::max(-1.0f, std::min(1.0f, v));
            return static_cast<int16_t>(v * 32767.0f);
        };
        int16_t l = clamp16(left[i]);
        int16_t r = clamp16(right[i]);
        f.write(reinterpret_cast<const char*>(&l), 2);
        f.write(reinterpret_cast<const char*>(&r), 2);
    }
}

int main() {
    const int sr = 44100;
    const int blockSize = 512;
    const float duration = 12.0f; // seconds
    const int totalSamples = static_cast<int>(sr * duration);

    PseudoHarmonicEngine engine;
    engine.prepareToPlay(sr, blockSize);

    // Set up pseudoharmonic spectrum for 12-TET
    auto& p = engine.params();
    p.stretch2 = 2.0f;
    p.stretch3 = std::pow(2.0f, 1900.0f / 1200.0f); // ~2.9966 (12-TET optimized)
    p.stretch5 = std::pow(2.0f, 2800.0f / 1200.0f); // ~4.9246
    p.stretch7 = 7.0f; // leave just
    p.decay = 3.0f;
    p.release = 2.0f;
    p.strikePos = 0.5f;
    p.oddEven = 0.8f;
    p.volume = 0.015f;
    p.noiseMix = 0.0f;
    p.detune = 1.0f;
    p.relaxTime = 0.1f;
    engine.paramsChanged();

    std::vector<float> left(totalSamples), right(totalSamples);

    // Play a chord progression: C major -> F major -> G major -> C major
    struct NoteEvent { float time; int note; float vel; bool on; };
    std::vector<NoteEvent> events = {
        // C major chord (C4 E4 G4)
        {0.0f, 60, 0.7f, true}, {0.0f, 64, 0.6f, true}, {0.0f, 67, 0.6f, true},
        {2.5f, 60, 0, false}, {2.5f, 64, 0, false}, {2.5f, 67, 0, false},

        // F major (F4 A4 C5)
        {3.0f, 65, 0.7f, true}, {3.0f, 69, 0.6f, true}, {3.0f, 72, 0.6f, true},
        {5.5f, 65, 0, false}, {5.5f, 69, 0, false}, {5.5f, 72, 0, false},

        // G major (G4 B4 D5)
        {6.0f, 67, 0.7f, true}, {6.0f, 71, 0.6f, true}, {6.0f, 74, 0.6f, true},
        {8.5f, 67, 0, false}, {8.5f, 71, 0, false}, {8.5f, 74, 0, false},

        // C major again (C4 E4 G4 C5)
        {9.0f, 60, 0.7f, true}, {9.0f, 64, 0.6f, true}, {9.0f, 67, 0.6f, true}, {9.0f, 72, 0.5f, true},
        {11.5f, 60, 0, false}, {11.5f, 64, 0, false}, {11.5f, 67, 0, false}, {11.5f, 72, 0, false},
    };

    int eventIdx = 0;
    int samplePos = 0;

    while (samplePos < totalSamples) {
        int blockEnd = std::min(samplePos + blockSize, totalSamples);
        int n = blockEnd - samplePos;

        // Process note events for this block
        float blockStartTime = float(samplePos) / float(sr);
        float blockEndTime = float(blockEnd) / float(sr);
        while (eventIdx < (int)events.size() && events[eventIdx].time < blockEndTime) {
            auto& e = events[eventIdx];
            if (e.on)
                engine.noteOn(e.note, e.vel);
            else
                engine.noteOff(e.note);
            eventIdx++;
        }

        engine.processBlock(left.data() + samplePos, right.data() + samplePos, n);
        samplePos += n;
    }

    writeWav("pseudoharmonic_sample.wav", left, right, sr);
    printf("Written pseudoharmonic_sample.wav (%.1fs, %d Hz)\n", duration, sr);

    // Also render with exact harmonics for comparison
    p.stretch3 = 3.0f;
    p.stretch5 = 5.0f;
    engine.paramsChanged();

    std::fill(left.begin(), left.end(), 0.0f);
    std::fill(right.begin(), right.end(), 0.0f);
    eventIdx = 0;
    samplePos = 0;

    while (samplePos < totalSamples) {
        int blockEnd = std::min(samplePos + blockSize, totalSamples);
        int n = blockEnd - samplePos;
        float blockEndTime = float(blockEnd) / float(sr);
        while (eventIdx < (int)events.size() && events[eventIdx].time < blockEndTime) {
            auto& e = events[eventIdx];
            if (e.on) engine.noteOn(e.note, e.vel);
            else engine.noteOff(e.note);
            eventIdx++;
        }
        engine.processBlock(left.data() + samplePos, right.data() + samplePos, n);
        samplePos += n;
    }

    writeWav("harmonic_sample.wav", left, right, sr);
    printf("Written harmonic_sample.wav (%.1fs, %d Hz)\n", duration, sr);

    return 0;
}
