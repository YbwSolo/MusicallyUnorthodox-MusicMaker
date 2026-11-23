#include "Generator.h"

namespace mm
{
EngineWatch::EngineWatch (Settings& s) : juce::Thread ("MU Music Maker engine watch"), settings (s) { startThread(); }
EngineWatch::~EngineWatch() { signalThreadShouldExit(); notify(); stopThread (8000); }

EngineStatus EngineWatch::status() const
{
    const juce::ScopedLock sl (lock);
    return current;
}

juce::Array<EngineModel> EngineWatch::models() const
{
    const juce::ScopedLock sl (lock);
    return modelList;
}

void EngineWatch::checkSoon() { notify(); }

void EngineWatch::run()
{
    while (! threadShouldExit())
    {
        const EngineClient client (settings.engineUrl(), settings.engineKey());
        auto now = client.health();
        juce::Array<EngineModel> found;
        if (now.reachable)
        {
            juce::String ignored;
            found = client.models (ignored);
        }
        if (threadShouldExit()) break;

        bool changed = false;
        {
            const juce::ScopedLock sl (lock);
            changed = ! checked.load() || now.reachable != current.reachable || now.modelsReady != current.modelsReady
                   || now.languageModelReady != current.languageModelReady || now.model != current.model
                   || now.error != current.error || found.size() != modelList.size();
            current = now;
            if (now.reachable) modelList = found;
        }
        checked.store (true);
        if (changed) sendChangeMessage();

        // quick while someone is looking at the plugin and waiting for the engine, relaxed otherwise
        const bool waiting = watchers.load() > 0 && ! now.reachable;
        wait (waiting ? 1500 : (watchers.load() > 0 ? 5000 : 15000));
    }
}

//==============================================================================
Generator::Generator (Settings& s, Library& l, EngineWatch& w)
    : juce::Thread ("MU Music Maker generator"), settings (s), library (l), watch (w)
{
    startThread();
}

Generator::~Generator() { signalThreadShouldExit(); notify(); stopThread (10000); }

juce::Array<Job> Generator::jobs() const
{
    const juce::ScopedLock sl (lock);
    return list;
}

bool Generator::busy() const
{
    const juce::ScopedLock sl (lock);
    for (auto& j : list)
        if (j.active()) return true;
    return false;
}

juce::String Generator::add (const SongRequest& request)
{
    Job j;
    j.id = juce::Uuid().toString().substring (0, 12);
    j.request = request;
    j.title = request.suggestedTitle();
    j.message = "Waiting its turn";
    j.startedMs = juce::Time::currentTimeMillis();
    {
        const juce::ScopedLock sl (lock);
        list.insert (0, j);
    }
    sendChangeMessage();
    notify();
    return j.id;
}

void Generator::cancel (const juce::String& jobId)
{
    {
        const juce::ScopedLock sl (lock);
        cancelledIds.addIfNotAlreadyThere (jobId);
        for (auto& j : list)
            if (j.id == jobId && j.state == Job::State::waiting)
            {
                j.state = Job::State::cancelled;
                j.message = "Cancelled";
                j.finishedMs = juce::Time::currentTimeMillis();
            }
    }
    sendChangeMessage();
    notify();
}

void Generator::dismiss (const juce::String& jobId)
{
    {
        const juce::ScopedLock sl (lock);
        for (int i = list.size(); --i >= 0;)
            if (list.getReference (i).id == jobId && ! list.getReference (i).active())
                list.remove (i);
    }
    sendChangeMessage();
}

bool Generator::isCancelled (const juce::String& jobId) const
{
    const juce::ScopedLock sl (lock);
    return cancelledIds.contains (jobId);
}

void Generator::update (const juce::String& jobId, std::function<void (Job&)> change)
{
    {
        const juce::ScopedLock sl (lock);
        for (auto& j : list)
            if (j.id == jobId) change (j);
    }
    sendChangeMessage();
}

void Generator::run()
{
    while (! threadShouldExit())
    {
        juce::String next;
        {
            const juce::ScopedLock sl (lock);
            for (int i = list.size(); --i >= 0;)        // oldest first
                if (list.getReference (i).state == Job::State::waiting) { next = list.getReference (i).id; break; }
        }
        if (next.isEmpty()) { wait (-1); continue; }
        process (next);
    }
}

void Generator::process (const juce::String& jobId)
{
    SongRequest request;
    juce::String title;
    {
        const juce::ScopedLock sl (lock);
        for (auto& j : list)
            if (j.id == jobId) { request = j.request; title = j.title; }
    }

    auto stopped = [this, &jobId] { return threadShouldExit() || isCancelled (jobId); };
    auto finish = [this, &jobId] (Job::State state, const juce::String& message)
    {
        update (jobId, [&] (Job& j) { j.state = state; j.message = message; j.finishedMs = juce::Time::currentTimeMillis(); if (state == Job::State::done) j.progress = 1.0; });
    };

    update (jobId, [] (Job& j) { j.state = Job::State::sending; j.message = "Sending it to the engine"; j.progress = 0.0; });

    EngineClient client (settings.engineUrl(), settings.engineKey());
    const auto health = client.health (2500);
    if (! health.reachable)
    {
        finish (Job::State::failed, "The engine is not running. Start it in Settings, then try again.");
        watch.checkSoon();
        return;
    }
   #if JUCE_MAC && JUCE_ARM
    const auto host = splitUrl (settings.engineUrl()).host;
    if (host == "127.0.0.1" || host == "localhost") client.writerBackend = "mlx";
   #endif

    // The first go lets the engine's own writer do the lyrics and the planning. If that writer is not
    // loaded on this computer the engine says so, and the song goes again without it.
    TaskUpdate latest;
    juce::String error;
    bool useWriter = true, withoutWriter = false;
    for (;;)
    {
        juce::String taskId;
        if (! client.submit (request, settings.audioFormat(), useWriter, taskId, error, stopped))
        {
            finish (stopped() ? Job::State::cancelled : Job::State::failed, stopped() ? juce::String ("Cancelled") : error);
            return;
        }

        update (jobId, [] (Job& j) { j.state = Job::State::making; j.message = "Waiting for the engine"; });

        // ask how it is getting on until it has finished
        int misses = 0;
        bool failed = false;
        const auto began = juce::Time::getMillisecondCounter();
        for (;;)
        {
            if (stopped()) { finish (Job::State::cancelled, "Cancelled"); return; }
            wait (pollMs);
            if (stopped()) { finish (Job::State::cancelled, "Cancelled"); return; }

            TaskUpdate now;
            if (! client.query (taskId, now, error))
            {
                // the engine can go quiet for a moment while it is working flat out; only give up if it stays quiet
                if (++misses >= 45) { finish (Job::State::failed, "Lost touch with the engine: " + error); watch.checkSoon(); return; }
                continue;
            }
            misses = 0;
            latest = now;
            if (now.status == 2) { failed = true; break; }
            if (now.status == 1) break;

            update (jobId, [&] (Job& j)
            {
                j.progress = juce::jmax (j.progress, now.progress * 0.92);
                j.message = EngineClient::friendlyStage (now);
            });

            if (juce::Time::getMillisecondCounter() - began > 60u * 60u * 1000u) { finish (Job::State::failed, "The engine took over an hour, so this one was dropped."); return; }
        }

        if (! failed) break;
        if (useWriter && EngineClient::needsWriter (request) && EngineClient::isWriterMissing (latest.error))
        {
            useWriter = false;
            withoutWriter = true;
            update (jobId, [] (Job& j) { j.message = "The engine's writer is off, going without it"; j.progress = 0.0; });
            continue;
        }
        finish (Job::State::failed, latest.error);
        return;
    }

    // bring the audio into the library
    update (jobId, [] (Job& j) { j.state = Job::State::fetching; j.message = "Bringing the audio in"; j.progress = 0.93; });
    juce::StringArray made;
    const int total = latest.items.size();
    for (int i = 0; i < total; ++i)
    {
        if (stopped()) break;
        const auto& item = latest.items.getReference (i);
        auto extension = juce::URL::removeEscapeChars (item.fileUrl).fromLastOccurrenceOf (".", true, false).toLowerCase();
        if (extension.length() > 5 || extension.containsAnyOf ("/\\?&=")) extension = "." + settings.audioFormat();
        const auto incoming = library.folder().getChildFile (".incoming-" + jobId + "-" + juce::String (i + 1) + extension);

        if (! client.download (item.fileUrl, incoming, error, stopped))
        {
            if (made.isEmpty() && i == total - 1) { finish (Job::State::failed, "The song was made but could not be brought in: " + error); return; }
            continue;
        }

        Song s;
        s.title = total > 1 ? title + " " + juce::String (i + 1) : title;
        s.style = item.prompt.isNotEmpty() ? item.prompt : (request.mode == SongRequest::Mode::custom ? request.style : request.description);
        s.lyrics = item.lyrics.trim().equalsIgnoreCase ("[Instrumental]") ? juce::String() : item.lyrics;
        s.bpm = item.bpm > 0.0 ? item.bpm : (double) request.bpm;
        s.seconds = item.seconds > 0.0 ? item.seconds : request.seconds;
        s.key = item.key.isNotEmpty() ? item.key : request.key;
        s.timeSignature = item.timeSignature.isNotEmpty() ? item.timeSignature : request.timeSignature;
        s.model = item.model;
        s.seed = item.seed;
        s.jobId = jobId;
        s.variation = i + 1;
        s.variationsTotal = total;
        s.request = request.toVar();
        const auto saved = library.add (s, incoming);
        if (saved.id.isNotEmpty()) made.add (saved.id);
        incoming.deleteFile();
        update (jobId, [&] (Job& j) { j.progress = 0.93 + 0.07 * (double) (i + 1) / (double) juce::jmax (1, total); j.songIds = made; });
    }

    if (made.isEmpty()) { finish (stopped() ? Job::State::cancelled : Job::State::failed, stopped() ? juce::String ("Cancelled") : juce::String ("The song could not be brought in.")); return; }
    auto summary = made.size() == 1 ? juce::String ("1 song ready") : juce::String (made.size()) + " songs ready";
    if (withoutWriter && request.mode == SongRequest::Mode::simple && ! request.instrumental)
        summary << ", without lyrics because the engine's writer is off";
    finish (Job::State::done, summary);
}
}
