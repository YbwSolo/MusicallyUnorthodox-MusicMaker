#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace mm
{
double audioLengthSeconds (const juce::File&);      // 0 when it cannot be read

/** Plays a song from the library out through the plugin, on top of whatever the track is already playing. */
class Player : public juce::ChangeBroadcaster
{
public:
    Player();
    ~Player() override;

    void prepare (double sampleRate, int blockSize);
    void release();
    void process (juce::AudioBuffer<float>&);        // audio thread

    bool load (const juce::File&, const juce::String& songId);
    void play();
    void pause();
    void stop();                                     // pause and go back to the start
    void unload();
    bool playing() const                             { return transport.isPlaying(); }
    double position() const                          { return transport.getCurrentPosition(); }
    double length() const                            { return transport.getLengthInSeconds(); }
    void setPosition (double seconds);
    void setGain (float gain);
    float gain() const                               { return currentGain; }
    juce::String songId() const                      { return currentId; }
    juce::File file() const                          { return currentFile; }
    bool finished() const                            { return transport.hasStreamFinished(); }
    juce::AudioFormatManager& formatManager()        { return formats; }

private:
    juce::AudioFormatManager formats;
    juce::TimeSliceThread readAhead { "MU Music Maker player" };
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> source;
    juce::AudioBuffer<float> scratch;
    juce::String currentId;
    juce::File currentFile;
    float currentGain = 0.9f;
    double rate = 0.0;
    int block = 0;
};
}
