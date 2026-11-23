#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

namespace mm
{
/** Catches the audio passing through the track, to use as the starting point for a song.
    The audio thread only pushes samples into a lock-free queue; a background thread writes the file. */
class Recorder : private juce::Timer
{
public:
    enum class State { idle, armed, recording };

    explicit Recorder (juce::File folder);
    ~Recorder() override;

    void prepare (double sampleRate, int numChannels);
    void release();
    /** Audio thread. When the host is rendering faster than real time, it waits for the file to keep up. */
    void process (const juce::AudioBuffer<float>&, bool hostIsPlaying, bool hostHasTransport, bool offlineRender = false);

    void arm();             // starts with the host's play button, or at once in a host that has none
    void startNow();        // do not wait for play
    void stop();            // keep what was caught, or stop waiting

    State state() const     { return currentState.load(); }
    double seconds() const;
    float level() const     { return peak.load(); }

    /** Called on the message thread with the finished WAV and its length. */
    std::function<void (juce::File, double)> onFinished;

    static constexpr double longestTake = 600.0;
    static constexpr double shortestTake = 1.0;

private:
    void timerCallback() override;
    bool openWriter();
    void finish (bool keep);

    juce::File folder, currentFile;
    juce::TimeSliceThread writerThread { "MU Music Maker recorder" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::SpinLock writerLock;
    std::atomic<State> currentState { State::idle };
    std::atomic<bool> force { false }, sawPlaying { false }, takeEnded { false };
    std::atomic<juce::int64> samplesWritten { 0 };
    std::atomic<float> peak { 0.0f };
    double rate = 44100.0;
    int channels = 2;
};
}
