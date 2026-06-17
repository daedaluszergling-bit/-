#pragma once

#include "PluginProcessor.h"
#include <juce_opengl/juce_opengl.h>

//==============================================================================
class AnAEditor : public juce::AudioProcessorEditor,
                  private juce::Timer
{
public:
    explicit AnAEditor (AnAProcessor&);
    ~AnAEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    float getP (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    void  setP (const char* id, float v01)
    {
        if (auto* p = proc.apvts.getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v01));
    }

    static constexpr int kVF = 5;
    float locPhase[kVF] = {};
    double lastMs = 0.0;
    float vBreath = 1.0f;

    struct Skin { juce::Colour bg, ink, inkSoft, gridLine, accent, barTop, barBot, barField, glowCol; bool dark; };
    Skin skin() const;

    float field (float nx, float ny) const;
    float edgeFade (juce::Point<float>, juce::Rectangle<float>) const;
    juce::Point<float> warp (float nx, float ny, juce::Point<float> puck, float pull) const;
    void drawGravityField (juce::Graphics&, const Skin&, juce::Rectangle<float>, juce::Point<float>, float) const;
    void drawMassMarker (juce::Graphics&, const Skin&, juce::Point<float>) const;

    juce::Rectangle<float> fieldRect() const;
    void layout();
    juce::Rectangle<float> rFreeze, rDrift, rWorld, rMirage, rMix;
    juce::Rectangle<float> rPrev, rNext, rMenu, rUndo, rRedo;

    enum class Hit { none, puck, mix, freeze, drift, world, mirage, prev, next, menu, undo, redo };
    Hit hitTest (juce::Point<float>) const;
    Hit dragging = Hit::none;
    float dragStartVal = 0.0f;

    void showPresetMenu();

    void sLine (juce::Graphics&, juce::Point<float>, juce::Point<float>, juce::Colour, float w, float jit, juce::Random&) const;
    void sRect (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float w, float jit, juce::Random&) const;
    void sCircle (juce::Graphics&, juce::Point<float> c, float r, juce::Colour, float w, float jit, juce::Random&) const;

    juce::Font fred (float h)   const { return juce::Font (juce::FontOptions (fredFace).withHeight (h)); }
    juce::Font barlow (float h) const { return juce::Font (juce::FontOptions (barlowFace).withHeight (h)); }

    AnAProcessor& proc;
    juce::OpenGLContext openGL;
    juce::Typeface::Ptr fredFace, barlowFace;
    juce::Image zlabLight, zlabDark;
    juce::String currentPreset { "Default" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnAEditor)
};
