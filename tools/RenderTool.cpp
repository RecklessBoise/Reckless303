// Offline check tool: renders the front panel to PNG files and the
// sequencer output to WAV files, without needing a host.
//   Acid303RenderTool <output-dir>

#include "PluginProcessor.h"
#include "presets/PresetManager.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

using namespace juce;

static void snapshot (Acid303Processor& proc, int width, const File& file)
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

static bool renderAudio (Acid303Processor& proc, const File& file, int presetIndex, double seconds)
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

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? File::getCurrentWorkingDirectory().getChildFile (argv[1]) : File::getCurrentWorkingDirectory());
    outDir.createDirectory();

    Acid303Processor proc;
    std::cout << "presets: " << proc.getPresetManager().getNumPresets()
              << " (factory " << proc.getPresetManager().getNumFactoryPresets() << ")\n";

    bool ok = true;
    for (int idx : { 1, 14, 24, 45, 60 })
        ok = renderAudio (proc, outDir.getChildFile ("preset_" + String (idx) + ".wav"), idx, 6.0) && ok;

    proc.getPresetManager().loadPreset (1);
    snapshot (proc, 1200, outDir.getChildFile ("ui_1200.png"));
    snapshot (proc, 700, outDir.getChildFile ("ui_700.png"));

    std::cout << (ok ? "ALL OK" : "AUDIO CHECK FAILED") << "\n";
    return ok ? 0 : 1;
}
