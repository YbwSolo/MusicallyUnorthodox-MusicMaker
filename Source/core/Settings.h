#pragma once
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

namespace mm
{
/** Everything the plugin remembers between sessions, shared by every instance in the host. */
class Settings
{
public:
    Settings();

    juce::String get (const juce::String& key, const juce::String& fallback = {}) const;
    double getNumber (const juce::String& key, double fallback) const;
    bool getBool (const juce::String& key, bool fallback) const;
    void set (const juce::String& key, const juce::String& value);
    void setBool (const juce::String& key, bool value)      { set (key, value ? "1" : "0"); }
    void remove (const juce::String& key);

    juce::String engineUrl() const;         // where the engine answers, without a trailing slash
    juce::String engineKey() const;
    juce::File engineFolder() const;        // where the engine is (or will be) installed on this computer
    juce::String audioFormat() const;       // wav, flac or mp3

    bool online() const;                    // songs are made by the online service instead of an engine of our own
    juce::String onlineUrl() const;
    juce::String onlineKey() const;

    static juce::File appDataDir();         // per-user settings, the engine and its log
    static juce::File libraryDir();         // the songs themselves: Music/MU Music Maker
    static juce::File songsDir();
    static juce::File takesDir();           // audio caught from the track

private:
    std::unique_ptr<juce::PropertiesFile> props;
};
}
