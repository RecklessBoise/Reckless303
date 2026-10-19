#include "PresetManager.h"
#include "PluginProcessor.h"

namespace
{
struct Def
{
    const char* name;
    const char* category;
    float cutoff, resonance, envMod, decay, accent;
    int square;
    float drive;
    const char* pattern;
    float tuning = 0.0f;
};

// Hand-programmed patterns. Notation: C..B, C' = high C, '-' rest, '_' tie,
// flags u (up) d (down) a (accent) s (slide).
const Def kDefs[] = {
    // ---- Classic --------------------------------------------------------
    { "Init 303",          "Classic", 0.45f, 0.70f, 0.55f, 0.40f, 0.60f, 0, 0.00f, "C C C C C C C C C C C C C C C C" },
    { "Acid Tracker",      "Classic", 0.35f, 0.85f, 0.60f, 0.45f, 0.70f, 0, 0.00f, "C Ca C'u C - Cs D# Ca C'us C G# Ca - C F#a Cs" },
    { "First Squelch",     "Classic", 0.30f, 0.90f, 0.70f, 0.35f, 0.80f, 0, 0.00f, "C C C'ua C Cs D#a C - C C'u Ca G#d C A#s C' C" },
    { "Chicago 87",        "Classic", 0.40f, 0.80f, 0.50f, 0.50f, 0.65f, 0, 0.00f, "C - Ca C - C'u Cs C D# - Fa C - G Cas C'" },
    { "Warehouse Loop",    "Classic", 0.38f, 0.78f, 0.62f, 0.42f, 0.70f, 0, 0.05f, "Ca C C'us C D#s F C Ca C C'us C G#s G C Ca _" },
    { "Phuture Shock",     "Classic", 0.25f, 0.95f, 0.75f, 0.30f, 0.85f, 0, 0.00f, "C Cs C'ua C - C Ca D#s E F C' Cs Ca - C G" },
    { "Box of Tricks",     "Classic", 0.42f, 0.75f, 0.45f, 0.55f, 0.55f, 1, 0.00f, "C C D# C F C G# C A# C G C F C D# Ca" },
    { "Silver Box",        "Classic", 0.50f, 0.65f, 0.40f, 0.60f, 0.50f, 0, 0.00f, "Ca - C D# - F - G Ca - C'u - A#s G F D#" },
    { "Bass Line Day",     "Classic", 0.33f, 0.70f, 0.50f, 0.40f, 0.60f, 1, 0.00f, "C _ D# C G _ F D# C _ D# F G _ A# G" },
    { "Jack Line",         "Classic", 0.36f, 0.82f, 0.58f, 0.38f, 0.75f, 0, 0.00f, "C C'u C C D#a C F C Gs G#a G F D# C C'us C" },
    { "Detroit Machine",   "Classic", 0.45f, 0.72f, 0.48f, 0.50f, 0.55f, 1, 0.10f, "C - G - C'u - G - A# - G - F - D# -" },
    { "Hardfloor Walk",    "Classic", 0.32f, 0.88f, 0.66f, 0.36f, 0.78f, 0, 0.15f, "C Cs C Ca C'us C C Ca D#s D# C Ca F#s F C Ca" },
    { "Rubber Groove",     "Classic", 0.40f, 0.60f, 0.55f, 0.65f, 0.50f, 0, 0.00f, "C _ C'us C - D#s F - G _ F D#s C - A#d C" },
    { "Kitchen Acid",      "Classic", 0.44f, 0.84f, 0.52f, 0.46f, 0.66f, 0, 0.00f, "Ca C C C'u Ca C D# C Ca C F C Ca G C'u C" },

    // ---- Squelch --------------------------------------------------------
    { "Rubber Acid",       "Squelch", 0.20f, 0.96f, 0.85f, 0.30f, 0.90f, 0, 0.20f, "Cs C'ua Cs Ca D#s C'ua Cs Ca Fs C'ua Cs Ca Gs C'ua Cs Ca" },
    { "Wet Snake",         "Squelch", 0.18f, 0.97f, 0.90f, 0.45f, 0.95f, 0, 0.00f, "Cs D#s Fs Gs A#s C'us A#s Gs Fs D#s Cs C'ua - Ca C Cs" },
    { "Liquid Mercury",    "Squelch", 0.22f, 0.92f, 0.80f, 0.55f, 0.85f, 0, 0.10f, "C Cs D# Ds C Cs C'u C's A# Gs F D#s C Cs - Ca" },
    { "Gurgle",            "Squelch", 0.15f, 0.98f, 0.95f, 0.25f, 1.00f, 1, 0.10f, "Ca C Ca C Ca Cs C'ua C Ca C Ca Cs D#a C Ca C" },
    { "Squelch Factory",   "Squelch", 0.25f, 0.93f, 0.78f, 0.33f, 0.88f, 0, 0.25f, "C'ua C C'ua Cs C'ua C C'ua Cs D#ua C D#ua Cs G#a C Ga Cs" },
    { "Talking Filter",    "Squelch", 0.28f, 0.95f, 0.70f, 0.70f, 0.80f, 0, 0.00f, "C _ _ Cs D#a _ Cs C C _ Fa Gs _ C - Ca" },
    { "Bubble Bath",       "Squelch", 0.30f, 0.94f, 0.65f, 0.25f, 0.70f, 1, 0.00f, "C C'u C C'u Ca C'u C C'u Ds Du C C'u Ca C'u Gd C'u" },
    { "Mouth Harp",        "Squelch", 0.12f, 0.99f, 0.92f, 0.40f, 0.95f, 0, 0.05f, "Ca Cs C Ca Cs C Ca Cs C Ca Cs C Ca Cs C Ca" },
    { "Acid Rain",         "Squelch", 0.26f, 0.91f, 0.82f, 0.50f, 0.85f, 0, 0.30f, "C - C'us C D#a - Fs G G#a - Gs F D#s C - Ca" },
    { "Screaming 303",     "Squelch", 0.35f, 0.97f, 0.88f, 0.28f, 1.00f, 0, 0.55f, "C'ua C'u Cs C'ua D#ua Cs C'ua C'u Cs Fua D#ua Cs C'ua A#a Gs C" },

    // ---- Bass -----------------------------------------------------------
    { "Deep Rubber",       "Bass",    0.20f, 0.45f, 0.35f, 0.55f, 0.40f, 0, 0.00f, "Cd _ - Cd - - Cd _ D#d _ - Cd - Gd A#d _" },
    { "Round Square",      "Bass",    0.25f, 0.35f, 0.30f, 0.60f, 0.35f, 1, 0.00f, "C _ - C - C _ - D# _ - D# - C _ -" },
    { "Sub Pulse",         "Bass",    0.15f, 0.30f, 0.25f, 0.70f, 0.30f, 1, 0.00f, "Cd Cd - Cd Cd - Cd Cd D#d D#d - D#d Fd Fd - Gd" },
    { "Minimal Bass",      "Bass",    0.30f, 0.50f, 0.30f, 0.35f, 0.50f, 0, 0.00f, "C - - C - - C - D# - - C - - G -" },
    { "Dub Wobble",        "Bass",    0.22f, 0.60f, 0.40f, 0.80f, 0.45f, 1, 0.00f, "Cd _ _ _ Cs D# _ _ Fd _ _ _ G#ds G _ _" },
    { "Electro Funk",      "Bass",    0.38f, 0.55f, 0.45f, 0.30f, 0.60f, 0, 0.10f, "C - C'ua - C C - D#a - F - Ca C'u - A#s G" },
    { "Disco Octaves",     "Bass",    0.40f, 0.50f, 0.40f, 0.30f, 0.55f, 0, 0.00f, "C C'u C C'u C C'u C C'u F Fu F Fu G Gu G Gu" },
    { "Tight Square",      "Bass",    0.35f, 0.40f, 0.50f, 0.15f, 0.60f, 1, 0.00f, "Ca C C - Ca C D# - Ca C F - Ca C G -" },
    { "Warm Thump",        "Bass",    0.18f, 0.40f, 0.45f, 0.45f, 0.65f, 0, 0.15f, "Ca - - - Ca - - C Ca - - - D#a - Cs C" },
    { "Mono Pluck",        "Bass",    0.32f, 0.62f, 0.55f, 0.12f, 0.50f, 0, 0.00f, "C D# G C' G D# C - Cd D# G A# G D# Cd -" },

    // ---- Lead -----------------------------------------------------------
    { "Lead Riser",        "Lead",    0.55f, 0.80f, 0.55f, 0.50f, 0.70f, 0, 0.30f, "C' C' D#u C' Fu C' Gu C' A#u C' Gu C' Fu C' D#u C'", 12.0f },
    { "Square Melody",     "Lead",    0.60f, 0.60f, 0.40f, 0.60f, 0.60f, 1, 0.20f, "C D# F G A# G F D# C' A# G F D# F G _", 12.0f },
    { "Hoover Ish",        "Lead",    0.58f, 0.85f, 0.50f, 0.75f, 0.70f, 0, 0.60f, "C _ _ Cs C'u _ _ C'us A# _ Gs G _ F D#s C", 12.0f },
    { "Arp Machine",       "Lead",    0.52f, 0.75f, 0.60f, 0.30f, 0.65f, 0, 0.10f, "C D# G C'u C D# G C'u D# G A# D#u D# G A# D#u" },
    { "Sharp Pluck",       "Lead",    0.62f, 0.70f, 0.65f, 0.10f, 0.75f, 1, 0.25f, "C'a G D# C G D# C' G Ca G D# C' A# G F D#", 12.0f },
    { "Siren Lead",        "Lead",    0.65f, 0.88f, 0.45f, 0.85f, 0.80f, 0, 0.40f, "Cs C'us Cs C'us Cs C'us Cs C'us Cs C'us Cs C'us Cs C'us Cs C'u", 12.0f },

    // ---- Dark -----------------------------------------------------------
    { "Phrygian Night",    "Dark",    0.24f, 0.86f, 0.60f, 0.45f, 0.75f, 0, 0.20f, "C C#s C Ca - C# C Cs F D#a C# C - Ca G#ds G" },
    { "Basement Acid",     "Dark",    0.20f, 0.90f, 0.70f, 0.38f, 0.80f, 1, 0.35f, "Cd Cd C# Cda - Cd Gd G#ds Cd Cd C#a Cd - Cd F#d Cs" },
    { "Black Ice",         "Dark",    0.16f, 0.93f, 0.75f, 0.50f, 0.85f, 0, 0.45f, "C _ C# _ C _ F# _ G _ F# _ C# _ C Ca" },
    { "Toxic Waste",       "Dark",    0.28f, 0.95f, 0.80f, 0.32f, 0.90f, 0, 0.70f, "Ca C# Cs C'ua C# C Ca F#s G Ca C# Cs C'ua G# G F#s" },
    { "Industrial Grind",  "Dark",    0.34f, 0.80f, 0.50f, 0.25f, 0.70f, 1, 0.85f, "Cd Cd Cd Cda Cd Cd C#da Cd Cd Cd Cda Cd F#d Cd Gda Cd" },
    { "Haunted Box",       "Dark",    0.21f, 0.88f, 0.66f, 0.65f, 0.70f, 0, 0.15f, "C - - C#s D - - C# C - F# - G - F#s C#" },
    { "Corroded",          "Dark",    0.27f, 0.92f, 0.72f, 0.40f, 0.82f, 0, 0.60f, "C Cs C'ua C - C# Ca D# C - Cs F#a G F# C# Cs" },

    // ---- Distorted -----------------------------------------------------
    { "Fuzz Acid",         "Distorted", 0.30f, 0.85f, 0.65f, 0.40f, 0.80f, 0, 0.80f, "C Ca C'us C D# Ca C - C C'ua Cs G# G Ca F Cs" },
    { "Overdrive Monster", "Distorted", 0.38f, 0.90f, 0.70f, 0.35f, 0.90f, 0, 0.95f, "Ca C Ca C'us C Ca D#s C Ca C F#a G Ca C'us C Ca" },
    { "Hard Acid 140",     "Distorted", 0.33f, 0.94f, 0.78f, 0.30f, 0.95f, 0, 0.75f, "C C'ua C C D#ua C C C'ua C G#a C Gs C'ua C F#a Cs" },
    { "Saturated Square",  "Distorted", 0.36f, 0.78f, 0.60f, 0.45f, 0.70f, 1, 0.65f, "C C D# C F C G C A# C G C F C D# C" },
    { "Broken Speaker",    "Distorted", 0.42f, 0.88f, 0.55f, 0.50f, 0.75f, 0, 1.00f, "Ca - Ca - C'ua - Ca Cs D#a - Ca - Ga - Fa D#s" },
};

PresetData toPreset (const Def& d)
{
    PresetData p;
    p.name     = d.name;
    p.category = d.category;
    p.pattern  = d.pattern;
    p.params   = {
        { ParamID::tuning, d.tuning },     { ParamID::cutoff, d.cutoff }, { ParamID::resonance, d.resonance },
        { ParamID::envMod, d.envMod },     { ParamID::decay, d.decay },   { ParamID::accent, d.accent },
        { ParamID::waveform, (float) d.square }, { ParamID::drive, d.drive },
    };
    return p;
}

// Deterministic "generated" bank: the same seed always gives the same presets,
// so the bank is identical on every machine and host session.
void addGenerated (std::vector<PresetData>& out, int count)
{
    static const char* const adjectives[] = { "Acid", "Rusty", "Chrome", "Liquid", "Toxic", "Neon", "Molten", "Static",
                                              "Hollow", "Bent", "Frozen", "Burnt", "Electric", "Feral", "Lunar", "Vapor",
                                              "Wired", "Sour", "Plastic", "Nervous" };
    static const char* const nouns[] = { "Worm", "Engine", "Circuit", "Spiral", "Reactor", "Serpent", "Pulse", "Signal",
                                         "Tunnel", "Machine", "Fever", "Drift", "Voltage", "Mirage", "Orbit", "Riddle",
                                         "Furnace", "Gremlin", "Lizard", "Siren" };
    static const char* const cats[] = { "Generated Acid", "Generated Bass", "Generated Dark", "Generated Lead" };

    juce::Random rng (303303);
    juce::StringArray used;

    while (used.size() < count)
    {
        const juce::String name = juce::String (adjectives[rng.nextInt (20)]) + " " + nouns[rng.nextInt (20)];
        if (used.contains (name))
            continue;
        used.add (name);

        const int kind = rng.nextInt (4);
        auto r = [&rng] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };

        PresetData p;
        p.name     = name;
        p.category = cats[kind];

        float cutoff = r (0.15f, 0.6f), reso = r (0.6f, 0.97f), env = r (0.4f, 0.9f), dec = r (0.2f, 0.6f);
        float acc = r (0.5f, 1.0f), drive = rng.nextFloat() < 0.5f ? 0.0f : r (0.1f, 0.6f), tune = 0.0f;

        if (kind == 1) { cutoff = r (0.1f, 0.35f); reso = r (0.3f, 0.65f); env = r (0.2f, 0.5f); dec = r (0.3f, 0.8f); drive = r (0.0f, 0.2f); }
        if (kind == 2) { cutoff = r (0.1f, 0.3f); drive = r (0.3f, 0.9f); }
        if (kind == 3) { cutoff = r (0.45f, 0.7f); tune = 12.0f; }

        p.params = {
            { ParamID::tuning, tune },     { ParamID::cutoff, cutoff }, { ParamID::resonance, reso },
            { ParamID::envMod, env },      { ParamID::decay, dec },     { ParamID::accent, acc },
            { ParamID::waveform, rng.nextFloat() < 0.65f ? 0.0f : 1.0f }, { ParamID::drive, drive },
        };
        p.pattern = acid::Pattern::random (rng).toString();
        out.push_back (std::move (p));
    }
}
} // namespace

std::vector<PresetData> makeFactoryPresets()
{
    std::vector<PresetData> presets;
    for (const auto& d : kDefs)
        presets.push_back (toPreset (d));

    addGenerated (presets, 100);
    return presets;
}
