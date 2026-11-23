#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "core/Services.h"
#include "core/Player.h"
#include "core/Recorder.h"

/** Sits on any track. The track's own sound passes straight through; songs from the library play on top of it,
    and what the track plays can be caught to use as the starting point for a new song. */
class MakerProcessor : public juce::AudioProcessor, public juce::ChangeBroadcaster
{
public:
    MakerProcessor();
    ~MakerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                         { return true; }
    const juce::String getName() const override             { return "MU Music Maker"; }
    bool acceptsMidi() const override                       { return false; }
    bool producesMidi() const override                      { return false; }
    double getTailLengthSeconds() const override;
    int getNumPrograms() override                           { return 1; }
    int getCurrentProgram() override                        { return 0; }
    void setCurrentProgram (int) override                   {}
    const juce::String getProgramName (int) override        { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    mm::Services& services()                                { return *shared; }
    mm::Player player;
    mm::Recorder recorder { mm::Settings::takesDir() };

    /** What is on the Create page. Saved with the project. */
    mm::SongRequest draft;
    bool followProjectTempo = true;
    void draftChanged()                                     { sendChangeMessage(); }

    double hostBpm() const                                  { return bpm.load(); }
    bool hostKnowsTempo() const                             { return bpm.load() > 0.0; }

    bool playSong (const mm::Song&);                        // false when the file cannot be read

private:
    juce::SharedResourcePointer<mm::Services> shared;
    std::atomic<double> bpm { 0.0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MakerProcessor)
};
