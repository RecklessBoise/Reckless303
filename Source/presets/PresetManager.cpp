#include "PresetManager.h"
#include "PluginProcessor.h"

namespace
{
// Parameters stored in a preset. Tempo, mode, run and volume stay under the
// player's control when browsing.
const char* const kPresetParams[] = { ParamID::tuning, ParamID::cutoff, ParamID::resonance, ParamID::envMod,
                                      ParamID::decay,  ParamID::accent, ParamID::waveform,  ParamID::drive };

const juce::String kExtension = ".acid303";
} // namespace

PresetManager::PresetManager (Acid303Processor& p) : processor (p)
{
    factory = makeFactoryPresets();
    rescanUserPresets();
    loadFavorites();
}

juce::File PresetManager::getDataDirectory()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support/Acid303");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Acid303");
   #endif
}

juce::File PresetManager::getUserPresetDirectory()
{
    return getDataDirectory().getChildFile ("Presets");
}

const PresetData& PresetManager::getPreset (int index) const
{
    jassert (index >= 0 && index < getNumPresets());
    if (index < (int) factory.size())
        return factory[(size_t) index];
    return user[(size_t) (index - (int) factory.size())];
}

juce::String PresetManager::getPresetName (int index) const
{
    return juce::isPositiveAndBelow (index, getNumPresets()) ? getPreset (index).name : juce::String();
}

juce::String PresetManager::getCurrentCategory() const
{
    if (juce::isPositiveAndBelow (current, getNumPresets()))
        return getPreset (current).category;
    return "Random";
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray cats;
    for (auto& f : factory)
        cats.addIfNotAlreadyThere (f.category);
    return cats;
}

void PresetManager::apply (const PresetData& data, bool includePattern)
{
    for (auto& [id, value] : data.params)
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));

    if (includePattern && data.pattern.isNotEmpty())
        processor.setPattern (acid::Pattern::fromString (data.pattern));
}

void PresetManager::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPresets()))
        return;

    apply (getPreset (index));
    current = index;
    currentName = getPreset (index).name;
    sendChangeMessage();
}

int PresetManager::step (int from, int delta) const
{
    const int n = getNumPresets();
    if (n == 0)
        return -1;

    if (favoritesOnly && favorites.isEmpty())
        return -1;

    int idx = from < 0 ? (delta > 0 ? -1 : 0) : from;
    for (int tries = 0; tries < n; ++tries)
    {
        idx = ((idx + delta) % n + n) % n;
        if (! favoritesOnly || isFavorite (idx))
            return idx;
    }
    return -1;
}

void PresetManager::next()     { if (auto i = step (current, 1); i >= 0) loadPreset (i); }
void PresetManager::previous() { if (auto i = step (current, -1); i >= 0) loadPreset (i); }

void PresetManager::setFavoritesOnly (bool shouldOnly)
{
    favoritesOnly = shouldOnly;
    sendChangeMessage();
}

juce::String PresetManager::keyFor (int index) const
{
    const auto& p = getPreset (index);
    return (p.isFactory ? "factory/" : "user/") + p.name;
}

bool PresetManager::isFavorite (int index) const
{
    return juce::isPositiveAndBelow (index, getNumPresets()) && favorites.contains (keyFor (index));
}

void PresetManager::toggleFavorite (int index)
{
    if (! juce::isPositiveAndBelow (index, getNumPresets()))
        return;

    const auto key = keyFor (index);
    if (favorites.contains (key))
        favorites.removeString (key);
    else
        favorites.add (key);

    saveFavorites();
    sendChangeMessage();
}

void PresetManager::loadFavorites()
{
    favorites.clear();
    if (auto xml = juce::XmlDocument::parse (getDataDirectory().getChildFile ("favorites.xml")))
        for (auto* e : xml->getChildWithTagNameIterator ("Fav"))
            favorites.addIfNotAlreadyThere (e->getStringAttribute ("key"));
}

void PresetManager::saveFavorites() const
{
    juce::XmlElement xml ("Favorites");
    for (auto& f : favorites)
        xml.createNewChildElement ("Fav")->setAttribute ("key", f);

    getDataDirectory().createDirectory();
    xml.writeTo (getDataDirectory().getChildFile ("favorites.xml"));
}

void PresetManager::rescanUserPresets()
{
    user.clear();
    for (const auto& entry : juce::RangedDirectoryIterator (getUserPresetDirectory(), false, "*" + kExtension))
    {
        auto xml = juce::XmlDocument::parse (entry.getFile());
        if (xml == nullptr || ! xml->hasTagName ("Acid303Preset"))
            continue;

        PresetData d;
        d.isFactory = false;
        d.file      = entry.getFile();
        d.name      = xml->getStringAttribute ("name", entry.getFile().getFileNameWithoutExtension());
        d.category  = "User";
        d.pattern   = xml->getStringAttribute ("pattern");

        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
            d.params.emplace_back (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));

        user.push_back (std::move (d));
    }

    std::sort (user.begin(), user.end(), [] (const PresetData& a, const PresetData& b)
               { return a.name.compareNatural (b.name) < 0; });
}

int PresetManager::findPreset (const juce::String& name, bool userOnly) const
{
    for (int i = userOnly ? (int) factory.size() : 0; i < getNumPresets(); ++i)
        if (getPreset (i).name == name)
            return i;
    return -1;
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    juce::XmlElement xml ("Acid303Preset");
    xml.setAttribute ("name", name);
    xml.setAttribute ("category", "User");
    xml.setAttribute ("pattern", processor.getPattern().toString());

    for (auto* id : kPresetParams)
    {
        if (auto* param = processor.apvts.getParameter (id))
        {
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", id);
            e->setAttribute ("value", param->convertFrom0to1 (param->getValue()));
        }
    }

    auto dir = getUserPresetDirectory();
    dir.createDirectory();
    if (! xml.writeTo (dir.getChildFile (name + kExtension)))
        return false;

    rescanUserPresets();
    current = findPreset (name, true);
    currentName = name;
    sendChangeMessage();
    return true;
}

bool PresetManager::deleteUserPreset (int index)
{
    if (! isUserPreset (index))
        return false;

    const auto key = keyFor (index);
    // Moves the file to the system trash so a deleted preset can be recovered.
    if (! getPreset (index).file.moveToTrash())
        return false;

    favorites.removeString (key);
    saveFavorites();
    rescanUserPresets();
    current = -1;
    currentName = "Init";
    sendChangeMessage();
    return true;
}

void PresetManager::randomize (RandomMode mode)
{
    if (mode != RandomMode::Pattern)
    {
        PresetData d;
        auto r = [this] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };

        d.params = {
            { ParamID::tuning, 0.0f },
            { ParamID::cutoff, r (0.08f, 0.75f) },
            { ParamID::resonance, r (0.35f, 0.97f) },
            { ParamID::envMod, r (0.15f, 0.95f) },
            { ParamID::decay, r (0.05f, 0.85f) },
            { ParamID::accent, r (0.25f, 1.0f) },
            { ParamID::waveform, rng.nextFloat() < 0.6f ? 0.0f : 1.0f },
            { ParamID::drive, rng.nextFloat() < 0.45f ? 0.0f : r (0.1f, 0.75f) },
        };
        apply (d, false);
    }

    if (mode != RandomMode::Sound)
        processor.setPattern (acid::Pattern::random (rng));

    current = -1;
    currentName = "Random " + juce::String (++randomCounter).paddedLeft ('0', 3);
    sendChangeMessage();
}

void PresetManager::restoreCurrent (const juce::String& name)
{
    current = findPreset (name);
    currentName = name.isNotEmpty() ? name : juce::String ("Init");
    sendChangeMessage();
}
