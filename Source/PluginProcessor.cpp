#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "presets/PresetManager.h"

namespace
{
constexpr int baseNote = 36; // C2: the lowest key of the TB-303 keyboard (without DOWN)
}

Reckless303Processor::Reckless303Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Reckless303", createLayout())
{
    for (auto& s : steps)
        s.store (acid::Step().pack());

    presets = std::make_unique<PresetManager> (*this);
    presets->loadPreset (0);
}

Reckless303Processor::~Reckless303Processor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout Reckless303Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tuning, 1 }, "Tuning",
                                                       NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("st")));

    auto unit = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    };

    unit (ParamID::cutoff, "Cut Off Freq", 0.45f);
    unit (ParamID::resonance, "Resonance", 0.7f);
    unit (ParamID::envMod, "Env Mod", 0.55f);
    unit (ParamID::decay, "Decay", 0.4f);
    unit (ParamID::accent, "Accent", 0.6f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::waveform, 1 }, "Waveform",
                                                        StringArray { "Saw", "Square" }, 0));
    unit (ParamID::drive, "Drive", 0.0f);
    unit (ParamID::volume, "Volume", 0.75f);

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamID::tempo, 1 }, "Tempo",
                                                       NormalisableRange<float> (40.0f, 300.0f, 0.1f), 125.0f,
                                                       AudioParameterFloatAttributes().withLabel ("BPM")));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamID::mode, 1 }, "Mode",
                                                        StringArray { "MIDI", "Pattern", "Key Trig" }, ModePattern));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::run, 1 }, "Run", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamID::sync, 1 }, "Host Sync", true));

    return layout;
}

bool Reckless303Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Reckless303Processor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    voice.prepare (sr);
    lastAbsStep = -1;
    gateOffPending = false;
    prevSlide = false;
    wasRunning = false;
    heldNotes.clear();
}

void Reckless303Processor::updateVoiceParams()
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    acid::Voice303::Params p;
    p.tuning    = get (ParamID::tuning);
    p.cutoff    = get (ParamID::cutoff);
    p.resonance = get (ParamID::resonance);
    p.envMod    = get (ParamID::envMod);
    p.decay     = get (ParamID::decay);
    p.accent    = get (ParamID::accent);
    p.waveform  = get (ParamID::waveform);
    p.drive     = get (ParamID::drive);
    p.volume    = get (ParamID::volume);
    voice.setParams (p);
}

acid::Pattern Reckless303Processor::getPattern() const
{
    acid::Pattern p;
    p.length = patternLength.load();
    for (int i = 0; i < acid::maxSteps; ++i)
        p.steps[(size_t) i] = getStep (i);
    return p;
}

void Reckless303Processor::setPattern (const acid::Pattern& p)
{
    for (int i = 0; i < acid::maxSteps; ++i)
        setStep (i, p.steps[(size_t) i]);
    setPatternLength (p.length);
}

void Reckless303Processor::triggerStep (int index)
{
    const int len = patternLength.load();
    const auto st = getStep (index);
    const auto next = getStep ((index + 1) % len);

    playingStep.store (index);

    switch (st.time)
    {
        case acid::Time::Rest:
            if (voice.isGated())
                voice.noteOff();
            gateOffPending = false;
            prevSlide = false;
            break;

        case acid::Time::Tie:
            // Extends the previous note; it releases halfway unless it slides on
            // or the next step ties again.
            if (voice.isGated())
                gateOffPending = ! (prevSlide || next.time == acid::Time::Tie);
            break;

        case acid::Time::Note:
        {
            const float note = (float) (baseNote + st.semitone() + transpose);
            voice.noteOn (note, st.accent, prevSlide && voice.isGated());
            gateOffPending = ! (st.slide || next.time == acid::Time::Tie);
            prevSlide = st.slide;
            break;
        }
    }
}

void Reckless303Processor::handleMidi (const juce::MidiMessage& m, int mode)
{
    if (m.isNoteOn())
    {
        const int note = m.getNoteNumber();
        heldNotes.removeFirstMatchingValue (note);
        heldNotes.add (note);

        if (mode == ModeMidi)
        {
            const bool legato = heldNotes.size() > 1 && voice.isGated();
            voice.noteOn ((float) note, m.getVelocity() >= 100, legato);
        }
        else
        {
            transpose = note - 60;
        }
    }
    else if (m.isNoteOff())
    {
        const int note = m.getNoteNumber();
        const bool wasLast = ! heldNotes.isEmpty() && heldNotes.getLast() == note;
        heldNotes.removeFirstMatchingValue (note);

        if (mode == ModeMidi)
        {
            if (heldNotes.isEmpty())
                voice.noteOff();
            else if (wasLast)
                voice.noteOn ((float) heldNotes.getLast(), false, true);
        }
        else if (mode == ModeKeyTrig && ! heldNotes.isEmpty())
        {
            transpose = heldNotes.getLast() - 60;
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNotes.clear();
        voice.noteOff();
    }
}

void Reckless303Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    updateVoiceParams();

    const int mode       = (int) apvts.getRawParameterValue (ParamID::mode)->load();
    const bool runParam  = apvts.getRawParameterValue (ParamID::run)->load() > 0.5f;
    const double knobBpm = apvts.getRawParameterValue (ParamID::tempo)->load();

    if (mode != lastMode)
    {
        lastMode = mode;
        heldNotes.clear();
        voice.noteOff();
        transpose = 0;
    }

    const bool syncParam = apvts.getRawParameterValue (ParamID::sync)->load() > 0.5f;

    bool hostPlaying = false, hostHasBpm = false, hostHasPosition = false;
    double hostBpm = knobBpm, hostPpq = 0.0;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();

            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 1.0)
            {
                hostBpm = *bpm;
                hostHasBpm = true;
            }

            if (auto ppq = pos->getPpqPosition())
            {
                hostPpq = *ppq;
                hostHasPosition = true;
            }
        }
    }

    // SYNC on: the host tempo drives the sequencer (even while stopped) and,
    // while the host plays, steps are locked to its bar/beat position.
    // SYNC off: the TEMPO knob drives a free clock that still starts and
    // stops with the host transport.
    const bool followHostTempo = syncParam && hostHasBpm;
    const bool lockToHost = syncParam && hostPlaying && hostHasPosition;
    const double bpm = followHostTempo ? hostBpm : knobBpm;
    const double stepsPerSample = bpm / 60.0 * 4.0 / sampleRate;

    currentBpm.store (bpm);
    hostIsPlaying.store (hostPlaying);
    tempoFromHost.store (followHostTempo);

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    auto render = [&] (int start, int end)
    {
        for (int i = start; i < end; ++i)
        {
            bool running = false;
            if (mode == ModePattern)      running = hostPlaying || runParam;
            else if (mode == ModeKeyTrig) running = ! heldNotes.isEmpty();

            if (running)
            {
                if (! wasRunning)
                {
                    lastAbsStep = std::numeric_limits<long long>::min();
                    internalPos = 0.0;
                    prevSlide = false;
                }

                double absPos;
                if (lockToHost)
                {
                    absPos = hostPpq * 4.0 + i * stepsPerSample;
                }
                else
                {
                    absPos = internalPos;
                    internalPos += stepsPerSample;
                }

                const auto absStep = (long long) std::floor (absPos);
                if (absStep != lastAbsStep)
                {
                    lastAbsStep = absStep;
                    const int len = patternLength.load();
                    triggerStep ((int) (((absStep % len) + len) % len));
                }

                if (gateOffPending && absPos - std::floor (absPos) >= 0.5)
                {
                    voice.noteOff();
                    gateOffPending = false;
                }
            }
            else if (wasRunning)
            {
                voice.noteOff();
                gateOffPending = false;
                prevSlide = false;
                playingStep.store (-1);
            }

            wasRunning = running;

            const float s = voice.render();
            left[i] = s;
            if (right != nullptr)
                right[i] = s;
        }
    };

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        render (pos, at);
        handleMidi (meta.getMessage(), mode);
        pos = at;
    }
    render (pos, numSamples);

    seqRunning.store (wasRunning);
    midi.clear();
}

//==============================================================================
int Reckless303Processor::getNumPrograms()                      { return juce::jmax (1, presets->getNumFactoryPresets()); }
int Reckless303Processor::getCurrentProgram()                   { return juce::jmax (0, presets->getCurrentIndex()); }
void Reckless303Processor::setCurrentProgram (int index)        { presets->loadPreset (index); }
const juce::String Reckless303Processor::getProgramName (int i) { return presets->getPresetName (i); }

void Reckless303Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", getPattern().toString(), nullptr);
    state.setProperty ("presetName", presets->getCurrentName(), nullptr);
    state.setProperty ("editorWidth", editorWidth.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Reckless303Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType()))
            return;

        apvts.replaceState (tree);

        if (tree.hasProperty ("pattern"))
            setPattern (acid::Pattern::fromString (tree["pattern"].toString()));

        editorWidth.store ((int) tree.getProperty ("editorWidth", 0));
        presets->restoreCurrent (tree["presetName"].toString());
    }
}

juce::AudioProcessorEditor* Reckless303Processor::createEditor()
{
    return new Reckless303Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Reckless303Processor();
}
