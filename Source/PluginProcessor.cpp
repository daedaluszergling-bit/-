#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace ana;

//==============================================================================
namespace
{
    inline float expMap (float t, float lo, float hi) noexcept { return lo * std::pow (hi / lo, juce::jlimit (0.0f, 1.0f, t)); }
    inline float lin    (float t, float lo, float hi) noexcept { return lo + (hi - lo) * juce::jlimit (0.0f, 1.0f, t); }
    inline float slew   (float cur, float tgt, float k = 0.3f) noexcept { return cur + k * (tgt - cur); }
}

//==============================================================================
AnAProcessor::AnAProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < kLines;  ++i) uiWarp[i].store (0.0f);
    for (int k = 0; k < kFieldK; ++k) uiFieldPhase[k].store (0.0f);
    for (int m = 0; m < kMeteors; ++m) { mActive[m].store (0); mX[m].store (0); mY[m].store (0); mAge[m].store (0); mEnergy[m].store (0); }
}

juce::AudioProcessorValueTreeState::ParameterLayout AnAProcessor::createLayout()
{
    using F = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    auto u = juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f);

    L.add (std::make_unique<F> (juce::ParameterID { "posx", 1 }, "Space",  u, 0.42f));
    L.add (std::make_unique<F> (juce::ParameterID { "posy", 1 }, "Ink",    u, 0.30f));
    L.add (std::make_unique<F> (juce::ParameterID { "mix",  1 }, "Mix",    u, 0.32f));
    L.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "world", 1 }, "World",
            juce::StringArray { "Zapis", "Zrenje" }, 0));
    L.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "freeze", 1 }, "Freeze", false));
    L.add (std::make_unique<juce::AudioParameterBool>  (juce::ParameterID { "drift",  1 }, "Drift",  false));

    L.add (std::make_unique<F> (juce::ParameterID { "damp",       1 }, "Damp",        u, 0.40f));
    L.add (std::make_unique<F> (juce::ParameterID { "lowcut",     1 }, "Low Cut",     u, 0.10f));
    L.add (std::make_unique<F> (juce::ParameterID { "width",      1 }, "Width",       u, 1.00f));
    L.add (std::make_unique<F> (juce::ParameterID { "predelay",   1 }, "Pre-Delay",
            juce::NormalisableRange<float> (0.0f, kMaxPredelayMs, 0.1f), 0.0f));
    L.add (std::make_unique<F> (juce::ParameterID { "diffuse",    1 }, "Diffuse",     u, 0.60f));
    L.add (std::make_unique<F> (juce::ParameterID { "warpdepth",  1 }, "Warp Depth",  u, 0.50f));
    L.add (std::make_unique<F> (juce::ParameterID { "warprate",   1 }, "Warp Rate",   u, 0.50f));
    L.add (std::make_unique<F> (juce::ParameterID { "crosscouple",1 }, "Cross-Couple",u, 0.45f));
    L.add (std::make_unique<F> (juce::ParameterID { "spin",       1 }, "Spin",        u, 0.40f));
    L.add (std::make_unique<F> (juce::ParameterID { "smear",      1 }, "Smear",       u, 0.45f));
    L.add (std::make_unique<F> (juce::ParameterID { "warmth",     1 }, "Warmth",      u, 0.30f));
    L.add (std::make_unique<F> (juce::ParameterID { "freezemorph",1 }, "Freeze Morph",u, 0.50f));
    L.add (std::make_unique<F> (juce::ParameterID { "life",       1 }, "Life",        u, 0.45f));
    L.add (std::make_unique<F> (juce::ParameterID { "duck",       1 }, "Duck",        u, 0.00f));
    L.add (std::make_unique<F> (juce::ParameterID { "outtrim",    1 }, "Output",      u, 0.50f));
    return L;
}

//==============================================================================
void AnAProcessor::prepareToPlay (double sampleRate, int spb)
{
    juce::ignoreUnused (spb);
    fs = sampleRate; const float f = (float) fs;

    const int maxLine = (int) std::ceil (kBaseMs[kLines - 1] * 0.001f * kMaxSizeScale * f * 1.5f) + 2048;
    for (int i = 0; i < kLines; ++i)
    {
        line[i].prepare (maxLine);
        baseDelaySamp[i] = kBaseMs[i] * 0.001f * f;
        dampLP[i].reset(); disp[i][0].reset(); disp[i][1].reset();
        lfoPhase[i] = (float) i / kLines * juce::MathConstants<float>::twoPi;
        lfoSlow[i]  = (float) i * 0.37f;
        lineEnergy[i] = 0.0f;
        const float ang = juce::MathConstants<float>::twoPi * (float) i / kLines;
        linePosX[i] = 0.5f + 0.34f * std::cos (ang);
        linePosY[i] = 0.5f + 0.34f * std::sin (ang * 1.3f);
    }
    for (int k = 0; k < 4; ++k) { diff[k].prepare ((int) std::ceil (kDiffMs[k] * 0.001f * kMaxSizeScale * f) + (int) std::ceil (0.006f * f) + 128); diffDelaySamp[k] = kDiffMs[k] * 0.001f * f; }
    predelay.prepare ((int) std::ceil (kMaxPredelayMs * 0.001f * f) + 64);
    lowCut.reset(); warmthLP.reset();

    for (int k = 0; k < 8; ++k) { gC[k] = 1.0f; gS[k] = 0.0f; gdC[k] = 1.0f; gdS[k] = 0.0f; }
    for (int k = 0; k < kFieldK; ++k) { fieldPhase[k] = (float) k * 1.1f; fieldDPhase[k] = juce::MathConstants<float>::twoPi * kFieldRate[k] / f; }

    for (int i = 0; i < ana::kLines; ++i) dlyZ[i] = baseDelaySamp[i];
    lvlZ = 1.0f;
    inEnv = 0.0f; envAtk = 1.0f - std::exp (-1.0f / (0.005f * f)); envRel = 1.0f - std::exp (-1.0f / (0.220f * f));
    nextSpawn = 1.0; for (auto& m : met) m = {};

    const double t = 0.02;
    smPosX.reset (fs, t); smPosY.reset (fs, t); smMix.reset (fs, t); smDry.reset (fs, t);
    smWidth.reset (fs, t); smDuck.reset (fs, t); smOut.reset (fs, t); smInject.reset (fs, 0.05);
    primed = false;
}

bool AnAProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& o = l.getMainOutputChannelSet();
    if (o != juce::AudioChannelSet::mono() && o != juce::AudioChannelSet::stereo()) return false;
    return l.getMainInputChannelSet() == o;
}

//==============================================================================
void AnAProcessor::mixMatrix (float* v, bool black) noexcept
{
    if (! black) { hadamard8 (v); return; }
    givens (v, 0, 1, gC[0], gS[0]); givens (v, 2, 3, gC[1], gS[1]); givens (v, 4, 5, gC[2], gS[2]); givens (v, 6, 7, gC[3], gS[3]);
    hadamard8 (v);
    givens (v, 1, 2, gC[4], gS[4]); givens (v, 3, 4, gC[5], gS[5]); givens (v, 5, 6, gC[6], gS[6]); givens (v, 7, 0, gC[7], gS[7]);
}

void AnAProcessor::advanceSpin (bool black) noexcept
{
    if (! black) return;
    for (int k = 0; k < 8; ++k) { const float c = gC[k] * gdC[k] - gS[k] * gdS[k]; const float s = gS[k] * gdC[k] + gC[k] * gdS[k]; gC[k] = c; gS[k] = s; }
}

float AnAProcessor::sampleField (float x, float y) const noexcept
{
    float v = 0.0f;
    for (int k = 0; k < kFieldK; ++k)
        v += kFieldAmp[k] * std::sin (juce::MathConstants<float>::twoPi * (kFieldFx[k] * x + kFieldFy[k] * y) + fieldPhase[k]);
    for (const auto& m : met)
        if (m.on)
        {
            const float dx = x - m.x, dy = y - m.y;
            const float env = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, m.age / m.life));
            v += m.energy * env * std::exp (-(dx * dx + dy * dy) * 16.0f);
        }
    return v;
}

void AnAProcessor::updateMeteors (float dt) noexcept
{
    const bool black = uiWorld.load() == 1;
    nextSpawn -= dt;
    if (nextSpawn <= 0.0)
        for (int m = 0; m < kMeteors; ++m)
            if (! met[m].on)
            {
                met[m].on = true; met[m].x = frand(); met[m].y = frand(); met[m].age = 0.0f;
                met[m].life = 0.6f + frand() * 0.9f;
                met[m].energy = (black ? 0.9f : 0.5f) * (0.5f + 0.6f * frand());
                const float base = black ? 3.0f : 7.5f;
                nextSpawn = (double) (base * (0.45f + frand()));
                break;
            }
    for (int m = 0; m < kMeteors; ++m)
        if (met[m].on)
        {
            met[m].age += dt;
            if (met[m].age >= met[m].life) met[m].on = false;
            mX[m].store (met[m].x); mY[m].store (met[m].y); mAge[m].store (met[m].age); mEnergy[m].store (met[m].energy); mActive[m].store (1);
        }
        else mActive[m].store (0);
}

//==============================================================================
void AnAProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = juce::jmin (buffer.getNumChannels(), maxChannels);
    const int N = buffer.getNumSamples();
    if (N <= 0 || numCh <= 0) return;
    const float f = (float) fs;

    auto P = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const int   world   = (int) P ("world");
    const bool  black   = world == 1;
    const bool  freeze  = P ("freeze") > 0.5f;
    const bool  drift   = P ("drift")  > 0.5f;
    float posx = P ("posx"), posy = P ("posy");
    const float mix = P ("mix"), damp = P ("damp"), lowcut = P ("lowcut"), width = P ("width");
    const float predelayMs = P ("predelay"), diffuse = P ("diffuse");
    const float warpDepthT = P ("warpdepth"), warpRateT = P ("warprate"), crossT = P ("crosscouple");
    const float spinT = P ("spin"), smearT = P ("smear"), warmthT = P ("warmth"), fmorph = P ("freezemorph");
    const float lifeT = P ("life"), duckT = P ("duck"), outT = P ("outtrim");
    uiWorld.store (world);

    if (drift)
    {
        driftPhX += juce::MathConstants<float>::twoPi * 0.013f * (float) N / f;
        driftPhY += juce::MathConstants<float>::twoPi * 0.019f * (float) N / f;
        posx = juce::jlimit (0.0f, 1.0f, posx + 0.18f * std::sin (driftPhX));
        posy = juce::jlimit (0.0f, 1.0f, posy + 0.18f * std::sin (driftPhY * 1.37f));
    }

    const float inkY = posy * 0.778f;
    float sizeScale, rt60, predMacro, warpDepth, warpRateHz, blackBend, dispA, warmthAmt, spinRate, breathBase, inExcite;
    if (! black)
    {
        sizeScale  = expMap (posx, 0.5f, 2.2f);
        rt60       = expMap (posx, 0.3f, 6.0f);
        predMacro  = lin    (posx, 0.0f, 60.0f);
        warpDepth  = inkY * 1.25f;
        warpRateHz = expMap (inkY, 0.10f, 3.0f);
        blackBend  = 0.0f;
        dispA      = 0.0f;
        warmthAmt  = warmthT;
        spinRate   = 0.0f;
        breathBase = 0.90f;
        inExcite   = 0.35f;
    }
    else
    {
        sizeScale  = expMap (posx, 0.6f, kMaxSizeScale);
        rt60       = freeze ? 1.0e9f : expMap (posx, 0.6f, 30.0f);
        predMacro  = lin    (posx, 0.0f, 120.0f);
        warpDepth  = inkY * 2.60f;
        warpRateHz = expMap (inkY, 0.05f, 6.5f);
        blackBend  = inkY * 7.5f;
        dispA      = smearT * 0.7f;
        warmthAmt  = 0.0f;
        spinRate   = (0.3f + spinT) * 0.000025f * (freeze ? (1.0f + 2.0f * fmorph) : 1.0f);
        breathBase = 1.15f;
        inExcite   = 0.45f;
    }
    const float diffAmt = juce::jlimit (0.0f, 1.0f, 0.30f * posx + 0.70f * diffuse + (black ? 0.08f : 0.0f));
    if (freeze && ! black) rt60 = 1.0e9f;
    warpDepth *= (0.4f + warpDepthT * 1.2f);
    warpRateHz *= (0.4f + warpRateT * 1.2f);
    const float crossAmt = black ? crossT * 0.6f : 0.0f;
    const float dampHzEff = (freeze && ! black) ? 19000.0f : 18000.0f * std::pow (1500.0f / 18000.0f, damp);
    const float lowHz     = 20.0f * std::pow (500.0f / 20.0f, lowcut);
    const float modSampMax = warpDepth * 0.0032f * f * (black ? 1.8f : 1.0f);
    const float predSamp  = juce::jlimit (0.0f, (float) predelay.size() - 8.0f, (predMacro + predelayMs) * 0.001f * f);

    const float mixGain = std::sin (mix * juce::MathConstants<float>::halfPi);
    const float dryGain = std::cos (mix * juce::MathConstants<float>::halfPi);

    if (! primed)
    {
        smPosX.setCurrentAndTargetValue (posx); smPosY.setCurrentAndTargetValue (posy);
        smMix.setCurrentAndTargetValue (mixGain); smDry.setCurrentAndTargetValue (dryGain);
        smWidth.setCurrentAndTargetValue (width); smDuck.setCurrentAndTargetValue (duckT);
        smInject.setCurrentAndTargetValue (freeze ? 0.0f : 1.0f); smOut.setCurrentAndTargetValue (lin (outT, 0.5f, 1.6f));
        gSize = sizeScale; gDampHz = dampHzEff; gLowHz = lowHz; gDiffG = 0.45f + diffAmt * 0.32f;
        gModSamp = modSampMax; gBlackBend = blackBend; gCross = crossAmt;
        gDispA = dispA; gWarmth = warmthAmt; gPredelay = predSamp; gLife = lifeT * 2.0f; gBreathBase = breathBase; gInExcite = inExcite;
        for (int i = 0; i < kLines; ++i) { const float ds = baseDelaySamp[i] * sizeScale / f; gRtGain[i] = freeze ? 1.0f : std::pow (10.0f, -3.0f * ds / rt60); }
        primed = true;
    }
    smPosX.setTargetValue (posx); smPosY.setTargetValue (posy);
    smMix.setTargetValue (mixGain); smDry.setTargetValue (dryGain); smWidth.setTargetValue (width);
    smDuck.setTargetValue (duckT); smInject.setTargetValue (freeze ? 0.0f : 1.0f); smOut.setTargetValue (lin (outT, 0.5f, 1.6f));
    gDampHz = slew (gDampHz, dampHzEff);          gLowHz = slew (gLowHz, lowHz);
    gCross = slew (gCross, crossAmt);             gDispA = slew (gDispA, dispA);
    gWarmth = slew (gWarmth, warmthAmt);          gLife = slew (gLife, lifeT * 2.0f);
    gBreathBase = slew (gBreathBase, breathBase); gInExcite = slew (gInExcite, inExcite);

    const float diffGTarget = 0.45f + diffAmt * 0.32f;
    const float smpCoef = 1.0f - std::exp (-1.0f / (0.025f * f));
    const float czDly   = 1.0f - std::exp (-1.0f / (0.004f * f));
    const float czVol   = 1.0f - std::exp (-1.0f / (0.006f * f));
    float rtTarget[kLines];
    for (int i = 0; i < kLines; ++i)
        rtTarget[i] = freeze ? 1.0f : std::pow (10.0f, -3.0f * (baseDelaySamp[i] * sizeScale / f) / rt60);

    for (int i = 0; i < kLines; ++i)
    {
        dampLP[i].setCutoff (gDampHz, f);
        disp[i][0].a = disp[i][1].a = gDispA;
    }
    lowCut.setCutoff (gLowHz, f);
    warmthLP.setCutoff (16000.0f - gWarmth * 8000.0f, f);
    tailSeconds.store (freeze ? 30.0f : juce::jlimit (0.5f, 30.0f, rt60 * 1.2f));

    for (int k = 0; k < 8; ++k)
    {
        const float w = spinRate * (0.6f + 0.13f * k);
        gdC[k] = std::cos (w); gdS[k] = std::sin (w);
        const float inv = 1.0f / std::sqrt (gC[k] * gC[k] + gS[k] * gS[k] + 1e-12f);
        gC[k] *= inv; gS[k] *= inv;
    }

    const float dLfo     = juce::MathConstants<float>::twoPi * warpRateHz / f;
    const float dLfoSlow = juce::MathConstants<float>::twoPi * warpRateHz * 0.13f / f;
    const float wetTrim  = 0.6f * 0.35355339f;

    updateMeteors ((float) N / f);

    float* Lc = buffer.getWritePointer (0);
    float* Rc = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float inPk = 0, outPk = 0, tailPk = 0, lastWarp[kLines] = {};

    for (int n = 0; n < N; ++n)
    {
        const float dryL = Lc[n], dryR = Rc ? Rc[n] : dryL;
        const float mono = 0.5f * (dryL + dryR);
        const float ax = std::abs (mono); inPk = juce::jmax (inPk, ax);
        inEnv += (ax > inEnv ? envAtk : envRel) * (ax - inEnv);
        const float breath = gBreathBase + inEnv * gInExcite;

        for (int k = 0; k < kFieldK; ++k) { fieldPhase[k] += fieldDPhase[k]; if (fieldPhase[k] > juce::MathConstants<float>::twoPi) fieldPhase[k] -= juce::MathConstants<float>::twoPi; }
        const float gBreathVal = sampleField (0.5f, 0.5f);

        gSize      += smpCoef * (sizeScale   - gSize);
        gPredelay  += smpCoef * (predSamp    - gPredelay);
        gModSamp   += smpCoef * (modSampMax  - gModSamp);
        gBlackBend += smpCoef * (blackBend   - gBlackBend);
        gDiffG     += smpCoef * (diffGTarget - gDiffG);
        for (int i = 0; i < kLines; ++i) gRtGain[i] += smpCoef * (rtTarget[i] - gRtGain[i]);

        predelay.write (mono);
        float x = lowCut.process (predelay.readH (gPredelay));
        if (! black && gWarmth > 0.0001f) { const float d = 1.0f + gWarmth * 0.6f; x = std::tanh (x * d) / d; x = warmthLP.process (x); }
        for (int k = 0; k < 4; ++k)
        {
            float md = diffDelaySamp[k] * gSize;
            if (k >= 2) md += 0.4f * gModSamp * std::sin (lfoPhase[k]);
            x = diff[k].process (x, juce::jlimit (1.0f, (float) diff[k].d.size() - 4.0f, md), gDiffG);
        }
        const float inj = x * smInject.getNextValue();

        float s[kLines], wetL = 0, wetR = 0;
        for (int i = 0; i < kLines; ++i)
        {
            const float fld = sampleField (linePosX[i], linePosY[i]);
            const float lifeMod = fld * breath * gLife;
            float modd = gModSamp * (1.0f + 0.4f * lifeMod);
            if (black) modd += crossAmt * lineEnergy[(i + 1) & 7] * gModSamp;
            const float lfo = std::sin (lfoPhase[i] + gBlackBend * std::sin (lfoSlow[i]));
            const float lifeSway = lifeMod * baseDelaySamp[i] * 0.07f;
            lastWarp[i] = juce::jlimit (-1.0f, 1.0f, 0.5f * lfo + 0.8f * lifeMod);
            float dlyTgt = baseDelaySamp[i] * gSize + lfo * modd + lifeSway;
            dlyTgt = juce::jlimit (4.0f, (float) line[i].size() - 4.0f, dlyTgt);
            dlyZ[i] += czDly * (dlyTgt - dlyZ[i]);
            float v = line[i].readH (dlyZ[i]);
            v = dampLP[i].process (v);
            if (black) v = disp[i][1].process (disp[i][0].process (v));
            lineEnergy[i] += 0.0015f * (std::abs (v) - lineEnergy[i]);
            s[i] = v; wetL += kTapL[i] * v; wetR += kTapR[i] * v;

            lfoPhase[i] += dLfo * (1.0f + 0.03f * i);     if (lfoPhase[i] > juce::MathConstants<float>::twoPi) lfoPhase[i] -= juce::MathConstants<float>::twoPi;
            lfoSlow[i]  += dLfoSlow * (1.0f + 0.021f * i); if (lfoSlow[i]  > juce::MathConstants<float>::twoPi) lfoSlow[i]  -= juce::MathConstants<float>::twoPi;
        }

        float fb[kLines];
        for (int i = 0; i < kLines; ++i) fb[i] = gRtGain[i] * s[i];
        mixMatrix (fb, black);
        for (int i = 0; i < kLines; ++i) line[i].write (inj + fb[i]);
        advanceSpin (black);

        wetL *= wetTrim; wetR *= wetTrim;
        const float mid = 0.5f * (wetL + wetR), side = 0.5f * (wetL - wetR) * smWidth.getNextValue();
        wetL = mid + side; wetR = mid - side;
        const float lvl = 1.0f + 0.30f * juce::jlimit (-1.0f, 1.0f, gBreathVal) * gLife;
        lvlZ += czVol * (lvl - lvlZ);
        wetL *= lvlZ; wetR *= lvlZ;
        const float duckg = 1.0f - smDuck.getNextValue() * juce::jlimit (0.0f, 1.0f, inEnv * 1.5f);
        wetL *= duckg; wetR *= duckg;
        tailPk = juce::jmax (tailPk, std::abs (wetL), std::abs (wetR));

        const float mg = smMix.getNextValue(), dg = smDry.getNextValue(), og = smOut.getNextValue();
        const float oL = (dryL * dg + wetL * mg) * og, oR = (dryR * dg + wetR * mg) * og;
        Lc[n] = oL; if (Rc) Rc[n] = oR; outPk = juce::jmax (outPk, std::abs (oL), std::abs (oR));
        smPosX.getNextValue(); smPosY.getNextValue();
    }

    for (int c = numCh; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, N);

    for (int i = 0; i < kLines; ++i) uiWarp[i].store (lastWarp[i]);
    for (int k = 0; k < kFieldK; ++k) uiFieldPhase[k].store (fieldPhase[k]);
    uiPosX.store (smPosX.getCurrentValue()); uiPosY.store (smPosY.getCurrentValue());
    uiBreath.store ((gBreathBase + inEnv * gInExcite) * gLife);
    inLevel.store (inPk); outLevel.store (outPk); tailLevel.store (tailPk);
}

//==============================================================================
void AnAProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto st = apvts.copyState(); st.isValid())
        if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void AnAProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AnAProcessor::createEditor()
{
    return new AnAEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AnAProcessor(); }
