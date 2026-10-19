#include "LookAndFeel303.h"

namespace acid
{
using namespace juce;

namespace
{
constexpr float pi = MathConstants<float>::pi;
constexpr float lightAngle = -pi * 0.25f; // light from the top-left

Point<float> polar (Point<float> c, float angle, float r)
{
    return c + Point<float> (std::sin (angle) * r, -std::cos (angle) * r);
}

Colour grey (float level, float warmth = 0.0f)
{
    level = jlimit (0.0f, 1.0f, level);
    return Colour::fromFloatRGBA (level, level * (1.0f - warmth * 0.3f), level * (1.0f - warmth), 1.0f);
}

// Anisotropic "conic" shading of turned aluminium: two broad highlights
// opposite each other, fixed relative to the light.
void fillConic (Graphics& g, Point<float> c, float radius, float lo, float hi)
{
    constexpr int segments = 96;
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = (float) i / segments * 2.0f * pi;
        const float a1 = (float) (i + 1) / segments * 2.0f * pi + 0.02f;
        const float am = 0.5f * (a0 + a1);

        float l = 0.5f + 0.5f * std::cos (2.0f * (am - lightAngle));
        l = std::pow (l, 1.5f);
        l = l * 0.8f + 0.2f * (0.5f + 0.5f * std::cos (am - lightAngle)); // one side brighter

        Path p;
        p.addPieSegment (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, a0, a1, 0.0f);
        g.setColour (grey (lo + (hi - lo) * l, 0.04f));
        g.fillPath (p);
    }
}

void dropShadow (Graphics& g, Point<float> c, float r, float strength)
{
    const auto sc = c.translated (r * 0.12f, r * 0.24f);
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (strength), sc, Colours::black.withAlpha (0.0f),
                                       sc.translated (r * 1.32f, 0.0f), true));
    g.fillEllipse (Rectangle<float> (r * 2.64f, r * 2.64f).withCentre (sc));
}
} // namespace

Font panelFont (float height, bool bold)
{
    return Font (FontOptions ("Helvetica Neue", height, bold ? Font::bold : Font::plain));
}

Font logoFont (float height)
{
    return Font (FontOptions ("Avenir Next", height, Font::bold));
}

void drawMetalKnob (Graphics& g, Point<float> c, float r, float angle)
{
    dropShadow (g, c, r, 0.55f);

    // Black flange with grip ridges that turn with the knob.
    const auto skirt = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xff4c4c4c), c.x, c.y - r, Colour (0xff070707), c.x, c.y + r, false));
    g.fillEllipse (skirt);

    for (int i = 0; i < 44; ++i)
    {
        const float a = angle + (float) i / 44.0f * 2.0f * pi;
        const float lit = jmax (0.0f, std::cos (a - lightAngle));
        g.setColour (Colours::white.withAlpha (0.05f + 0.22f * lit));
        g.drawLine (Line<float> (polar (c, a, r * 0.80f), polar (c, a, r * 0.97f)), r * 0.035f);
    }

    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.45f), c.x, c.y - r, Colours::white.withAlpha (0.0f), c.x, c.y + r * 0.2f, false));
    g.drawEllipse (skirt.reduced (r * 0.01f), r * 0.03f);

    // Cylinder side of the metal cap.
    const float sideR = r * 0.78f;
    dropShadow (g, c, sideR, 0.35f);
    fillConic (g, c, sideR, 0.32f, 0.82f);

    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.0f), c.x, c.y - sideR, Colours::black.withAlpha (0.45f), c.x, c.y + sideR, false));
    g.fillEllipse (Rectangle<float> (sideR * 2.0f, sideR * 2.0f).withCentre (c));

    // Top face, slightly raised.
    const auto capC = c.translated (-r * 0.015f, -r * 0.05f);
    const float capR = r * 0.64f;
    const auto cap = Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (capC);
    fillConic (g, capC, capR, 0.50f, 0.98f);

    // Machining rings.
    for (int k = 1; k < 9; ++k)
    {
        const float rr = capR * (float) k / 9.0f;
        g.setColour ((k % 2 ? Colours::black : Colours::white).withAlpha (k % 2 ? 0.035f : 0.06f));
        g.drawEllipse (Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (capC), jmax (0.5f, r * 0.012f));
    }

    // Specular hot-spot.
    const auto spec = capC.translated (-capR * 0.35f, -capR * 0.4f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.55f), spec, Colours::white.withAlpha (0.0f), spec.translated (capR * 0.75f, 0.0f), true));
    g.fillEllipse (cap);

    // Bevel: bright top edge, dark lower edge.
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.9f), capC.x, capC.y - capR, Colours::black.withAlpha (0.55f), capC.x, capC.y + capR, false));
    g.drawEllipse (cap, jmax (0.8f, r * 0.035f));

    // Engraved pointer.
    const auto p0 = polar (capC, angle, capR * 0.18f);
    const auto p1 = polar (capC, angle, capR * 0.92f);
    const auto off = Point<float> (std::cos (angle), std::sin (angle)) * (r * 0.025f);
    g.setColour (Colours::white.withAlpha (0.8f));
    g.drawLine (Line<float> (p0 + off, p1 + off), r * 0.06f);
    g.setColour (Colour (0xff141414));
    g.drawLine (Line<float> (p0, p1), r * 0.085f);
}

void drawBarKnob (Graphics& g, Point<float> c, float r, float angle)
{
    dropShadow (g, c, r, 0.6f);

    const auto body = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (Colour (0xff474747), c.x - r * 0.6f, c.y - r * 0.8f, Colour (0xff050505), c.x + r * 0.6f, c.y + r, false));
    g.fillEllipse (body);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.4f), c.x, c.y - r, Colours::white.withAlpha (0.0f), c.x, c.y, false));
    g.drawEllipse (body.reduced (r * 0.02f), r * 0.04f);

    // Raised metal bar.
    const auto toWorld = AffineTransform::rotation (angle).translated (c);
    Path bar;
    bar.addRoundedRectangle (-r * 0.25f, -r * 0.94f, r * 0.5f, r * 1.88f, r * 0.12f);

    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (bar, toWorld.translated (r * 0.06f, r * 0.12f));

    g.setGradientFill (ColourGradient (grey (0.92f), c.x - r, c.y - r, grey (0.42f), c.x + r, c.y + r, false));
    g.fillPath (bar, toWorld);

    Path face;
    face.addRoundedRectangle (-r * 0.16f, -r * 0.84f, r * 0.32f, r * 1.68f, r * 0.08f);
    g.setGradientFill (ColourGradient (grey (0.98f), c.x - r * 0.5f, c.y - r, grey (0.62f), c.x + r * 0.5f, c.y + r, false));
    g.fillPath (face, toWorld);

    g.setColour (Colours::black.withAlpha (0.35f));
    g.strokePath (bar, PathStrokeType (r * 0.025f), toWorld);

    // Pointer line at the tip.
    g.setColour (Colour (0xff111111));
    g.drawLine (Line<float> (polar (c, angle, r * 0.45f), polar (c, angle, r * 0.86f)), r * 0.07f);
}

void drawLed (Graphics& g, Point<float> c, float rad, bool on)
{
    if (on)
    {
        g.setGradientFill (ColourGradient (Palette::ledOn.withAlpha (0.6f), c, Palette::ledOn.withAlpha (0.0f), c.translated (rad * 3.0f, 0.0f), true));
        g.fillEllipse (Rectangle<float> (rad * 6.0f, rad * 6.0f).withCentre (c));
    }

    // Bezel.
    g.setColour (Colour (0xff121212));
    g.fillEllipse (Rectangle<float> (rad * 2.5f, rad * 2.5f).withCentre (c));

    const auto inner = on ? Colour (0xffffd9c8) : Colour (0xff7a2a20);
    const auto mid   = on ? Palette::ledOn : Palette::ledOff;
    const auto outer = on ? Colour (0xff9a1608) : Colour (0xff1c0503);
    ColourGradient grad (inner, c.translated (-rad * 0.25f, -rad * 0.3f), outer, c.translated (rad, rad * 0.4f), true);
    grad.addColour (0.45, mid);
    g.setGradientFill (grad);
    g.fillEllipse (Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c));

    g.setColour (Colours::white.withAlpha (on ? 0.7f : 0.35f));
    g.fillEllipse (Rectangle<float> (rad * 0.7f, rad * 0.5f).withCentre (c.translated (-rad * 0.35f, -rad * 0.45f)));
}

void drawKeyCap (Graphics& g, Rectangle<float> r, bool down, bool highlighted, bool dark)
{
    const float corner = jmin (r.getWidth(), r.getHeight()) * 0.09f;

    // Recess in the panel.
    g.setColour (Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.expanded (1.5f), corner + 1.5f);

    if (! down)
    {
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0.0f, r.getHeight() * 0.05f).expanded (0.5f), corner);
    }

    if (down)
        r = r.translated (0.0f, 1.2f).reduced (0.5f);

    const auto topCol = dark ? Colour (0xff4a4a4a) : Colour (0xffbdbab4);
    const auto botCol = dark ? Colour (0xff121212) : Colour (0xff66635e);
    g.setGradientFill (ColourGradient (topCol, r.getX(), r.getY(), botCol, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);

    // Slightly concave top face.
    auto face = r.reduced (r.getWidth() * 0.11f, r.getHeight() * 0.13f).translated (0.0f, -r.getHeight() * 0.035f);
    const auto faceTop = dark ? Colour (0xff303030) : Colour (0xff9d9a94);
    const auto faceMid = dark ? Colour (0xff3d3d3d) : Colour (0xffd4d1ca);
    const auto faceBot = dark ? Colour (0xff222222) : Colour (0xffb2afa8);
    ColourGradient fg (faceTop, face.getX(), face.getY(), faceBot, face.getX(), face.getBottom(), false);
    fg.addColour (0.55, faceMid);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (face, corner * 0.7f);

    g.setColour (Colours::white.withAlpha (dark ? 0.12f : 0.45f));
    g.drawLine (face.getX() + corner, face.getY() + 0.5f, face.getRight() - corner, face.getY() + 0.5f, 1.0f);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (face, corner * 0.7f, 0.7f);

    g.setColour (Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, corner, 1.0f);

    if (highlighted)
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (r, corner);
    }
    if (down)
    {
        g.setColour (Colours::black.withAlpha (0.12f));
        g.fillRoundedRectangle (r, corner);
    }
}

void drawScrew (Graphics& g, Point<float> c, float rad, float angle)
{
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (Rectangle<float> (rad * 2.3f, rad * 2.3f).withCentre (c.translated (0.0f, rad * 0.15f)));

    const auto head = Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (grey (0.92f), c.x - rad, c.y - rad, grey (0.38f), c.x + rad, c.y + rad, false));
    g.fillEllipse (head);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.drawEllipse (head, 0.8f);

    for (float a : { angle, angle + pi * 0.5f })
    {
        const auto p0 = polar (c, a, rad * 0.72f), p1 = polar (c, a + pi, rad * 0.72f);
        g.setColour (Colours::white.withAlpha (0.5f));
        g.drawLine (Line<float> (p0, p1), rad * 0.34f);
        g.setColour (Colour (0xff1e1e1e));
        g.drawLine (Line<float> (p0.translated (-0.4f, -0.5f), p1.translated (-0.4f, -0.5f)), rad * 0.26f);
    }
}

//==============================================================================
LookAndFeel303::LookAndFeel303()
{
    setColour (PopupMenu::backgroundColourId, Colour (0xff1d1d1c));
    setColour (PopupMenu::textColourId, Colour (0xffe8e4da));
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::orange);
    setColour (PopupMenu::highlightedTextColourId, Colours::white);
    setColour (PopupMenu::headerTextColourId, Palette::lcdText);
    setColour (AlertWindow::backgroundColourId, Colour (0xff2a2a28));
    setColour (AlertWindow::textColourId, Colour (0xffe8e4da));
    setColour (AlertWindow::outlineColourId, Palette::orange);
    setColour (TextEditor::backgroundColourId, Palette::lcd);
    setColour (TextEditor::textColourId, Palette::lcdText);
    setColour (TextEditor::outlineColourId, Colour (0xff555550));
    setColour (TextEditor::focusedOutlineColourId, Palette::orange);
    setColour (TextButton::buttonColourId, Colour (0xffbdbab4));
    setColour (TextButton::textColourOffId, Palette::ink);
    setColour (TextButton::textColourOnId, Palette::orange);
    setColour (TooltipWindow::backgroundColourId, Colour (0xff1d1d1c));
    setColour (TooltipWindow::textColourId, Colour (0xffe8e4da));
}

void LookAndFeel303::drawRotarySlider (Graphics& g, int x, int y, int width, int height, float pos,
                                       float startAngle, float endAngle, Slider& slider)
{
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto c = bounds.getCentre();
    const float half = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    const int ticks = (int) slider.getProperties().getWithDefault ("ticks", 11);
    g.setColour (Palette::ink);
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (float) i / (float) jmax (1, ticks - 1) * (endAngle - startAngle);
        const bool major = (i == 0 || i == ticks - 1 || i == ticks / 2);
        g.drawLine (Line<float> (polar (c, a, half * 0.86f), polar (c, a, half * (major ? 1.0f : 0.95f))), half * (major ? 0.045f : 0.03f));
    }

    if (slider.getProperties()["bar"])
        drawBarKnob (g, c, half * 0.74f, angle);
    else
        drawMetalKnob (g, c, half * 0.76f, angle);
}

void LookAndFeel303::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    drawKeyCap (g, b.getLocalBounds().toFloat().reduced (2.0f), down, highlighted, (bool) b.getProperties()["dark"]);
}

void LookAndFeel303::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    const bool dark = b.getProperties()["dark"];
    auto col = b.getToggleState() ? Palette::orange : (dark ? Colour (0xffe8e4da) : Palette::ink);
    if (! b.isEnabled())
        col = col.withAlpha (0.35f);

    g.setColour (col);
    g.setFont (panelFont (jmin (13.0f, (float) b.getHeight() * 0.42f)));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2).translated (0, down ? 1 : -1), Justification::centred, 1);
}
} // namespace acid
