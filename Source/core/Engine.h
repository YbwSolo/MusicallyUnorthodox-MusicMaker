#pragma once
#include "Http.h"

namespace mm
{
/** Everything on the Create page: what to make and how. Saved with each song so it can be made again. */
struct SongRequest
{
    enum class Mode { simple, custom };
    enum class Source { none, reference, cover, repaint };

    Mode mode = Mode::simple;
    juce::String description;       // simple: the song in a sentence or two
    juce::String title;
    juce::String style;             // custom: genre, mood, instruments
    juce::String lyrics;
    bool instrumental = false;

    int bpm = 0;                    // 0 lets the engine choose
    juce::String key;               // "" lets the engine choose, otherwise e.g. "C minor"
    juce::String timeSignature;     // "" or "4"
    double seconds = 0.0;           // 0 lets the engine choose
    juce::String language = "en";

    int variations = 2;
    int steps = 8;
    double guidance = 7.0;
    bool randomSeed = true;
    juce::int64 seed = 0;
    bool thinking = false;          // the engine plans the song first; needs its language model
    bool enhance = false;           // the engine rewrites the style and lyrics more fully
    juce::String model;             // "" for the engine's default

    Source source = Source::none;
    juce::String audioPath;         // the audio to start from, for anything but Source::none
    double strength = 1.0;          // cover: how closely to follow the original
    double repaintStart = 0.0, repaintEnd = -1.0;

    juce::var toVar() const;
    static SongRequest fromVar (const juce::var&);
    juce::String suggestedTitle() const;
    juce::String problem() const;   // why it cannot be sent, or empty
};

struct EngineStatus
{
    bool reachable = false;
    bool modelsReady = false;
    bool languageModelReady = false;
    juce::String model, languageModel, error;
};

struct EngineModel
{
    juce::String name;
    bool isDefault = false, isLoaded = false;
};

struct ResultItem
{
    juce::String fileUrl, prompt, lyrics, key, timeSignature, seed, model;
    double bpm = 0.0, seconds = 0.0;
};

struct TaskUpdate
{
    int status = 0;                 // 0 working, 1 finished, 2 failed
    double progress = 0.0;
    juce::String stage, progressText, error;
    juce::Array<ResultItem> items;
};

/** Talks to the ACE-Step engine. Every call blocks, so call it from a background thread. */
class EngineClient
{
public:
    EngineClient (juce::String baseUrl, juce::String apiKey);

    EngineStatus health (int connectTimeoutMs = 1500) const;
    juce::Array<EngineModel> models (juce::String& error) const;
    /** useWriter lets the engine's language model write the lyrics and plan the song. When that model is not
        there the engine refuses, and the song is sent again without it. */
    bool submit (const SongRequest&, const juce::String& audioFormat, bool useWriter, juce::String& taskId, juce::String& error,
                 std::function<bool()> shouldCancel = {}) const;
    bool query (const juce::String& taskId, TaskUpdate&, juce::String& error) const;
    bool download (const juce::String& fileUrl, const juce::File& target, juce::String& error,
                   std::function<bool()> shouldCancel = {}, std::function<void (double)> progress = {}) const;
    /** Asks the engine's language model to fill out the style and lyrics. */
    bool enhance (SongRequest&, juce::String& error) const;

    // Pure helpers, covered by the tests.
    static juce::var buildPayload (const SongRequest&, const juce::String& audioFormat, bool useWriter, const juce::String& writerBackend = {});
    static bool needsWriter (const SongRequest&);
    static bool isWriterMissing (const juce::String& engineError);
    static bool parseTaskUpdate (const juce::String& responseBody, const juce::String& taskId, TaskUpdate&);
    static EngineStatus parseHealth (const juce::String& responseBody);
    static juce::String friendlyStage (const TaskUpdate&);
    juce::String absoluteUrl (const juce::String& fileUrl) const;

    /** Which language-model backend to ask for: "mlx" when the engine is on this Apple Silicon Mac, otherwise left to the engine. */
    juce::String writerBackend;

private:
    HttpRequest request (const juce::String& method, const juce::String& path) const;
    HttpResponse postJson (const juce::String& path, const juce::var& body, int idleTimeoutMs = 30000) const;

    juce::String base, key;
};

juce::StringArray keyNames();       // "C major" ... "B minor"
}
