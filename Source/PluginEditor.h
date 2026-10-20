#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "gui/LookAndFeel303.h"
#include "gui/Widgets.h"

// The whole front panel, laid out at a fixed 1200 x 640 design size.
// The editor scales it with an AffineTransform, so every control stays
// vector-sharp at any window size; the metal texture is re-rendered at the
// physical pixel resolution after a resize.
class MainPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1200;
    static constexpr int designHeight = 640;

    explicit MainPanel (Reckless303Processor&);
    ~MainPanel() override;

    void paint (juce::Graphics&) override;
    void setDisplayScale (float editorScale);

    std::function<void()> onSizeMenu;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void layout();
    void setupKnob (juce::Slider&, const char* paramId, int ticks = 11);
    void regenerateTexture();

    void editSelected (const std::function<void (acid::Step&)>& fn);
    void showStepMenu (int index);
    void paintBottomSection (juce::Graphics&);
    void paintLabel (juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size = 12.5f,
                     juce::Colour colour = acid::Palette::ink, juce::Justification j = juce::Justification::centred);

    Reckless303Processor& processor;

    juce::Slider tuning, cutoff, resonance, envMod, decay, accent, tempo, drive, volume, mode;
    acid::SlideSwitch waveform;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<ButtonAttachment> waveAttachment;

    std::array<std::unique_ptr<acid::KeyCap>, 13> keys;
    acid::KeyCap downKey { "DOWN", true }, upKey { "UP", true }, accentKey { "ACCENT", true }, slideKey { "SLIDE", true };
    acid::KeyCap backKey { "BACK", false }, nextKey { "NEXT", false }, clearKey { "CLEAR", false };
    acid::KeyCap runKey { "RUN", true }, timeKey { "TIME", false }, syncKey { "SYNC", true };
    std::unique_ptr<ButtonAttachment> syncAttachment;
    juce::String bpmText;
    juce::TextButton lenDown { "-" }, lenUp { "+" };
    acid::StepRow stepRow;
    acid::PresetBar presetBar;

    int selectedStep = 0;
    acid::Time shownTime = acid::Time::Note;
    juce::String lastInfo;

    juce::Image texture;
    float textureScale = 0.0f, wantedTextureScale = 1.0f;
    juce::uint32 lastScaleChange = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

class Reckless303Editor : public juce::AudioProcessorEditor
{
public:
    explicit Reckless303Editor (Reckless303Processor&);
    ~Reckless303Editor() override;

    void resized() override;

private:
    void showSizeMenu();

    Reckless303Processor& audioProcessor;
    acid::LookAndFeel303 lnf;
    juce::TooltipWindow tooltips { this, 600 };
    MainPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Reckless303Editor)
};
