#include "Pages.h"
#include "../PluginEditor.h"

namespace mm
{
namespace
{
struct Section { const char* title; const char* body; };

const Section kSections[] = {
    { "WHAT THIS IS",
      "MU Music Maker makes whole songs from words: beats, instruments and singing. You describe what you want or write the lyrics yourself, "
      "it makes a few versions, and you drag the one you like straight into your project.\n\n"
      "The music is made by an engine called ACE-Step that runs on your own computer, so it is free to use as much as you like and nothing leaves the machine. "
      "The engine is separate from the plugin and has to be set up once." },

    { "SETTING UP THE ENGINE (ONCE)",
      "1.  Go to Settings and press Set up the engine. A window opens and does the work by itself. It downloads about 10 GB, so give it time and leave it open.\n"
      "2.  When that window says All done, come back to Settings and press Start the engine. The first start takes a minute or two while it loads.\n"
      "3.  The dot at the bottom left turns green and says Engine ready. From then on the plugin starts the engine by itself whenever you open it.\n\n"
      "What it needs: on a Mac, an Apple Silicon machine (M1 or newer) with 16 GB of memory is comfortable. On Windows, an NVIDIA graphics card with 6 GB or more. "
      "It wants about 20 GB of free disk space." },

    { "MAKING A SONG",
      "Describe it is the quick way: say what you want in a sentence or two, the style, the mood, what it is about, and the engine writes the lyrics and the music. "
      "Press Give me an idea if you are stuck.\n\n"
      "Write it yourself gives you the controls. Put the genre, mood and instruments in Style (the tags underneath add them for you), and write your own lyrics. "
      "Start each part with a tag like [Verse] or [Chorus] on its own line so the engine knows the shape of the song. Write it out for me turns a rough idea into a full sheet.\n\n"
      "Under Music you can set the tempo, key and length, or leave them on Auto. Use the project's tempo makes the song at the speed your project is set to, so it drops in on the grid. "
      "Versions is how many takes you get back: two is a good number, since no two come out the same." },

    { "STARTING FROM AUDIO",
      "A song does not have to start from nothing.\n\n"
      "Its sound: give it a piece of audio and the new song borrows that sound, the instruments and the feel, with a new tune.\n"
      "Remake it: the tune and timing of your audio stay and the style changes. Hum a melody or play a rough loop and have it turned into a finished beat.\n"
      "Redo a part: keep a song and have the engine make one stretch of it again, between the two times you give.\n\n"
      "To give it audio, press Catch from the track and press play in your project: the plugin records what its track is playing and stops when you stop. "
      "Or choose a file, or drop one onto the plugin. Any song in your library can be used too, from its menu." },

    { "GETTING IT INTO YOUR PROJECT",
      "Every song is a normal audio file. Drag a row out of the list, or drag the Drag to project handle at the bottom, and drop it on a track in your DAW.\n\n"
      "Songs play back through the plugin, so you hear them through whatever track it is on. If you hear nothing in Logic, select the plugin's track or put the plugin on the Stereo Out, "
      "since Logic only runs a plugin when its track is in use.\n\n"
      "The files live in Music > MU Music Maker > Songs. The heart keeps your favourites together in the Library." },

    { "USING A SECOND COMPUTER",
      "The engine can run on one computer while you make music on another, for example a PC with a strong graphics card doing the work for a laptop.\n\n"
      "On the computer with the engine: open the plugin, tick Let other computers on my network use this engine in Settings, and start the engine again.\n"
      "On the other computer: in Settings, change Where the engine is to the first computer's address, like http://192.168.1.20:8001. Both have to be on the same network." },

    { "IF SOMETHING GOES WRONG",
      "Engine not running: press Start the engine in Settings. If it will not start, Show the engine's log says why.\n"
      "It stays on Engine is starting: the very first start also downloads anything that is missing, which can take several minutes.\n"
      "A song fails straight away: the computer may be out of memory. Close other heavy apps, ask for one version instead of two, or a shorter length.\n"
      "The song has no words: on computers with less memory the engine leaves its lyric writer switched off. Use Write it yourself and type the lyrics in.\n"
      "It sounds rough: turn Quality up under More options, or try again. Results vary from take to take, which is why you get more than one." } };

class HelpContent : public juce::Component
{
public:
    int layout (int width)
    {
        boxes.clear();
        int y = 0;
        for (auto& s : kSections)
        {
            const auto body = juce::String::fromUTF8 (s.body);
            const int h = 20 + 24 + (int) std::ceil (theme::wrappedHeight (body, (float) width - 44.0f, theme::body (13.5f), 1.5f)) + 22;
            boxes.add ({ 0, y, width, h });
            y += h + 14;
        }
        return y + 10;
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < boxes.size(); ++i)
        {
            const auto box = boxes.getReference (i);
            theme::fillPanel (g, box.toFloat(), theme::panel, 12.0f);
            g.setColour (i == 1 ? theme::yellow : theme::text);
            g.setFont (theme::headline (16.0f));
            g.drawText (kSections[i].title, box.getX() + 22, box.getY() + 18, box.getWidth() - 44, 22, juce::Justification::centredLeft);
            theme::drawWrapped (g, juce::String::fromUTF8 (kSections[i].body),
                                juce::Rectangle<float> ((float) box.getX() + 22.0f, (float) box.getY() + 48.0f, (float) box.getWidth() - 44.0f, (float) box.getHeight() - 60.0f),
                                theme::body (13.5f), theme::textDim, 1.5f);
        }
    }

private:
    juce::Array<juce::Rectangle<int>> boxes;
};

class HelpPage : public Page
{
public:
    explicit HelpPage (MakerEditor&)
    {
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (9);
        addAndMakeVisible (viewport);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::text);
        g.setFont (theme::headline (24.0f));
        g.drawText ("HELP", 28, 20, 300, 30, juce::Justification::centredLeft);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds().withTrimmedTop (66).withTrimmedLeft (28).withTrimmedRight (14));
        const int width = juce::jmin (760, juce::jmax (300, viewport.getWidth() - 24));
        content.setSize (width, content.layout (width));
    }

private:
    juce::Viewport viewport;
    HelpContent content;
};
}

std::unique_ptr<Page> makeHelpPage (MakerEditor& e) { return std::make_unique<HelpPage> (e); }
}
