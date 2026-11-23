#pragma once
#include "PluginProcessor.h"
#include "ui/Pages.h"
#include "ui/PlayerBar.h"

class MakerEditor : public juce::AudioProcessorEditor,
                    public juce::DragAndDropContainer,
                    private juce::ChangeListener,
                    private juce::Timer
{
public:
    enum { pageCreate, pageLibrary, pageSettings, pageHelp, pageCount };

    explicit MakerEditor (MakerProcessor&);
    ~MakerEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&, juce::StringArray& files, bool& canMove) override;

    MakerProcessor& processor;
    mm::Services& services;

    void showPage (int index);
    void selectPage (int index)                 { showPage (index); }
    int currentPage() const                     { return current; }
    void toast (const juce::String& text, bool isError = false);

    /** A panel over the plugin: some words to read, or a question with a box to type in. */
    void showText (const juce::String& title, const juce::String& text);
    void ask (const juce::String& title, const juce::String& message, const juce::String& initial, const juce::String& okLabel,
              std::function<void (juce::String)> onOk, bool wantsText = true, bool danger = false);

    // what can be done with a song, from anywhere it is listed
    void playOrPause (const mm::Song&);
    void makeMoreLike (const mm::Song&);
    void startFrom (const mm::Song&);
    void showLyrics (const mm::Song&);
    void rename (const mm::Song&);
    void remove (const mm::Song&);
    void reveal (const mm::Song&);
    void songMenu (const mm::Song&, juce::Component* anchor);
    void dragOut (const mm::Song&, juce::Component* from);

    /** Sends what is on the Create page to the engine. */
    void makeSong();

    struct EngineSummary { juce::String text, detail; juce::Colour colour; bool ready = false, starting = false; };
    EngineSummary engineSummary() const;
    void startEngine (bool byItself = false);

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refreshAll();
    void closeOverlaySoon();

    mm::theme::Look look;
    juce::Image logo;
    juce::OwnedArray<juce::Button> nav;
    std::unique_ptr<mm::Page> pages[pageCount];
    mm::PlayerBar playerBar;
    std::unique_ptr<juce::Component> overlay;
    juce::TooltipWindow tooltips { this, 600 };
    int current = pageCreate;

    juce::String toastText;
    bool toastIsError = false;
    juce::uint32 toastUntil = 0;
    int toldAboutBuild = 0;
    bool lastKnownOnline = true;
    juce::StringArray seenDone;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MakerEditor)
};
