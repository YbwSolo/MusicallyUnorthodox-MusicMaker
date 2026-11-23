#include "PluginProcessor.h"
#include "PluginEditor.h"

MakerProcessor::MakerProcessor()
    : juce::AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    auto& s = services().settings;
    draft.variations = juce::jlimit (1, 4, (int) s.getNumber ("defaultVariations", 2.0));
    player.setGain ((float) s.getNumber ("playerGain", 0.9));

    recorder.onFinished = [this] (juce::File file, double)
    {
        // a take caught from the track becomes the audio to start from
        draft.audioPath = file.getFullPathName();
        if (draft.source == mm::SongRequest::Source::none) draft.source = mm::SongRequest::Source::cover;
        draftChanged();
    };
}

MakerProcessor::~MakerProcessor() { recorder.onFinished = nullptr; }

void MakerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    player.prepare (sampleRate, samplesPerBlock);
    recorder.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
}

void MakerProcessor::releaseResources()
{
    player.release();
    recorder.release();
}

bool MakerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == out || in.isDisabled();
}

double MakerProcessor::getTailLengthSeconds() const
{
    // while a song is playing the host should keep asking for sound even if the track itself is silent
    return player.playing() ? juce::jmax (0.0, player.length() - player.position()) : 0.0;
}

void MakerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    bool hostPlaying = false, hostHasTransport = false;
    if (auto* head = getPlayHead())
        if (auto position = head->getPosition())
        {
            hostHasTransport = true;
            hostPlaying = position->getIsPlaying();
            if (auto tempo = position->getBpm()) bpm.store (*tempo);
        }

    if (getTotalNumInputChannels() > 0)
        recorder.process (buffer, hostPlaying, hostHasTransport, isNonRealtime());
    player.process (buffer);
}

bool MakerProcessor::playSong (const mm::Song& song)
{
    const auto file = services().library.audioOf (song);
    if (! file.existsAsFile()) return false;
    if (player.songId() != song.id || player.file() != file)
        if (! player.load (file, song.id)) return false;
    player.play();
    return true;
}

juce::AudioProcessorEditor* MakerProcessor::createEditor() { return new MakerEditor (*this); }

void MakerProcessor::getStateInformation (juce::MemoryBlock& data)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("draft", draft.toVar());
    o->setProperty ("followProjectTempo", followProjectTempo);
    const auto text = juce::JSON::toString (juce::var (o), true);
    data.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
}

void MakerProcessor::setStateInformation (const void* data, int size)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), size));
    if (! v.isObject()) return;
    draft = mm::SongRequest::fromVar (v["draft"]);
    followProjectTempo = v.hasProperty ("followProjectTempo") ? (bool) v["followProjectTempo"] : true;
    draftChanged();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MakerProcessor(); }
