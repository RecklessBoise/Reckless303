#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace acid
{
namespace Palette
{
const juce::Colour ink       { 0xff24231f };
const juce::Colour inkSoft   { 0xff4a4842 };
const juce::Colour keyWhite  { 0xffe8e4da };
const juce::Colour keyBlack  { 0xff19191a };
const juce::Colour orange    { 0xffe0571d };
const juce::Colour ledOn     { 0xffff3a24 };
const juce::Colour ledOff    { 0xff4a100b };
const juce::Colour lcd       { 0xff0c1513 };
const juce::Colour lcdText   { 0xff59f0c0 };
} // namespace Palette

juce::Font panelFont (float height, bool bold = true);
juce::Font logoFont (float height);

// Draws the 3D machined aluminium knob used for every continuous control.
void drawMetalKnob (juce::Graphics& g, juce::Point<float> centre, float radius, float angle);
// The big black MODE selector with a raised metal bar.
void drawBarKnob (juce::Graphics& g, juce::Point<float> centre, float radius, float angle);
// A round red LED with glow.
void drawLed (juce::Graphics& g, juce::Point<float> centre, float radius, bool on);
// A rubbery rectangular key cap, as on the TB-303 front panel.
void drawKeyCap (juce::Graphics& g, juce::Rectangle<float> r, bool down, bool highlighted, bool dark = false);
// A cross-head screw.
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius, float angle);

class LookAndFeel303 : public juce::LookAndFeel_V4
{
public:
    LookAndFeel303();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    juce::Font getPopupMenuFont() override { return panelFont (15.0f, false); }
    juce::Font getAlertWindowMessageFont() override { return panelFont (15.0f, false); }
};
} // namespace acid
