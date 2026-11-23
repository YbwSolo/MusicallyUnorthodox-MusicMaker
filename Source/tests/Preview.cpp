// Draws every page of the plugin to a PNG with some example songs in it. Run with HOME pointing at an empty folder.
//   MUMPreview <output folder>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mm;

static juce::File makeAudio (const juce::File& dir, const juce::String& name, double seconds)
{
    auto file = dir.getChildFile (name + ".wav");
    file.deleteFile();
    juce::WavAudioFormat wav;
    if (auto* stream = file.createOutputStream().release())
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream, 48000.0, 2, 16, {}, 0)))
        {
            juce::Random random (name.hashCode());
            juce::AudioBuffer<float> buffer (2, 4800);
            const int blocks = (int) (seconds * 10.0);
            for (int b = 0; b < blocks; ++b)
            {
                // something with the rough shape of a song: quiet in, loud in the middle, parts that drop
                const float place = (float) b / (float) blocks;
                const float shape = (0.25f + 0.75f * std::sin (place * 3.1f)) * ((b / 40) % 5 == 3 ? 0.35f : 1.0f);
                for (int i = 0; i < 4800; ++i)
                {
                    const float beat = 1.0f - (float) (i % 2400) / 2400.0f;
                    const float v = shape * beat * (random.nextFloat() * 2.0f - 1.0f) * 0.8f;
                    buffer.setSample (0, i, v);
                    buffer.setSample (1, i, v);
                }
                writer->writeFromAudioSampleBuffer (buffer, 0, 4800);
            }
        }
    return file;
}

class PreviewApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "MUMPreview"; }
    const juce::String getApplicationVersion() override { return "1"; }

    void save (const juce::String& name)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (250);
        const auto shot = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        juce::PNGImageFormat png;
        auto file = out.getChildFile (name + ".png");
        file.deleteFile();
        if (auto stream = file.createOutputStream()) png.writeImageToStream (shot, *stream);
    }

    void initialise (const juce::String& commandLine) override
    {
        out = juce::File::getCurrentWorkingDirectory().getChildFile (commandLine.trim().unquoted().isEmpty() ? juce::String ("preview") : commandLine.trim().unquoted());
        out.createDirectory();
        const auto scratch = out.getChildFile ("scratch");
        scratch.createDirectory();

        processor = std::make_unique<MakerProcessor>();
        auto& s = processor->services();
        s.settings.setBool ("seenHelp", true);
        s.settings.set ("windowWidth", "1060");
        s.settings.set ("windowHeight", "700");

        struct Example { const char* title; const char* style; double bpm; const char* key; double seconds; bool liked; const char* lyrics; };
        const Example examples[] = {
            { "Cold Outside 2", "dark uk drill, sliding 808s, haunting piano, male vocals", 142, "F minor", 164, false, "[Verse]\nCold outside but the block stay warm\nCame from the bottom, been through the storm\n\n[Chorus]\nNo sleep, no sleep till we on" },
            { "Cold Outside 1", "dark uk drill, sliding 808s, haunting piano, male vocals", 142, "F minor", 171, true, "[Verse]\nCold outside but the block stay warm" },
            { "Late Night Drive", "smooth r&b, warm keys, female vocals, slow", 92, "Eb major", 198, true, "[Verse]\nTwo in the morning and I'm thinking of you" },
            { "Sunday Service", "soulful hip hop, chopped vocal samples, boom bap drums", 88, "A minor", 143, false, "" },
            { "Lagos Summer", "afrobeats, bright guitars, catchy hook, dancing", 108, "G major", 187, false, "[Chorus]\nDance till the morning light" },
            { "Study Tape", "lo-fi, dusty drums, jazzy chords, instrumental", 76, "D minor", 121, false, "" } };

        for (int i = juce::numElementsInArray (examples); --i >= 0;)
        {
            auto& e = examples[i];
            Song song;
            song.title = e.title; song.style = e.style; song.bpm = e.bpm; song.key = e.key; song.seconds = e.seconds; song.liked = e.liked; song.lyrics = e.lyrics;
            song.createdMs = juce::Time::currentTimeMillis() - (juce::int64) i * 60000;
            s.library.add (song, makeAudio (scratch, juce::String (e.title) + " src", 6.0));
        }

        auto& d = processor->draft;
        d.mode = SongRequest::Mode::simple;
        d.description = "A dark UK drill track with sliding 808s and a haunting piano, about coming up from nothing";

        editor.reset (dynamic_cast<MakerEditor*> (processor->createEditor()));
        editor->setVisible (true);
        editor->addToDesktop (0);
        editor->selectPage (MakerEditor::pageCreate);
        save ("1-create-describe");

        // a song playing, and the form in its fuller mode with audio to start from
        const auto songs = s.library.songs();
        if (! songs.isEmpty())
        {
            processor->prepareToPlay (48000.0, 512);
            processor->playSong (songs.getReference (1));
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            for (int i = 0; i < 180; ++i) { buffer.clear(); processor->processBlock (buffer, midi); }
        }
        d.mode = SongRequest::Mode::custom;
        d.title = "Cold Outside";
        d.style = "dark, uk drill, 808s, piano, male vocals";
        d.lyrics = "[Verse]\nCold outside but the block stay warm\nCame from the bottom, been through the storm\n\n[Chorus]\nNo sleep, no sleep till we on";
        d.key = "F minor";
        d.seconds = 150;
        processor->followProjectTempo = false;
        d.bpm = 142;
        processor->draftChanged();
        save ("2-create-write");

        d.source = SongRequest::Source::cover;
        d.audioPath = s.library.audioOf (songs.getReference (2)).getFullPathName();
        d.strength = 0.7;
        s.settings.setBool ("showMoreOptions", true);
        SongRequest queued = d;
        queued.title = "Night Shift";
        s.generator.add (queued);
        processor->draftChanged();
        editor.reset();
        editor.reset (dynamic_cast<MakerEditor*> (processor->createEditor()));
        editor->setVisible (true);
        editor->addToDesktop (0);
        save ("3-create-from-audio");
        editor->setSize (1060, 1560);
        save ("3b-create-whole-form");
        editor->setSize (1060, 700);

        editor->selectPage (MakerEditor::pageLibrary);
        save ("4-library");
        editor->selectPage (MakerEditor::pageSettings);
        save ("5-settings");
        editor->selectPage (MakerEditor::pageHelp);
        save ("6-help");
        editor->showLyrics (songs.getReference (0));
        save ("7-lyrics");

        quit();
    }

    void shutdown() override { editor.reset(); processor.reset(); }

private:
    juce::File out;
    std::unique_ptr<MakerProcessor> processor;
    std::unique_ptr<MakerEditor> editor;
};

START_JUCE_APPLICATION (PreviewApp)
