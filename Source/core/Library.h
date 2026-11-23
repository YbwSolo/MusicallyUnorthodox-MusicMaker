#pragma once
#include "Settings.h"
#include <juce_events/juce_events.h>

namespace mm
{
struct Song
{
    juce::String id, title, style, lyrics, key, timeSignature, model, seed, fileName, jobId;
    double bpm = 0.0, seconds = 0.0;
    bool liked = false;
    juce::int64 createdMs = 0;
    int variation = 1, variationsTotal = 1;
    juce::var request;              // what was asked for, so it can be made again

    juce::var toVar() const;
    static Song fromVar (const juce::var&);
    juce::String lengthText() const;
    juce::String detailText() const;    // "140 BPM  ·  C minor"
};

/** Every song that has been made, kept in Music/MU Music Maker. Shared by every instance in the host. */
class Library : public juce::ChangeBroadcaster
{
public:
    Library();
    explicit Library (const juce::File& folder);     // for the tests

    juce::Array<Song> songs() const;                 // newest first
    juce::Array<Song> search (const juce::String& text, bool likedOnly) const;
    bool find (const juce::String& id, Song& out) const;
    int count() const;

    /** Moves the audio into the library under a readable name and remembers the song. */
    Song add (Song, const juce::File& audio);
    void remove (const juce::String& id);
    void setLiked (const juce::String& id, bool liked);
    void rename (const juce::String& id, const juce::String& title);

    juce::File audioOf (const Song&) const;
    juce::File folder() const                        { return songsFolder; }

    static juce::String safeFileName (const juce::String&);
    static juce::String formatLength (double seconds);

private:
    void load();
    void save();

    juce::File songsFolder, indexFile;
    mutable juce::CriticalSection lock;
    juce::Array<Song> all;
};
}
