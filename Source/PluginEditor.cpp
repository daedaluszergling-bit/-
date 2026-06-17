#include "PluginEditor.h"
#include "BinaryData.h"
#include <vector>

using juce::Colour; using juce::Point; using juce::Rectangle;

namespace
{
    constexpr float TB = 30.0f, FRX = 20.0f, FRY = 50.0f, FRW = 960.0f, FRH = 490.0f, GA = 9.0f;
    using PST = juce::PathStrokeType;

    void chevron (juce::Graphics& g, Rectangle<float> b, bool next, Colour c)
    {
        auto a = b.reduced (8.0f, 5.0f);
        const float x0 = next ? a.getX() : a.getRight();
        const float x1 = next ? a.getRight() : a.getX();
        juce::Path p; p.startNewSubPath (x0, a.getY()); p.lineTo (x1, a.getCentreY()); p.lineTo (x0, a.getBottom());
        g.setColour (c.withAlpha (0.12f)); g.strokePath (p, PST (4.2f, PST::curved, PST::rounded));
        g.setColour (c.withAlpha (0.86f)); g.strokePath (p, PST (2.1f, PST::curved, PST::rounded));
    }

    void histIcon (juce::Graphics& g, Rectangle<float> b, bool redo, Colour c, float dim)
    {
        auto area = b.reduced (4.5f, 3.2f);
        const float cx = area.getCentreX(), cy = area.getCentreY();
        const float ang = -juce::MathConstants<float>::pi / 12.0f, cs = std::cos (ang), sn = std::sin (ang);
        auto mp = [&] (float nx, float ny)
        {
            const float x = area.getX() + nx * area.getWidth(), y = area.getY() + ny * area.getHeight();
            const float dx = x - cx, dy = y - cy;
            float rx = cx + dx * cs - dy * sn; const float ry = cy + dx * sn + dy * cs;
            if (redo) rx = cx + (cx - rx);
            return Point<float> (rx, ry);
        };
        auto p0 = mp (0.86f, 0.82f), p1 = mp (0.86f, 0.10f), p2 = mp (0.25f, 0.10f), p3 = mp (0.19f, 0.58f);
        auto h0 = mp (0.19f, 0.16f), h1 = mp (0.19f, 0.58f), h2 = mp (0.55f, 0.58f);
        juce::Path curve; curve.startNewSubPath (p0.x, p0.y); curve.cubicTo (p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
        juce::Path head;  head.startNewSubPath (h0.x, h0.y); head.lineTo (h1.x, h1.y); head.lineTo (h2.x, h2.y);
        g.setColour (c.withAlpha (0.13f * dim)); g.strokePath (curve, PST (4.4f, PST::curved, PST::rounded)); g.strokePath (head, PST (4.4f, PST::curved, PST::rounded));
        g.setColour (c.withAlpha (0.84f * dim)); g.strokePath (curve, PST (2.4f, PST::curved, PST::rounded)); g.strokePath (head, PST (2.4f, PST::curved, PST::rounded));
    }
}

//==============================================================================
AnAEditor::AnAEditor (AnAProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    fredFace   = juce::Typeface::createSystemTypefaceFor (BinaryData::FrederickatheGreatRegular_ttf, (size_t) BinaryData::FrederickatheGreatRegular_ttfSize);
    barlowFace = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedBold_ttf, (size_t) BinaryData::BarlowCondensedBold_ttfSize);

    auto raw = juce::ImageFileFormat::loadFrom (BinaryData::zlab_logo_png, (size_t) BinaryData::zlab_logo_pngSize);
    if (raw.isValid())
    {
        auto tint = [&] (Colour c)
        {
            juce::Image o (juce::Image::ARGB, raw.getWidth(), raw.getHeight(), true);
            for (int y = 0; y < raw.getHeight(); ++y)
                for (int x = 0; x < raw.getWidth(); ++x)
                {
                    auto pp = raw.getPixelAt (x, y);
                    o.setPixelAt (x, y, c.withAlpha (pp.getAlpha() / 255.0f));
                }
            return o;
        };
        zlabLight = tint (Colour (0xffcbcbc4));
        zlabDark  = tint (Colour (0xff66666c));
    }

    for (int k = 0; k < kVF; ++k) locPhase[k] = (float) k * 0.7f;
    setSize (1000, 560);
    openGL.attachTo (*this);
    startTimerHz (60);
}

AnAEditor::~AnAEditor() { stopTimer(); openGL.detach(); }

void AnAEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = lastMs > 0.0 ? (float) ((now - lastMs) * 0.001) : 0.0f;
    lastMs = now;

    static const float vRate[kVF] = { 0.10f, 0.17f, 0.27f, 0.13f, 0.20f };
    const float tp = juce::MathConstants<float>::twoPi;
    for (int k = 0; k < kVF; ++k)
    {
        locPhase[k] += tp * vRate[k] * dt;
        if (k < ana::kFieldK)
        {
            const float published = proc.uiFieldPhase[k].load();
            const bool hasAudioClock = published > 0.0001f || proc.outLevel.load() > 0.0001f || proc.inLevel.load() > 0.0001f;
            if (hasAudioClock)
            {
                float d = published - locPhase[k];
                while (d >  juce::MathConstants<float>::pi) d -= tp;
                while (d < -juce::MathConstants<float>::pi) d += tp;
                locPhase[k] += d * 0.08f;
            }
        }
        while (locPhase[k] > tp) locPhase[k] -= tp;
        while (locPhase[k] < 0.0f) locPhase[k] += tp;
    }
    repaint();
}

AnAEditor::Skin AnAEditor::skin() const
{
    if (getP ("world") > 0.5f)
        return { Colour (0xff0e0e10), Colour (0xfff2f0e9), Colour (0xff6e6e72), Colour (0xffd5d4cc), Colour (0xfff2f0e9),
                 Colour (0xff242729), Colour (0xff101112), Colour (0xff16181a), Colour (0xffd6d4cd), true };
    return     { Colour (0xfff3f2ed), Colour (0xff2c2c30), Colour (0xff66666c), Colour (0xff55555a), Colour (0xff2c2c30),
                 Colour (0xffeeece5), Colour (0xffe2e0d8), Colour (0xfff6f4ee), Colour (0xff46464c), false };
}

Rectangle<float> AnAEditor::fieldRect() const { return { FRX, FRY, FRW, FRH }; }

void AnAEditor::layout()
{
    rFreeze = { 338, 506, 72, 18 }; rDrift = { 418, 506, 64, 18 }; rWorld = { 490, 506, 78, 18 }; rMirage = { 648, 506, 80, 18 };
    rMix = { 38, 491, 44, 44 };
    rPrev = { 16, 5, 28, 20 }; rNext = { 48, 5, 28, 20 }; rMenu = { 86, 5, 188, 20 }; rUndo = { 284, 5, 28, 20 }; rRedo = { 316, 5, 28, 20 };
}

float AnAEditor::field (float nx, float ny) const
{
    static const float fx[kVF] = { 0.8f, 1.4f, 2.2f, 3.1f, 4.3f };
    static const float fy[kVF] = { 0.6f, 1.2f, 1.9f, 2.7f, 3.8f };
    static const float am[kVF] = { 0.5f, 0.32f, 0.2f, 0.13f, 0.09f };
    float v = 0.0f;
    for (int k = 0; k < kVF; ++k)
        v += am[k] * std::sin (juce::MathConstants<float>::twoPi * (fx[k] * nx + fy[k] * ny) + locPhase[k]);
    return v * vBreath;
}

float AnAEditor::edgeFade (Point<float> p, Rectangle<float> fr) const
{
    const float d = juce::jmin (juce::jmin (p.x - fr.getX(), fr.getRight() - p.x),
                                juce::jmin (p.y - fr.getY(), fr.getBottom() - p.y));
    const float t = juce::jlimit (0.0f, 1.0f, d / 66.0f);
    return t * t * (3.0f - 2.0f * t);
}

Point<float> AnAEditor::warp (float nx, float ny, Point<float> puck, float pull) const
{
    const float bx = FRX + nx * FRW, by = FRY + ny * FRH;
    const float dx = puck.x - bx, dy = puck.y - by;
    const float dist = std::sqrt (dx * dx + dy * dy) + 1.0e-3f;
    const float ux = dx / dist, uy = dy / dist;
    const float px = -uy, py = ux;

    const float objectBreath = (0.42f + 0.18f * juce::jlimit (0.0f, 1.8f, proc.uiBreath.load()))
                             * std::sin (locPhase[0] * 0.61f + locPhase[2] * 0.23f);
    const float soft = 23.0f;
    const float radius = 68.0f * (1.0f + 0.045f * objectBreath);
    const float q = dist / radius;
    const float lens = std::exp (-q * q);
    const float near = lens / (1.0f + 0.18f * (dist / soft) * (dist / soft));
    const float th = std::atan2 (dy, dx);
    const float asym = 1.0f + 0.15f * std::sin (2.0f * th + 0.65f)
                             + 0.065f * std::sin (5.0f * th + 1.7f);
    const float localPulse = 1.0f + 0.18f * objectBreath;
    float radial = pull * localPulse * near * asym;
    radial = juce::jmin (radial, dist * 0.80f);

    const float frameDrag = 0.42f * pull * localPulse * near;
    const float shear = 5.0f * lens * std::sin (3.0f * th + 0.9f);

    const float fv = field (nx, ny);
    float hn = std::sin (nx * 127.1f + ny * 311.7f) * 43758.5453f;
    const float jx = (hn - std::floor (hn)) - 0.5f;
    float hm = std::sin (nx * 269.5f + ny * 183.3f) * 43758.5453f;
    const float jy = (hm - std::floor (hm)) - 0.5f;

    const float ddx = ux * radial + px * (frameDrag + shear) + fv * GA + jx * 4.0f;
    const float ddy = uy * radial + py * (frameDrag + shear) + fv * GA * 0.9f + jy * 4.0f;

    auto ss = [] (float a, float b, float x)
    {
        float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
        return t * t * (3.0f - 2.0f * t);
    };
    const float win = 0.12f + 0.88f * ss (0.0f, 0.12f, nx) * ss (0.0f, 0.12f, 1.0f - nx)
                            * ss (0.0f, 0.10f, ny) * ss (0.0f, 0.10f, 1.0f - ny);
    return { bx + ddx * win, by + ddy * win };
}

void AnAEditor::drawGravityField (juce::Graphics& g, const Skin& s, Rectangle<float> fr, Point<float> puck, float pull) const
{
    g.saveState();
    g.reduceClipRegion (fr.toNearestInt());

    constexpr int COLS = 30, ROWS = 18;
    std::vector<Point<float>> pts ((size_t) (COLS * ROWS));
    for (int j = 0; j < ROWS; ++j)
        for (int i = 0; i < COLS; ++i)
            pts[(size_t) (j * COLS + i)] = warp ((float) i / (COLS - 1), (float) j / (ROWS - 1), puck, pull);

    auto ef = [&] (Point<float> p)
    {
        return edgeFade (p, fr);
    };
    const float baseA = s.dark ? 0.50f : 0.55f;
    auto seg = [&] (Point<float> a, Point<float> b)
    {
        const float e = 0.5f * (ef (a) + ef (b));
        if (e <= 0.001f) return;
        g.setColour (s.gridLine.withAlpha (std::pow (e, 0.55f) * baseA));
        g.drawLine ({ a, b }, 1.0f);
    };

    for (int j = 0; j < ROWS; ++j)
        for (int i = 0; i < COLS - 1; ++i)
            seg (pts[(size_t) (j * COLS + i)], pts[(size_t) (j * COLS + i + 1)]);
    for (int i = 0; i < COLS; ++i)
        for (int j = 0; j < ROWS - 1; ++j)
            seg (pts[(size_t) (j * COLS + i)], pts[(size_t) ((j + 1) * COLS + i)]);

    for (int m = 0; m < ana::kMeteors; ++m)
    {
        if (proc.mActive[m].load() == 0) continue;
        const float mx = fr.getX() + proc.mX[m].load() * fr.getWidth();
        const float my = fr.getY() + (1.0f - proc.mY[m].load()) * fr.getHeight();
        const float e = juce::jlimit (0.0f, 1.0f, proc.mEnergy[m].load());
        const float r = s.dark ? (2.5f + e * 3.0f) : (1.6f + e * 2.0f);
        g.setColour ((s.dark ? s.gridLine : s.inkSoft).withAlpha (s.dark ? 0.42f * e : 0.22f * e));
        g.drawEllipse (mx - r, my - r, r * 2.0f, r * 2.0f, s.dark ? 0.9f : 0.65f);
    }

    g.restoreState();
}

void AnAEditor::drawMassMarker (juce::Graphics& g, const Skin& s, Point<float> puck) const
{
    const float cx = puck.x, cy = puck.y;
    const Colour outer = s.dark ? s.gridLine.withAlpha (0.88f) : s.ink.withAlpha (0.88f);
    const Colour fill  = s.dark ? juce::Colours::black.withAlpha (0.94f) : s.bg.withAlpha (0.94f);
    g.setColour (fill);
    g.fillEllipse (cx - 15.0f, cy - 15.0f, 30.0f, 30.0f);
    g.setColour (outer);
    g.drawEllipse (cx - 15.0f, cy - 15.0f, 30.0f, 30.0f, 1.8f);
    g.setColour (s.dark ? juce::Colours::white.withAlpha (0.90f) : s.ink.withAlpha (0.92f));
    g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
}

void AnAEditor::sLine (juce::Graphics& g, Point<float> a, Point<float> b, Colour col, float w, float jit, juce::Random& rng) const
{
    const int seg = 5; juce::Path p;
    for (int pass = 0; pass < 2; ++pass)
    {
        p.clear();
        for (int s = 0; s <= seg; ++s)
        {
            float t = (float) s / seg;
            float x = a.x + (b.x - a.x) * t + (rng.nextFloat() * 2.0f - 1.0f) * jit;
            float y = a.y + (b.y - a.y) * t + (rng.nextFloat() * 2.0f - 1.0f) * jit;
            if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col); g.strokePath (p, PST (w, PST::curved, PST::rounded));
    }
}

void AnAEditor::sRect (juce::Graphics& g, Rectangle<float> r, Colour col, float w, float jit, juce::Random& rng) const
{
    Point<float> a { r.getX(), r.getY() }, b { r.getRight(), r.getY() }, c { r.getRight(), r.getBottom() }, d { r.getX(), r.getBottom() };
    sLine (g, a, b, col, w, jit, rng); sLine (g, b, c, col, w, jit, rng); sLine (g, c, d, col, w, jit, rng); sLine (g, d, a, col, w, jit, rng);
}

void AnAEditor::sCircle (juce::Graphics& g, Point<float> c, float r, Colour col, float w, float jit, juce::Random& rng) const
{
    const int n = 36;
    for (int pass = 0; pass < 2; ++pass)
    {
        juce::Path p;
        for (int k = 0; k <= n; ++k)
        {
            float a = juce::MathConstants<float>::twoPi * k / n;
            float x = c.x + std::cos (a) * r + (rng.nextFloat() * 2.0f - 1.0f) * jit;
            float y = c.y + std::sin (a) * r + (rng.nextFloat() * 2.0f - 1.0f) * jit;
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col); g.strokePath (p, PST (w, PST::curved, PST::rounded));
    }
}

//==============================================================================
void AnAEditor::paint (juce::Graphics& g)
{
    layout();
    const Skin s = skin();
    g.fillAll (s.bg);

    const auto fr = fieldRect();
    const float uiBreath = juce::jlimit (0.0f, 1.8f, proc.uiBreath.load());
    vBreath = 0.9f + 0.4f * juce::jlimit (0.0f, 1.6f, uiBreath);
    const float px = juce::jlimit (0.0f, 1.0f, proc.uiPosX.load());
    const float py = juce::jlimit (0.0f, 1.0f, proc.uiPosY.load());
    const Point<float> puck { fr.getX() + px * fr.getWidth(), fr.getY() + (1.0f - py) * fr.getHeight() };
    const float pull = 190.0f;

    {
        const float gr = 76.0f;
        const float discA = s.dark ? 0.050f : 0.026f;
        juce::ColourGradient gg (s.glowCol.withAlpha (discA), puck.x, puck.y,
                                 s.glowCol.withAlpha (0.0f), puck.x + gr, puck.y, true);
        gg.addColour (0.48, s.glowCol.withAlpha (discA * 0.38f));
        gg.addColour (0.78, s.glowCol.withAlpha (discA * 0.08f));
        g.setGradientFill (gg); g.fillEllipse (puck.x - gr, puck.y - gr, gr * 2.0f, gr * 2.0f);
    }

    drawGravityField (g, s, fr, puck, pull);

    {
        const float wmH = 150.0f;
        auto wmRect = Rectangle<float> (0.0f, fr.getY() + 0.40f * fr.getHeight() - wmH * 0.5f, (float) getWidth(), wmH);
        g.setFont (fred (wmH));
        const float baseline = wmRect.getBottom() - 10.0f;
        g.saveState();
        g.addTransform (juce::AffineTransform (1.0f, 0.0f, 0.0f, 0.0f, -0.55f, 1.55f * baseline));
        g.setColour (s.ink.withAlpha (s.dark ? 0.12f : 0.10f));
        g.drawText ("AnA", wmRect.toNearestInt(), juce::Justification::centred, false);
        g.restoreState();
        g.setColour (s.ink.withAlpha (s.dark ? 0.93f : 0.79f));
        g.drawText ("AnA", wmRect.toNearestInt(), juce::Justification::centred, false);
    }

    drawMassMarker (g, s, puck);

    g.setColour (s.inkSoft); g.setFont (barlow (6.5f));
    g.drawText ("SPILL", Rectangle<int> ((int) fr.getCentreX() - 60, 37, 120, 12), juce::Justification::centred);
    g.drawText ("STILL", Rectangle<int> ((int) fr.getCentreX() - 60, 540, 120, 12), juce::Justification::centred);
    g.drawText ("NEAR",  Rectangle<int> ((int) fr.getX(), 289, 70, 12), juce::Justification::centredLeft);
    g.drawText ("WHERE", Rectangle<int> ((int) fr.getRight() - 70, 289, 70, 12), juce::Justification::centredRight);

    juce::Random rng (20260616);
    auto drawBtn = [&] (Rectangle<float> r, const juce::String& tx, bool on, bool tri)
    {
        Colour col = on ? s.accent : s.inkSoft;
        g.setColour (s.bg.withAlpha (0.6f)); g.fillRect (r);
        sRect (g, r, col.withAlpha (on ? 0.95f : 0.62f), 1.2f, 0.7f, rng);
        const float ly = r.getCentreY(), dot = r.getX() + 9.0f, rr = on ? 3.0f : 2.3f;
        sCircle (g, { dot, ly }, rr, col.withAlpha (0.86f), 1.2f, 0.4f, rng);
        if (on) { g.setColour (col); g.fillEllipse (dot - 1.4f, ly - 1.4f, 2.8f, 2.8f); }
        g.setColour (col); g.setFont (barlow (6.5f));
        g.drawText (tx, r.withTrimmedLeft (16.0f).toNearestInt(), juce::Justification::centredLeft);
        if (tri)
        {
            const float t2 = r.getRight() - 8.0f; juce::Path p;
            p.addTriangle (t2 - 3.5f, ly - 1.6f, t2 + 3.5f, ly - 1.6f, t2, ly + 2.4f);
            g.setColour (col.withAlpha (0.9f)); g.fillPath (p);
        }
    };
    drawBtn (rFreeze, "FREEZE", getP ("freeze") > 0.5f, false);
    drawBtn (rDrift,  "DRIFT",  getP ("drift")  > 0.5f, false);
    drawBtn (rWorld,  s.dark ? "ZRENJE" : "ZAPIS", s.dark, false);
    drawBtn (rMirage, "MIRAGE", false, true);

    {
        const auto c = rMix.getCentre(); const float R = 22.0f, val = proc.apvts.getParameter ("mix")->getValue();
        g.setColour (s.bg.withAlpha (0.6f)); g.fillEllipse (c.x - R - 3, c.y - R - 3, (R + 3) * 2, (R + 3) * 2);
        sCircle (g, c, R, s.inkSoft.withAlpha (0.82f), 1.2f, 1.0f, rng);
        const float a0 = juce::MathConstants<float>::pi;
        const float aa = a0 + juce::MathConstants<float>::twoPi * val;
        if (val > 0.002f)
        {
            juce::Path arc; arc.startNewSubPath (c.x + std::sin (a0) * R, c.y - std::cos (a0) * R);
            for (float t = a0; t <= aa; t += 0.08f) arc.lineTo (c.x + std::sin (t) * R, c.y - std::cos (t) * R);
            g.setColour ((s.dark ? s.gridLine : s.ink).withAlpha (0.82f)); g.strokePath (arc, PST (2.0f));
        }
        sLine (g, { c.x + std::sin (aa) * R * 0.16f, c.y - std::cos (aa) * R * 0.16f },
                  { c.x + std::sin (aa) * R * 0.82f, c.y - std::cos (aa) * R * 0.82f },
                  s.dark ? s.gridLine : s.ink, 2.2f, 0.7f, rng);
        g.setColour (s.inkSoft); g.setFont (barlow (6.5f));
        g.drawText ("MIX", Rectangle<int> ((int) c.x - 20, (int) (c.y + R + 4), 40, 12), juce::Justification::centred);
    }

    {
        const juce::Image& z = s.dark ? zlabLight : zlabDark;
        if (z.isValid())
        {
            const float lh = 30.0f, lw = lh * (float) z.getWidth() / (float) z.getHeight();
            g.drawImage (z, Rectangle<float> ((float) getWidth() - lw - 18.0f, 513.0f - lh * 0.5f, lw, lh));
        }
    }

    {
        juce::ColourGradient bar (s.barTop, 0.0f, 0.0f, s.barBot, 0.0f, TB, false);
        g.setGradientFill (bar); g.fillRect (0.0f, 0.0f, (float) getWidth(), TB);
        g.setColour (juce::Colours::white.withAlpha (s.dark ? 0.06f : 0.30f)); g.drawHorizontalLine (1, 0.0f, (float) getWidth());
        g.setColour ((s.dark ? juce::Colours::black : Colour (0xff909094)).withAlpha (s.dark ? 0.7f : 0.55f)); g.drawHorizontalLine ((int) TB - 1, 0.0f, (float) getWidth());
        chevron (g, rPrev, false, s.ink);
        chevron (g, rNext, true,  s.ink);
        g.setColour (s.barField); g.fillRoundedRectangle (rMenu, 2.0f);
        g.setColour (s.ink.withAlpha (0.28f)); g.drawRoundedRectangle (rMenu, 2.0f, 1.0f);
        g.setColour (s.ink.withAlpha (0.92f)); g.setFont (barlow (11.0f));
        g.drawText (currentPreset, rMenu.toNearestInt(), juce::Justification::centred);
        { const float t2 = rMenu.getRight() - 12.0f, ly = rMenu.getCentreY(); juce::Path p; p.addTriangle (t2 - 3, ly - 2, t2 + 3, ly - 2, t2, ly + 2); g.setColour (s.ink.withAlpha (0.7f)); g.fillPath (p); }
        histIcon (g, rUndo, false, s.ink, 0.5f);
        histIcon (g, rRedo, true,  s.ink, 0.5f);
    }
}

//==============================================================================
AnAEditor::Hit AnAEditor::hitTest (Point<float> p) const
{
    if (rPrev.contains (p)) return Hit::prev;
    if (rNext.contains (p)) return Hit::next;
    if (rMenu.contains (p)) return Hit::menu;
    if (rUndo.contains (p)) return Hit::undo;
    if (rRedo.contains (p)) return Hit::redo;
    if (p.getDistanceFrom (rMix.getCentre()) < 26.0f) return Hit::mix;
    if (rFreeze.contains (p)) return Hit::freeze;
    if (rDrift.contains (p))  return Hit::drift;
    if (rWorld.contains (p))  return Hit::world;
    if (rMirage.contains (p)) return Hit::mirage;
    if (p.y > TB && fieldRect().contains (p)) return Hit::puck;
    return Hit::none;
}

void AnAEditor::mouseDown (const juce::MouseEvent& e)
{
    layout();
    const Hit h = hitTest (e.position); dragging = h;
    switch (h)
    {
        case Hit::freeze: setP ("freeze", getP ("freeze") > 0.5f ? 0.0f : 1.0f); break;
        case Hit::drift:  setP ("drift",  getP ("drift")  > 0.5f ? 0.0f : 1.0f); break;
        case Hit::world:  setP ("world",  getP ("world")  > 0.5f ? 0.0f : 1.0f); break;
        case Hit::mix:    dragStartVal = proc.apvts.getParameter ("mix")->getValue(); break;
        case Hit::puck:   mouseDrag (e); break;
        case Hit::menu:   showPresetMenu(); break;
        default: break;
    }
}

void AnAEditor::mouseDrag (const juce::MouseEvent& e)
{
    const auto fr = fieldRect();
    if (dragging == Hit::puck)
    {
        setP ("posx", (e.position.x - fr.getX()) / fr.getWidth());
        setP ("posy", 1.0f - (e.position.y - fr.getY()) / fr.getHeight());
    }
    else if (dragging == Hit::mix)
    {
        setP ("mix", dragStartVal - (float) e.getDistanceFromDragStartY() * 0.011f);
    }
}

void AnAEditor::mouseUp (const juce::MouseEvent&) { dragging = Hit::none; }

void AnAEditor::showPresetMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Default", true, currentPreset == "Default");
    m.addSeparator();
    m.addItem (2, "Save preset...", false, false);
    m.addItem (3, "Load preset...", false, false);
    m.showMenuAsync (juce::PopupMenu::Options(), [] (int) {});
}
