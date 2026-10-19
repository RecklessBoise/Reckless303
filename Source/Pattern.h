#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>

namespace acid
{
// TB-303 "time mode" for a step.
enum class Time : uint8_t { Note = 0, Tie = 1, Rest = 2 };

// One sequencer step, mirroring the TB-303 pitch-mode data:
// a key of the one-octave keyboard (C..C'), transpose up/down, accent and slide.
struct Step
{
    int  note   = 0; // 0..12 (C .. high C)
    bool up     = false;
    bool down   = false;
    bool accent = false;
    bool slide  = false;
    Time time   = Time::Note;

    int semitone() const noexcept { return note + (up ? 12 : 0) - (down ? 12 : 0); }

    uint32_t pack() const noexcept
    {
        return (uint32_t) (note & 15) | (up ? 16u : 0u) | (down ? 32u : 0u)
             | (accent ? 64u : 0u) | (slide ? 128u : 0u) | ((uint32_t) time << 8);
    }

    static Step unpack (uint32_t v) noexcept
    {
        Step s;
        s.note   = juce::jlimit (0, 12, (int) (v & 15));
        s.up     = (v & 16) != 0;
        s.down   = (v & 32) != 0;
        s.accent = (v & 64) != 0;
        s.slide  = (v & 128) != 0;
        s.time   = (Time) juce::jlimit (0, 2, (int) ((v >> 8) & 3));
        return s;
    }
};

constexpr int maxSteps = 16;

struct Pattern
{
    std::array<Step, maxSteps> steps {};
    int length = maxSteps;

    // Text notation, one token per step separated by spaces:
    //   "-" rest, "_" tie, otherwise a note name C C# D ... B, C' (high C)
    //   followed by optional flags: u (octave up) d (octave down) a (accent) s (slide)
    // e.g. "C Ca C'us - D#d _ G#as"
    juce::String toString() const;
    static Pattern fromString (const juce::String& text);

    static Pattern random (juce::Random& rng);
};

juce::String noteName (int note);
} // namespace acid
