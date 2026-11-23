#pragma once
#include "Pages.h"
#include "../core/Player.h"

class MakerEditor;

namespace mm
{
/** The strip along the bottom: what is playing, where it has got to, and a handle to drag it into the project. */
class PlayerBar : public juce::Component, private juce::ChangeListener
{
public:
    explicit PlayerBar (MakerEditor&);
    ~PlayerBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void tick();
    void songsChanged();

    static constexpr int height = 78;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void loadCurrent();
    juce::Rectangle<int> waveArea() const;
    juce::Rectangle<int> dragArea() const;
    void scrubTo (int x);

    MakerEditor& editor;
    juce::AudioThumbnailCache cache { 8 };
    juce::AudioThumbnail thumbnail;
    IconButton playButton { "Play", theme::Icon::play };
    IconButton likeButton { "Like", theme::Icon::heart };
    juce::Slider volume;
    Song song;
    bool hasSong = false, scrubbing = false, draggingOut = false;
    juce::String shownId;
};
}
