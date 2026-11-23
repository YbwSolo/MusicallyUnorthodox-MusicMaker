#pragma once
#include "Theme.h"
#include "../core/Services.h"

class MakerEditor;

namespace mm
{
class Page : public juce::Component
{
public:
    virtual void refresh() {}       // something changed: the library, the queue, the engine, the form
    virtual void shown() {}
    virtual void tick() {}          // a few times a second, for things that move
};

std::unique_ptr<Page> makeCreatePage (MakerEditor&);
std::unique_ptr<Page> makeLibraryPage (MakerEditor&);
std::unique_ptr<Page> makeSettingsPage (MakerEditor&);
std::unique_ptr<Page> makeHelpPage (MakerEditor&);

/** A small heading in capitals above a group of controls. */
inline void drawSectionTitle (juce::Graphics& g, const juce::String& text, int x, int y, int w)
{
    g.setColour (theme::textDim);
    g.setFont (theme::bodyBold (11.0f));
    g.drawText (text, x, y, w, 16, juce::Justification::centredLeft);
}

/** A button that shows one of the line icons instead of words. */
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, theme::Icon i) : juce::Button (name), icon (i) { setTooltip (name); setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void setIcon (theme::Icon i)                { if (icon != i) { icon = i; repaint(); } }
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        if (filled)
        {
            g.setColour (down ? theme::yellow.darker (0.25f) : over ? theme::yellow.brighter (0.15f) : theme::yellow);
            g.fillEllipse (r.reduced (1.0f));
            theme::drawIcon (g, icon, r.reduced (r.getWidth() * 0.27f), theme::onYellow, 2.0f);
            return;
        }
        if (over || down)
        {
            g.setColour (theme::control.brighter (down ? 0.12f : 0.04f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        theme::drawIcon (g, icon, r.reduced (r.getWidth() * 0.24f), lit ? theme::yellow : (over ? theme::text : theme::textDim), 1.7f);
    }
    bool filled = false, lit = false;

private:
    theme::Icon icon;
};

/** Two or more choices side by side, one of them picked. */
class Segmented : public juce::Component
{
public:
    explicit Segmented (juce::StringArray names) : labels (std::move (names)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    int selected() const                        { return current; }
    void select (int index, bool tell = false)  { index = juce::jlimit (0, labels.size() - 1, index); if (index != current) { current = index; repaint(); if (tell && onChange) onChange (current); } }
    std::function<void (int)> onChange;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::control);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        const float w = r.getWidth() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (3.0f);
            if (i == current) { g.setColour (theme::yellow); g.fillRoundedRectangle (cell, 6.0f); }
            g.setColour (i == current ? theme::onYellow : (i == hover ? theme::text : theme::textDim));
            g.setFont (theme::bodyBold (12.5f));
            g.drawFittedText (labels[i], cell.toNearestInt(), juce::Justification::centred, 1);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = indexAt (e.x); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override   { hover = -1; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override   { if (isEnabled() && getLocalBounds().contains (e.getPosition())) select (indexAt (e.x), true); }

private:
    int indexAt (int x) const                   { return juce::jlimit (0, labels.size() - 1, x * labels.size() / juce::jmax (1, getWidth())); }
    juce::StringArray labels;
    int current = 0, hover = -1;
};
}
