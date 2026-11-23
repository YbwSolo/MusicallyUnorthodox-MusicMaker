#include "SongList.h"
#include "../PluginEditor.h"

namespace mm
{
class SongList::Row : public juce::Component
{
public:
    explicit Row (MakerEditor& e) : editor (e) {}

    void set (const Song& s)
    {
        song = s;
        repaint();
    }

    juce::Rectangle<int> coverArea() const  { return { 10, (getHeight() - 46) / 2, 46, 46 }; }
    juce::Rectangle<int> dotsArea() const   { return { getWidth() - 40, (getHeight() - 30) / 2, 30, 30 }; }
    juce::Rectangle<int> heartArea() const  { return dotsArea().translated (-32, 0); }

    void paint (juce::Graphics& g) override
    {
        auto& player = editor.processor.player;
        const bool isCurrent = player.songId() == song.id;
        const bool isPlaying = isCurrent && player.playing();

        auto r = getLocalBounds().toFloat().reduced (2.0f, 2.0f);
        if (hover || isCurrent)
        {
            g.setColour (isCurrent ? theme::raised : theme::panel);
            g.fillRoundedRectangle (r, 9.0f);
        }

        const auto cover = coverArea().toFloat();
        theme::drawCover (g, cover, song.id, 7.0f);
        if (hover || isCurrent)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (cover, 7.0f);
            theme::drawIcon (g, isPlaying ? theme::Icon::pause : theme::Icon::play, cover.reduced (13.0f), isCurrent ? theme::yellow : juce::Colours::white, 2.0f);
        }

        const int right = heartArea().getX() - 8;
        const bool wide = getWidth() > 520;
        const int lengthWidth = 46;
        const int detailWidth = wide ? 150 : 0;
        int x = coverArea().getRight() + 14;
        const int textRight = right - lengthWidth - detailWidth - 10;

        g.setColour (isCurrent ? theme::yellow : theme::text);
        g.setFont (theme::bodyBold (14.0f));
        g.drawText (song.title, x, 12, textRight - x, 20, juce::Justification::centredLeft, true);
        g.setColour (theme::textDim);
        g.setFont (theme::body (12.0f));
        g.drawText (song.style.replaceCharacters ("\r\n", "  "), x, 33, textRight - x, 18, juce::Justification::centredLeft, true);

        if (wide)
        {
            g.setColour (theme::textMute);
            g.setFont (theme::bodyBold (11.0f));
            g.drawText (song.detailText(), textRight + 10, 0, detailWidth, getHeight(), juce::Justification::centredRight, true);
        }
        g.setColour (theme::textDim);
        g.setFont (theme::body (12.0f));
        g.drawText (song.lengthText(), right - lengthWidth, 0, lengthWidth, getHeight(), juce::Justification::centredRight);

        theme::drawIcon (g, song.liked ? theme::Icon::heartFilled : theme::Icon::heart, heartArea().toFloat().reduced (6.5f),
                         song.liked ? theme::yellow : (hoverHeart ? theme::text : theme::textMute), 1.7f);
        theme::drawIcon (g, theme::Icon::dots, dotsArea().toFloat().reduced (6.0f), hoverDots ? theme::text : theme::textMute, 1.7f);
    }

    void mouseEnter (const juce::MouseEvent&) override  { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { hover = hoverHeart = hoverDots = false; repaint(); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool h = heartArea().contains (e.getPosition()), d = dotsArea().contains (e.getPosition());
        if (h != hoverHeart || d != hoverDots) { hoverHeart = h; hoverDots = d; repaint(); }
        setMouseCursor (h || d || coverArea().contains (e.getPosition()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = false;
        if (e.mods.isPopupMenu()) { dragged = true; editor.songMenu (song, this); }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragged || e.getDistanceFromDragStart() < 8) return;
        if (heartArea().contains (e.getMouseDownPosition()) || dotsArea().contains (e.getMouseDownPosition())) return;
        dragged = true;
        editor.dragOut (song, this);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragged || ! getLocalBounds().contains (e.getPosition())) return;
        if (heartArea().contains (e.getPosition()))       editor.services.library.setLiked (song.id, ! song.liked);
        else if (dotsArea().contains (e.getPosition()))   editor.songMenu (song, this);
        else if (coverArea().contains (e.getPosition()) || e.getNumberOfClicks() >= 2) editor.playOrPause (song);
    }

private:
    MakerEditor& editor;
    Song song;
    bool hover = false, hoverHeart = false, hoverDots = false, dragged = false;
};

//==============================================================================
SongList::SongList (MakerEditor& e) : editor (e)
{
    list.setModel (this);
    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.getViewport()->setScrollBarThickness (9);
    addAndMakeVisible (list);
}

void SongList::setSongs (const juce::Array<Song>& s)
{
    songs = s;
    list.updateContent();
    list.repaint();
    repaint();
}

void SongList::setEmptyMessage (const juce::String& title, const juce::String& body)
{
    emptyTitle = title;
    emptyBody = body;
    repaint();
}

void SongList::paint (juce::Graphics& g)
{
    if (! songs.isEmpty()) return;
    auto r = getLocalBounds().reduced (24, 0);
    const int top = juce::jmax (20, getHeight() / 2 - 70);
    theme::drawIcon (g, theme::Icon::library, juce::Rectangle<float> ((float) r.getCentreX() - 17.0f, (float) top, 34.0f, 34.0f), theme::textMute, 1.6f);
    g.setColour (theme::text);
    g.setFont (theme::bodyBold (15.0f));
    g.drawText (emptyTitle, r.getX(), top + 46, r.getWidth(), 22, juce::Justification::centred);
    const auto font = theme::body (13.0f);
    const float w = (float) juce::jmin (r.getWidth(), 380);
    juce::AttributedString text;
    text.setJustification (juce::Justification::centredTop);
    text.setLineSpacing (font.getHeight() * 0.35f);
    text.append (emptyBody, font, theme::textDim);
    juce::TextLayout layout;
    layout.createLayout (text, w);
    layout.draw (g, juce::Rectangle<float> ((float) r.getCentreX() - w * 0.5f, (float) top + 74.0f, w, 120.0f));
}

void SongList::resized() { list.setBounds (getLocalBounds()); }

juce::Component* SongList::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    auto* r = dynamic_cast<Row*> (existing);
    if (row < 0 || row >= songs.size())
    {
        delete existing;
        return nullptr;
    }
    if (r == nullptr)
    {
        delete existing;
        r = new Row (editor);
    }
    r->set (songs.getReference (row));
    return r;
}
}
