#pragma once
#include "Engine.h"
#include "Library.h"

namespace mm
{
/** Keeps an eye on the engine: is it there, are its models loaded, which models does it have. */
class EngineWatch : public juce::ChangeBroadcaster, private juce::Thread
{
public:
    explicit EngineWatch (Settings&);
    ~EngineWatch() override;

    EngineStatus status() const;
    juce::Array<EngineModel> models() const;
    bool everChecked() const                { return checked.load(); }
    void checkSoon();                       // after a setting changed or the engine was started
    void startOver();                       // after switching between online and this computer: forget what was known
    void setWatchedClosely (bool closely)   { closely ? ++watchers : --watchers; checkSoon(); }

private:
    void run() override;

    Settings& settings;
    mutable juce::CriticalSection lock;
    EngineStatus current;
    juce::Array<EngineModel> modelList;
    std::atomic<bool> checked { false };
    bool wasOnline = false;
    std::atomic<int> watchers { 0 };
};

struct Job
{
    enum class State { waiting, sending, making, fetching, done, failed, cancelled };

    juce::String id, title, message;
    SongRequest request;
    State state = State::waiting;
    double progress = 0.0;
    juce::int64 startedMs = 0, finishedMs = 0;
    juce::StringArray songIds;

    bool active() const     { return state == State::waiting || state == State::sending || state == State::making || state == State::fetching; }
};

/** The queue of songs being made. One at a time is sent to the engine, watched until it finishes,
    and its audio brought into the library. Shared by every instance in the host. */
class Generator : public juce::ChangeBroadcaster, private juce::Thread
{
public:
    Generator (Settings&, Library&, EngineWatch&);
    ~Generator() override;

    juce::String add (const SongRequest&);
    void cancel (const juce::String& jobId);
    void dismiss (const juce::String& jobId);       // take a finished or failed one off the list
    juce::Array<Job> jobs() const;                  // newest first
    bool busy() const;

    /** How often the engine is asked how far it has got. The tests turn this down. */
    int pollMs = 1000;

private:
    void run() override;
    void process (const juce::String& jobId);
    void update (const juce::String& jobId, std::function<void (Job&)>);
    bool isCancelled (const juce::String& jobId) const;

    Settings& settings;
    Library& library;
    EngineWatch& watch;
    mutable juce::CriticalSection lock;
    juce::Array<Job> list;
    juce::StringArray cancelledIds;
};
}
