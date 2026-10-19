#include "MetalTexture.h"
#include <vector>
#include <cmath>

namespace acid
{
namespace
{
struct FastRandom
{
    juce::uint32 state;
    explicit FastRandom (juce::uint32 seed) : state (seed * 2654435761u + 1u) {}

    juce::uint32 nextInt() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float next() noexcept { return (float) (nextInt() & 0xffffff) / 16777216.0f; } // 0..1
    float bipolar() noexcept { return next() * 2.0f - 1.0f; }
    float range (float lo, float hi) noexcept { return lo + next() * (hi - lo); }
};

void boxBlur (const std::vector<float>& src, std::vector<float>& dst, int radius)
{
    const int n = (int) src.size();
    const float norm = 1.0f / (float) (2 * radius + 1);
    float acc = 0.0f;

    for (int i = -radius; i <= radius; ++i)
        acc += src[(size_t) juce::jlimit (0, n - 1, i)];

    for (int x = 0; x < n; ++x)
    {
        dst[(size_t) x] = acc * norm;
        acc += src[(size_t) juce::jlimit (0, n - 1, x + radius + 1)];
        acc -= src[(size_t) juce::jlimit (0, n - 1, x - radius)];
    }
}

void drawBrushedBase (juce::Image& img, FastRandom& rnd)
{
    const int w = img.getWidth(), h = img.getHeight();
    const float s = (float) w / 1200.0f;
    const int radius = juce::jmax (3, (int) (22.0f * s));

    std::vector<float> noise ((size_t) w), blurA ((size_t) w), blurB ((size_t) w), longStreak ((size_t) w);

    juce::Image::BitmapData data (img, juce::Image::BitmapData::writeOnly);

    float rowWalk = 0.0f;
    for (int y = 0; y < h; ++y)
    {
        // Slowly wandering row tone: the parallel bands of a brushed sheet.
        rowWalk = rowWalk * 0.92f + rnd.bipolar() * 0.08f;
        const float rowJitter = rnd.bipolar() * 0.012f;

        for (int x = 0; x < w; ++x)
            noise[(size_t) x] = rnd.bipolar();

        boxBlur (noise, blurA, radius);
        boxBlur (blurA, blurB, radius);
        boxBlur (blurB, longStreak, radius * 4);

        const float fy = (float) y / (float) h;
        const float light = 1.05f - 0.10f * fy; // light from above

        for (int x = 0; x < w; ++x)
        {
            const float fx = (float) x / (float) w;
            float v = 0.765f
                    + rowWalk * 0.035f + rowJitter
                    + blurB[(size_t) x] * 0.22f
                    + longStreak[(size_t) x] * 0.35f
                    + noise[(size_t) x] * 0.012f
                    + 0.025f * std::sin (fx * juce::MathConstants<float>::pi); // broad sheen
            v *= light;

            // Aged, slightly yellowed aluminium.
            const auto r = (juce::uint8) juce::jlimit (0, 255, (int) (v * 255.0f * 1.00f));
            const auto g = (juce::uint8) juce::jlimit (0, 255, (int) (v * 255.0f * 0.985f));
            const auto b = (juce::uint8) juce::jlimit (0, 255, (int) (v * 255.0f * 0.935f));
            data.setPixelColour (x, y, juce::Colour (r, g, b));
        }
    }
}

void drawScratches (juce::Graphics& g, FastRandom& rnd, float s)
{
    // Long, faint scratches mostly along the brushing direction.
    for (int i = 0; i < 260; ++i)
    {
        const float x = rnd.range (0.0f, 1200.0f), y = rnd.range (0.0f, 640.0f);
        const bool alongGrain = rnd.next() < 0.7f;
        const float angle = alongGrain ? rnd.bipolar() * 0.12f : rnd.range (0.0f, juce::MathConstants<float>::pi);
        const float len = alongGrain ? rnd.range (30.0f, 260.0f) : rnd.range (6.0f, 70.0f);
        const float bend = rnd.bipolar() * len * 0.08f;

        juce::Path p;
        const juce::Point<float> a (x, y);
        const juce::Point<float> b (x + std::cos (angle) * len, y + std::sin (angle) * len);
        const auto mid = (a + b) * 0.5f + juce::Point<float> (-std::sin (angle), std::cos (angle)) * bend;
        p.startNewSubPath (a * s);
        p.quadraticTo (mid * s, b * s);

        const float thick = rnd.range (0.35f, 1.0f) * s;
        g.setColour (juce::Colours::black.withAlpha (rnd.range (0.04f, 0.11f)));
        g.strokePath (p, juce::PathStrokeType (thick), juce::AffineTransform::translation (0.0f, 0.8f * s));
        g.setColour (juce::Colours::white.withAlpha (rnd.range (0.07f, 0.22f)));
        g.strokePath (p, juce::PathStrokeType (thick));
    }

    // Tiny swirls of micro-scratches (cleaning marks).
    for (int i = 0; i < 18; ++i)
    {
        const juce::Point<float> c (rnd.range (40.0f, 1160.0f), rnd.range (30.0f, 610.0f));
        const float r = rnd.range (8.0f, 40.0f);
        for (int k = 0; k < 10; ++k)
        {
            juce::Path arc;
            const float rr = r * rnd.range (0.4f, 1.0f);
            const float start = rnd.range (0.0f, 6.28f);
            arc.addCentredArc (c.x * s, c.y * s, rr * s, rr * s * rnd.range (0.6f, 1.0f), 0.0f, start, start + rnd.range (0.4f, 1.8f), true);
            g.setColour (juce::Colours::white.withAlpha (rnd.range (0.04f, 0.10f)));
            g.strokePath (arc, juce::PathStrokeType (0.4f * s));
        }
    }
}

void drawWear (juce::Graphics& g, FastRandom& rnd, float s, int w, int h)
{
    const auto grime = juce::Colour (0xff3b3020);

    // Dirt accumulated near the edges.
    const float edge = 55.0f * s;
    auto edgeFill = [&] (juce::Point<float> from, juce::Point<float> to, juce::Rectangle<float> area, float alpha)
    {
        g.setGradientFill (juce::ColourGradient (grime.withAlpha (alpha), from, grime.withAlpha (0.0f), to, false));
        g.fillRect (area);
    };
    const float fw = (float) w, fh = (float) h;
    edgeFill ({ 0, 0 }, { 0, edge }, { 0, 0, fw, edge }, 0.10f);
    edgeFill ({ 0, fh }, { 0, fh - edge * 1.4f }, { 0, fh - edge * 1.4f, fw, edge * 1.4f }, 0.22f);
    edgeFill ({ 0, 0 }, { edge, 0 }, { 0, 0, edge, fh }, 0.16f);
    edgeFill ({ fw, 0 }, { fw - edge, 0 }, { fw - edge, 0, edge, fh }, 0.16f);

    // Soft grime blotches and rubbed (shinier) spots.
    for (int i = 0; i < 70; ++i)
    {
        const juce::Point<float> c (rnd.range (0.0f, 1200.0f) * s, rnd.range (0.0f, 640.0f) * s);
        const float r = rnd.range (15.0f, 110.0f) * s;
        const bool shiny = rnd.next() < 0.3f;
        const auto col = shiny ? juce::Colours::white.withAlpha (rnd.range (0.03f, 0.07f))
                               : grime.withAlpha (rnd.range (0.025f, 0.07f));
        g.setGradientFill (juce::ColourGradient (col, c, col.withAlpha (0.0f), c.translated (r, 0), true));
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f * rnd.range (0.5f, 1.0f)).withCentre (c));
    }

    // Specks, dings and pits.
    for (int i = 0; i < 900; ++i)
    {
        const float x = rnd.range (0.0f, fw), y = rnd.range (0.0f, fh);
        const float d = rnd.range (0.4f, 1.6f) * s;
        g.setColour ((rnd.next() < 0.75f ? juce::Colours::black : juce::Colours::white).withAlpha (rnd.range (0.05f, 0.22f)));
        g.fillEllipse (x, y, d, d);
    }

    // A few small chips where the finish is worn through.
    for (int i = 0; i < 12; ++i)
    {
        const juce::Point<float> c (rnd.range (20.0f, 1180.0f) * s, rnd.range (20.0f, 620.0f) * s);
        juce::Path chip;
        const int pts = 7;
        for (int k = 0; k < pts; ++k)
        {
            const float a = (float) k / (float) pts * juce::MathConstants<float>::twoPi;
            const float r = rnd.range (1.0f, 3.2f) * s;
            const auto p = c + juce::Point<float> (std::cos (a) * r, std::sin (a) * r);
            if (k == 0) chip.startNewSubPath (p); else chip.lineTo (p);
        }
        chip.closeSubPath();
        g.setColour (juce::Colour (0xff6d6a63).withAlpha (0.5f));
        g.fillPath (chip);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.strokePath (chip, juce::PathStrokeType (0.5f * s), juce::AffineTransform::translation (0.0f, 0.6f * s));
    }
}
} // namespace

juce::Image createBrushedMetal (int widthPx, int heightPx, juce::uint32 seed)
{
    widthPx  = juce::jmax (16, widthPx);
    heightPx = juce::jmax (16, heightPx);

    juce::Image img (juce::Image::RGB, widthPx, heightPx, false);
    FastRandom noiseRnd (seed);
    drawBrushedBase (img, noiseRnd);

    // Wear uses its own generator so the layout of scratches is independent
    // of the pixel count (stays identical at every size).
    FastRandom wearRnd (seed ^ 0x9e3779b9u);
    juce::Graphics g (img);
    const float s = (float) widthPx / 1200.0f;
    drawScratches (g, wearRnd, s);
    drawWear (g, wearRnd, s, widthPx, heightPx);
    return img;
}
} // namespace acid
