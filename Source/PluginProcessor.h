#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

//==============================================================================
// Z Lab AnA - modulation reverb with two audio/visual worlds.
// ZAPIS is the clear, pencil-like side. ZRENJE is the dark, unstable side.
//==============================================================================

namespace ana
{
    constexpr int   kLines   = 8;
    constexpr int   kFieldK  = 3;
    constexpr int   kMeteors = 6;

    constexpr float kBaseMs[kLines] = { 29.3f, 34.1f, 40.7f, 44.3f, 49.7f, 54.3f, 60.1f, 63.7f };
    constexpr float kDiffMs[4]      = { 13.1f, 9.7f, 21.3f, 17.9f };
    constexpr float kMaxSizeScale   = 3.0f;
    constexpr float kMaxPredelayMs  = 250.0f;

    constexpr float kFieldFx[kFieldK]   = { 1.7f,  2.9f,  4.3f };
    constexpr float kFieldFy[kFieldK]   = { 1.3f,  2.3f,  3.7f };
    constexpr float kFieldRate[kFieldK] = { 0.10f, 0.17f, 0.27f };
    constexpr float kFieldAmp[kFieldK]  = { 0.55f, 0.30f, 0.18f };

    constexpr float kTapL[kLines] = { +1, -1, +1, -1, +1, -1, +1, -1 };
    constexpr float kTapR[kLines] = { +1, +1, -1, -1, +1, +1, -1, -1 };

    struct OnePoleLP
    {
        float a = 0.0f, z = 0.0f;
        void  setCutoff (float fc, float fs) noexcept { a = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmax (1.0f, fc) / fs); }
        void  reset() noexcept { z = 0.0f; }
        inline float process (float x) noexcept { z += a * (x - z); return z; }
    };

    struct OnePoleHP
    {
        OnePoleLP lp;
        void  setCutoff (float fc, float fs) noexcept { lp.setCutoff (fc, fs); }
        void  reset() noexcept { lp.reset(); }
        inline float process (float x) noexcept { return x - lp.process (x); }
    };

    struct FracDelay
    {
        std::vector<float> buf; int mask = 0, w = 0;
        void prepare (int maxLen) { int n = 1; while (n < maxLen + 4) n <<= 1; buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
        void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
        int  size() const noexcept { return (int) buf.size(); }
        inline void  write (float x) noexcept { buf[(size_t) w] = x; w = (w + 1) & mask; }
        inline float readH (float d) const noexcept
        {
            const float rp = (float) w - 1.0f - d + (float) buf.size();
            const int   i1 = (int) std::floor (rp); const float fr = rp - (float) i1;
            const int   im1 = (i1 - 1) & mask, i0 = i1 & mask, ip1 = (i1 + 1) & mask, ip2 = (i1 + 2) & mask;
            const float y0 = buf[(size_t) im1], y1 = buf[(size_t) i0], y2 = buf[(size_t) ip1], y3 = buf[(size_t) ip2];
            const float c0 = y1, c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * fr + c2) * fr + c1) * fr + c0;
        }
    };

    struct Allpass
    {
        FracDelay d;
        void prepare (int maxLen) { d.prepare (maxLen); }
        void reset() noexcept { d.reset(); }
        inline float process (float x, float delay, float g) noexcept
        {
            const float r = d.readH (delay); const float y = -g * x + r; d.write (x + g * y); return y;
        }
    };

    struct DispAP
    {
        float a = 0.0f, x1 = 0.0f, y1 = 0.0f;
        void  reset() noexcept { x1 = y1 = 0.0f; }
        inline float process (float x) noexcept { const float y = a * x + x1 - a * y1; x1 = x; y1 = y; return y; }
    };

    inline void givens (float* v, int i, int j, float c, float s) noexcept
    {
        const float a = v[i], b = v[j]; v[i] = c * a - s * b; v[j] = s * a + c * b;
    }

    inline void hadamard8 (float* v) noexcept
    {
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += (len << 1))
                for (int k = i; k < i + len; ++k) { const float a = v[k], b = v[k + len]; v[k] = a + b; v[k + len] = a - b; }
        constexpr float sc = 0.35355339f;
        for (int i = 0; i < 8; ++i) v[i] *= sc;
    }
}

//==============================================================================
class AnAProcessor : public juce::AudioProcessor
{
public:
    AnAProcessor();
    ~AnAProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                              { return true; }
    const juce::String getName() const override                  { return "Z Lab AnA"; }
    bool acceptsMidi() const override                            { return false; }
    bool producesMidi() const override                           { return false; }
    double getTailLengthSeconds() const override                 { return tailSeconds.load(); }
    int  getNumPrograms() override                               { return 1; }
    int  getCurrentProgram() override                            { return 0; }
    void setCurrentProgram (int) override                        {}
    const juce::String getProgramName (int) override             { return {}; }
    void changeProgramName (int, const juce::String&) override   {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float> uiPosX { 0.5f }, uiPosY { 0.5f };
    std::atomic<float> uiFieldPhase[ana::kFieldK];
    std::atomic<float> uiBreath  { 0.0f };
    std::atomic<float> uiWarp[ana::kLines];
    std::atomic<int>   uiWorld   { 0 };
    std::atomic<float> inLevel { 0.0f }, outLevel { 0.0f }, tailLevel { 0.0f };
    std::atomic<float> mX[ana::kMeteors], mY[ana::kMeteors], mAge[ana::kMeteors], mEnergy[ana::kMeteors];
    std::atomic<int>   mActive[ana::kMeteors];

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static constexpr int maxChannels = 2;
    double fs = 44100.0;
    std::atomic<float> tailSeconds { 4.0f };

    ana::FracDelay line[ana::kLines];
    ana::OnePoleLP dampLP[ana::kLines];
    ana::DispAP    disp[ana::kLines][2];
    float          baseDelaySamp[ana::kLines] = {};
    float          lfoPhase[ana::kLines] = {}, lfoSlow[ana::kLines] = {};
    float          lineEnergy[ana::kLines] = {};
    float          linePosX[ana::kLines] = {}, linePosY[ana::kLines] = {};
    ana::Allpass   diff[4];
    float          diffDelaySamp[4] = {};
    ana::OnePoleHP lowCut;
    ana::OnePoleLP warmthLP;
    ana::FracDelay predelay;

    float gC[8] = {}, gS[8] = {};
    float gdC[8] = {}, gdS[8] = {};
    void  mixMatrix (float* v, bool black) noexcept;
    void  advanceSpin (bool black) noexcept;

    float fieldPhase[ana::kFieldK] = {};
    float fieldDPhase[ana::kFieldK] = {};
    inline float sampleField (float x, float y) const noexcept;

    struct Meteor { bool on = false; float x = 0, y = 0, age = 0, life = 1, energy = 0; };
    Meteor met[ana::kMeteors];
    double nextSpawn = 0.0;
    juce::uint32 rng = 0x9e3779b9u;
    inline float frand() noexcept { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return (float) (rng & 0xFFFFFFu) / 16777216.0f; }
    void  updateMeteors (float dt) noexcept;

    juce::SmoothedValue<float> smPosX, smPosY, smMix, smDry, smWidth, smDuck, smInject, smOut;
    float gSize = 1, gRtGain[ana::kLines] = {}, gDampHz = 8000, gLowHz = 40;
    float gDiffG = 0.6f, gModSamp = 0, gBlackBend = 0, gCross = 0;
    float gDispA = 0, gWarmth = 0, gPredelay = 0, gLife = 0, gBreathBase = 0.15f, gInExcite = 0;
    float dlyZ[ana::kLines] = {};
    float lvlZ = 1.0f;
    bool  primed = false;

    float inEnv = 0.0f, envAtk = 0, envRel = 0;
    float driftPhX = 0.0f, driftPhY = 1.7f;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AnAProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnAProcessor)
};
