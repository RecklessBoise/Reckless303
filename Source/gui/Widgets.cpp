#include "gui/Widgets.h"
#include "presets/PresetManager.h"

namespace acid
{
using namespace juce;

//==============================================================================
KeyCap::KeyCap (const String& name, bool withLed, bool darkCap)
    : Button (name), hasLed (withLed), dark (darkCap)
{
}

void KeyCap::setLed (bool on)
{
    if (on != ledOn)
    {
        ledOn = on;
        repaint();
    }
}

void KeyCap::paintButton (Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat();

    if (hasLed)
    {
        drawLed (g, { b.getCentreX(), b.getY() + 9.0f }, 5.0f, ledOn);
        b.removeFromTop (22.0f);
    }

    drawKeyCap (g, b.reduced (3.0f), down, highlighted, dark);
}

//==============================================================================
void SlideSwitch::paintButton (Graphics& g, bool, bool)
{
    const auto b = getLocalBounds().toFloat();
    const auto slot = b.withSizeKeepingCentre (b.getWidth() * 0.92f, b.getHeight() * 0.62f);

    // Recessed slot.
    g.setColour (Colours::white.withAlpha (0.45f));
    g.fillRoundedRectangle (slot.translated (0.0f, 1.0f), 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff050505), slot.getX(), slot.getY(), Colour (0xff2a2a2a), slot.getX(), slot.getBottom(), false));
    g.fillRoundedRectangle (slot, 3.0f);

    // Metal thumb.
    const float tw = slot.getWidth() * 0.5f;
    auto thumb = Rectangle<float> (tw, b.getHeight() * 0.86f).withCentre ({ getToggleState() ? slot.getRight() - tw * 0.5f - 1.0f
                                                                                           : slot.getX() + tw * 0.5f + 1.0f,
                                                                            b.getCentreY() });
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (thumb.translated (1.5f, 2.5f), 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xffeeeeea), thumb.getX(), thumb.getY(), Colour (0xff6f6d68), thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle (thumb, 3.0f);

    for (int i = 1; i < 6; ++i)
    {
        const float x = thumb.getX() + thumb.getWidth() * (float) i / 6.0f;
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawLine (x, thumb.getY() + 4.0f, x, thumb.getBottom() - 4.0f, 1.0f);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawLine (x + 1.0f, thumb.getY() + 4.0f, x + 1.0f, thumb.getBottom() - 4.0f, 1.0f);
    }
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (thumb, 3.0f, 1.0f);
}

//==============================================================================
void StepRow::update (const std::array<Step, maxSteps>& steps, int length, int selected, int playing)
{
    bool changed = length != len || selected != sel || playing != play;
    for (int i = 0; i < maxSteps && ! changed; ++i)
        changed = steps[(size_t) i].pack() != data[(size_t) i].pack();

    if (changed)
    {
        data = steps;
        len = length;
        sel = selected;
        play = playing;
        repaint();
    }
}

Rectangle<float> StepRow::cell (int i) const
{
    const float w = (float) getWidth() / (float) maxSteps;
    return Rectangle<float> ((float) i * w, 0.0f, w, (float) getHeight()).reduced (2.0f, 3.0f);
}

int StepRow::indexAt (Point<float> p) const
{
    return jlimit (0, maxSteps - 1, (int) (p.x / ((float) getWidth() / (float) maxSteps)));
}

void StepRow::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (Palette::keyBlack);
    g.fillRoundedRectangle (b, 4.0f);

    for (int i = 0; i < maxSteps; ++i)
    {
        const auto r = cell (i);
        const auto& s = data[(size_t) i];
        const bool active = i < len;
        const float alpha = active ? 1.0f : 0.28f;

        if (i == sel)
        {
            g.setColour (Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Colours::white.withAlpha (0.85f));
            g.drawRoundedRectangle (r, 3.0f, 1.4f);
        }

        // Step number in the TB-303 orange.
        g.setColour (Palette::orange.withAlpha (alpha));
        g.setFont (panelFont (14.0f));
        g.drawText (String (i + 1), r.withHeight (18.0f).translated (0.0f, 1.0f), Justification::centred);

        drawLed (g, { r.getCentreX(), r.getY() + 27.0f }, 4.0f, active && i == play);

        // Step content.
        String text;
        if (s.time == Time::Rest)      text = CharPointer_UTF8 ("\xc2\xb7");     // middle dot
        else if (s.time == Time::Tie)  text = CharPointer_UTF8 ("\xe2\x80\x94"); // em dash
        else
        {
            text = noteName (s.note);
            if (s.up)   text << String (CharPointer_UTF8 ("\xe2\x86\x91"));
            if (s.down) text << String (CharPointer_UTF8 ("\xe2\x86\x93"));
        }

        g.setColour (Palette::keyWhite.withAlpha (alpha));
        g.setFont (panelFont (12.5f));
        g.drawText (text, Rectangle<float> (r.getX(), r.getY() + 35.0f, r.getWidth(), 15.0f), Justification::centred);

        String flags;
        if (s.accent) flags << "A";
        if (s.slide)  flags << (flags.isEmpty() ? "S" : " S");
        g.setColour (Palette::orange.withAlpha (alpha * 0.9f));
        g.setFont (panelFont (10.5f));
        g.drawText (flags, Rectangle<float> (r.getX(), r.getY() + 50.0f, r.getWidth(), 12.0f), Justification::centred);
    }
}

void StepRow::mouseDown (const MouseEvent& e)
{
    const int idx = indexAt (e.position);
    if (e.mods.isPopupMenu())
    {
        if (onMenu) onMenu (idx);
    }
    else if (onSelect)
    {
        onSelect (idx);
    }
}

//==============================================================================
void PresetBar::Heart::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (3.0f);
    const float w = b.getWidth(), h = b.getHeight();
    Path p;
    p.startNewSubPath (w * 0.5f, h * 0.92f);
    p.cubicTo (w * -0.1f, h * 0.45f, w * 0.12f, h * -0.05f, w * 0.5f, h * 0.26f);
    p.cubicTo (w * 0.88f, h * -0.05f, w * 1.1f, h * 0.45f, w * 0.5f, h * 0.92f);
    p.closeSubPath();
    p.applyTransform (AffineTransform::translation (b.getX(), b.getY()));

    if (filled)
    {
        g.setColour (Palette::ledOn.withAlpha (0.3f));
        g.strokePath (p, PathStrokeType (4.0f));
        g.setColour (Palette::ledOn);
        g.fillPath (p);
    }
    else
    {
        g.setColour (Palette::lcdText.withAlpha (0.55f));
        g.strokePath (p, PathStrokeType (1.4f));
    }
}

PresetBar::PresetBar (PresetManager& pm) : presets (pm)
{
    for (auto* b : { &prev, &next, &favs, &random, &save, &del, &size })
        addAndMakeVisible (b);

    addAndMakeVisible (heart);
    heart.setMouseCursor (MouseCursor::PointingHandCursor);

    prev.onClick   = [this] { presets.previous(); };
    next.onClick   = [this] { presets.next(); };
    favs.onClick   = [this] { presets.setFavoritesOnly (! presets.getFavoritesOnly()); };
    random.onClick = [this] { presets.randomize (PresetManager::RandomMode::All); };
    save.onClick   = [this] { showSaveDialog(); };
    del.onClick    = [this] { confirmDelete(); };
    size.onClick   = [this] { if (onSizeMenu) onSizeMenu(); };
    heart.onClick  = [this] { presets.toggleFavorite (presets.getCurrentIndex()); };

    favs.setTooltip ("Browse liked presets only");
    random.setTooltip ("Generate a random sound and pattern (more options in the preset menu)");
    save.setTooltip ("Save the current sound and pattern as a user preset");
    del.setTooltip ("Delete the current user preset");
    size.setTooltip ("Plugin window size");

    presets.addChangeListener (this);
    refresh();
}

PresetBar::~PresetBar()
{
    presets.removeChangeListener (this);
}

void PresetBar::changeListenerCallback (ChangeBroadcaster*) { refresh(); }

void PresetBar::refresh()
{
    const int cur = presets.getCurrentIndex();
    name = presets.getCurrentName();
    category = presets.getCurrentCategory().toUpperCase();
    counter = (cur >= 0 ? String (cur + 1).paddedLeft ('0', 3) : String ("---")) + " / " + String (presets.getNumPresets());

    heart.filled = presets.isFavorite (cur);
    heart.setVisible (cur >= 0);
    heart.repaint();

    favs.setToggleState (presets.getFavoritesOnly(), dontSendNotification);
    del.setEnabled (presets.isUserPreset (cur));
    repaint();
}

void PresetBar::resized()
{
    auto b = getLocalBounds();
    auto top = b.removeFromTop (50);
    prev.setBounds (top.removeFromLeft (38));
    next.setBounds (top.removeFromRight (38));

    const auto lcd = lcdArea().reduced (42, 0);
    heart.setBounds (lcd.getRight() - 34, lcd.getY() + 12, 26, 24);

    b.removeFromTop (6);
    const int w = b.getWidth() / 6;
    favs.setBounds (b.removeFromLeft (w));
    random.setBounds (b.removeFromLeft (w + w / 2));
    save.setBounds (b.removeFromLeft (w));
    del.setBounds (b.removeFromLeft (w));
    size.setBounds (b);
}

void PresetBar::paint (Graphics& g)
{
    const auto lcd = lcdArea().reduced (42, 0).toFloat();

    // Bezel and screen.
    g.setColour (Colours::white.withAlpha (0.5f));
    g.fillRoundedRectangle (lcd.expanded (2.0f).translated (0.0f, 1.0f), 5.0f);
    g.setColour (Colour (0xff1a1a1a));
    g.fillRoundedRectangle (lcd.expanded (2.0f), 5.0f);
    g.setGradientFill (ColourGradient (Palette::lcd.brighter (0.15f), lcd.getX(), lcd.getY(), Palette::lcd, lcd.getX(), lcd.getBottom(), false));
    g.fillRoundedRectangle (lcd, 4.0f);

    auto text = lcd.reduced (10.0f, 4.0f);
    text.removeFromRight (30.0f);

    // Name with a soft glow.
    const auto nameArea = text.removeFromTop (27.0f);
    g.setFont (panelFont (19.0f));
    g.setColour (Palette::lcdText.withAlpha (0.25f));
    g.drawText (name, nameArea.translated (0.0f, 0.6f), Justification::centredLeft, true);
    g.setColour (Palette::lcdText);
    g.drawText (name, nameArea, Justification::centredLeft, true);

    g.setFont (panelFont (11.0f, false));
    g.setColour (Palette::lcdText.withAlpha (0.7f));
    g.drawText (category + (presets.getFavoritesOnly() ? "   [LIKED ONLY]" : ""), text, Justification::centredLeft, true);
    g.drawText (counter, text, Justification::centredRight, true);

    // Glass reflection.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.08f), lcd.getX(), lcd.getY(), Colours::white.withAlpha (0.0f), lcd.getX(), lcd.getCentreY(), false));
    g.fillRoundedRectangle (lcd.withHeight (lcd.getHeight() * 0.5f), 4.0f);
}

void PresetBar::mouseDown (const MouseEvent& e)
{
    if (lcdArea().contains (e.getPosition()))
        showBrowser();
}

void PresetBar::showBrowser()
{
    PopupMenu menu;
    const int cur = presets.getCurrentIndex();

    auto label = [this] (int i)
    {
        return (presets.isFavorite (i) ? String (CharPointer_UTF8 ("\xe2\x99\xa5 ")) : String()) + presets.getPresetName (i);
    };

    menu.addSectionHeader ("FACTORY");
    for (const auto& cat : presets.getCategories())
    {
        PopupMenu sub;
        for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
            if (presets.getPreset (i).category == cat)
                sub.addItem (i + 1, label (i), true, i == cur);
        menu.addSubMenu (cat, sub);
    }

    PopupMenu userMenu, likedMenu;
    for (int i = presets.getNumFactoryPresets(); i < presets.getNumPresets(); ++i)
        userMenu.addItem (i + 1, label (i), true, i == cur);
    for (int i = 0; i < presets.getNumPresets(); ++i)
        if (presets.isFavorite (i))
            likedMenu.addItem (i + 1, presets.getPresetName (i), true, i == cur);

    menu.addSectionHeader ("MY SOUNDS");
    menu.addSubMenu ("User", userMenu, userMenu.getNumItems() > 0);
    menu.addSubMenu (String (CharPointer_UTF8 ("\xe2\x99\xa5 Liked")), likedMenu, likedMenu.getNumItems() > 0);

    menu.addSectionHeader ("RANDOM");
    menu.addItem (10001, "Random sound + pattern");
    menu.addItem (10002, "Random sound only (keep pattern)");
    menu.addItem (10003, "Random pattern only (keep sound)");

    menu.addSeparator();
    menu.addItem (10010, "Open user preset folder");
    menu.addItem (10011, "Rescan user presets");

    SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth() / 2),
                        [safe] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            auto& pm = safe->presets;
                            if (result <= pm.getNumPresets())             pm.loadPreset (result - 1);
                            else if (result == 10001)                     pm.randomize (PresetManager::RandomMode::All);
                            else if (result == 10002)                     pm.randomize (PresetManager::RandomMode::Sound);
                            else if (result == 10003)                     pm.randomize (PresetManager::RandomMode::Pattern);
                            else if (result == 10010)
                            {
                                PresetManager::getUserPresetDirectory().createDirectory();
                                PresetManager::getUserPresetDirectory().startAsProcess();
                            }
                            else if (result == 10011)
                            {
                                pm.rescanUserPresets();
                                pm.restoreCurrent (pm.getCurrentName());
                            }
                        });
}

void PresetBar::showSaveDialog()
{
    auto* aw = new AlertWindow ("Save preset", "Preset name:", MessageBoxIconType::NoIcon, this);
    aw->setLookAndFeel (&getLookAndFeel());
    aw->addTextEditor ("name", presets.getCurrentName());
    aw->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    aw->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    SafePointer<PresetBar> safe (this);
    aw->enterModalState (true, ModalCallbackFunction::create ([safe, aw] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const auto newName = aw->getTextEditorContents ("name").trim();
        if (newName.isEmpty())
            return;

        auto& pm = safe->presets;
        if (pm.findPreset (newName, true) < 0)
        {
            pm.saveUserPreset (newName);
            return;
        }

        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Overwrite preset?",
                                      "A user preset called \"" + newName + "\" already exists.",
                                      "Overwrite", "Cancel", safe.getComponent(),
                                      ModalCallbackFunction::create ([safe, newName] (int r)
                                      {
                                          if (safe != nullptr && r == 1)
                                              safe->presets.saveUserPreset (newName);
                                      }));
    }), true);
}

void PresetBar::confirmDelete()
{
    const int cur = presets.getCurrentIndex();
    if (! presets.isUserPreset (cur))
        return;

    SafePointer<PresetBar> safe (this);
    AlertWindow::showOkCancelBox (MessageBoxIconType::WarningIcon, "Delete preset?",
                                  "\"" + presets.getPresetName (cur) + "\" will be moved to the trash.",
                                  "Delete", "Cancel", this,
                                  ModalCallbackFunction::create ([safe, cur] (int r)
                                  {
                                      if (safe != nullptr && r == 1)
                                          safe->presets.deleteUserPreset (cur);
                                  }));
}
} // namespace acid
