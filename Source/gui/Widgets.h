#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

#include "Pattern.h"
#include "gui/LookAndFeel303.h"

class PresetManager;

namespace acid
{
// Rubber key cap with an optional red LED above it.
class KeyCap : public juce::Button
{
public:
    KeyCap (const juce::String& name, bool withLed, bool darkCap = false);

    void setLed (bool on);
    bool getLed() const noexcept { return ledOn; }

    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool hasLed, dark, ledOn = false;
};

// Two-position slide switch (waveform selector).
class SlideSwitch : public juce::Button
{
public:
    SlideSwitch() : juce::Button ("switch") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// The 16-step strip under the keyboard: shows the pattern, the play position
// and the step being edited.
class StepRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onSelect;
    std::function<void (int)> onMenu;

    void update (const std::array<Step, maxSteps>& steps, int length, int selected, int playing);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int i) const;
    int indexAt (juce::Point<float>) const;

    std::array<Step, maxSteps> data {};
    int len = 16, sel = 0, play = -1;
};

// Preset display and controls: browse, like, favourites filter, random,
// save, delete and plugin size.
class PresetBar : public juce::Component, private juce::ChangeListener
{
public:
    explicit PresetBar (PresetManager&);
    ~PresetBar() override;

    std::function<void()> onSizeMenu;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void showBrowser();
    void showSaveDialog();
    void confirmDelete();

    juce::Rectangle<int> lcdArea() const { return { 0, 0, getWidth(), 50 }; }

    PresetManager& presets;
    juce::TextButton prev { "<" }, next { ">" }, favs { "FAVS" }, random { "RANDOM" },
                     save { "SAVE" }, del { "DEL" }, size { "SIZE" };

    struct Heart : juce::Component
    {
        bool filled = false;
        std::function<void()> onClick;
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override { if (onClick) onClick(); }
    } heart;

    juce::String name, category, counter;
};
} // namespace acid
