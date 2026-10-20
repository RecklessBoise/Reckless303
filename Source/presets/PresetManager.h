#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class Reckless303Processor;

struct PresetData
{
    juce::String name;
    juce::String category;
    std::vector<std::pair<juce::String, float>> params; // parameter id -> real value
    juce::String pattern;                               // acid::Pattern text notation
    bool isFactory = true;
    juce::File file;
};

std::vector<PresetData> makeFactoryPresets();

// Factory + user presets, favourites ("likes"), random generation.
// Lives in the processor; the UI talks to it on the message thread and
// listens for changes through ChangeBroadcaster.
class PresetManager : public juce::ChangeBroadcaster
{
public:
    enum class RandomMode { All, Sound, Pattern };

    explicit PresetManager (Reckless303Processor&);

    int getNumPresets() const               { return (int) (factory.size() + user.size()); }
    int getNumFactoryPresets() const        { return (int) factory.size(); }
    const PresetData& getPreset (int index) const;
    juce::String getPresetName (int index) const;

    int getCurrentIndex() const             { return current; }
    juce::String getCurrentName() const     { return currentName; }
    juce::String getCurrentCategory() const;

    void loadPreset (int index);
    void next();
    void previous();

    bool getFavoritesOnly() const           { return favoritesOnly; }
    void setFavoritesOnly (bool shouldOnly);

    bool isFavorite (int index) const;
    void toggleFavorite (int index);
    int getNumFavorites() const             { return favorites.size(); }

    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (int index);
    bool isUserPreset (int index) const     { return index >= (int) factory.size() && index < getNumPresets(); }
    int findPreset (const juce::String& name, bool userOnly = false) const;

    void randomize (RandomMode mode);
    void restoreCurrent (const juce::String& name);
    void rescanUserPresets();

    static juce::File getDataDirectory();
    static juce::File getUserPresetDirectory();

    juce::StringArray getCategories() const;

private:
    void apply (const PresetData& data, bool includePattern = true);
    juce::String keyFor (int index) const;
    void loadFavorites();
    void saveFavorites() const;
    int step (int from, int delta) const;

    Reckless303Processor& processor;
    std::vector<PresetData> factory, user;
    juce::StringArray favorites;
    bool favoritesOnly = false;

    int current = -1;
    juce::String currentName { "Init" };
    int randomCounter = 0;
    juce::Random rng;
};
