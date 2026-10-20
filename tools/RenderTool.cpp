// Offline check tool: renders the front panel to PNG files and the
// sequencer output to WAV files, without needing a host.
//   Reckless303RenderTool <output-dir>

#include "PluginProcessor.h"
#include "presets/PresetManager.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

using namespace juce;

static void snapshot (Reckless303Processor& proc, int width, const File& file)
{
    std::unique_ptr<AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize (width, roundToInt (width * 640.0 / 1200.0));

    // Let the panel generate its texture for this size.
    for (int i = 0; i < 3; ++i)
        MessageManager::getInstance()->runDispatchLoopUntil (120);

    const auto img = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    file.deleteFile();
    FileOutputStream out (file);
    PNGImageFormat().writeImageToStream (img, out);
    std::cout << "wrote " << file.getFullPathName() << " (" << img.getWidth() << "x" << img.getHeight() << ")\n";
}

static bool renderAudio (Reckless303Processor& proc, const File& file, int presetIndex, double seconds)
{
    constexpr double sr = 44100.0;
    constexpr int block = 512;

    proc.getPresetManager().loadPreset (presetIndex);
    proc.apvts.getParameter (ParamID::run)->setValueNotifyingHost (1.0f);
    proc.prepareToPlay (sr, block);

    const int total = (int) (seconds * sr);
    AudioBuffer<float> all (2, total), buf (2, block);
    MidiBuffer midi;
    float peak = 0.0f;
    bool finite = true;

    for (int pos = 0; pos < total; pos += block)
    {
        const int n = jmin (block, total - pos);
        buf.setSize (2, n, false, false, true);
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
        {
            all.copyFrom (ch, pos, buf, ch, 0, n);
            for (int i = 0; i < n; ++i)
            {
                const float v = buf.getSample (ch, i);
                finite = finite && std::isfinite (v);
                peak = jmax (peak, std::abs (v));
            }
        }
    }

    proc.apvts.getParameter (ParamID::run)->setValueNotifyingHost (0.0f);

    file.deleteFile();
    if (auto stream = std::unique_ptr<OutputStream> (file.createOutputStream()))
    {
        WavAudioFormat wav;
        if (auto writer = std::unique_ptr<AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (all, 0, total);
        }
    }

    std::cout << "preset " << proc.getPresetManager().getPresetName (presetIndex) << ": peak " << peak
              << " rms " << all.getRMSLevel (0, 0, total) << (finite ? "" : "  NON-FINITE!") << "\n";
    return finite && peak > 0.01f && peak < 4.0f;
}

// Simulated DAW transport.
struct FakeHost : AudioPlayHead
{
    double bpm = 140.0, ppq = 0.0;
    bool playing = false;

    Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (ppq);
        info.setIsPlaying (playing);
        return info;
    }
};

static bool checkHostSync (Reckless303Processor& proc)
{
    FakeHost host;
    proc.getPresetManager().loadPreset (1); // 16-step pattern
    proc.setPlayHead (&host);
    proc.prepareToPlay (44100.0, 512);
    proc.apvts.getParameter (ParamID::run)->setValueNotifyingHost (0.0f);
    proc.apvts.getParameter (ParamID::mode)->setValueNotifyingHost (proc.apvts.getParameter (ParamID::mode)->convertTo0to1 (1.0f));

    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    auto run = [&] (int blocks)
    {
        for (int i = 0; i < blocks; ++i)
        {
            proc.processBlock (buf, midi);
            if (host.playing)
                host.ppq += 512.0 / 44100.0 * host.bpm / 60.0;
        }
    };

    bool ok = true;
    auto expect = [&] (bool cond, const char* what) { std::cout << (cond ? "  ok   " : "  FAIL ") << what << "\n"; ok = ok && cond; };

    proc.apvts.getParameter (ParamID::sync)->setValueNotifyingHost (1.0f);
    run (4);
    expect (std::abs (proc.getCurrentBpm() - 140.0) < 0.01, "sync on: follows host tempo while stopped");
    expect (! proc.isSequencerRunning(), "host stopped: pattern stopped");

    host.playing = true;
    host.ppq = 8.0; // bar 3, beat 1
    run (2);
    expect (proc.isSequencerRunning(), "host play: pattern starts");
    expect (proc.getPlayingStep() == 0, "sync on: locked to host bar position (step 1 on the bar)");

    host.playing = false;
    run (2);
    expect (! proc.isSequencerRunning(), "host stop: pattern stops");

    proc.apvts.getParameter (ParamID::sync)->setValueNotifyingHost (0.0f);
    host.playing = true;
    host.ppq = 8.75;
    run (2);
    expect (std::abs (proc.getCurrentBpm() - 125.0) < 0.01, "sync off: TEMPO knob drives the speed");
    expect (proc.isSequencerRunning(), "sync off: still starts with host play");

    host.playing = false;
    run (2);
    proc.apvts.getParameter (ParamID::sync)->setValueNotifyingHost (1.0f);
    proc.setPlayHead (nullptr);
    return ok;
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    Reckless303Processor proc;
    std::cout << "presets: " << proc.getPresetManager().getNumPresets()
              << " (factory " << proc.getPresetManager().getNumFactoryPresets() << ")\n";

    bool ok = true;
    for (int idx : { 1, 14, 24, 45, 60 })
        ok = renderAudio (proc, outDir.getChildFile ("preset_" + String (idx) + ".wav"), idx, 6.0) && ok;

    std::cout << "host sync:\n";
    ok = checkHostSync (proc) && ok;

    proc.getPresetManager().loadPreset (1);
    snapshot (proc, 1200, outDir.getChildFile ("ui_1200.png"));
    snapshot (proc, 700, outDir.getChildFile ("ui_700.png"));

    std::cout << (ok ? "ALL OK" : "AUDIO CHECK FAILED") << "\n";
    return ok ? 0 : 1;
}
