#include "PluginEditor.h"
#include "BinaryData.h"

using namespace mm;

namespace
{
constexpr int kSidebar = 188;

struct NavButton : juce::Button
{
    NavButton (const juce::String& name, theme::Icon i) : juce::Button (name), icon (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (10.0f, 3.0f);
        if (selected)       { g.setColour (theme::yellow); g.fillRoundedRectangle (r, 8.0f); }
        else if (over)      { g.setColour (theme::raised); g.fillRoundedRectangle (r, 8.0f); }
        const auto colour = selected ? theme::onYellow : (over ? theme::text : theme::textDim);
        theme::drawIcon (g, icon, juce::Rectangle<float> (r.getX() + 12.0f, r.getCentreY() - 9.0f, 18.0f, 18.0f), colour, 1.8f);
        g.setColour (colour);
        g.setFont (theme::bodyBold (13.5f));
        g.drawText (getName(), r.withTrimmedLeft (42.0f).toNearestInt(), juce::Justification::centredLeft);
        if (badge > 0)
        {
            const auto b = juce::Rectangle<float> (r.getRight() - 32.0f, r.getCentreY() - 9.0f, 24.0f, 18.0f);
            g.setColour (selected ? theme::onYellow : theme::yellow);
            g.fillRoundedRectangle (b, 9.0f);
            g.setColour (selected ? theme::yellow : theme::onYellow);
            g.setFont (theme::bodyBold (10.5f));
            g.drawText (juce::String (badge), b.toNearestInt(), juce::Justification::centred);
        }
    }
    theme::Icon icon;
    bool selected = false;
    int badge = 0;
};

/** Covers the plugin with a dimmed layer and a panel in the middle. Used instead of pop-up windows,
    which some hosts put behind their own. */
class Overlay : public juce::Component
{
public:
    Overlay (const juce::String& t, const juce::String& m, const juce::String& initial, const juce::String& okLabel,
             bool wantsText, bool reader, bool danger, std::function<void (juce::String)> ok, std::function<void()> closed)
        : title (t), message (m), isReader (reader), onOk (std::move (ok)), onClosed (std::move (closed))
    {
        if (wantsText || reader)
        {
            theme::styleEditor (box, {}, reader);
            box.setText (initial, false);
            box.setReadOnly (reader);
            box.setCaretVisible (! reader);
            if (reader) { box.setScrollbarsShown (true); box.setFont (theme::body (14.0f)); box.applyFontToAllText (theme::body (14.0f)); }
            box.onReturnKey = [this] { if (! isReader) accept(); };
            box.onEscapeKey = [this] { close(); };
            addAndMakeVisible (box);
            hasBox = true;
        }
        okButton.setButtonText (okLabel);
        if (danger) okButton.getProperties().set ("danger", true); else theme::makePrimary (okButton);
        okButton.onClick = [this] { accept(); };
        cancelButton.setButtonText (reader ? "Close" : "Cancel");
        cancelButton.onClick = [this] { close(); };
        addAndMakeVisible (cancelButton);
        addAndMakeVisible (okButton);
        setWantsKeyboardFocus (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.72f));
        const auto p = panel().toFloat();
        theme::fillPanel (g, p, theme::raised, 12.0f);
        g.setColour (theme::text);
        g.setFont (theme::headline (20.0f));
        g.drawText (title.toUpperCase(), panel().reduced (22, 0).removeFromTop (58), juce::Justification::centredLeft);
        if (message.isNotEmpty())
            theme::drawWrapped (g, message, p.reduced (22.0f, 0.0f).withTrimmedTop (56.0f).withHeight (60.0f), theme::body (13.5f), theme::textDim);
    }

    void resized() override
    {
        auto p = panel().reduced (22, 18);
        auto buttons = p.removeFromBottom (36);
        okButton.setBounds (buttons.removeFromRight (isReader ? 130 : 150));
        buttons.removeFromRight (10);
        cancelButton.setBounds (buttons.removeFromRight (110));
        p.removeFromBottom (14);
        p.removeFromTop (40 + (message.isNotEmpty() ? messageHeight() + 10 : 0));
        if (hasBox) box.setBounds (isReader ? p : p.removeFromTop (36));
    }

    void mouseDown (const juce::MouseEvent& e) override { if (! panel().contains (e.getPosition())) close(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { close(); return true; }
        if (k == juce::KeyPress::returnKey && ! isReader) { accept(); return true; }
        return false;
    }
    void focusBox() { if (hasBox && ! isReader) { box.grabKeyboardFocus(); box.selectAll(); } else grabKeyboardFocus(); }

private:
    int messageHeight() const { return (int) std::ceil (theme::wrappedHeight (message, (float) panelWidth() - 44.0f, theme::body (13.5f))); }
    int panelWidth() const { return juce::jmin (getWidth() - 60, isReader ? 620 : 460); }
    int readerHeight() const
    {
        const int wanted = (int) std::ceil (theme::wrappedHeight (box.getText(), (float) panelWidth() - 70.0f, theme::body (14.0f), 1.2f)) + 34;
        return juce::jlimit (90, juce::jmax (90, getHeight() - 220), wanted);
    }
    juce::Rectangle<int> panel() const
    {
        int h = 58 + 18 + 36 + 14;
        if (message.isNotEmpty()) h += messageHeight() + 10;
        if (hasBox) h += isReader ? readerHeight() : 36;
        return juce::Rectangle<int> (panelWidth(), h).withCentre (getLocalBounds().getCentre());
    }
    void accept()
    {
        auto done = std::move (onOk);
        const auto text = isReader ? juce::String() : box.getText();
        if (isReader) juce::SystemClipboard::copyTextToClipboard (box.getText());
        close();
        if (done) done (text);
    }
    void close() { if (auto c = std::move (onClosed)) c(); }

    juce::String title, message;
    bool isReader = false, hasBox = false;
    juce::TextEditor box;
    juce::TextButton okButton, cancelButton;
    std::function<void (juce::String)> onOk;
    std::function<void()> onClosed;
};
}

//==============================================================================
MakerEditor::MakerEditor (MakerProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), services (p.services()), playerBar (*this)
{
    setLookAndFeel (&look);
    logo = juce::ImageCache::getFromMemory (BinaryData::mu_logo_png, BinaryData::mu_logo_pngSize);

    struct Item { const char* name; theme::Icon icon; };
    const Item items[] = { { "Create", theme::Icon::create }, { "Library", theme::Icon::library },
                           { "Settings", theme::Icon::settings }, { "Help", theme::Icon::help } };
    for (int i = 0; i < pageCount; ++i)
    {
        auto* b = nav.add (new NavButton (items[i].name, items[i].icon));
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
    }

    pages[pageCreate] = makeCreatePage (*this);
    pages[pageLibrary] = makeLibraryPage (*this);
    pages[pageSettings] = makeSettingsPage (*this);
    pages[pageHelp] = makeHelpPage (*this);
    for (auto& page : pages) addChildComponent (*page);
    addAndMakeVisible (playerBar);

    processor.addChangeListener (this);
    processor.player.addChangeListener (this);
    services.library.addChangeListener (this);
    services.generator.addChangeListener (this);
    services.watch.addChangeListener (this);
    services.updater.addChangeListener (this);
    services.watch.setWatchedClosely (true);

    setResizable (true, true);
    setResizeLimits (940, 620, 1700, 1200);
    setSize (juce::jlimit (940, 1700, (int) services.settings.getNumber ("windowWidth", 1060.0)),
             juce::jlimit (620, 1200, (int) services.settings.getNumber ("windowHeight", 700.0)));

    // the first time, start on Help if the engine is not set up, since nothing works until it is
    const bool needsSetup = services.runner.usesThisComputer() && ! services.runner.installed() && ! services.settings.getBool ("seenHelp", false);
    showPage (needsSetup ? (int) pageHelp : (int) pageCreate);
    if (needsSetup) services.settings.setBool ("seenHelp", true);

    services.updater.autoCheck();
    startTimerHz (8);
}

MakerEditor::~MakerEditor()
{
    stopTimer();
    services.watch.setWatchedClosely (false);
    services.settings.set ("windowWidth", juce::String (getWidth()));
    services.settings.set ("windowHeight", juce::String (getHeight()));
    processor.removeChangeListener (this);
    processor.player.removeChangeListener (this);
    services.library.removeChangeListener (this);
    services.generator.removeChangeListener (this);
    services.watch.removeChangeListener (this);
    services.updater.removeChangeListener (this);
    overlay.reset();
    setLookAndFeel (nullptr);
}

MakerEditor::EngineSummary MakerEditor::engineSummary() const
{
    EngineSummary s;
    const auto status = services.watch.status();
    auto& runner = services.runner;

    if (! services.watch.everChecked())
    {
        s.text = "Looking for the engine";
        s.colour = theme::textMute;
    }
    else if (status.reachable && status.modelsReady)
    {
        s.text = "Engine ready";
        s.detail = status.model;
        s.colour = theme::good;
        s.ready = true;
    }
    else if (status.reachable)
    {
        s.text = "Engine is loading";
        s.detail = "Loading its models";
        s.colour = theme::yellow;
        s.starting = true;
    }
    else if (runner.startedHere() && juce::Time::currentTimeMillis() - runner.startedAtMs() < 20 * 60 * 1000)
    {
        s.text = "Engine is starting";
        s.detail = runner.lastLogLine();
        s.colour = theme::yellow;
        s.starting = true;
    }
    else if (runner.usesThisComputer() && ! runner.installed())
    {
        s.text = "Engine not set up";
        s.detail = current == pageSettings ? "Press Set up the engine. It only has to be done once." : "Set it up in Settings";
        s.colour = theme::bad;
    }
    else
    {
        s.text = "Engine not running";
        s.detail = runner.usesThisComputer() ? juce::String (current == pageSettings ? "Press Start the engine" : "Start it in Settings") : status.error;
        s.colour = theme::bad;
    }
    return s;
}

void MakerEditor::startEngine (bool byItself)
{
    juce::String error;
    if (services.runner.start (error))
    {
        if (! byItself) toast ("Starting the engine. The first start takes a minute or two.");
        services.watch.checkSoon();
    }
    else if (! byItself)
    {
        toast (error, true);
    }
    refreshAll();
}

void MakerEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::black);

    auto side = getLocalBounds().removeFromLeft (kSidebar);
    g.setColour (theme::panel);
    g.fillRect (side);
    g.setColour (theme::line);
    g.fillRect (side.getRight() - 1, 0, 1, getHeight());

    // brand: the MU wordmark with the product name under it
    if (logo.isValid())
        g.drawImage (logo, juce::Rectangle<float> (20.0f, 22.0f, (float) kSidebar - 40.0f, 24.0f), juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid);
    g.setColour (theme::yellow);
    g.setFont (theme::headline (17.0f));
    g.drawText ("MUSIC MAKER", 20, 50, kSidebar - 40, 22, juce::Justification::centredLeft);

    // the engine, at the bottom of the sidebar
    const auto summary = engineSummary();
    auto status = side.removeFromBottom (62).reduced (20, 0);
    g.setColour (summary.colour);
    g.fillEllipse ((float) status.getX(), (float) status.getY() + 6.0f, 8.0f, 8.0f);
    g.setColour (theme::text);
    g.setFont (theme::bodyBold (11.5f));
    g.drawText (summary.text, status.getX() + 16, status.getY(), status.getWidth() - 16, 20, juce::Justification::centredLeft);
    g.setColour (theme::textMute);
    g.setFont (theme::body (10.5f));
    g.drawText (summary.detail, status.getX() + 16, status.getY() + 18, status.getWidth() - 16, 16, juce::Justification::centredLeft, true);
}

void MakerEditor::paintOverChildren (juce::Graphics& g)
{
    if (toastText.isEmpty()) return;
    const auto font = theme::bodyBold (13.0f);
    const float room = (float) (getWidth() - kSidebar) - 80.0f;
    const float w = juce::jmin (room, juce::GlyphArrangement::getStringWidth (font, toastText) + 44.0f);
    const float h = juce::GlyphArrangement::getStringWidth (font, toastText) + 44.0f > room ? 54.0f : 38.0f;
    const auto r = juce::Rectangle<float> (w, h).withCentre ({ (float) kSidebar + (float) (getWidth() - kSidebar) * 0.5f, (float) getHeight() - (float) PlayerBar::height - 44.0f });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 19.0f);
    g.setColour (toastIsError ? theme::bad : theme::yellow);
    g.fillRoundedRectangle (r, 19.0f);
    g.setColour (toastIsError ? juce::Colours::white : theme::onYellow);
    g.setFont (font);
    g.drawFittedText (toastText, r.toNearestInt().reduced (16, 2), juce::Justification::centred, 2);
}

void MakerEditor::resized()
{
    auto r = getLocalBounds();
    auto side = r.removeFromLeft (kSidebar);
    side.removeFromTop (94);
    for (auto* b : nav)
        b->setBounds (side.removeFromTop (44));

    playerBar.setBounds (r.removeFromBottom (PlayerBar::height));
    for (auto& page : pages)
        if (page != nullptr) page->setBounds (r);
    if (overlay != nullptr) overlay->setBounds (getLocalBounds());
}

void MakerEditor::mouseUp (const juce::MouseEvent&) { if (toastText.isNotEmpty()) { toastText.clear(); repaint(); } }

void MakerEditor::showPage (int index)
{
    current = juce::jlimit (0, (int) pageCount - 1, index);
    for (int i = 0; i < pageCount; ++i)
    {
        pages[i]->setVisible (i == current);
        static_cast<NavButton*> (nav[i])->selected = i == current;
        nav[i]->repaint();
    }
    pages[current]->refresh();
    pages[current]->shown();
}

void MakerEditor::toast (const juce::String& text, bool isError)
{
    toastText = text;
    toastIsError = isError;
    toastUntil = juce::Time::getMillisecondCounter() + (isError ? 7000u : 3200u);
    repaint();
}

void MakerEditor::showText (const juce::String& title, const juce::String& text)
{
    toastText.clear();
    auto* o = new Overlay (title, {}, text, "Copy", false, true, false, {}, [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MakerEditor> (this)] { if (safe != nullptr) safe->overlay.reset(); }); });
    overlay.reset (o);
    addAndMakeVisible (*overlay);
    overlay->setBounds (getLocalBounds());
    o->focusBox();
}

void MakerEditor::ask (const juce::String& title, const juce::String& message, const juce::String& initial, const juce::String& okLabel,
                       std::function<void (juce::String)> onOk, bool wantsText, bool danger)
{
    toastText.clear();
    auto* o = new Overlay (title, message, initial, okLabel, wantsText, false, danger, std::move (onOk),
                           [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MakerEditor> (this)] { if (safe != nullptr) safe->overlay.reset(); }); });
    overlay.reset (o);
    addAndMakeVisible (*overlay);
    overlay->setBounds (getLocalBounds());
    o->focusBox();
}

//==============================================================================
void MakerEditor::playOrPause (const Song& song)
{
    auto& player = processor.player;
    if (player.songId() == song.id && player.file().existsAsFile())
    {
        if (player.playing()) player.pause(); else player.play();
        return;
    }
    if (! processor.playSong (song)) toast ("That song's file has gone missing.", true);
}

void MakerEditor::makeMoreLike (const Song& song)
{
    auto& d = processor.draft;
    if (song.request.isObject())
    {
        d = SongRequest::fromVar (song.request);
    }
    else
    {
        d = SongRequest();
        d.mode = SongRequest::Mode::custom;
    }
    // what the engine actually wrote is more use than the sentence that started it
    if (song.style.isNotEmpty() && d.mode == SongRequest::Mode::simple)
    {
        d.mode = SongRequest::Mode::custom;
        d.style = song.style;
        d.lyrics = song.lyrics;
        d.instrumental = song.lyrics.trim().isEmpty();
    }
    d.title = song.title.upToLastOccurrenceOf (" ", false, false).isNotEmpty() && song.variationsTotal > 1
                ? song.title.upToLastOccurrenceOf (" ", false, false) : song.title;
    if (song.bpm > 0.0 && d.bpm == 0) d.bpm = juce::roundToInt (song.bpm);
    if (song.key.isNotEmpty() && d.key.isEmpty()) d.key = song.key;
    d.randomSeed = true;
    processor.followProjectTempo = false;
    processor.draftChanged();
    showPage (pageCreate);
    toast ("Loaded its settings. Change what you like and make it again.");
}

void MakerEditor::startFrom (const Song& song)
{
    const auto file = services.library.audioOf (song);
    if (! file.existsAsFile()) { toast ("That song's file has gone missing.", true); return; }
    auto& d = processor.draft;
    d.audioPath = file.getFullPathName();
    if (d.source == SongRequest::Source::none) d.source = SongRequest::Source::cover;
    processor.draftChanged();
    showPage (pageCreate);
    toast ("That song is now the audio to start from.");
}

void MakerEditor::showLyrics (const Song& song)
{
    showText (song.title, song.lyrics.trim().isEmpty() ? juce::String ("This one is an instrumental.") : song.lyrics.trim());
}

void MakerEditor::rename (const Song& song)
{
    const auto id = song.id;
    ask ("Rename", {}, song.title, "Save", [this, id] (juce::String name)
    {
        if (name.trim().isNotEmpty()) services.library.rename (id, name);
    });
}

void MakerEditor::remove (const Song& song)
{
    const auto id = song.id;
    ask ("Delete this song?", "\"" + song.title + "\" and its audio file will be deleted from this computer. A project that already uses the file will lose it too, unless your DAW made its own copy.",
         {}, "Delete", [this, id] (juce::String)
    {
        if (processor.player.songId() == id) processor.player.unload();
        services.library.remove (id);
    }, false, true);
}

void MakerEditor::reveal (const Song& song) { services.library.audioOf (song).revealToUser(); }

void MakerEditor::songMenu (const Song& song, juce::Component* anchor)
{
    juce::PopupMenu m;
    m.setLookAndFeel (&look);
    const bool isCurrent = processor.player.songId() == song.id && processor.player.playing();
    m.addItem (1, isCurrent ? "Pause" : "Play");
    m.addItem (2, song.liked ? "Unlike" : "Like");
    m.addSeparator();
    m.addItem (3, "Make more like this");
    m.addItem (4, "Start a new song from this audio");
    m.addItem (5, "Show the lyrics", song.lyrics.trim().isNotEmpty());
    m.addSeparator();
    m.addItem (6, "Rename");
   #if JUCE_MAC
    m.addItem (7, "Show in Finder");
   #else
    m.addItem (7, "Show the file");
   #endif
    m.addItem (8, "Delete");

    const auto copy = song;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor).withParentComponent (this),
                     [safe = juce::Component::SafePointer<MakerEditor> (this), copy] (int choice)
    {
        if (safe == nullptr) return;
        switch (choice)
        {
            case 1: safe->playOrPause (copy); break;
            case 2: safe->services.library.setLiked (copy.id, ! copy.liked); break;
            case 3: safe->makeMoreLike (copy); break;
            case 4: safe->startFrom (copy); break;
            case 5: safe->showLyrics (copy); break;
            case 6: safe->rename (copy); break;
            case 7: safe->reveal (copy); break;
            case 8: safe->remove (copy); break;
            default: break;
        }
    });
}

void MakerEditor::dragOut (const Song& song, juce::Component* from)
{
    const auto file = services.library.audioOf (song);
    if (! file.existsAsFile() || isDragAndDropActive()) return;

    // what follows the mouse: the song's cover with its name beside it
    const auto font = theme::bodyBold (13.0f);
    const int textWidth = juce::jmin (220, juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, song.title)) + 8);
    juce::Image image (juce::Image::ARGB, 44 + 10 + textWidth + 14, 44, true);
    {
        juce::Graphics g (image);
        g.setColour (theme::raised.withAlpha (0.94f));
        g.fillRoundedRectangle (image.getBounds().toFloat(), 8.0f);
        theme::drawCover (g, juce::Rectangle<float> (4.0f, 4.0f, 36.0f, 36.0f), song.id, 5.0f);
        g.setColour (theme::text);
        g.setFont (font);
        g.drawText (song.title, 50, 0, textWidth, 44, juce::Justification::centredLeft, true);
    }
    startDragging ("songfile:" + file.getFullPathName(), from, juce::ScaledImage (image), false);
}

bool MakerEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details, juce::StringArray& files, bool& canMove)
{
    const auto description = details.description.toString();
    if (! description.startsWith ("songfile:")) return false;
    const juce::File file (description.fromFirstOccurrenceOf ("songfile:", false, false));
    if (! file.existsAsFile()) return false;
    files.add (file.getFullPathName());
    canMove = false;
    return true;
}

void MakerEditor::makeSong()
{
    auto request = processor.draft;
    if (processor.followProjectTempo && processor.hostKnowsTempo())
        request.bpm = juce::jlimit (30, 300, juce::roundToInt (processor.hostBpm()));

    const auto problem = request.problem();
    if (problem.isNotEmpty()) { toast (problem, true); return; }

    const auto summary = engineSummary();
    if (! services.watch.status().reachable)
    {
        if (summary.starting)                       toast ("The engine is still starting. Give it a moment and press it again.", true);
        else if (! services.runner.usesThisComputer()) toast ("The engine is not answering at " + services.settings.engineUrl() + ". Check it is running on that computer.", true);
        else if (! services.runner.installed())     { toast ("The engine is not set up yet. Settings has the button for it.", true); showPage (pageSettings); }
        else                                        { startEngine(); }
        return;
    }

    services.generator.add (request);
    toast (request.variations > 1 ? "Making " + juce::String (request.variations) + " versions" : juce::String ("Making it"));
}

//==============================================================================
void MakerEditor::timerCallback()
{
    if (toastText.isNotEmpty() && juce::Time::getMillisecondCounter() > toastUntil)
    {
        toastText.clear();
        repaint();
    }

    // finished songs leave the queue a few seconds after they land in the list
    const auto now = juce::Time::currentTimeMillis();
    for (auto& j : services.generator.jobs())
        if ((j.state == Job::State::done || j.state == Job::State::cancelled) && now - j.finishedMs > 4000)
            services.generator.dismiss (j.id);

    playerBar.tick();
    pages[current]->tick();

    // the engine line in the sidebar moves while it starts
    if (engineSummary().starting) repaint (0, getHeight() - 70, kSidebar, 70);
}

void MakerEditor::refreshAll()
{
    int making = 0;
    for (auto& j : services.generator.jobs())
        if (j.active()) ++making;
    static_cast<NavButton*> (nav[pageCreate])->badge = current == pageCreate ? 0 : making;
    nav[pageCreate]->repaint();

    pages[current]->refresh();
    playerBar.songsChanged();
    repaint (0, 0, kSidebar, getHeight());
}

void MakerEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &services.generator)
    {
        // say so once when a song lands, wherever in the plugin they are
        for (auto& j : services.generator.jobs())
        {
            if (seenDone.contains (j.id)) continue;
            if (j.state == Job::State::done)        { seenDone.add (j.id); toast (j.title + ": " + j.message.toLowerCase()); }
            else if (j.state == Job::State::failed) { seenDone.add (j.id); toast (j.message, true); }
        }
    }

    if (source == &services.watch)
    {
        // start the engine by itself the first time the plugin finds it switched off
        auto& runner = services.runner;
        if (services.watch.everChecked() && ! services.watch.status().reachable && ! runner.triedAutoStart
            && services.settings.getBool ("autoStartEngine", true) && runner.usesThisComputer() && runner.installed() && ! runner.startedHere())
        {
            runner.triedAutoStart = true;
            startEngine (true);
        }
        if (services.watch.status().reachable) runner.triedAutoStart = true;
    }

    if (source == &services.updater)
    {
        const auto update = services.updater.getState();
        if (update.phase == UpdateState::Phase::available && toldAboutBuild != update.latest.build && current != pageSettings)
        {
            toldAboutBuild = update.latest.build;
            toast ("Version " + update.latest.version + " is ready. Install it in Settings.");
        }
    }

    refreshAll();
}
