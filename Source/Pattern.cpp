#include "Pattern.h"

namespace acid
{
static const char* const kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C'" };

juce::String noteName (int note)
{
    return kNoteNames[juce::jlimit (0, 12, note)];
}

juce::String Pattern::toString() const
{
    juce::StringArray tokens;

    for (int i = 0; i < length; ++i)
    {
        const auto& s = steps[(size_t) i];
        juce::String t;

        if (s.time == Time::Rest)      t = "-";
        else if (s.time == Time::Tie)  t = "_";
        else                           t = noteName (s.note);

        if (s.up)     t << "u";
        if (s.down)   t << "d";
        if (s.accent) t << "a";
        if (s.slide)  t << "s";
        tokens.add (t);
    }

    return tokens.joinIntoString (" ");
}

Pattern Pattern::fromString (const juce::String& text)
{
    Pattern p;
    auto tokens = juce::StringArray::fromTokens (text.trim(), " ", "");
    tokens.removeEmptyStrings();

    p.length = juce::jlimit (1, maxSteps, tokens.size());

    for (int i = 0; i < juce::jmin (tokens.size(), maxSteps); ++i)
    {
        const auto tok = tokens[i];
        Step s;
        int pos = 0;
        const auto c = tok[0];

        if (c == '-')      { s.time = Time::Rest; pos = 1; }
        else if (c == '_') { s.time = Time::Tie;  pos = 1; }
        else
        {
            static const int base[] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
            if (c >= 'A' && c <= 'G')
            {
                s.note = base[c - 'A'];
                pos = 1;
                if (tok[pos] == '#') { ++s.note; ++pos; }
                if (tok[pos] == '\'') { s.note = 12; ++pos; }
                s.note = juce::jlimit (0, 12, s.note);
            }
        }

        for (; pos < tok.length(); ++pos)
        {
            switch (tok[pos])
            {
                case 'u': s.up = true; break;
                case 'd': s.down = true; break;
                case 'a': s.accent = true; break;
                case 's': s.slide = true; break;
                default: break;
            }
        }

        p.steps[(size_t) i] = s;
    }

    return p;
}

Pattern Pattern::random (juce::Random& rng)
{
    // Musically weighted generator: pick a scale typical of acid lines and
    // sprinkle accents, slides and octave jumps.
    static const std::vector<std::vector<int>> scales = {
        { 0, 3, 5, 7, 10, 12 },       // minor pentatonic
        { 0, 1, 3, 5, 7, 8, 10, 12 }, // phrygian
        { 0, 2, 3, 5, 7, 8, 10, 12 }, // aeolian
        { 0, 3, 5, 6, 7, 10, 12 },    // blues
        { 0, 2, 3, 5, 7, 9, 10, 12 }, // dorian
        { 0, 1, 4, 5, 7, 8, 10, 12 }, // phrygian dominant
        { 0, 7, 12 },                 // root/fifth only
    };

    const auto& scale = scales[(size_t) rng.nextInt ((int) scales.size())];

    Pattern p;
    const int lengths[] = { 16, 16, 16, 16, 16, 8, 12, 15, 7 };
    p.length = lengths[rng.nextInt ((int) std::size (lengths))];

    const float restProb   = 0.08f + rng.nextFloat() * 0.18f;
    const float tieProb    = rng.nextFloat() * 0.12f;
    const float accentProb = 0.15f + rng.nextFloat() * 0.3f;
    const float slideProb  = 0.1f + rng.nextFloat() * 0.25f;
    const float upProb     = rng.nextFloat() * 0.3f;
    const float rootProb   = 0.25f + rng.nextFloat() * 0.3f;

    for (int i = 0; i < p.length; ++i)
    {
        Step s;
        const float r = rng.nextFloat();

        if (i > 0 && r < restProb)                  s.time = Time::Rest;
        else if (i > 0 && r < restProb + tieProb)   s.time = Time::Tie;

        s.note   = rng.nextFloat() < rootProb ? 0 : scale[(size_t) rng.nextInt ((int) scale.size())];
        s.up     = rng.nextFloat() < upProb;
        s.down   = ! s.up && rng.nextFloat() < 0.08f;
        s.accent = rng.nextFloat() < accentProb;
        s.slide  = rng.nextFloat() < slideProb;

        if (s.time == Time::Tie)
            s.up = s.down = s.accent = false;

        p.steps[(size_t) i] = s;
    }

    return p;
}
} // namespace acid
