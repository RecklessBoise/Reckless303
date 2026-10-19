#include "PluginEditor.h"
#include "gui/MetalTexture.h"
#include "presets/PresetManager.h"

using namespace juce;
using namespace acid;

namespace
{
constexpr float kbX = 280.0f, kbW = 540.0f, whiteW = kbW / 8.0f;
constexpr int whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11, 12 };
constexpr int blackNotes[] = { 1, 3, 6, 8, 10 };
constexpr int blackSlots[] = { 1, 2, 4, 5, 6 }; // boundary between white keys

float keyCentreX (int note)
{
    for (int i = 0; i < 8; ++i)
        if (whiteNotes[i] == note)
            return kbX + ((float) i + 0.5f) * whiteW;
    for (int i = 0; i < 5; ++i)
        if (blackNotes[i] == note)
            return kbX + (float) blackSlots[i] * whiteW;
    return kbX;
}

bool isBlack (int note)
{
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

const Rectangle<float> infoLcd { 838.0f, 378.0f, 258.0f, 54.0f };
const Rectangle<float> lenLcd { 70.0f, 592.0f, 54.0f, 28.0f };
const Rectangle<float> timeLeds { 180.0f, 380.0f, 90.0f, 70.0f };
} // namespace

//==============================================================================
MainPanel::MainPanel (Acid303Processor& p)
    : processor (p), presetBar (p.getPresetManager())
{
    setupKnob (tuning, ParamID::tuning);
    setupKnob (cutoff, ParamID::cutoff);
    setupKnob (resonance, ParamID::resonance);
    setupKnob (envMod, ParamID::envMod);
    setupKnob (decay, ParamID::decay);
    setupKnob (accent, ParamID::accent);
    setupKnob (tempo, ParamID::tempo, 13);
    setupKnob (drive, ParamID::drive);
    setupKnob (volume, ParamID::volume, 13);
    setupKnob (mode, ParamID::mode, 3);

    // MODE is a 3-position rotary switch, labelled on its right side like the original.
    mode.getProperties().set ("bar", true);
    mode.setRotaryParameters (MathConstants<float>::pi * 0.28f, MathConstants<float>::pi * 0.72f, true);
    mode.setPopupDisplayEnabled (false, false, nullptr);

    addAndMakeVisible (waveform);
    waveAttachment = std::make_unique<ButtonAttachment> (processor.apvts, ParamID::waveform, waveform);
    waveform.setTooltip ("Waveform: sawtooth / square");

    for (int k = 0; k < 13; ++k)
    {
        keys[(size_t) k] = std::make_unique<KeyCap> (noteName (k), true);
        auto* key = keys[(size_t) k].get();
        addAndMakeVisible (key);
        key->onClick = [this, k] { editSelected ([k] (Step& s) { s.note = k; s.time = acid::Time::Note; }); };
    }

    for (auto* b : { &downKey, &upKey, &accentKey, &slideKey, &backKey, &nextKey, &clearKey, &runKey, &timeKey })
        addAndMakeVisible (b);
    addAndMakeVisible (lenDown);
    addAndMakeVisible (lenUp);
    addAndMakeVisible (stepRow);
    addAndMakeVisible (presetBar);

    downKey.onClick   = [this] { editSelected ([] (Step& s) { s.down = ! s.down; if (s.down) s.up = false; }); };
    upKey.onClick     = [this] { editSelected ([] (Step& s) { s.up = ! s.up; if (s.up) s.down = false; }); };
    accentKey.onClick = [this] { editSelected ([] (Step& s) { s.accent = ! s.accent; }); };
    slideKey.onClick  = [this] { editSelected ([] (Step& s) { s.slide = ! s.slide; }); };
    timeKey.onClick   = [this] { editSelected ([] (Step& s) { s.time = (acid::Time) (((int) s.time + 1) % 3); }); };

    backKey.onClick = [this]
    {
        const int len = processor.getPatternLength();
        selectedStep = (jmin (selectedStep, len - 1) - 1 + len) % len;
    };
    nextKey.onClick = [this] { selectedStep = (selectedStep + 1) % processor.getPatternLength(); };

    clearKey.onClick = [this]
    {
        processor.setPattern (Pattern::fromString ("C C C C C C C C C C C C C C C C"));
        selectedStep = 0;
    };

    runKey.onClick = [this]
    {
        if (auto* param = processor.apvts.getParameter (ParamID::run))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->getValue() > 0.5f ? 0.0f : 1.0f);
            param->endChangeGesture();
        }
    };

    lenDown.onClick = [this] { processor.setPatternLength (processor.getPatternLength() - 1); };
    lenUp.onClick   = [this] { processor.setPatternLength (processor.getPatternLength() + 1); };

    stepRow.onSelect = [this] (int i) { selectedStep = i; };
    stepRow.onMenu   = [this] (int i) { showStepMenu (i); };
    presetBar.onSizeMenu = [this] { if (onSizeMenu) onSizeMenu(); };

    runKey.setTooltip ("Run / stop the pattern (it also follows the host transport)");
    clearKey.setTooltip ("Clear the pattern");
    timeKey.setTooltip ("Time mode of the selected step: note / tie / rest");
    backKey.setTooltip ("Select the previous step");
    nextKey.setTooltip ("Select the next step");
    stepRow.setTooltip ("Click a step to edit it, right-click for pattern tools");

    setSize (designWidth, designHeight);
    layout();
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
}

void MainPanel::setupKnob (Slider& s, const char* paramId, int ticks)
{
    s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    s.setMouseDragSensitivity (220);
    s.setPopupDisplayEnabled (true, true, this);
    s.getProperties().set ("ticks", ticks);
    addAndMakeVisible (s);

    sliderAttachments.push_back (std::make_unique<SliderAttachment> (processor.apvts, paramId, s));

    if (auto* param = processor.apvts.getParameter (paramId))
        s.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void MainPanel::layout()
{
    // Top row.
    waveform.setBounds (58, 48, 104, 34);
    const int knobX[] = { 380, 485, 590, 695, 800, 905 };
    Slider* top[] = { &tuning, &cutoff, &resonance, &envMod, &decay, &accent };
    for (int i = 0; i < 6; ++i)
        top[i]->setBounds (knobX[i] - 50, 36, 100, 100);

    // Middle row.
    tempo.setBounds (65, 182, 130, 130);
    drive.setBounds (228, 197, 100, 100);
    mode.setBounds (372, 185, 124, 124);
    presetBar.setBounds (585, 206, 395, 94);
    volume.setBounds (1035, 182, 130, 130);

    // Keyboard.
    for (int k = 0; k < 13; ++k)
    {
        const int cx = (int) keyCentreX (k);
        if (isBlack (k)) keys[(size_t) k]->setBounds (cx - 22, 378, 44, 88);
        else             keys[(size_t) k]->setBounds (cx - 22, 474, 44, 86);
    }

    downKey.setBounds (838, 474, 62, 86);
    upKey.setBounds (904, 474, 62, 86);
    accentKey.setBounds (970, 474, 62, 86);
    slideKey.setBounds (1036, 474, 62, 86);

    backKey.setBounds (1108, 380, 64, 64);
    nextKey.setBounds (1108, 494, 64, 66);

    clearKey.setBounds (44, 376, 106, 52);
    runKey.setBounds (36, 450, 122, 110);
    timeKey.setBounds (186, 492, 78, 68);

    lenDown.setBounds (30, 590, 38, 32);
    lenUp.setBounds (126, 590, 38, 32);
    stepRow.setBounds (176, 564, 1000, 64);
}

void MainPanel::setDisplayScale (float editorScale)
{
    float displayScale = 1.0f;
    if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        displayScale = (float) d->scale;

    const float wanted = jlimit (0.25f, 4096.0f / (float) designWidth, editorScale * displayScale);
    if (std::abs (wanted - wantedTextureScale) > 0.001f)
    {
        wantedTextureScale = wanted;
        lastScaleChange = juce::Time::getMillisecondCounter();
    }
}

void MainPanel::regenerateTexture()
{
    textureScale = wantedTextureScale;
    texture = createBrushedMetal (roundToInt ((float) designWidth * textureScale),
                                  roundToInt ((float) designHeight * textureScale));
    repaint();
}

void MainPanel::editSelected (const std::function<void (Step&)>& fn)
{
    auto s = processor.getStep (selectedStep);
    fn (s);
    processor.setStep (selectedStep, s);
}

void MainPanel::showStepMenu (int index)
{
    PopupMenu m;
    m.addItem (1, "Pattern length: " + String (index + 1) + " steps (end here)");
    m.addSeparator();
    m.addItem (2, "Shift pattern left");
    m.addItem (3, "Shift pattern right");
    m.addItem (4, "Randomize pattern");
    m.addItem (5, "Clear pattern");
    m.addSeparator();
    m.addItem (6, "Copy pattern");
    m.addItem (7, "Paste pattern", SystemClipboard::getTextFromClipboard().isNotEmpty());

    SafePointer<MainPanel> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&stepRow), [safe, index] (int r)
    {
        if (safe == nullptr || r == 0)
            return;

        auto& proc = safe->processor;
        auto pat = proc.getPattern();
        const int len = pat.length;

        switch (r)
        {
            case 1: proc.setPatternLength (index + 1); break;
            case 2: std::rotate (pat.steps.begin(), pat.steps.begin() + 1, pat.steps.begin() + len); proc.setPattern (pat); break;
            case 3: std::rotate (pat.steps.begin(), pat.steps.begin() + len - 1, pat.steps.begin() + len); proc.setPattern (pat); break;
            case 4: { Random rng; proc.setPattern (Pattern::random (rng)); break; }
            case 5: proc.setPattern (Pattern::fromString ("C C C C C C C C C C C C C C C C")); break;
            case 6: SystemClipboard::copyTextToClipboard (pat.toString()); break;
            case 7: proc.setPattern (Pattern::fromString (SystemClipboard::getTextFromClipboard().substring (0, 400))); break;
            default: break;
        }
    });
}

void MainPanel::timerCallback()
{
    // Re-render the texture once the window has stopped changing size.
    if (std::abs (wantedTextureScale - textureScale) > 0.001f && juce::Time::getMillisecondCounter() - lastScaleChange > 150)
        regenerateTexture();

    const auto pattern = processor.getPattern();
    selectedStep = jlimit (0, maxSteps - 1, selectedStep);
    const auto& sel = pattern.steps[(size_t) selectedStep];
    const bool running = processor.isSequencerRunning();

    stepRow.update (pattern.steps, pattern.length, selectedStep, running ? processor.getPlayingStep() : -1);

    for (int k = 0; k < 13; ++k)
        keys[(size_t) k]->setLed (sel.time == acid::Time::Note && sel.note == k);

    downKey.setLed (sel.down);
    upKey.setLed (sel.up);
    accentKey.setLed (sel.accent);
    slideKey.setLed (sel.slide);
    runKey.setLed (running);

    const auto info = String (selectedStep) + "/" + String (pattern.length) + "/" + String (sel.pack());
    if (info != lastInfo)
    {
        lastInfo = info;
        shownTime = sel.time;
        repaint (infoLcd.expanded (4.0f).getSmallestIntegerContainer());
        repaint (lenLcd.expanded (4.0f).getSmallestIntegerContainer());
        repaint (timeLeds.expanded (8.0f).getSmallestIntegerContainer());
    }
}

void MainPanel::paintLabel (Graphics& g, const String& text, Rectangle<float> area, float size, Colour colour, Justification j)
{
    g.setColour (colour);
    g.setFont (panelFont (size));
    g.drawText (text, area, j, false);
}

void MainPanel::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    if (! texture.isValid())
        regenerateTexture();

    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (texture, bounds);

    // Case bevel.
    g.setColour (Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 2.0f);
    g.setColour (Colours::black.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 7.0f, 1.2f);

    for (auto pt : { Point<float> (12, 12), Point<float> (1188, 12), Point<float> (12, 628), Point<float> (1188, 628) })
        drawScrew (g, pt, 6.0f, 0.4f + pt.x * 0.001f);

    // Engraved grooves between the sections.
    auto groove = [&g] (float y)
    {
        g.setColour (Colours::black.withAlpha (0.4f));
        g.fillRect (8.0f, y, 1184.0f, 1.5f);
        g.setColour (Colours::white.withAlpha (0.6f));
        g.fillRect (8.0f, y + 1.5f, 1184.0f, 1.2f);
    };
    groove (152.0f);
    groove (338.0f);

    // ---- Top row -------------------------------------------------------
    paintLabel (g, "WAVEFORM", { 40, 22, 140, 16 });

    // Saw and square icons either side of the switch.
    {
        g.setColour (Palette::ink);
        Path saw;
        saw.startNewSubPath (34, 74); saw.lineTo (42, 58); saw.lineTo (42, 74); saw.lineTo (50, 58); saw.lineTo (50, 74);
        g.strokePath (saw, PathStrokeType (1.6f));
        Path sq;
        sq.startNewSubPath (170, 74); sq.lineTo (170, 58); sq.lineTo (178, 58); sq.lineTo (178, 74); sq.lineTo (186, 74); sq.lineTo (186, 58);
        g.strokePath (sq, PathStrokeType (1.6f));
    }

    g.setColour (Palette::ink);
    g.setFont (logoFont (42.0f));
    g.drawText ("ACID-303", Rectangle<float> (30, 92, 270, 50), Justification::centredLeft);

    g.setColour (Palette::ink.withAlpha (0.85f));
    g.fillRect (316.0f, 26.0f, 2.0f, 104.0f);
    g.fillRect (964.0f, 26.0f, 2.0f, 104.0f);

    const char* topNames[] = { "TUNING", "CUT OFF FREQ", "RESONANCE", "ENV MOD", "DECAY", "ACCENT" };
    const int knobX[] = { 380, 485, 590, 695, 800, 905 };
    for (int i = 0; i < 6; ++i)
        paintLabel (g, topNames[i], { (float) knobX[i] - 55.0f, 16.0f, 110.0f, 16.0f });

    // Jack labels like the back edge of the original.
    const char* jacks[] = { "CV", "GATE", "HEADPHONE", "OUTPUT" };
    const float jackX[] = { 995.0f, 1040.0f, 1095.0f, 1155.0f };
    for (int i = 0; i < 4; ++i)
    {
        Path tri;
        tri.addTriangle (jackX[i] - 4.0f, 22.0f, jackX[i] + 4.0f, 22.0f, jackX[i], 15.0f);
        g.setColour (Palette::ink);
        g.fillPath (tri);
        paintLabel (g, jacks[i], { jackX[i] - 40.0f, 24.0f, 80.0f, 13.0f }, 9.5f, Palette::inkSoft);
    }

    g.setColour (Palette::ink);
    g.setFont (logoFont (48.0f));
    g.drawText ("Bass Line", Rectangle<float> (975, 62, 210, 64), Justification::centred);

    // ---- Middle row ----------------------------------------------------
    paintLabel (g, "TEMPO", { 70, 162, 120, 16 });
    paintLabel (g, "SLOW", { 52, 300, 60, 12 }, 9.5f, Palette::inkSoft);
    paintLabel (g, "FAST", { 150, 300, 60, 12 }, 9.5f, Palette::inkSoft);

    paintLabel (g, "DRIVE", { 228, 175, 100, 16 });

    paintLabel (g, "MODE", { 372, 162, 124, 16 });
    {
        const Point<float> c (434.0f, 247.0f);
        const char* modeNames[] = { "MIDI", "PATTERN", "KEY TRIG" };
        for (int i = 0; i < 3; ++i)
        {
            const float a = MathConstants<float>::pi * (0.28f + 0.22f * (float) i);
            const auto pt = c + Point<float> (std::sin (a), -std::cos (a)) * 70.0f;
            g.setColour (Palette::ink);
            g.fillEllipse (pt.x - 2.0f, pt.y - 2.0f, 4.0f, 4.0f);
            g.setColour (Palette::keyBlack);
            g.fillRoundedRectangle (pt.x + 6.0f, pt.y - 7.5f, 66.0f, 15.0f, 2.0f);
            paintLabel (g, modeNames[i], { pt.x + 6.0f, pt.y - 7.5f, 66.0f, 15.0f }, 10.5f, Palette::keyWhite);
        }
    }

    g.setColour (Palette::ink);
    g.setFont (logoFont (30.0f));
    g.drawText ("Computer Controlled", Rectangle<float> (585, 160, 395, 38), Justification::centred);
    g.fillRect (600.0f, 197.0f, 365.0f, 1.5f);

    paintLabel (g, "VOLUME", { 1040, 162, 120, 16 });
    paintLabel (g, "MIN", { 1025, 300, 50, 12 }, 9.5f, Palette::inkSoft);
    paintLabel (g, "MAX", { 1125, 300, 50, 12 }, 9.5f, Palette::inkSoft);

    paintBottomSection (g);
}

void MainPanel::paintBottomSection (Graphics& g)
{
    const auto frame = Rectangle<float> (18, 348, 1164, 284);

    // Slightly darker inset for the whole sequencer area.
    g.setColour (Colours::black.withAlpha (0.05f));
    g.fillRoundedRectangle (frame, 5.0f);
    g.setColour (Palette::ink.withAlpha (0.85f));
    g.drawRoundedRectangle (frame, 5.0f, 1.6f);

    auto box = [&g] (Rectangle<float> r)
    {
        g.setColour (Palette::ink.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    };

    // ---- Left column: clear / run / length -------------------------------
    box ({ 24, 354, 146, 206 });
    paintLabel (g, "PATTERN CLEAR", { 24, 358, 146, 14 }, 11.0f);
    paintLabel (g, "RUN / STOP", { 24, 434, 146, 14 }, 11.0f);
    paintLabel (g, "RUN", { 104, 452, 40, 14 }, 9.5f, Palette::inkSoft, Justification::centredLeft);

    box ({ 24, 566, 146, 60 });
    paintLabel (g, "PATTERN LENGTH", { 24, 570, 146, 14 }, 10.0f);
    g.setColour (Palette::lcd);
    g.fillRoundedRectangle (lenLcd, 3.0f);
    paintLabel (g, String (processor.getPatternLength()), lenLcd, 16.0f, Palette::lcdText);

    // ---- Time mode ------------------------------------------------------
    box ({ 176, 354, 98, 206 });
    g.setColour (Palette::keyBlack);
    g.fillRect (176.0f, 354.0f, 98.0f, 18.0f);
    paintLabel (g, "TIME MODE", { 176, 354, 98, 18 }, 10.5f, Palette::keyWhite);

    const char* timeNames[] = { "NOTE", "TIE", "REST" };
    for (int i = 0; i < 3; ++i)
    {
        const float y = 392.0f + 22.0f * (float) i;
        drawLed (g, { 196.0f, y }, 4.5f, (int) shownTime == i);
        paintLabel (g, timeNames[i], { 208, y - 7, 60, 14 }, 11.0f, Palette::ink, Justification::centredLeft);
    }
    paintLabel (g, "TIME", { 176, 474, 98, 14 }, 11.0f);

    // ---- Keyboard -------------------------------------------------------
    const auto kb = Rectangle<float> (kbX, 354.0f, kbW, 206.0f);
    g.setColour (Palette::keyWhite.withAlpha (0.94f));
    g.fillRect (kb.withTrimmedTop (18.0f));
    g.setColour (Palette::keyBlack);
    g.fillRect (kb.withHeight (18.0f));

    for (int b = 1; b < 8; ++b)
    {
        const float x = kbX + (float) b * whiteW;
        g.setColour (Palette::keyBlack);
        g.fillRect (x - 0.75f, 372.0f, 1.5f, 188.0f);
    }
    for (int i = 0; i < 5; ++i)
    {
        const float x = kbX + (float) blackSlots[i] * whiteW;
        g.setColour (Palette::keyBlack);
        g.fillRoundedRectangle (x - 28.0f, 368.0f, 56.0f, 102.0f, 3.0f);
    }
    for (int k = 0; k < 13; ++k)
        paintLabel (g, noteName (k), { keyCentreX (k) - 20.0f, 354.0f, 40.0f, 18.0f }, 11.5f, Palette::keyWhite);

    g.setColour (Palette::ink);
    g.drawRect (kb, 1.5f);

    // ---- Step info display ---------------------------------------------
    {
        g.setColour (Colours::white.withAlpha (0.45f));
        g.fillRoundedRectangle (infoLcd.expanded (2.0f).translated (0.0f, 1.0f), 4.0f);
        g.setColour (Colour (0xff1a1a1a));
        g.fillRoundedRectangle (infoLcd.expanded (2.0f), 4.0f);
        g.setColour (Palette::lcd);
        g.fillRoundedRectangle (infoLcd, 3.0f);

        const auto s = processor.getStep (selectedStep);
        String line1 = "STEP " + String (selectedStep + 1).paddedLeft ('0', 2) + " / " + String (processor.getPatternLength());
        String line2;
        if (s.time == acid::Time::Rest)      line2 = "REST";
        else if (s.time == acid::Time::Tie)  line2 = "TIE";
        else
        {
            line2 = "NOTE " + noteName (s.note);
            if (s.up)   line2 << "  UP";
            if (s.down) line2 << "  DOWN";
        }
        if (s.accent) line2 << "  ACC";
        if (s.slide)  line2 << "  SLIDE";

        paintLabel (g, line1, infoLcd.reduced (10, 6).withHeight (20), 15.0f, Palette::lcdText, Justification::centredLeft);
        paintLabel (g, line2, infoLcd.reduced (10, 6).withTrimmedTop (22), 12.5f, Palette::lcdText.withAlpha (0.8f), Justification::centredLeft);
    }

    // ---- Transpose / accent / slide ------------------------------------
    box ({ 832, 354, 270, 206 });
    paintLabel (g, "TRANSPOSE", { 838, 440, 128, 14 }, 10.5f);
    g.setColour (Palette::ink);
    g.fillRect (842.0f, 455.0f, 120.0f, 1.2f);

    const char* flagNames[] = { "DOWN", "UP", "ACCENT", "SLIDE" };
    const float flagX[] = { 838, 904, 970, 1036 };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (Palette::keyBlack);
        g.fillRect (flagX[i] + 1.0f, 458.0f, 60.0f, 15.0f);
        paintLabel (g, flagNames[i], { flagX[i], 458.0f, 62.0f, 15.0f }, 10.5f, Palette::keyWhite);
    }

    // ---- Back / next ------------------------------------------------------
    box ({ 1104, 354, 72, 206 });
    paintLabel (g, "BACK", { 1104, 360, 72, 14 }, 11.0f);
    paintLabel (g, "NEXT", { 1104, 474, 72, 14 }, 11.0f);
}

//==============================================================================
Acid303Editor::Acid303Editor (Acid303Processor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), panel (p)
{
    setLookAndFeel (&lnf);
    tooltips.setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    panel.onSizeMenu = [this] { showSizeMenu(); };

    const double ratio = (double) MainPanel::designWidth / (double) MainPanel::designHeight;
    setResizable (true, true);
    setResizeLimits (600, 320, 2400, 1280);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (ratio);

    const int w = audioProcessor.editorWidth.load() > 0 ? audioProcessor.editorWidth.load() : 1020;
    setSize (w, roundToInt (w / ratio));
}

Acid303Editor::~Acid303Editor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void Acid303Editor::resized()
{
    const float scale = (float) getWidth() / (float) MainPanel::designWidth;
    panel.setTransform (AffineTransform::scale (scale));
    panel.setDisplayScale (scale);
    audioProcessor.editorWidth.store (getWidth());
}

void Acid303Editor::showSizeMenu()
{
    const float sizes[] = { 0.5f, 0.65f, 0.75f, 0.85f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };
    PopupMenu m;
    m.addSectionHeader ("WINDOW SIZE");
    for (int i = 0; i < (int) std::size (sizes); ++i)
    {
        const int w = roundToInt (MainPanel::designWidth * sizes[i]);
        m.addItem (i + 1, String (roundToInt (sizes[i] * 100.0f)) + " %", true, std::abs (getWidth() - w) < 4);
    }

    SafePointer<Acid303Editor> safe (this);
    m.showMenuAsync (PopupMenu::Options(), [safe, sizes] (int r)
    {
        if (safe == nullptr || r <= 0)
            return;
        const float f = sizes[r - 1];
        safe->setSize (roundToInt (MainPanel::designWidth * f), roundToInt (MainPanel::designHeight * f));
    });
}
