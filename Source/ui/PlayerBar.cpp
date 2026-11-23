#include "PlayerBar.h"
#include "../PluginEditor.h"

namespace mm
{
PlayerBar::PlayerBar (MakerEditor& e)
    : editor (e), thumbnail (256, e.processor.player.formatManager(), cache)
{
    playButton.filled = true;
    playButton.onClick = [this] { if (hasSong) editor.playOrPause (song); };
    likeButton.onClick = [this] { if (hasSong) editor.services.library.setLiked (song.id, ! song.liked); };
    addAndMakeVisible (playButton);
    addAndMakeVisible (likeButton);

    volume.setSliderStyle (juce::Slider::LinearHorizontal);
    volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volume.setRange (0.0, 1.2, 0.01);
    volume.setValue (editor.processor.player.gain(), juce::dontSendNotification);
    volume.setColour (juce::Slider::trackColourId, theme::yellow);
    volume.setColour (juce::Slider::backgroundColourId, theme::control);
    volume.setColour (juce::Slider::thumbColourId, theme::text);
    volume.setTooltip ("How loud songs play through the plugin");
    volume.onValueChange = [this]
    {
        editor.processor.player.setGain ((float) volume.getValue());
        editor.services.settings.set ("playerGain", juce::String (volume.getValue(), 2));
    };
    addAndMakeVisible (volume);

    thumbnail.addChangeListener (this);
    loadCurrent();
}

PlayerBar::~PlayerBar() { thumbnail.removeChangeListener (this); }

void PlayerBar::changeListenerCallback (juce::ChangeBroadcaster*) { repaint(); }

void PlayerBar::loadCurrent()
{
    auto& player = editor.processor.player;
    const auto id = player.songId();
    hasSong = id.isNotEmpty() && editor.services.library.find (id, song);
    if (! hasSong) { shownId.clear(); thumbnail.clear(); }
    else if (shownId != id)
    {
        shownId = id;
        thumbnail.setSource (new juce::FileInputSource (player.file()));
    }
    playButton.setIcon (player.playing() ? theme::Icon::pause : theme::Icon::play);
    playButton.setEnabled (hasSong);
    likeButton.setVisible (hasSong);
    likeButton.setIcon (hasSong && song.liked ? theme::Icon::heartFilled : theme::Icon::heart);
    likeButton.lit = hasSong && song.liked;
    repaint();
}

void PlayerBar::songsChanged() { loadCurrent(); }

void PlayerBar::tick()
{
    auto& player = editor.processor.player;
    playButton.setIcon (player.playing() ? theme::Icon::pause : theme::Icon::play);
    if (player.songId() != shownId) loadCurrent();
    if (player.playing() || scrubbing) repaint (waveArea().expanded (60, 4));
}

juce::Rectangle<int> PlayerBar::waveArea() const
{
    const int left = 16 + 46 + 12 + juce::jlimit (120, 220, getWidth() / 5) + 12 + 44 + 14 + 44;
    const int right = getWidth() - 16 - 118 - 14 - 92 - 30 - 44;
    return { left, 14, juce::jmax (40, right - left), getHeight() - 28 };
}

juce::Rectangle<int> PlayerBar::dragArea() const { return { getWidth() - 16 - 118, (getHeight() - 34) / 2, 118, 34 }; }

void PlayerBar::resized()
{
    const int textWidth = juce::jlimit (120, 220, getWidth() / 5);
    const int x = 16 + 46 + 12 + textWidth + 12;
    playButton.setBounds (x, (getHeight() - 44) / 2, 44, 44);
    likeButton.setBounds (16 + 46 + 12 + textWidth - 28, (getHeight() - 28) / 2, 28, 28);
    volume.setBounds (dragArea().getX() - 14 - 92, (getHeight() - 22) / 2, 92, 22);
}

void PlayerBar::paint (juce::Graphics& g)
{
    g.fillAll (theme::panel);
    g.setColour (theme::line);
    g.fillRect (0, 0, getWidth(), 1);

    auto& player = editor.processor.player;
    const auto cover = juce::Rectangle<float> (16.0f, ((float) getHeight() - 46.0f) * 0.5f, 46.0f, 46.0f);
    const int textWidth = juce::jlimit (120, 220, getWidth() / 5);
    const int textX = 16 + 46 + 12;

    if (! hasSong)
    {
        g.setColour (theme::control);
        g.fillRoundedRectangle (cover, 7.0f);
        theme::drawIcon (g, theme::Icon::library, cover.reduced (13.0f), theme::textMute, 1.6f);
        g.setColour (theme::textDim);
        g.setFont (theme::body (12.5f));
        g.drawText ("Nothing playing", textX, 0, textWidth, getHeight(), juce::Justification::centredLeft);
    }
    else
    {
        theme::drawCover (g, cover, song.id, 7.0f);
        g.setColour (theme::text);
        g.setFont (theme::bodyBold (13.5f));
        g.drawText (song.title, textX, 19, textWidth - 34, 20, juce::Justification::centredLeft, true);
        g.setColour (theme::textDim);
        g.setFont (theme::body (11.5f));
        g.drawText (song.detailText().isNotEmpty() ? song.detailText() : song.style, textX, 39, textWidth - 34, 18, juce::Justification::centredLeft, true);
    }

    // the song's shape, yellow up to where it has played
    const auto wave = waveArea();
    const double length = player.length();
    const double fraction = (hasSong && length > 0.0) ? juce::jlimit (0.0, 1.0, player.position() / length) : 0.0;
    const int playedX = wave.getX() + juce::roundToInt (fraction * (double) wave.getWidth());

    g.setColour (theme::textDim);
    g.setFont (theme::body (11.5f));
    g.drawText (hasSong ? Library::formatLength (player.position()) : juce::String ("0:00"), wave.getX() - 44, 0, 36, getHeight(), juce::Justification::centredRight);
    g.drawText (hasSong ? Library::formatLength (length) : juce::String ("0:00"), wave.getRight() + 8, 0, 36, getHeight(), juce::Justification::centredLeft);

    if (hasSong && thumbnail.getTotalLength() > 0.0)
    {
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (wave.withRight (playedX));
            g.setColour (theme::yellow);
            thumbnail.drawChannel (g, wave, 0.0, thumbnail.getTotalLength(), 0, 1.0f);
        }
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (wave.withLeft (playedX));
            g.setColour (theme::lineHi);
            thumbnail.drawChannel (g, wave, 0.0, thumbnail.getTotalLength(), 0, 1.0f);
        }
        g.setColour (theme::text);
        g.fillRect (playedX, wave.getY() + 2, 1, wave.getHeight() - 4);
    }
    else
    {
        g.setColour (theme::control);
        g.fillRoundedRectangle (wave.toFloat().withSizeKeepingCentre ((float) wave.getWidth(), 4.0f), 2.0f);
    }

    theme::drawIcon (g, theme::Icon::speaker, juce::Rectangle<float> ((float) volume.getX() - 22.0f, ((float) getHeight() - 16.0f) * 0.5f, 16.0f, 16.0f), theme::textDim, 1.5f);

    // the handle for dragging the song into the project
    const auto drag = dragArea().toFloat();
    g.setColour (hasSong ? theme::control : theme::panel);
    g.fillRoundedRectangle (drag, 8.0f);
    g.setColour (hasSong ? theme::lineHi : theme::line);
    g.drawRoundedRectangle (drag.reduced (0.5f), 8.0f, 1.0f);
    theme::drawIcon (g, theme::Icon::grip, juce::Rectangle<float> (drag.getX() + 8.0f, drag.getCentreY() - 9.0f, 18.0f, 18.0f), hasSong ? theme::yellow : theme::textMute, 1.6f);
    g.setColour (hasSong ? theme::text : theme::textMute);
    g.setFont (theme::bodyBold (11.5f));
    g.drawText ("Drag to project", drag.withTrimmedLeft (30.0f).toNearestInt(), juce::Justification::centredLeft);
}

void PlayerBar::scrubTo (int x)
{
    const auto wave = waveArea();
    auto& player = editor.processor.player;
    if (! hasSong || player.length() <= 0.0) return;
    player.setPosition (player.length() * juce::jlimit (0.0, 1.0, (double) (x - wave.getX()) / (double) wave.getWidth()));
    repaint();
}

void PlayerBar::mouseMove (const juce::MouseEvent& e)
{
    const bool onDrag = hasSong && dragArea().contains (e.getPosition());
    const bool onWave = hasSong && waveArea().contains (e.getPosition());
    setMouseCursor (onDrag ? juce::MouseCursor::DraggingHandCursor : onWave ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void PlayerBar::mouseDown (const juce::MouseEvent& e)
{
    scrubbing = hasSong && waveArea().expanded (0, 8).contains (e.getPosition());
    draggingOut = false;
    if (scrubbing) scrubTo (e.x);
}

void PlayerBar::mouseDrag (const juce::MouseEvent& e)
{
    if (scrubbing) { scrubTo (e.x); return; }
    if (! hasSong || draggingOut || e.getDistanceFromDragStart() < 6) return;
    if (dragArea().contains (e.getMouseDownPosition()) || e.getMouseDownX() < 16 + 46 + 12 + juce::jlimit (120, 220, getWidth() / 5))
    {
        draggingOut = true;
        editor.dragOut (song, this);
    }
}

void PlayerBar::mouseUp (const juce::MouseEvent&) { scrubbing = false; }
}
