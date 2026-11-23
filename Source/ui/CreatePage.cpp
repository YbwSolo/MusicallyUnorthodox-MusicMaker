#include "Pages.h"
#include "SongList.h"
#include "../PluginEditor.h"
#include "BinaryData.h"

namespace mm
{
namespace
{
const char* const kGenres[] = { "trap", "drill", "uk drill", "r&b", "hip hop", "afrobeats", "grime", "dancehall", "amapiano", "uk garage",
                                "house", "pop", "soul", "lo-fi", "jazz", "rock" };
const char* const kExtras[] = { "dark", "melodic", "hard", "chilled", "808s", "piano", "guitar", "strings", "male vocals", "female vocals" };

const char* const kIdeas[] = {
    "A dark UK drill track with sliding 808s and a haunting piano, about coming up from nothing",
    "Smooth late night R&B with warm keys and a female vocal, about missing someone you should not",
    "A melodic trap song with a sad guitar loop and autotuned vocals about loyalty",
    "An afrobeats summer tune with bright guitars and a catchy hook about dancing till morning",
    "A hard grime banger at 140 with a gritty bassline and fast bars about proving people wrong",
    "A soulful hip hop track with chopped vocal samples and boom bap drums about growing up in the city",
    "An amapiano groove with deep log drums and soft piano, relaxed and happy",
    "A UK garage tune with skippy drums, a warm bassline and a sweet female hook about a new crush",
    "A slow emotional piano ballad with strings, about saying goodbye",
    "A lo-fi beat with dusty drums and jazzy chords, something to study to",
    "An upbeat pop song with a huge chorus about the best summer of your life",
    "A dancehall riddim with bouncing drums and a confident male vocal about the weekend",
    "A moody trap beat with bells and heavy 808s, like the soundtrack to a night drive",
    "A feel-good house track with a piano riff and a gospel style vocal about better days",
    "An acoustic guitar song with a gentle male voice about home",
    "A cinematic hip hop track with big strings and choir, about never giving up" };

struct Choice { const char* label; const char* value; };
const Choice kLanguages[] = { { "English", "en" }, { "Spanish", "es" }, { "French", "fr" }, { "Portuguese", "pt" }, { "German", "de" },
                              { "Italian", "it" }, { "Japanese", "ja" }, { "Korean", "ko" }, { "Chinese", "zh" }, { "Hindi", "hi" },
                              { "Arabic", "ar" }, { "Russian", "ru" }, { "Turkish", "tr" }, { "Polish", "pl" }, { "Dutch", "nl" } };
const Choice kTimes[] = { { "Auto", "" }, { "2/4", "2" }, { "3/4", "3" }, { "4/4", "4" }, { "6/8", "6" } };
const int kLengths[] = { 0, 30, 60, 90, 120, 150, 180, 210, 240, 300 };

void setTextIfChanged (juce::TextEditor& e, const juce::String& text)
{
    if (e.getText() != text) e.setText (text, false);
}

juce::String formatSeconds (double seconds)
{
    if (seconds < 0.0) return {};
    return Library::formatLength (seconds);
}

double parseSeconds (const juce::String& text, double fallback)
{
    const auto t = text.trim();
    if (t.isEmpty()) return fallback;
    if (t.containsChar (':'))
        return juce::jmax (0.0, t.upToFirstOccurrenceOf (":", false, false).getDoubleValue() * 60.0 + t.fromFirstOccurrenceOf (":", false, false).getDoubleValue());
    return juce::jmax (0.0, t.getDoubleValue());
}

/** The style tags: click one to add it to the style, click again to take it out. */
class StyleChips : public juce::Component
{
public:
    std::function<void (const juce::String& tag)> onToggle;
    std::function<void()> onMore;

    void setStyle (const juce::String& s)       { if (style != s) { style = s; repaint(); } }

    static bool contains (const juce::String& style, const juce::String& tag)
    {
        for (auto& part : juce::StringArray::fromTokens (style, ",", {}))
            if (part.trim().equalsIgnoreCase (tag)) return true;
        return false;
    }

    int layout (int width)
    {
        boxes.clear();
        const auto font = theme::bodyBold (11.5f);
        int x = 0, y = 0;
        auto place = [&] (const juce::String& label, bool isMore)
        {
            const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, label)) + 22;
            if (x + w > width && x > 0) { x = 0; y += 30; }
            boxes.add ({ label, { x, y, w, 25 }, isMore });
            x += w + 6;
        };
        for (auto* g : kGenres) place (g, false);
        for (auto* e : kExtras) place (e, false);
        place ("more styles", true);
        return y + 25;
    }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < boxes.size(); ++i)
        {
            auto& b = boxes.getReference (i);
            const bool lit = ! b.more && contains (style, b.label);
            const auto r = b.area.toFloat();
            g.setColour (lit ? theme::yellow : (i == hover ? theme::control.brighter (0.08f) : theme::control));
            g.fillRoundedRectangle (r, 12.5f);
            if (! lit) { g.setColour (b.more ? theme::lineHi : theme::line); g.drawRoundedRectangle (r.reduced (0.5f), 12.5f, 1.0f); }
            g.setColour (lit ? theme::onYellow : (b.more ? theme::text : theme::textDim));
            g.setFont (theme::bodyBold (11.5f));
            g.drawText (b.more ? b.label + " +" : b.label, b.area, juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = indexAt (e.getPosition());
        if (h != hover) { hover = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i < 0) return;
        if (boxes.getReference (i).more) { if (onMore) onMore(); }
        else if (onToggle) onToggle (boxes.getReference (i).label);
    }

private:
    struct Box { juce::String label; juce::Rectangle<int> area; bool more; };
    int indexAt (juce::Point<int> p) const
    {
        for (int i = 0; i < boxes.size(); ++i)
            if (boxes.getReference (i).area.contains (p)) return i;
        return -1;
    }
    juce::Array<Box> boxes;
    juce::String style;
    int hover = -1;
};

/** The songs being made right now, each with how far it has got. */
class JobsView : public juce::Component
{
public:
    explicit JobsView (MakerEditor& e) : editor (e) {}

    void setJobs (const juce::Array<Job>& j)     { jobs = j; repaint(); }
    int idealHeight() const                      { return jobs.size() * cardHeight; }
    bool anyActive() const                       { for (auto& j : jobs) if (j.active()) return true; return false; }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < jobs.size(); ++i)
        {
            auto& j = jobs.getReference (i);
            auto card = juce::Rectangle<int> (0, i * cardHeight, getWidth(), cardHeight - 8).toFloat();
            const bool failed = j.state == Job::State::failed;
            theme::fillPanel (g, card, theme::panel, 10.0f);

            auto inner = card.reduced (14.0f, 10.0f);
            const auto close = closeArea (i).toFloat();
            theme::drawIcon (g, theme::Icon::close, close.reduced (8.0f), i == hoverClose ? theme::text : theme::textMute, 1.7f);
            inner.removeFromRight (30.0f);

            g.setColour (theme::text);
            g.setFont (theme::bodyBold (13.5f));
            g.drawText (j.title, inner.removeFromTop (18.0f).toNearestInt(), juce::Justification::centredLeft, true);

            auto status = inner.removeFromTop (18.0f);
            g.setFont (theme::body (12.0f));
            if (j.active())
            {
                const auto seconds = (juce::Time::currentTimeMillis() - j.startedMs) / 1000;
                g.setColour (theme::textMute);
                g.drawText (Library::formatLength ((double) seconds), status.removeFromRight (44.0f).toNearestInt(), juce::Justification::centredRight);
            }
            g.setColour (failed ? theme::bad : (j.state == Job::State::done ? theme::good : theme::textDim));
            g.drawText (j.message, status.toNearestInt(), juce::Justification::centredLeft, true);

            if (j.active())
            {
                auto bar = juce::Rectangle<float> (card.getX() + 14.0f, card.getBottom() - 13.0f, card.getWidth() - 28.0f, 4.0f);
                g.setColour (theme::control);
                g.fillRoundedRectangle (bar, 2.0f);
                g.setColour (theme::yellow);
                if (j.progress > 0.02)
                {
                    g.fillRoundedRectangle (bar.withWidth (juce::jmax (6.0f, bar.getWidth() * (float) j.progress)), 2.0f);
                }
                else
                {
                    // nothing to measure yet: a short bar that travels, so it is plain something is happening
                    const float phase = (float) ((juce::Time::getMillisecondCounter() / 12u) % 1000u) / 1000.0f;
                    const float w = bar.getWidth() * 0.22f;
                    juce::Graphics::ScopedSaveState state (g);
                    g.reduceClipRegion (bar.toNearestInt());
                    g.fillRoundedRectangle (bar.getX() - w + (bar.getWidth() + w) * phase, bar.getY(), w, bar.getHeight(), 2.0f);
                }
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (int i = 0; i < jobs.size(); ++i)
            if (closeArea (i).contains (e.getPosition())) h = i;
        if (h != hoverClose) { hoverClose = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { hoverClose = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < jobs.size(); ++i)
            if (closeArea (i).contains (e.getPosition()))
            {
                auto& j = jobs.getReference (i);
                if (j.active()) editor.services.generator.cancel (j.id);
                else            editor.services.generator.dismiss (j.id);
                return;
            }
    }

    static constexpr int cardHeight = 76;

private:
    juce::Rectangle<int> closeArea (int i) const { return { getWidth() - 40, i * cardHeight + 8, 30, 30 }; }
    MakerEditor& editor;
    juce::Array<Job> jobs;
    int hoverClose = -1;
};

//==============================================================================
/** Everything that describes the song to make. It writes straight into the processor's draft as it is changed. */
class CreateForm : public juce::Component
{
public:
    explicit CreateForm (MakerEditor& e) : editor (e)
    {
        mode.onChange = [this] (int) { push(); relayout(); };
        addAndMakeVisible (mode);

        theme::styleEditor (description, "A dark UK drill track with sliding 808s and a haunting piano, about coming up from nothing", true);
        theme::styleEditor (title, "Leave empty and it is named for you");
        theme::styleEditor (style, "dark trap, 808s, sad piano, male vocals", true);
        theme::styleEditor (lyrics, "[Verse]\nWrite the words here\n\n[Chorus]\nAnd the hook here", true);
        for (auto* ed : { &description, &title, &style, &lyrics })
        {
            ed->onTextChange = [this] { push(); };
            addAndMakeVisible (*ed);
        }

        chips.onToggle = [this] (const juce::String& tag) { toggleTag (tag); };
        chips.onMore = [this] { showAllStyles(); };
        addAndMakeVisible (chips);

        ideaButton.setButtonText ("Give me an idea");
        ideaButton.onClick = [this]
        {
            ideaIndex = (ideaIndex + 1 + juce::Random::getSystemRandom().nextInt (4)) % juce::numElementsInArray (kIdeas);
            description.setText (kIdeas[ideaIndex], true);
        };
        addAndMakeVisible (ideaButton);

        fillButton.setButtonText ("Write it out for me");
        fillButton.setTooltip ("The engine turns your style and rough lyrics into a full song sheet");
        fillButton.onClick = [this] { fillOut(); };
        addAndMakeVisible (fillButton);

        instrumental.setButtonText ("Instrumental");
        instrumental.onClick = [this] { push(); relayout(); };
        addAndMakeVisible (instrumental);

        for (auto* tag : { "Intro", "Verse", "Hook", "Chorus", "Bridge", "Outro" })
        {
            auto* b = tagButtons.add (new juce::TextButton (tag));
            theme::makeQuiet (*b);
            b->getProperties().set ("small", true);
            b->onClick = [this, tag] { insertTag (tag); };
            addAndMakeVisible (b);
        }

        theme::styleEditor (tempo, "Auto");
        tempo.setInputRestrictions (3, "0123456789");
        tempo.onTextChange = [this] { push(); };
        addAndMakeVisible (tempo);
        followTempo.setButtonText ("Use the project's tempo");
        followTempo.onClick = [this] { editor.processor.followProjectTempo = followTempo.getToggleState(); pull(); };
        addAndMakeVisible (followTempo);

        key.addItem ("Auto", 1);
        const auto keys = keyNames();
        for (int i = 0; i < keys.size(); ++i) key.addItem (keys[i], i + 2);
        for (int i = 0; i < juce::numElementsInArray (kTimes); ++i) time.addItem (kTimes[i].label, i + 1);
        for (int i = 0; i < juce::numElementsInArray (kLengths); ++i)
            length.addItem (kLengths[i] == 0 ? juce::String ("Auto") : Library::formatLength (kLengths[i]), i + 1);
        for (int i = 1; i <= 4; ++i) versions.addItem (juce::String (i), i);
        for (auto* box : { &key, &time, &length, &versions, &model, &language })
        {
            box->onChange = [this] { push(); };
            addAndMakeVisible (*box);
        }
        for (int i = 0; i < juce::numElementsInArray (kLanguages); ++i) language.addItem (kLanguages[i].label, i + 1);

        source.onChange = [this] (int) { push(); relayout(); };
        addAndMakeVisible (source);

        catchButton.onClick = [this] { catchPressed(); };
        chooseButton.setButtonText ("Choose a file");
        chooseButton.onClick = [this] { chooseFile(); };
        addAndMakeVisible (catchButton);
        addAndMakeVisible (chooseButton);

        for (auto* s : { &strength, &steps })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s->setColour (juce::Slider::trackColourId, theme::yellow);
            s->setColour (juce::Slider::backgroundColourId, theme::control);
            s->setColour (juce::Slider::thumbColourId, theme::text);
            s->onValueChange = [this] { push(); repaint(); };
            addAndMakeVisible (*s);
        }
        strength.setRange (0.1, 1.0, 0.05);
        steps.setRange (4.0, 80.0, 1.0);

        theme::styleEditor (redoFrom, "0:00");
        theme::styleEditor (redoTo, "end");
        for (auto* ed : { &redoFrom, &redoTo })
        {
            ed->setInputRestrictions (6, "0123456789:.");
            ed->onTextChange = [this] { push(); };
            addAndMakeVisible (*ed);
        }

        moreButton.onClick = [this]
        {
            showMore = ! showMore;
            editor.services.settings.setBool ("showMoreOptions", showMore);
            relayout();
        };
        moreButton.getProperties().set ("small", true);
        addAndMakeVisible (moreButton);
        showMore = editor.services.settings.getBool ("showMoreOptions", false);

        randomSeed.setButtonText ("A new random seed each time");
        randomSeed.onClick = [this] { push(); pull(); };
        addAndMakeVisible (randomSeed);
        theme::styleEditor (seed, "Seed");
        seed.setInputRestrictions (18, "0123456789");
        seed.onTextChange = [this] { push(); };
        addAndMakeVisible (seed);
        thinking.setButtonText ("Plan the song first (slower, better structure)");
        thinking.onClick = [this] { push(); };
        addAndMakeVisible (thinking);

        pull();
    }

    /** Copies the draft into the controls. */
    void pull()
    {
        pulling = true;
        auto& p = editor.processor;
        const auto& d = p.draft;
        const bool simple = d.mode == SongRequest::Mode::simple;

        mode.select (simple ? 0 : 1);
        setTextIfChanged (description, d.description);
        setTextIfChanged (title, d.title);
        setTextIfChanged (style, d.style);
        setTextIfChanged (lyrics, d.lyrics);
        chips.setStyle (d.style);
        instrumental.setToggleState (d.instrumental, juce::dontSendNotification);

        if (! filling)
        {
            const bool online = editor.services.settings.online();
            fillButton.setButtonText (online ? (d.enhance ? "Writing it out: on" : "Write it out for me") : juce::String ("Write it out for me"));
            fillButton.getProperties().set ("primary", online && d.enhance);
        }

        const bool follow = p.followProjectTempo && p.hostKnowsTempo();
        followTempo.setToggleState (p.followProjectTempo, juce::dontSendNotification);
        followTempo.setButtonText (p.hostKnowsTempo() ? "Use the project's tempo (" + juce::String (juce::roundToInt (p.hostBpm())) + ")"
                                                      : juce::String ("Use the project's tempo"));
        tempo.setEnabled (! follow);
        setTextIfChanged (tempo, follow ? juce::String (juce::roundToInt (p.hostBpm())) : (d.bpm > 0 ? juce::String (d.bpm) : juce::String()));

        selectText (key, d.key.isEmpty() ? juce::String ("Auto") : d.key);
        int timeIndex = 0;
        for (int i = 0; i < juce::numElementsInArray (kTimes); ++i) if (d.timeSignature == kTimes[i].value) timeIndex = i;
        time.setSelectedId (timeIndex + 1, juce::dontSendNotification);
        selectText (length, d.seconds <= 0.0 ? juce::String ("Auto") : Library::formatLength (d.seconds));
        versions.setSelectedId (juce::jlimit (1, 4, d.variations), juce::dontSendNotification);

        source.select ((int) d.source);
        strength.setValue (d.strength, juce::dontSendNotification);
        setTextIfChanged (redoFrom, d.repaintStart > 0.0 ? formatSeconds (d.repaintStart) : juce::String());
        setTextIfChanged (redoTo, d.repaintEnd >= 0.0 ? formatSeconds (d.repaintEnd) : juce::String());

        // the engine's models, with whichever one the draft asks for kept in the list even if the engine is off
        const auto models = editor.services.watch.models();
        juce::StringArray names;
        names.add ("The engine's usual model");
        for (auto& m : models) names.add (m.name);
        if (d.model.isNotEmpty()) names.addIfNotAlreadyThere (d.model);
        if (names != modelNames)
        {
            modelNames = names;
            model.clear (juce::dontSendNotification);
            for (int i = 0; i < names.size(); ++i) model.addItem (names[i], i + 1);
        }
        model.setSelectedId (d.model.isEmpty() ? 1 : names.indexOf (d.model) + 1, juce::dontSendNotification);

        int languageIndex = 0;
        for (int i = 0; i < juce::numElementsInArray (kLanguages); ++i) if (d.language == kLanguages[i].value) languageIndex = i;
        language.setSelectedId (languageIndex + 1, juce::dontSendNotification);
        steps.setValue (d.steps, juce::dontSendNotification);
        randomSeed.setToggleState (d.randomSeed, juce::dontSendNotification);
        seed.setEnabled (! d.randomSeed);
        setTextIfChanged (seed, d.randomSeed ? juce::String() : juce::String (d.seed));
        thinking.setToggleState (d.thinking, juce::dontSendNotification);

        pulling = false;
        if (simple != wasSimple || d.instrumental != wasInstrumental || d.source != wasSource) relayout();
        updateCatchButton();
        repaint();
    }

    /** Copies the controls into the draft. */
    void push()
    {
        if (pulling) return;
        auto& d = editor.processor.draft;
        d.mode = mode.selected() == 0 ? SongRequest::Mode::simple : SongRequest::Mode::custom;
        d.description = description.getText();
        d.title = title.getText();
        d.style = style.getText();
        d.lyrics = lyrics.getText();
        d.instrumental = instrumental.getToggleState();
        if (tempo.isEnabled()) d.bpm = juce::jlimit (0, 300, tempo.getText().getIntValue());
        d.key = key.getSelectedId() <= 1 ? juce::String() : key.getText();
        d.timeSignature = kTimes[juce::jlimit (0, juce::numElementsInArray (kTimes) - 1, time.getSelectedId() - 1)].value;
        d.seconds = length.getSelectedId() <= 1 ? 0.0 : parseSeconds (length.getText(), 0.0);
        d.variations = juce::jlimit (1, 4, versions.getSelectedId());
        d.source = (SongRequest::Source) juce::jlimit (0, 3, source.selected());
        d.strength = strength.getValue();
        d.repaintStart = parseSeconds (redoFrom.getText(), 0.0);
        d.repaintEnd = parseSeconds (redoTo.getText(), -1.0);
        d.model = model.getSelectedId() <= 1 ? juce::String() : model.getText();
        d.language = kLanguages[juce::jlimit (0, juce::numElementsInArray (kLanguages) - 1, language.getSelectedId() - 1)].value;
        d.steps = juce::roundToInt (steps.getValue());
        d.randomSeed = randomSeed.getToggleState();
        if (! d.randomSeed) d.seed = seed.getText().getLargeIntValue();
        editor.services.settings.set ("defaultVariations", juce::String (d.variations));
        chips.setStyle (d.style);
        editor.processor.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withNonParameterStateChanged (true));
    }

    void tick()
    {
        updateCatchButton();
        const auto state = editor.processor.recorder.state();
        if (state != Recorder::State::idle || lastState != state) repaint (sourcePanel);
        lastState = state;

        // keep the tempo box in step with the project while it follows it
        auto& p = editor.processor;
        if (p.followProjectTempo && p.hostKnowsTempo() && juce::roundToInt (p.hostBpm()) != tempo.getText().getIntValue()) pull();
    }

    int layout (int width)
    {
        titles.clear();
        hints.clear();
        const auto& d = editor.processor.draft;
        const bool simple = d.mode == SongRequest::Mode::simple;
        wasSimple = simple;
        wasInstrumental = d.instrumental;
        wasSource = d.source;
        int y = 0;
        auto heading = [&] (const juce::String& text, int gapAbove = 18) { y += gapAbove; titles.add ({ text, { 0, y, width, 16 } }); y += 22; };

        mode.setBounds (0, y, width, 38);
        y += 38;

        description.setVisible (simple);
        ideaButton.setVisible (simple);
        title.setVisible (! simple);
        style.setVisible (! simple);
        chips.setVisible (! simple);
        fillButton.setVisible (! simple);
        lyrics.setVisible (! simple && ! d.instrumental);
        for (auto* b : tagButtons) b->setVisible (! simple && ! d.instrumental);

        if (simple)
        {
            heading ("DESCRIBE THE SONG");
            description.setBounds (0, y, width, 112);
            y += 112 + 10;
            ideaButton.setBounds (0, y, 150, 32);
            instrumental.setBounds (width - 130, y, 130, 32);
            y += 32;
        }
        else
        {
            heading ("TITLE");
            title.setBounds (0, y, width, 34);
            y += 34;

            heading ("STYLE");
            style.setBounds (0, y, width, 56);
            y += 56 + 10;
            const int chipsHeight = chips.layout (width);
            chips.setBounds (0, y, width, chipsHeight);
            y += chipsHeight;

            heading ("LYRICS");
            if (! d.instrumental)
            {
                int x = 0;
                for (auto* b : tagButtons)
                {
                    const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (theme::bodyBold (11.5f), b->getButtonText())) + 22;
                    b->setBounds (x, y, w, 25);
                    x += w + 6;
                }
                y += 25 + 8;
                lyrics.setBounds (0, y, width, 190);
                y += 190 + 10;
            }
            else
            {
                hints.add ({ "No singing on this one. Switch Instrumental off to write lyrics.", { 0, y, width, 20 } });
                y += 20 + 10;
            }
            fillButton.setBounds (0, y, 170, 32);
            instrumental.setBounds (width - 130, y, 130, 32);
            y += 32;
        }

        heading ("MUSIC", 22);
        {
            // five boxes in a row when there is room, otherwise three over two, so a key like "F# minor" always fits
            const int gap = 10;
            juce::Component* boxes[] = { &tempo, &key, &time, &length, &versions };
            const char* names[] = { "Tempo", "Key", "Time", "Length", "Versions" };
            auto row = [&] (int first, int count, const float* weights)
            {
                const int room = width - gap * (count - 1);
                int x = 0;
                for (int i = 0; i < count; ++i)
                {
                    const int w = i == count - 1 ? width - x : juce::roundToInt ((float) room * weights[i]);
                    hints.add ({ names[first + i], { x, y, w, 16 } });
                    boxes[first + i]->setBounds (x, y + 20, w, 34);
                    x += w + gap;
                }
                y += 20 + 34;
            };
            if (width >= 500)
            {
                const float weights[] = { 0.15f, 0.27f, 0.18f, 0.20f, 0.20f };
                row (0, 5, weights);
            }
            else
            {
                const float top[] = { 0.26f, 0.42f, 0.32f };
                const float under[] = { 0.5f, 0.5f };
                row (0, 3, top);
                y += 10;
                row (3, 2, under);
            }
            y += 8;
            followTempo.setBounds (0, y, width, 26);
            y += 26;
        }

        heading ("START FROM AUDIO", 22);
        source.setBounds (0, y, width, 36);
        y += 36 + 8;
        {
            const char* explain[] = {
                "Make the song from the words alone.",
                "Borrow the sound of a piece of audio: its instruments, its mix, its feel. The tune is new.",
                "Remake a piece of audio in a new style. The tune and the timing stay, the sound changes.",
                "Keep a piece of audio and have the engine redo just one part of it." };
            const auto text = juce::String (explain[(int) d.source]);
            const int h = (int) std::ceil (theme::wrappedHeight (text, (float) width, theme::body (12.0f)));
            hints.add ({ text, { 0, y, width, h } });
            y += h;
        }

        const bool wantsAudio = d.source != SongRequest::Source::none;
        const bool cover = d.source == SongRequest::Source::cover, redo = d.source == SongRequest::Source::repaint;
        catchButton.setVisible (wantsAudio);
        chooseButton.setVisible (wantsAudio);
        strength.setVisible (cover);
        redoFrom.setVisible (redo);
        redoTo.setVisible (redo);
        sourcePanel = {};
        if (wantsAudio)
        {
            y += 10;
            const int panelHeight = 14 + 40 + 10 + 34 + ((cover || redo) ? 16 + 52 : 0) + 14;
            sourcePanel = { 0, y, width, panelHeight };
            int py = y + 14 + 40 + 10;
            const int half = (width - 28 - 10) / 2;
            catchButton.setBounds (14, py, half, 34);
            chooseButton.setBounds (14 + half + 10, py, half, 34);
            py += 34 + 16;
            if (cover) strength.setBounds (14, py + 24, width - 28, 24);
            if (redo)
            {
                redoFrom.setBounds (14, py + 20, half, 32);
                redoTo.setBounds (14 + half + 10, py + 20, half, 32);
            }
            extraRowY = py;
            y += panelHeight;
        }

        y += 18;
        moreButton.setButtonText (showMore ? "Fewer options" : "More options");
        moreButton.setBounds (0, y, 124, 28);
        y += 28;

        for (auto* c : std::initializer_list<juce::Component*> { &model, &language, &steps, &randomSeed, &seed, &thinking })
            c->setVisible (showMore);
        stepsLabelY = -1;
        if (showMore)
        {
            y += 10;
            const int half = (width - 10) / 2;
            hints.add ({ "Model", { 0, y, half, 16 } });
            hints.add ({ "Language of the lyrics", { half + 10, y, half, 16 } });
            model.setBounds (0, y + 20, half, 34);
            language.setBounds (half + 10, y + 20, half, 34);
            y += 20 + 34 + 14;

            stepsLabelY = y;
            steps.setBounds (0, y + 20, width, 24);
            y += 20 + 24 + 12;

            randomSeed.setBounds (0, y, width - 150, 30);
            seed.setBounds (width - 140, y, 140, 30);
            y += 30 + 6;
            thinking.setBounds (0, y, width, 30);
            y += 30;
        }
        return y + 16;
    }

    void paint (juce::Graphics& g) override
    {
        for (auto& t : titles) drawSectionTitle (g, t.text, t.area.getX(), t.area.getY(), t.area.getWidth());
        for (auto& h : hints)
            theme::drawWrapped (g, h.text, h.area.toFloat(), theme::body (12.0f), theme::textDim);

        if (stepsLabelY >= 0)
        {
            const int n = juce::roundToInt (steps.getValue());
            g.setColour (theme::textDim);
            g.setFont (theme::body (12.0f));
            g.drawText (editor.services.settings.online()
                            ? juce::String ("Quality: set by the online service (this only applies on your own computer)")
                            : "Quality: " + juce::String (n) + " steps" + (n <= 12 ? "  (quick, right for the turbo models)" : n <= 40 ? "  (slower)" : "  (slow, for the base models)"),
                        0, stepsLabelY, getWidth(), 16, juce::Justification::centredLeft);
        }

        if (sourcePanel.isEmpty()) return;
        theme::fillPanel (g, sourcePanel.toFloat(), theme::panel, 10.0f);
        const auto& d = editor.processor.draft;
        auto& recorder = editor.processor.recorder;
        auto row = juce::Rectangle<int> (sourcePanel.getX() + 14, sourcePanel.getY() + 14, sourcePanel.getWidth() - 28, 40);

        const juce::File audio (juce::File::isAbsolutePath (d.audioPath) ? d.audioPath : juce::String());
        const bool have = audio.existsAsFile();
        const auto state = recorder.state();
        auto icon = row.removeFromLeft (40).toFloat();
        g.setColour (state == Recorder::State::recording ? theme::bad.withAlpha (0.18f) : theme::control);
        g.fillRoundedRectangle (icon, 8.0f);
        theme::drawIcon (g, state == Recorder::State::idle ? theme::Icon::wave : theme::Icon::record, icon.reduced (11.0f),
                         state == Recorder::State::recording ? theme::bad : (have ? theme::yellow : theme::textMute), 1.3f);
        row.removeFromLeft (12);

        juce::String top, bottom;
        if (state == Recorder::State::recording)   { top = "Catching the track"; bottom = Library::formatLength (recorder.seconds()) + " so far. Stop your project, or press Stop."; }
        else if (state == Recorder::State::armed)  { top = "Ready to catch"; bottom = "Press play in your project and it starts by itself."; }
        else if (have)
        {
            if (audio != measured) { measured = audio; measuredSeconds = audioLengthSeconds (audio); }
            top = audio.getFileName();
            bottom = Library::formatLength (measuredSeconds) + " long";
        }
        else                                        { top = "No audio yet"; bottom = "Catch what this track is playing, choose a file, or drop one here."; }

        g.setColour (theme::text);
        g.setFont (theme::bodyBold (13.0f));
        g.drawText (top, row.removeFromTop (20), juce::Justification::centredLeft, true);
        g.setColour (theme::textDim);
        g.setFont (theme::body (11.5f));
        g.drawText (bottom, row.removeFromTop (18), juce::Justification::centredLeft, true);

        // how loud the track is right now, so it is plain the plugin can hear it
        if (state != Recorder::State::idle)
        {
            const float level = juce::jlimit (0.0f, 1.0f, recorder.level());
            auto meter = juce::Rectangle<float> ((float) sourcePanel.getRight() - 14.0f - 70.0f, (float) sourcePanel.getY() + 18.0f, 70.0f, 5.0f);
            g.setColour (theme::control);
            g.fillRoundedRectangle (meter, 2.5f);
            g.setColour (level > 0.95f ? theme::bad : theme::good);
            g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * std::sqrt (level)), 2.5f);
        }

        g.setColour (theme::textDim);
        g.setFont (theme::body (12.0f));
        if (d.source == SongRequest::Source::cover)
        {
            const int percent = juce::roundToInt (strength.getValue() * 100.0);
            g.drawText ("How closely to follow the original: " + juce::String (percent) + "%", sourcePanel.getX() + 14, extraRowY, sourcePanel.getWidth() - 28, 18, juce::Justification::centredLeft);
        }
        else if (d.source == SongRequest::Source::repaint)
        {
            const int half = (sourcePanel.getWidth() - 28 - 10) / 2;
            g.drawText ("Redo from", sourcePanel.getX() + 14, extraRowY, half, 16, juce::Justification::centredLeft);
            g.drawText ("Up to (empty for the end)", sourcePanel.getX() + 14 + half + 10, extraRowY, half, 16, juce::Justification::centredLeft);
        }
    }

    std::function<void()> onHeightChanged;

    void setAudio (const juce::File& file)
    {
        auto& d = editor.processor.draft;
        d.audioPath = file.getFullPathName();
        if (d.source == SongRequest::Source::none) d.source = SongRequest::Source::cover;
        editor.processor.draftChanged();
    }

private:
    void relayout() { if (onHeightChanged) onHeightChanged(); }

    void selectText (juce::ComboBox& box, const juce::String& text)
    {
        for (int i = 0; i < box.getNumItems(); ++i)
            if (box.getItemText (i).equalsIgnoreCase (text)) { box.setSelectedItemIndex (i, juce::dontSendNotification); return; }
        // a value the list does not have (it came back from the engine, or from an older song): add it
        box.addItem (text, box.getNumItems() + 100);
        box.setSelectedItemIndex (box.getNumItems() - 1, juce::dontSendNotification);
    }

    void toggleTag (const juce::String& tag)
    {
        auto parts = juce::StringArray::fromTokens (style.getText(), ",", {});
        parts.trim();
        parts.removeEmptyStrings();
        bool removed = false;
        for (int i = parts.size(); --i >= 0;)
            if (parts[i].equalsIgnoreCase (tag)) { parts.remove (i); removed = true; }
        if (! removed) parts.add (tag);
        style.setText (parts.joinIntoString (", "), true);
    }

    void showAllStyles()
    {
        // every style the engine was taught, filed under its first letter
        auto all = juce::StringArray::fromLines (juce::String::fromUTF8 (BinaryData::styles_txt, BinaryData::styles_txtSize));
        all.trim();
        all.removeEmptyStrings();
        all.sort (true);
        juce::PopupMenu menu;
        juce::PopupMenu letter;
        juce::String current;
        int id = 1;
        juce::StringArray byId;
        for (auto& s : all)
        {
            const auto first = s.substring (0, 1).toUpperCase();
            const auto group = first.containsOnly ("ABCDEFGHIJKLMNOPQRSTUVWXYZ") ? first : juce::String ("0-9");
            if (group != current)
            {
                if (current.isNotEmpty()) menu.addSubMenu (current, letter);
                letter = juce::PopupMenu();
                current = group;
            }
            letter.addItem (id++, s, true, StyleChips::contains (style.getText(), s));
            byId.add (s);
        }
        if (current.isNotEmpty()) menu.addSubMenu (current, letter);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&chips).withParentComponent (&editor).withMaximumNumColumns (1),
                            [safe = juce::Component::SafePointer<CreateForm> (this), byId] (int choice)
        {
            if (safe != nullptr && choice > 0 && choice <= byId.size()) safe->toggleTag (byId[choice - 1]);
        });
    }

    void insertTag (const juce::String& tag)
    {
        // a section tag goes on its own line, with an empty line above it unless it opens the lyrics
        const auto before = lyrics.getText().substring (0, lyrics.getCaretPosition());
        juce::String lead;
        if (before.trim().isNotEmpty())
            lead = before.endsWith ("\n\n") ? "" : (before.endsWith ("\n") ? "\n" : "\n\n");
        lyrics.insertTextAtCaret (lead + "[" + tag + "]\n");
        lyrics.grabKeyboardFocus();
    }

    void fillOut()
    {
        if (filling) return;
        push();
        auto request = editor.processor.draft;
        if (request.style.trim().isEmpty()) { editor.toast ("Add a style first, so the engine knows what to write.", true); return; }
        if (editor.services.settings.online())
        {
            // online, the service writes the song out as part of making it
            auto& d = editor.processor.draft;
            d.enhance = ! d.enhance;
            editor.processor.draftChanged();
            editor.toast (d.enhance ? "On: the online service will write your style and lyrics out more fully when it makes the song."
                                    : "Off: the song will be made from exactly what you wrote.");
            return;
        }
        if (! editor.services.watch.status().reachable) { editor.toast ("The engine has to be running for this.", true); return; }

        filling = true;
        fillButton.setButtonText ("Writing...");
        fillButton.setEnabled (false);
        const auto url = editor.services.settings.engineUrl();
        const auto apiKey = editor.services.settings.engineKey();
        juce::Thread::launch ([safe = juce::Component::SafePointer<CreateForm> (this), request, url, apiKey]() mutable
        {
            juce::String error;
            const bool ok = EngineClient (url, apiKey).enhance (request, error);
            juce::MessageManager::callAsync ([safe, request, ok, error]
            {
                if (safe == nullptr) return;
                safe->filling = false;
                safe->fillButton.setButtonText ("Write it out for me");
                safe->fillButton.setEnabled (true);
                if (! ok)
                {
                    safe->editor.toast (EngineClient::isWriterMissing (error) ? juce::String ("The engine's writer is not loaded on this computer, so it cannot write the song out.") : error, true);
                    return;
                }
                auto& d = safe->editor.processor.draft;
                d.style = request.style;
                d.lyrics = request.lyrics;
                if (request.bpm > 0) { d.bpm = request.bpm; safe->editor.processor.followProjectTempo = false; }
                d.key = request.key;
                d.timeSignature = request.timeSignature;
                if (request.seconds > 0.0) d.seconds = request.seconds;
                safe->editor.processor.draftChanged();
                safe->editor.toast ("Written out. Change anything you like before you make it.");
            });
        });
    }

    void updateCatchButton()
    {
        auto& recorder = editor.processor.recorder;
        juce::String text;
        switch (recorder.state())
        {
            case Recorder::State::idle:      text = "Catch from the track"; break;
            case Recorder::State::armed:     text = "Start now"; break;
            case Recorder::State::recording: text = "Stop  " + Library::formatLength (recorder.seconds()); break;
        }
        if (catchButton.getButtonText() != text) catchButton.setButtonText (text);
        const bool live = recorder.state() == Recorder::State::recording;
        if ((bool) catchButton.getProperties()["danger"] != live) { catchButton.getProperties().set ("danger", live); catchButton.repaint(); }
    }

    void catchPressed()
    {
        auto& recorder = editor.processor.recorder;
        switch (recorder.state())
        {
            case Recorder::State::idle:
                if (editor.processor.getTotalNumInputChannels() == 0) { editor.toast ("This track gives the plugin no sound to catch. Put the plugin on an audio track.", true); return; }
                recorder.arm();
                break;
            case Recorder::State::armed:     recorder.startNow(); break;
            case Recorder::State::recording: recorder.stop(); break;
        }
        updateCatchButton();
        repaint();
    }

    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose the audio to start from", juce::File(), "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.m4a;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe = juce::Component::SafePointer<CreateForm> (this)] (const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (safe != nullptr && file.existsAsFile()) safe->setAudio (file);
        });
    }

    struct Text { juce::String text; juce::Rectangle<int> area; };

    MakerEditor& editor;
    Segmented mode { { "Describe it", "Write it yourself" } };
    juce::TextEditor description, title, style, lyrics, tempo, redoFrom, redoTo, seed;
    StyleChips chips;
    juce::TextButton ideaButton, fillButton, catchButton, chooseButton, moreButton;
    juce::ToggleButton instrumental, followTempo, randomSeed, thinking;
    juce::OwnedArray<juce::TextButton> tagButtons;
    juce::ComboBox key, time, length, versions, model, language;
    Segmented source { { "Nothing", "Its sound", "Remake it", "Redo a part" } };
    juce::Slider strength, steps;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Array<Text> titles, hints;
    juce::StringArray modelNames;
    juce::Rectangle<int> sourcePanel;
    juce::File measured;
    double measuredSeconds = 0.0;
    int extraRowY = 0, stepsLabelY = -1, ideaIndex = -1;
    bool pulling = false, showMore = false, filling = false;
    bool wasSimple = true, wasInstrumental = false;
    SongRequest::Source wasSource = SongRequest::Source::none;
    Recorder::State lastState = Recorder::State::idle;
};

//==============================================================================
class CreatePage : public Page, public juce::FileDragAndDropTarget
{
public:
    explicit CreatePage (MakerEditor& e) : editor (e), form (e), jobs (e), songs (e)
    {
        viewport.setViewedComponent (&form, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (9);
        addAndMakeVisible (viewport);
        form.onHeightChanged = [this] { layoutForm(); };

        makeButton.setButtonText ("MAKE THE SONG");
        theme::makePrimary (makeButton);
        makeButton.getProperties().set ("big", true);
        makeButton.onClick = [this] { editor.makeSong(); };
        addAndMakeVisible (makeButton);

        addAndMakeVisible (jobs);
        songs.setEmptyMessage ("Nothing made yet", "Describe a song on the left and press Make the song. Everything you make lands here, ready to play and drag into your project.");
        addAndMakeVisible (songs);
        refresh();
    }

    void refresh() override
    {
        form.pull();
        jobs.setJobs (editor.services.generator.jobs());
        songs.setSongs (editor.services.library.songs());
        if (jobs.idealHeight() != jobs.getHeight()) resized();
        repaint();
    }

    void shown() override { layoutForm(); }

    void tick() override
    {
        form.tick();
        if (jobs.anyActive()) jobs.repaint();
        songs.playerChanged();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::text);
        g.setFont (theme::headline (24.0f));
        g.drawText ("CREATE", 28, 20, 300, 30, juce::Justification::centredLeft);

        g.setColour (theme::line);
        g.fillRect (divider, 0, 1, getHeight());

        g.setColour (theme::text);
        g.setFont (theme::headline (16.0f));
        g.drawText (jobs.idealHeight() > 0 ? (jobs.anyActive() ? "MAKING NOW" : "QUEUE") : "YOUR SONGS", divider + 24, 24, 300, 24, juce::Justification::centredLeft);
        if (jobs.idealHeight() > 0)
            g.drawText ("YOUR SONGS", divider + 24, songs.getY() - 30, 300, 24, juce::Justification::centredLeft);
        else
        {
            g.setColour (theme::textMute);
            g.setFont (theme::body (12.0f));
            g.drawText (juce::String (songs.numSongs()) + (songs.numSongs() == 1 ? " song" : " songs"), getWidth() - 24 - 120, 24, 120, 24, juce::Justification::centredRight);
        }

        // under the button: whether the engine is there to make it
        const auto summary = editor.engineSummary();
        if (! summary.ready)
        {
            g.setColour (summary.colour);
            g.fillEllipse (28.0f, (float) makeButton.getBottom() + 12.0f, 7.0f, 7.0f);
            g.setColour (theme::textDim);
            g.setFont (theme::body (11.5f));
            g.drawText (summary.text + (summary.detail.isNotEmpty() ? ": " + summary.detail : juce::String()), 42, makeButton.getBottom() + 6, divider - 70, 18, juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        divider = juce::jlimit (400, 560, getWidth() * 48 / 100);
        const bool engineNote = ! editor.engineSummary().ready;
        auto left = getLocalBounds().withWidth (divider).reduced (28, 0);
        left.removeFromTop (66);
        auto bottom = left.removeFromBottom (engineNote ? 86 : 70);
        makeButton.setBounds (bottom.getX(), bottom.getY() + 10, bottom.getWidth(), 46);
        viewport.setBounds (left.withWidth (left.getWidth() + 14));
        layoutForm();

        auto right = getLocalBounds().withTrimmedLeft (divider + 1);
        right.removeFromTop (60);
        const int jobsHeight = juce::jmin (jobs.idealHeight(), right.getHeight() / 2);
        jobs.setBounds (right.removeFromTop (jobsHeight).reduced (16, 0));
        if (jobsHeight > 0) right.removeFromTop (44);
        songs.setBounds (right.reduced (12, 0).withTrimmedBottom (8));
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        return files.size() == 1 && juce::File (files[0]).hasFileExtension ("wav;mp3;flac;aif;aiff;m4a;ogg");
    }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        if (files.size() == 1 && juce::File (files[0]).existsAsFile())
        {
            form.setAudio (juce::File (files[0]));
            editor.toast ("That audio is what the song will start from.");
        }
    }

private:
    void layoutForm()
    {
        const int width = juce::jmax (200, viewport.getWidth() - 14);
        const int height = form.layout (width);
        form.setSize (width, height);
        form.repaint();
    }

    MakerEditor& editor;
    juce::Viewport viewport;
    CreateForm form;
    juce::TextButton makeButton;
    JobsView jobs;
    SongList songs;
    int divider = 480;
};
}

std::unique_ptr<Page> makeCreatePage (MakerEditor& e) { return std::make_unique<CreatePage> (e); }
}
