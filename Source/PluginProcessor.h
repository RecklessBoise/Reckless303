#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>

#include "Pattern.h"
#include "dsp/Voice303.h"

class PresetManager;

namespace ParamID
{
inline constexpr const char* tuning    = "tuning";
inline constexpr const char* cutoff    = "cutoff";
inline constexpr const char* resonance = "resonance";
inline constexpr const char* envMod    = "envmod";
inline constexpr const char* decay     = "decay";
inline constexpr const char* accent    = "accent";
inline constexpr const char* waveform  = "waveform";
inline constexpr const char* drive     = "drive";
inline constexpr const char* volume    = "volume";
inline constexpr const char* tempo     = "tempo";
inline constexpr const char* mode      = "mode";
inline constexpr const char* run       = "run";
inline constexpr const char* sync      = "sync";
} // namespace ParamID

class Reckless303Processor : public juce::AudioProcessor
{
public:
    enum Mode { ModeMidi = 0, ModePattern = 1, ModeKeyTrig = 2 };

    Reckless303Processor();
    ~Reckless303Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Pattern access (lock free: written by the UI, read by the audio thread).
    acid::Pattern getPattern() const;
    void setPattern (const acid::Pattern& p);
    acid::Step getStep (int index) const { return acid::Step::unpack (steps[(size_t) index].load()); }
    void setStep (int index, const acid::Step& s) { steps[(size_t) index].store (s.pack()); }
    int getPatternLength() const { return patternLength.load(); }
    void setPatternLength (int len) { patternLength.store (juce::jlimit (1, acid::maxSteps, len)); }

    int getPlayingStep() const { return playingStep.load(); }
    bool isSequencerRunning() const { return seqRunning.load(); }

    // Tempo actually driving the sequencer, and whether the host transport is rolling.
    double getCurrentBpm() const { return currentBpm.load(); }
    bool isHostPlaying() const { return hostIsPlaying.load(); }
    bool isTempoFromHost() const { return tempoFromHost.load(); }

    juce::AudioProcessorValueTreeState apvts;
    PresetManager& getPresetManager() { return *presets; }

    std::atomic<int> editorWidth { 0 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void handleMidi (const juce::MidiMessage& m, int mode);
    void triggerStep (int index);
    void updateVoiceParams();

    acid::Voice303 voice;

    std::array<std::atomic<uint32_t>, acid::maxSteps> steps;
    std::atomic<int> patternLength { 16 };
    std::atomic<int> playingStep { -1 };
    std::atomic<bool> seqRunning { false };
    std::atomic<double> currentBpm { 125.0 };
    std::atomic<bool> hostIsPlaying { false }, tempoFromHost { false };

    // Sequencer state (audio thread only).
    double internalPos    = 0.0; // position in 16th steps
    long long lastAbsStep = -1;
    bool gateOffPending   = false;
    bool prevSlide        = false;
    bool wasRunning       = false;
    int transpose         = 0;
    int lastMode          = -1;

    // MIDI note stack (MIDI and key-trigger modes).
    juce::Array<int> heldNotes;

    double sampleRate = 44100.0;

    std::unique_ptr<PresetManager> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Reckless303Processor)
};
