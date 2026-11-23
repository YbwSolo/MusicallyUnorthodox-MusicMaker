#include "Pages.h"
#include "SongList.h"
#include "../PluginEditor.h"

namespace mm
{
namespace
{
class LibraryPage : public Page
{
public:
    explicit LibraryPage (MakerEditor& e) : editor (e), songs (e)
    {
        theme::styleEditor (search, "Search by name, style or lyrics");
        search.setIndents (34, 0);
        search.onTextChange = [this] { refresh(); };
        addAndMakeVisible (search);

        filter.onChange = [this] (int) { refresh(); };
        addAndMakeVisible (filter);

        folderButton.setButtonText ("Open the folder");
        folderButton.onClick = [this] { editor.services.library.folder().revealToUser(); };
        addAndMakeVisible (folderButton);

        addAndMakeVisible (songs);
        refresh();
    }

    void refresh() override
    {
        const bool likedOnly = filter.selected() == 1;
        const auto found = editor.services.library.search (search.getText(), likedOnly);
        const bool empty = editor.services.library.count() == 0;
        if (empty)              songs.setEmptyMessage ("Your library is empty", "Songs you make on the Create page are kept here, on this computer, in Music > MU Music Maker.");
        else if (likedOnly && search.getText().trim().isEmpty()) songs.setEmptyMessage ("No liked songs yet", "Press the heart on a song and it shows up here.");
        else                    songs.setEmptyMessage ("Nothing matches", "Try a different word, or clear the search.");
        songs.setSongs (found);
        shownCount = found.size();
        repaint();
    }

    void tick() override { songs.playerChanged(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::text);
        g.setFont (theme::headline (24.0f));
        g.drawText ("LIBRARY", 28, 20, 200, 30, juce::Justification::centredLeft);
        g.setColour (theme::textMute);
        g.setFont (theme::body (12.0f));
        g.drawText (juce::String (shownCount) + (shownCount == 1 ? " song" : " songs") + ". Drag one out of the plugin to drop it into your project.",
                    28, 110, getWidth() - 56, 18, juce::Justification::centredLeft, true);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        theme::drawIcon (g, theme::Icon::search, juce::Rectangle<float> ((float) search.getX() + 11.0f, (float) search.getY() + 9.0f, 16.0f, 16.0f), theme::textMute, 1.7f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (28, 0);
        r.removeFromTop (66);
        auto bar = r.removeFromTop (34);
        folderButton.setBounds (bar.removeFromRight (130));
        bar.removeFromRight (10);
        filter.setBounds (bar.removeFromRight (150));
        bar.removeFromRight (10);
        search.setBounds (bar);
        r.removeFromTop (38);
        songs.setBounds (r.expanded (10, 0).withTrimmedBottom (8));
    }

private:
    MakerEditor& editor;
    juce::TextEditor search;
    Segmented filter { { "All", "Liked" } };
    juce::TextButton folderButton;
    SongList songs;
    int shownCount = 0;
};
}

std::unique_ptr<Page> makeLibraryPage (MakerEditor& e) { return std::make_unique<LibraryPage> (e); }
}
