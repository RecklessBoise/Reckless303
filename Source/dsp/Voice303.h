#pragma once

#include <cmath>
#include <algorithm>

namespace acid
{
// Monophonic TB-303 style voice.
//
// Signal path, following the original block diagram:
//   VCO (saw / square) -> VCF (4-pole ladder, 24 dB/oct, highpass in the
//   resonance loop) -> VCA, with
//   - MEG (main envelope) driving the filter, decay 200 ms .. 2 s (DECAY knob)
//   - accented notes forcing the MEG to its shortest decay, and an accent
//     "capacitor" that charges across consecutive accents (the classic wow)
//   - VEG (amplitude envelope) with fast attack, long decay while the gate is held
//   - slide: constant-time portamento (~60 ms) without envelope retrigger.
class Voice303
{
public:
    struct Params
    {
        float tuning    = 0.0f;  // semitones
        float cutoff    = 0.5f;  // 0..1
        float resonance = 0.5f;  // 0..1
        float envMod    = 0.5f;  // 0..1
        float decay     = 0.5f;  // 0..1
        float accent    = 0.5f;  // 0..1
        float waveform  = 0.0f;  // 0 saw, 1 square
        float drive     = 0.0f;  // 0..1
        float volume    = 0.75f; // 0..1
    };

    void prepare (double sampleRate)
    {
        sr   = (float) sampleRate;
        osSr = sr * 2.0f;

        slideCoef      = coefFor (0.020f, sr);  // ~60 ms to settle
        ampAttackCoef  = coefFor (0.0015f, sr);
        ampDecayCoef   = std::exp (-1.0f / (3.0f * sr));
        ampReleaseCoef = coefFor (0.004f, sr);
        accentDecayK   = std::exp (-1.0f / (0.2f * sr / 2.3f));
        capChargeCoef  = coefFor (0.045f, sr);
        capDrainCoef   = coefFor (0.120f, sr);
        smoothCoef     = coefFor (0.008f, sr);
        dcCoef         = std::exp (-2.0f * pi * 25.0f / sr);
        fbHpCoef       = std::exp (-2.0f * pi * 150.0f / osSr);
        reset();
    }

    void reset()
    {
        phase = 0.0f;
        s1 = s2 = s3 = s4 = 0.0f;
        y4Last = 0.0f;
        fbHpX = fbHpY = 0.0f;
        meg = 0.0f;
        ampEnv = 0.0f;
        ampStage = Release;
        accentCap = 0.0f;
        gate = false;
        dcX = dcY = 0.0f;
    }

    void setParams (const Params& p) noexcept { target = p; }

    // Start a note. If slide is true and the voice is gated, the pitch glides
    // and the envelopes are not retriggered (TB-303 slide behaviour).
    void noteOn (float midiNote, bool accented, bool slide) noexcept
    {
        targetPitch = midiNote;

        if (slide && gate)
        {
            sliding = true;
            accentOn = accented;
            return;
        }

        sliding  = false;
        pitch    = midiNote;
        accentOn = accented;
        gate     = true;
        ampStage = Attack;
        meg      = 1.0f;
        megCoef  = accented ? accentDecayK : decayCoefFor (target.decay);
    }

    void noteOff() noexcept
    {
        gate = false;
        ampStage = Release;
    }

    bool isGated() const noexcept { return gate; }
    bool isSilent() const noexcept { return ! gate && ampEnv < 1.0e-5f; }

    float render() noexcept
    {
        // Parameter smoothing (one pole per sample).
        cur.cutoff    += (target.cutoff - cur.cutoff) * smoothCoef;
        cur.resonance += (target.resonance - cur.resonance) * smoothCoef;
        cur.envMod    += (target.envMod - cur.envMod) * smoothCoef;
        cur.accent    += (target.accent - cur.accent) * smoothCoef;
        cur.drive     += (target.drive - cur.drive) * smoothCoef;
        cur.volume    += (target.volume - cur.volume) * smoothCoef;
        cur.tuning    += (target.tuning - cur.tuning) * smoothCoef;
        cur.waveform   = target.waveform;

        // Pitch / slide.
        if (sliding)
            pitch += (targetPitch - pitch) * slideCoef;

        const float freq = 440.0f * std::exp2 ((pitch + cur.tuning - 69.0f) / 12.0f);
        const float dt   = std::min (freq / osSr, 0.45f);

        // Envelopes.
        meg *= megCoef;

        switch (ampStage)
        {
            case Attack:
                ampEnv += (1.02f - ampEnv) * ampAttackCoef;
                if (ampEnv >= 1.0f) { ampEnv = 1.0f; ampStage = Decay; }
                break;
            case Decay:   ampEnv *= ampDecayCoef; break;
            case Release: ampEnv -= ampEnv * ampReleaseCoef; break;
        }

        const float capTarget = accentOn ? meg : 0.0f;
        accentCap += (capTarget - accentCap) * (capTarget > accentCap ? capChargeCoef : capDrainCoef);

        // Filter cutoff: knob range ~120 Hz .. 4.7 kHz, env mod lowers the
        // base a little and sweeps up to ~4.5 octaves, accent adds on top.
        const float baseHz = 120.0f * std::exp2 (cur.cutoff * 5.3f - cur.envMod * 1.1f);
        const float modOct = cur.envMod * 4.5f * meg + cur.accent * accentCap * 3.2f;
        const float fc     = std::clamp (baseHz * std::exp2 (modOct), 20.0f, osSr * 0.42f);

        const float g = std::tan (pi * fc / osSr);
        const float G = g / (1.0f + g);
        const float k = std::pow (cur.resonance, 0.8f) * 4.05f;

        // Two oversampled filter iterations, box-decimated.
        float acc = 0.0f;
        for (int os = 0; os < 2; ++os)
        {
            const float osc = oscillator (dt);

            // Highpass inside the resonance loop: keeps the bass when
            // resonance is high, like the original circuit.
            const float hp = fbHpCoef * (fbHpY + y4Last - fbHpX);
            fbHpX = y4Last;
            fbHpY = hp;

            const float u  = std::tanh (1.6f * osc - k * hp);
            const float y1 = onePole (u, s1, G);
            const float y2 = onePole (y1, s2, G);
            const float y3 = onePole (y2, s3, G);
            const float y4 = onePole (y3, s4, G);
            y4Last = y4;

            acc += y4 * (1.0f + k * 0.45f) + y3 * 0.08f;
        }

        float out = acc * 0.5f;

        // VCA with accent boost.
        const float vca = ampEnv * (0.55f + cur.accent * accentCap * 0.9f + (accentOn ? cur.accent * 0.12f : 0.0f));
        out *= vca;

        // Overdrive.
        if (cur.drive > 0.001f)
        {
            const float d = 1.0f + cur.drive * cur.drive * 24.0f;
            out = std::tanh (out * d) * (1.0f / std::sqrt (d)) * (1.0f + cur.drive * 0.6f);
        }

        // DC blocker (output coupling capacitor).
        const float dc = out - dcX + dcCoef * dcY;
        dcX = out;
        dcY = dc;

        return dc * cur.volume * cur.volume * 0.9f;
    }

private:
    static constexpr float pi = 3.14159265358979f;
    enum AmpStage { Attack, Decay, Release };

    static float coefFor (float tauSeconds, float rate) { return 1.0f - std::exp (-1.0f / (tauSeconds * rate)); }

    float decayCoefFor (float decayKnob) const
    {
        // 200 ms .. 2 s to roughly -20 dB.
        const float seconds = 0.2f * std::pow (10.0f, decayKnob);
        return std::exp (-2.3f / (seconds * sr));
    }

    static float onePole (float in, float& s, float G) noexcept
    {
        const float v = (in - s) * G;
        const float y = v + s;
        s = y + v;
        return y;
    }

    static float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)        { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    float oscillator (float dt) noexcept
    {
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;

        if (cur.waveform < 0.5f)
            return (1.0f - 2.0f * phase) + polyBlep (phase, dt); // falling saw

        // The 303 square is derived from the saw and is not perfectly symmetric.
        constexpr float pw = 0.46f;
        float sq = phase < pw ? 0.85f : -0.85f;
        sq += 0.85f * polyBlep (phase, dt);
        float t2 = phase - pw;
        if (t2 < 0.0f) t2 += 1.0f;
        sq -= 0.85f * polyBlep (t2, dt);
        return sq;
    }

    float sr = 44100.0f, osSr = 88200.0f;
    Params target, cur;

    float phase = 0.0f;
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0, y4Last = 0;
    float fbHpX = 0, fbHpY = 0, fbHpCoef = 0.99f;

    float pitch = 36.0f, targetPitch = 36.0f;
    bool sliding = false, gate = false, accentOn = false;

    float meg = 0.0f, megCoef = 0.999f;
    float ampEnv = 0.0f;
    AmpStage ampStage = Release;
    float accentCap = 0.0f;

    float slideCoef = 0.001f, ampAttackCoef = 0.1f, ampDecayCoef = 0.9999f, ampReleaseCoef = 0.01f;
    float accentDecayK = 0.999f, capChargeCoef = 0.001f, capDrainCoef = 0.001f, smoothCoef = 0.01f;
    float dcCoef = 0.999f, dcX = 0.0f, dcY = 0.0f;
};
} // namespace acid
