#include "Pages.h"
#include "../PluginEditor.h"

namespace mm
{
namespace
{
class SettingsContent : public juce::Component
{
public:
    explicit SettingsContent (MakerEditor& e) : editor (e)
    {
        auto& s = editor.services.settings;

        startButton.onClick = [this]
        {
            auto& runner = editor.services.runner;
            if (editor.services.watch.status().reachable || editor.engineSummary().starting)
            {
                runner.stop();
                runner.triedAutoStart = true;
                editor.services.watch.checkSoon();
                editor.toast ("Engine stopped.");
            }
            else editor.startEngine();
            refresh();
        };
        setupButton.onClick = [this]
        {
            juce::String error;
            if (editor.services.runner.openInstaller (error)) editor.toast ("The setup has opened in its own window. Come back here when it says All done.");
            else                                              editor.toast (error, true);
        };
        checkButton.setButtonText ("Check again");
        checkButton.onClick = [this] { editor.services.watch.checkSoon(); };
        for (auto* b : { &startButton, &setupButton, &checkButton, &folderButton, &songsButton, &updateButton, &logButton }) addAndMakeVisible (*b);

        theme::styleEditor (address, "http://127.0.0.1:8001");
        address.setText (s.get ("engineUrl", "http://127.0.0.1:8001"), false);
        address.onTextChange = [this]
        {
            editor.services.settings.set ("engineUrl", address.getText().trim());
            editor.services.watch.checkSoon();
            refresh();
        };
        theme::styleEditor (apiKey, "Only if the engine was given one");
        apiKey.setText (s.get ("engineKey"), false);
        apiKey.setPasswordCharacter ((juce::juce_wchar) 0x2022);
        apiKey.onTextChange = [this] { editor.services.settings.set ("engineKey", apiKey.getText().trim()); editor.services.watch.checkSoon(); };
        addAndMakeVisible (address);
        addAndMakeVisible (apiKey);

        folderButton.setButtonText ("Change");
        folderButton.onClick = [this] { chooseEngineFolder(); };
        logButton.setButtonText ("Show the engine's log");
        theme::makeQuiet (logButton);
        logButton.onClick = [this]
        {
            const auto log = editor.services.runner.logFile();
            auto text = log.existsAsFile() ? log.loadFileAsString() : juce::String();
            if (text.length() > 12000) text = text.substring (text.length() - 12000);
            editor.showText ("The engine's log", text.trim().isEmpty() ? juce::String ("The engine has not been started from the plugin yet, so there is nothing here.") : text);
        };

        auto toggle = [this, &s] (juce::ToggleButton& b, const juce::String& text, const char* key, bool fallback)
        {
            b.setButtonText (text);
            b.setToggleState (s.getBool (key, fallback), juce::dontSendNotification);
            b.onClick = [this, &b, key] { editor.services.settings.setBool (key, b.getToggleState()); refresh(); };
            addAndMakeVisible (b);
        };
        toggle (autoStart, "Start the engine by itself when I open the plugin", "autoStartEngine", true);
        toggle (keepRunning, "Leave the engine running after I close my project", "keepEngineRunning", false);
        toggle (share, "Let other computers on my network use this engine", "shareEngine", false);

        format.addItem ("WAV (best for projects)", 1);
        format.addItem ("FLAC (same quality, half the size)", 2);
        format.addItem ("MP3 (smallest)", 3);
        const auto f = s.audioFormat();
        format.setSelectedId (f == "flac" ? 2 : f == "mp3" ? 3 : 1, juce::dontSendNotification);
        format.onChange = [this] { editor.services.settings.set ("audioFormat", format.getSelectedId() == 2 ? "flac" : format.getSelectedId() == 3 ? "mp3" : "wav"); };
        addAndMakeVisible (format);

        songsButton.setButtonText ("Open");
        songsButton.onClick = [] { Settings::libraryDir().revealToUser(); };

        updateButton.onClick = [this]
        {
            auto& u = editor.services.updater;
            if (u.getState().phase == UpdateState::Phase::available) u.installNow(); else u.checkNow();
        };
        refresh();
    }

    void refresh()
    {
        auto& runner = editor.services.runner;
        const auto summary = editor.engineSummary();
        const bool local = runner.usesThisComputer();
        const bool installed = runner.installed();
        const bool running = editor.services.watch.status().reachable || summary.starting;

        startButton.setButtonText (running ? "Stop the engine" : "Start the engine");
        startButton.setVisible (local && (installed || running));
        startButton.getProperties().set ("primary", ! running && installed);
        setupButton.setButtonText (installed ? "Set it up again" : "Set up the engine");
        setupButton.setVisible (local);
        setupButton.getProperties().set ("primary", ! installed && ! running);
        folderButton.setVisible (local);
        logButton.setVisible (local);
        autoStart.setVisible (local);
        keepRunning.setVisible (local);
        share.setVisible (local);

        const auto update = editor.services.updater.getState();
        const bool busy = update.phase == UpdateState::Phase::checking || update.phase == UpdateState::Phase::downloading || update.phase == UpdateState::Phase::installing;
        updateButton.setButtonText (update.phase == UpdateState::Phase::available ? "Install the update" : "Check for updates");
        updateButton.getProperties().set ("primary", update.phase == UpdateState::Phase::available);
        updateButton.setEnabled (! busy);
        repaint();
    }

    int layout (int width)
    {
        panels.clear();
        const auto summary = editor.engineSummary();
        const bool local = editor.services.runner.usesThisComputer();
        const int inner = width - 44;
        int y = 0;

        // the engine
        {
            const int top = y;
            y += 22 + 28;                               // heading
            statusY = y;
            y += 44 + 14;
            int x = 22;
            auto place = [&] (juce::Button& b, int w) { if (b.isVisible()) { b.setBounds (x, y, w, 34); x += w + 10; } };
            place (startButton, 150);
            place (setupButton, 160);
            place (checkButton, 120);
            y += 34 + 22;

            addressY = y;
            address.setBounds (22, y + 20, inner, 34);
            y += 20 + 34 + 6;
            addressHintHeight = (int) std::ceil (theme::wrappedHeight (addressHint(), (float) inner, theme::body (12.0f)));
            y += addressHintHeight + 16;

            keyY = y;
            apiKey.setBounds (22, y + 20, juce::jmin (inner, 360), 34);
            y += 20 + 34 + 16;

            if (local)
            {
                folderY = y;
                folderButton.setBounds (width - 22 - 90, y + 18, 90, 32);
                y += 20 + 34 + 12;
                autoStart.setBounds (22, y, inner, 28);    y += 30;
                keepRunning.setBounds (22, y, inner, 28);  y += 30;
                share.setBounds (22, y, inner, 28);        y += 30;
                shareHintY = share.getToggleState() ? y : -1;
                if (share.getToggleState()) y += 38;
                y += 6;
                logButton.setBounds (16, y, 180, 30);
                y += 30;
            }
            else folderY = shareHintY = -1;
            y += 18;
            panels.add ({ "THE ENGINE", { 0, top, width, y - top } });
            y += 16;
        }

        // songs
        {
            const int top = y;
            y += 22 + 28;
            formatY = y;
            format.setBounds (22, y + 20, juce::jmin (inner, 320), 34);
            y += 20 + 34 + 16;
            songsY = y;
            songsButton.setBounds (width - 22 - 90, y + 18, 90, 32);
            y += 20 + 34 + 18;
            panels.add ({ "YOUR SONGS", { 0, top, width, y - top } });
            y += 16;
        }

        // updates
        {
            const int top = y;
            y += 22 + 28;
            updateY = y;
            updateButton.setBounds (width - 22 - 170, y + 4, 170, 34);
            y += 44 + 18;
            panels.add ({ "UPDATES", { 0, top, width, y - top } });
            y += 16;
        }

        aboutY = y;
        y += (int) std::ceil (theme::wrappedHeight (aboutText(), (float) width - 8.0f, theme::body (12.0f))) + 20;
        juce::ignoreUnused (summary);
        return y;
    }

    void paint (juce::Graphics& g) override
    {
        const int width = getWidth();
        const int inner = width - 44;
        for (auto& p : panels)
        {
            theme::fillPanel (g, p.area.toFloat(), theme::panel, 12.0f);
            g.setColour (theme::text);
            g.setFont (theme::headline (16.0f));
            g.drawText (p.title, p.area.getX() + 22, p.area.getY() + 18, 300, 22, juce::Justification::centredLeft);
        }

        auto caption = [&] (const juce::String& text, int y) { g.setColour (theme::textDim); g.setFont (theme::bodyBold (11.0f)); g.drawText (text, 22, y, inner, 16, juce::Justification::centredLeft); };
        auto value = [&] (const juce::String& text, int y, int rightGap)
        {
            g.setColour (theme::text);
            g.setFont (theme::body (12.5f));
            g.drawText (text, 22, y + 20, inner - rightGap, 30, juce::Justification::centredLeft, true);
        };

        // where the engine stands, in one line with a second line of detail under it
        const auto summary = editor.engineSummary();
        auto status = juce::Rectangle<float> (22.0f, (float) statusY, (float) inner, 44.0f);
        g.setColour (theme::raised);
        g.fillRoundedRectangle (status, 9.0f);
        g.setColour (summary.colour);
        g.fillEllipse (status.getX() + 16.0f, status.getCentreY() - 5.0f, 10.0f, 10.0f);
        g.setColour (theme::text);
        g.setFont (theme::bodyBold (13.5f));
        const float nameWidth = juce::GlyphArrangement::getStringWidth (theme::bodyBold (13.5f), summary.text) + 14.0f;
        g.drawText (summary.text, juce::Rectangle<float> (status.getX() + 38.0f, status.getY(), nameWidth, status.getHeight()).toNearestInt(), juce::Justification::centredLeft);
        g.setColour (theme::textDim);
        g.setFont (theme::body (12.0f));
        g.drawText (summary.detail, juce::Rectangle<float> (status.getX() + 38.0f + nameWidth, status.getY(), status.getWidth() - 50.0f - nameWidth, status.getHeight()).toNearestInt(), juce::Justification::centredLeft, true);

        caption ("WHERE THE ENGINE IS", addressY);
        theme::drawWrapped (g, addressHint(), juce::Rectangle<float> (22.0f, (float) addressY + 60.0f, (float) inner, (float) addressHintHeight), theme::body (12.0f), theme::textDim);
        caption ("KEY", keyY);

        if (folderY >= 0)
        {
            caption ("ENGINE FOLDER ON THIS COMPUTER", folderY);
            value (editor.services.settings.engineFolder().getFullPathName(), folderY, 110);
        }
        if (shareHintY >= 0)
            theme::drawWrapped (g, "Start the engine again for this to take effect. On the other computer, put this computer's address in Where the engine is.",
                                juce::Rectangle<float> (50.0f, (float) shareHintY, (float) inner - 28.0f, 36.0f), theme::body (12.0f), theme::textDim);

        caption ("FILE TYPE FOR NEW SONGS", formatY);
        caption ("WHERE THEY ARE KEPT", songsY);
        value (Settings::libraryDir().getFullPathName(), songsY, 110);

        const auto update = editor.services.updater.getState();
        g.setColour (theme::text);
        g.setFont (theme::bodyBold (13.5f));
        g.drawText ("Version " + Updater::currentVersion(), 22, updateY, inner - 190, 20, juce::Justification::centredLeft);
        g.setColour (update.phase == UpdateState::Phase::failed ? theme::bad : theme::textDim);
        g.setFont (theme::body (12.0f));
        g.drawText (update.message.isNotEmpty() ? update.message : juce::String ("The plugin looks for a new version by itself."), 22, updateY + 21, inner - 190, 20, juce::Justification::centredLeft, true);
        if (update.phase == UpdateState::Phase::downloading)
        {
            auto bar = juce::Rectangle<float> (22.0f, (float) updateY + 46.0f, (float) inner - 190.0f, 4.0f);
            g.setColour (theme::control);
            g.fillRoundedRectangle (bar, 2.0f);
            g.setColour (theme::yellow);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * update.progress), 2.0f);
        }

        theme::drawWrapped (g, aboutText(), juce::Rectangle<float> (4.0f, (float) aboutY, (float) width - 8.0f, 80.0f), theme::body (12.0f), theme::textMute);
    }

    std::function<void()> onHeightChanged;

private:
    juce::String addressHint() const
    {
        return editor.services.runner.usesThisComputer()
            ? "Leave this alone when the engine runs on this computer. To use an engine on another computer, type that computer's address here, like http://192.168.1.20:8001."
            : "This points at another computer, so the engine is started and stopped over there. Put http://127.0.0.1:8001 back to use an engine on this one.";
    }

    static juce::String aboutText()
    {
        return "MU Music Maker makes its music with ACE-Step 1.5, an open source model that runs on your own computer. Your words and your audio stay on it; nothing is sent to a company's servers. Made for Musically Unorthodox.";
    }

    void chooseEngineFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose the folder the engine is in, or an empty one to set it up in", editor.services.settings.engineFolder());
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [safe = juce::Component::SafePointer<SettingsContent> (this)] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (safe == nullptr || ! dir.isDirectory()) return;
            safe->editor.services.settings.set ("engineFolder", dir.getFullPathName());
            safe->refresh();
            if (safe->onHeightChanged) safe->onHeightChanged();
        });
    }

    struct Panel { juce::String title; juce::Rectangle<int> area; };

    MakerEditor& editor;
    juce::TextButton startButton, setupButton, checkButton, folderButton, songsButton, updateButton, logButton;
    juce::TextEditor address, apiKey;
    juce::ToggleButton autoStart, keepRunning, share;
    juce::ComboBox format;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<Panel> panels;
    int statusY = 0, addressY = 0, addressHintHeight = 0, keyY = 0, folderY = -1, shareHintY = -1, formatY = 0, songsY = 0, updateY = 0, aboutY = 0;
};

class SettingsPage : public Page
{
public:
    explicit SettingsPage (MakerEditor& e) : editor (e), content (e)
    {
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (9);
        addAndMakeVisible (viewport);
        content.onHeightChanged = [this] { layoutContent(); };
    }

    void refresh() override { content.refresh(); layoutContent(); }
    void shown() override   { editor.services.watch.checkSoon(); }
    void tick() override    { if (editor.engineSummary().starting) content.repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::text);
        g.setFont (theme::headline (24.0f));
        g.drawText ("SETTINGS", 28, 20, 300, 30, juce::Justification::centredLeft);
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds().withTrimmedTop (66).withTrimmedLeft (28).withTrimmedRight (14));
        layoutContent();
    }

private:
    void layoutContent()
    {
        const int width = juce::jmin (760, juce::jmax (300, viewport.getWidth() - 24));
        content.setSize (width, content.layout (width));
        content.repaint();
    }

    MakerEditor& editor;
    juce::Viewport viewport;
    SettingsContent content;
};
}

std::unique_ptr<Page> makeSettingsPage (MakerEditor& e) { return std::make_unique<SettingsPage> (e); }
}
