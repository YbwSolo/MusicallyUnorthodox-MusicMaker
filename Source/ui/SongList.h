#pragma once
#include "Pages.h"

class MakerEditor;

namespace mm
{
/** A scrolling list of songs. Click the cover to play, the heart to like, the dots for more,
    and drag a row out of the plugin to drop the song into the project. */
class SongList : public juce::Component, private juce::ListBoxModel
{
public:
    explicit SongList (MakerEditor&);

    void setSongs (const juce::Array<Song>&);
    void setEmptyMessage (const juce::String& title, const juce::String& body);
    void playerChanged()                        { list.repaint(); }
    int numSongs() const                        { return songs.size(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int rowHeight = 66;

private:
    class Row;
    int getNumRows() override                   { return songs.size(); }
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    MakerEditor& editor;
    juce::ListBox list;
    juce::Array<Song> songs;
    juce::String emptyTitle, emptyBody;
};
}
