#include "Recorder.h"

namespace mm
{
Recorder::Recorder (juce::File f) : folder (std::move (f))
{
    writerThread.startThread();
    startTimerHz (10);
}

Recorder::~Recorder()
{
    stopTimer();
    onFinished = nullptr;
    finish (false);
    writerThread.stopThread (4000);
}

void Recorder::prepare (double sampleRate, int numChannels)
{
    // a change of sample rate ends the take, since the file was opened at the old one
    if (currentState.load() != State::idle && (std::abs (sampleRate - rate) > 0.5 || juce::jlimit (1, 2, numChannels) != channels)) finish (true);
    rate = sampleRate > 0.0 ? sampleRate : 44100.0;
    channels = juce::jlimit (1, 2, numChannels);
}

void Recorder::release() {}

double Recorder::seconds() const { return (double) samplesWritten.load() / rate; }

bool Recorder::openWriter()
{
    folder.createDirectory();
    // never the name of a take that is already there, even when two are caught in the same second
    currentFile = folder.getNonexistentChildFile ("Take " + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H-%M-%S"), ".wav", false);
    auto stream = currentFile.createOutputStream();
    if (stream == nullptr) return false;

    juce::WavAudioFormat wav;
    auto* w = wav.createWriterFor (stream.get(), rate, (unsigned int) channels, 24, {}, 0);
    if (w == nullptr) return false;
    stream.release();       // the writer owns it now

    auto threaded = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (w, writerThread, 1 << 18);
    const juce::SpinLock::ScopedLockType sl (writerLock);
    writer = std::move (threaded);
    return true;
}

void Recorder::arm()
{
    if (currentState.load() != State::idle) return;
    samplesWritten.store (0);
    sawPlaying.store (false);
    takeEnded.store (false);
    force.store (false);
    if (openWriter()) currentState.store (State::armed);
}

void Recorder::startNow()
{
    if (currentState.load() == State::idle) arm();
    force.store (true);
}

void Recorder::stop()
{
    if (currentState.load() == State::idle) return;
    finish (currentState.load() == State::recording);
}

void Recorder::process (const juce::AudioBuffer<float>& buffer, bool hostIsPlaying, bool hostHasTransport, bool offlineRender)
{
    float p = 0.0f;
    for (int c = 0; c < juce::jmin (2, buffer.getNumChannels()); ++c)
        p = juce::jmax (p, buffer.getMagnitude (c, 0, buffer.getNumSamples()));
    peak.store (juce::jmax (p, peak.load() * 0.92f));

    auto s = currentState.load();
    if (s == State::idle || takeEnded.load()) return;

    const bool go = force.load() || ! hostHasTransport || hostIsPlaying;
    if (s == State::armed)
    {
        if (! go) return;
        currentState.store (State::recording);
        s = State::recording;
    }

    if (hostIsPlaying) sawPlaying.store (true);
    if (hostHasTransport && ! force.load() && sawPlaying.load() && ! hostIsPlaying) { takeEnded.store (true); return; }     // the host stopped

    const juce::SpinLock::ScopedTryLockType sl (writerLock);
    if (! sl.isLocked() || writer == nullptr) return;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < channels) return;
    bool kept = writer->write (buffer.getArrayOfReadPointers(), n);
    for (int tries = 0; ! kept && offlineRender && tries < 4000; ++tries)
    {
        juce::Thread::sleep (1);
        kept = writer->write (buffer.getArrayOfReadPointers(), n);
    }
    if (kept) samplesWritten.fetch_add (n);
    if ((double) samplesWritten.load() / rate >= longestTake) takeEnded.store (true);
}

void Recorder::timerCallback()
{
    if (takeEnded.load() && currentState.load() != State::idle) finish (true);
}

void Recorder::finish (bool keep)
{
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> closing;
    {
        const juce::SpinLock::ScopedLockType sl (writerLock);
        closing = std::move (writer);
    }
    closing.reset();        // writes what is left and closes the file

    const bool was = currentState.exchange (State::idle) != State::idle;
    takeEnded.store (false);
    force.store (false);
    const double length = seconds();
    const auto file = currentFile;
    currentFile = juce::File();
    if (! was) return;

    if (! keep || length < shortestTake) { file.deleteFile(); return; }
    if (onFinished != nullptr) onFinished (file, length);
}
}
