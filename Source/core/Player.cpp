#include "Player.h"

namespace mm
{
double audioLengthSeconds (const juce::File& file)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formats.createReaderFor (file)))
        if (reader->sampleRate > 0.0) return (double) reader->lengthInSamples / reader->sampleRate;
    return 0.0;
}

Player::Player()
{
    formats.registerBasicFormats();
    readAhead.startThread();
    transport.setGain (currentGain);
}

Player::~Player()
{
    transport.setSource (nullptr);
    source.reset();
    readAhead.stopThread (4000);
}

void Player::prepare (double sampleRate, int blockSize)
{
    rate = sampleRate;
    block = juce::jmax (16, blockSize);
    scratch.setSize (2, block, false, false, true);
    transport.prepareToPlay (block, rate);
}

void Player::release() { transport.releaseResources(); }

void Player::process (juce::AudioBuffer<float>& out)
{
    if (! transport.isPlaying() || out.getNumChannels() == 0 || scratch.getNumSamples() == 0) return;

    int done = 0;
    while (done < out.getNumSamples())
    {
        const int n = juce::jmin (scratch.getNumSamples(), out.getNumSamples() - done);
        juce::AudioSourceChannelInfo info (&scratch, 0, n);
        transport.getNextAudioBlock (info);
        if (out.getNumChannels() == 1)
        {
            out.addFrom (0, done, scratch, 0, 0, n, 0.5f);
            out.addFrom (0, done, scratch, 1, 0, n, 0.5f);
        }
        else
        {
            out.addFrom (0, done, scratch, 0, 0, n);
            out.addFrom (1, done, scratch, 1, 0, n);
        }
        done += n;
    }
}

bool Player::load (const juce::File& file, const juce::String& songId)
{
    auto* reader = formats.createReaderFor (file);
    if (reader == nullptr) return false;

    transport.stop();
    transport.setSource (nullptr);
    auto next = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    transport.setSource (next.get(), 32768, &readAhead, reader->sampleRate, 2);
    source = std::move (next);
    currentId = songId;
    currentFile = file;
    sendChangeMessage();
    return true;
}

void Player::unload()
{
    transport.stop();
    transport.setSource (nullptr);
    source.reset();
    currentId.clear();
    currentFile = juce::File();
    sendChangeMessage();
}

void Player::play()
{
    if (source == nullptr) return;
    if (transport.hasStreamFinished() || transport.getCurrentPosition() >= transport.getLengthInSeconds() - 0.05) transport.setPosition (0.0);
    transport.start();
    sendChangeMessage();
}

void Player::pause()                    { transport.stop(); sendChangeMessage(); }
void Player::stop()                     { transport.stop(); transport.setPosition (0.0); sendChangeMessage(); }
void Player::setPosition (double s)     { transport.setPosition (juce::jlimit (0.0, juce::jmax (0.0, transport.getLengthInSeconds()), s)); }
void Player::setGain (float g)          { currentGain = juce::jlimit (0.0f, 1.5f, g); transport.setGain (currentGain); }
}
