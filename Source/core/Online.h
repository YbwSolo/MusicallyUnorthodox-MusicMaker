#pragma once
#include "Engine.h"

namespace mm
{
struct OnlineResult
{
    juce::Array<juce::File> files;
    juce::StringArray links;        // audio the service gave as a web address instead of inside the answer
    juce::String caption, lyrics, key, timeSignature;
    double bpm = 0.0, seconds = 0.0;
};

/** Talks to ACE Music's own online service (the engine, run by the people who make it), so songs can be made
    with nothing installed. One request per song: the answer streams back with the audio inside it.
    Every call blocks, so call it from a background thread. */
class OnlineClient
{
public:
    OnlineClient (juce::String baseUrl, juce::String apiKey);

    /** Whether the service answers and accepts the key, and which models it offers. */
    EngineStatus check (juce::Array<EngineModel>& models) const;

    /** Makes the song and saves each version into the folder. */
    bool generate (const SongRequest&, const juce::String& audioFormat, const juce::File& folder, const juce::String& filePrefix,
                   OnlineResult&, juce::String& error, std::function<bool()> shouldCancel = {}) const;

    // Pure helpers, covered by the tests.
    static juce::var buildPayload (const SongRequest&, const juce::String& model, const juce::String& audioFormat,
                                   const juce::String& audioBase64 = {}, const juce::String& audioFileFormat = {});
    static bool parseAnswer (const juce::String& body, const juce::File& folder, const juce::String& filePrefix, OnlineResult&, juce::String& error);
    static void parseDetails (const juce::String& text, OnlineResult&);
    static juce::String extensionFor (const juce::MemoryBlock& firstBytes, const juce::String& mimeType);
    /** The audio to start from, made small enough to send: lossless FLAC at 16 bit. */
    static juce::MemoryBlock packForSending (const juce::File&, juce::String& format);
    /** Turns a FLAC into the WAV beside it, sample for sample, and removes the FLAC. */
    static juce::File unpackToWav (const juce::File& flac);

    static constexpr const char* defaultUrl = "https://api.acemusic.ai";
    static constexpr const char* keyPage = "https://acemusic.ai/api-key";

private:
    HttpRequest request (const juce::String& method, const juce::String& path) const;
    juce::String base, key;
};
}
