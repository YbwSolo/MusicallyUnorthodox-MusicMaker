#include "Settings.h"

namespace mm
{
static const char* const kAppFolder = "MU Music Maker";

juce::File Settings::appDataDir()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile ("Application Support");
   #endif
    auto dir = base.getChildFile (kAppFolder);
    dir.createDirectory();
    return dir;
}

juce::File Settings::libraryDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile (kAppFolder);
    dir.createDirectory();
    return dir;
}

juce::File Settings::songsDir()
{
    auto dir = libraryDir().getChildFile ("Songs");
    dir.createDirectory();
    return dir;
}

juce::File Settings::takesDir()
{
    auto dir = libraryDir().getChildFile ("Takes");
    dir.createDirectory();
    return dir;
}

Settings::Settings()
{
    juce::PropertiesFile::Options o;
    o.millisecondsBeforeSaving = 400;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (appDataDir().getChildFile ("settings.xml"), o);
}

juce::String Settings::get (const juce::String& key, const juce::String& fallback) const { return props->getValue (key, fallback); }
double Settings::getNumber (const juce::String& key, double fallback) const              { return props->getDoubleValue (key, fallback); }
bool Settings::getBool (const juce::String& key, bool fallback) const                    { return props->getValue (key, fallback ? "1" : "0") == "1"; }
void Settings::set (const juce::String& key, const juce::String& value)                  { props->setValue (key, value); props->saveIfNeeded(); }
void Settings::remove (const juce::String& key)                                          { props->removeValue (key); props->saveIfNeeded(); }

juce::String Settings::engineUrl() const
{
    auto url = get ("engineUrl", "http://127.0.0.1:8001").trim();
    if (url.isEmpty()) url = "http://127.0.0.1:8001";
    if (! url.containsIgnoreCase ("://")) url = "http://" + url;
    while (url.endsWithChar ('/')) url = url.dropLastCharacters (1);
    return url;
}

juce::String Settings::engineKey() const { return get ("engineKey").trim(); }

juce::File Settings::engineFolder() const
{
    const auto chosen = get ("engineFolder").trim();
    if (chosen.isNotEmpty() && juce::File::isAbsolutePath (chosen)) return juce::File (chosen);
    return appDataDir().getChildFile ("Engine");
}

bool Settings::online() const { return get ("engineMode", "online") != "local"; }

juce::String Settings::onlineUrl() const
{
    auto url = get ("onlineUrl", "https://api.acemusic.ai").trim();
    if (url.isEmpty()) url = "https://api.acemusic.ai";
    while (url.endsWithChar ('/')) url = url.dropLastCharacters (1);
    return url;
}

juce::String Settings::onlineKey() const { return get ("onlineKey").trim(); }

juce::String Settings::audioFormat() const
{
    const auto f = get ("audioFormat", "wav").toLowerCase();
    return (f == "flac" || f == "mp3") ? f : juce::String ("wav");
}
}
