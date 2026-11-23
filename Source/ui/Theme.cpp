#include "Theme.h"
#include "BinaryData.h"

namespace mm::theme
{
static juce::Typeface::Ptr loadTypeface (const void* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
static const juce::Typeface::Ptr& antonFace()     { static juce::Typeface::Ptr t = loadTypeface (BinaryData::AntonRegular_ttf, BinaryData::AntonRegular_ttfSize); return t; }
static const juce::Typeface::Ptr& interFace()     { static juce::Typeface::Ptr t = loadTypeface (BinaryData::InterRegular_ttf, BinaryData::InterRegular_ttfSize); return t; }
static const juce::Typeface::Ptr& interBoldFace() { static juce::Typeface::Ptr t = loadTypeface (BinaryData::InterSemiBold_ttf, BinaryData::InterSemiBold_ttfSize); return t; }

juce::Font headline (float px) { return juce::Font (juce::FontOptions (antonFace()).withHeight (px)); }
juce::Font body (float px)     { return juce::Font (juce::FontOptions (interFace()).withHeight (px)); }
juce::Font bodyBold (float px) { return juce::Font (juce::FontOptions (interBoldFace()).withHeight (px)); }

void fillPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float radius)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

static juce::AttributedString attributed (const juce::String& s, const juce::Font& f, juce::Colour c, float lineSpacing)
{
    juce::AttributedString a;
    a.setJustification (juce::Justification::topLeft);
    a.setWordWrap (juce::AttributedString::byWord);
    a.setLineSpacing (f.getHeight() * (lineSpacing - 1.0f));
    a.append (s, f, c);
    return a;
}

void drawWrapped (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, const juce::Font& f, juce::Colour c, float lineSpacing)
{
    juce::TextLayout layout;
    layout.createLayout (attributed (s, f, c, lineSpacing), r.getWidth());
    layout.draw (g, r);
}

float wrappedHeight (const juce::String& s, float width, const juce::Font& f, float lineSpacing)
{
    juce::TextLayout layout;
    layout.createLayout (attributed (s, f, juce::Colours::white, lineSpacing), width);
    return layout.getHeight();
}

void drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour, float thickness)
{
    // every icon is drawn on a 24 x 24 grid and scaled into the square it is given
    const float side = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = juce::Rectangle<float> (side, side).withCentre (area.getCentre());
    const auto t = juce::AffineTransform::scale (side / 24.0f).translated (box.getX(), box.getY());
    juce::Path p, solid;

    switch (icon)
    {
        case Icon::create:    p.startNewSubPath (11, 3); p.lineTo (12.8f, 9.2f); p.lineTo (19, 11); p.lineTo (12.8f, 12.8f); p.lineTo (11, 19); p.lineTo (9.2f, 12.8f);
                              p.lineTo (3, 11); p.lineTo (9.2f, 9.2f); p.closeSubPath();
                              p.startNewSubPath (18.5f, 15.5f); p.lineTo (18.5f, 20.5f); p.startNewSubPath (16, 18); p.lineTo (21, 18); break;
        case Icon::library:   p.startNewSubPath (9, 18); p.lineTo (9, 5); p.lineTo (19, 3); p.lineTo (19, 16);
                              p.addEllipse (4, 15, 5, 5); p.addEllipse (14, 13, 5, 5); break;
        case Icon::settings:  p.addEllipse (9, 9, 6, 6);
                              for (int i = 0; i < 8; ++i)
                              {
                                  const float a = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
                                  p.startNewSubPath (12 + std::cos (a) * 7.2f, 12 + std::sin (a) * 7.2f);
                                  p.lineTo (12 + std::cos (a) * 9.6f, 12 + std::sin (a) * 9.6f);
                              }
                              break;
        case Icon::help:      p.addEllipse (3, 3, 18, 18);
                              p.startNewSubPath (9.2f, 9.5f); p.cubicTo (9.2f, 6.2f, 14.8f, 6.2f, 14.8f, 9.6f); p.cubicTo (14.8f, 11.8f, 12, 11.8f, 12, 14);
                              solid.addEllipse (11.1f, 16.2f, 1.8f, 1.8f); break;
        case Icon::play:      solid.addTriangle (8, 5, 8, 19, 19.5f, 12); break;
        case Icon::pause:     solid.addRoundedRectangle (7, 5, 3.6f, 14, 1); solid.addRoundedRectangle (13.4f, 5, 3.6f, 14, 1); break;
        case Icon::heart:
        case Icon::heartFilled:
        {
            juce::Path h;
            h.startNewSubPath (12, 20);
            h.cubicTo (4, 14.5f, 2.5f, 10.5f, 4.2f, 7.4f); h.cubicTo (6, 4.2f, 10.2f, 4.4f, 12, 7.6f);
            h.cubicTo (13.8f, 4.4f, 18, 4.2f, 19.8f, 7.4f); h.cubicTo (21.5f, 10.5f, 20, 14.5f, 12, 20);
            h.closeSubPath();
            if (icon == Icon::heartFilled) solid = h; else p = h;
            break;
        }
        case Icon::dots:      solid.addEllipse (4, 10.3f, 3.4f, 3.4f); solid.addEllipse (10.3f, 10.3f, 3.4f, 3.4f); solid.addEllipse (16.6f, 10.3f, 3.4f, 3.4f); break;
        case Icon::grip:      for (int y = 0; y < 3; ++y) { solid.addEllipse (8, 5.5f + 5.5f * (float) y, 2.6f, 2.6f); solid.addEllipse (13.4f, 5.5f + 5.5f * (float) y, 2.6f, 2.6f); } break;
        case Icon::dice:      p.addRoundedRectangle (4, 4, 16, 16, 3.5f);
                              solid.addEllipse (7.4f, 7.4f, 2.4f, 2.4f); solid.addEllipse (14.2f, 14.2f, 2.4f, 2.4f); solid.addEllipse (10.8f, 10.8f, 2.4f, 2.4f); break;
        case Icon::record:    solid.addEllipse (6, 6, 12, 12); break;
        case Icon::stop:      solid.addRoundedRectangle (7, 7, 10, 10, 1.5f); break;
        case Icon::folder:    p.startNewSubPath (3, 7); p.lineTo (3, 19); p.lineTo (21, 19); p.lineTo (21, 8); p.lineTo (12, 8); p.lineTo (10, 5); p.lineTo (3, 5); p.closeSubPath(); break;
        case Icon::plus:      p.startNewSubPath (12, 5); p.lineTo (12, 19); p.startNewSubPath (5, 12); p.lineTo (19, 12); break;
        case Icon::trash:     p.startNewSubPath (4, 7); p.lineTo (20, 7); p.startNewSubPath (9, 7); p.lineTo (9, 4); p.lineTo (15, 4); p.lineTo (15, 7);
                              p.startNewSubPath (6, 7); p.lineTo (7, 20); p.lineTo (17, 20); p.lineTo (18, 7); break;
        case Icon::check:     p.startNewSubPath (5, 12.5f); p.lineTo (10, 17); p.lineTo (19, 7); break;
        case Icon::edit:      p.startNewSubPath (4, 20); p.lineTo (5, 15.5f); p.lineTo (16, 4.5f); p.lineTo (19.5f, 8); p.lineTo (8.5f, 19); p.closeSubPath(); break;
        case Icon::close:     p.startNewSubPath (6, 6); p.lineTo (18, 18); p.startNewSubPath (18, 6); p.lineTo (6, 18); break;
        case Icon::wave:      { const float hs[] = { 4, 9, 14, 7, 11, 5 }; for (int i = 0; i < 6; ++i) { const float x = 4.5f + 3.0f * (float) i; p.startNewSubPath (x, 12 - hs[i] * 0.5f); p.lineTo (x, 12 + hs[i] * 0.5f); } break; }
        case Icon::refresh:   p.addCentredArc (12, 12, 7.5f, 7.5f, 0.0f, 0.9f, 5.6f, true);
                              p.startNewSubPath (17.2f, 3.4f); p.lineTo (17.6f, 7.6f); p.lineTo (13.4f, 7.4f); break;
        case Icon::search:    p.addEllipse (4, 4, 11, 11); p.startNewSubPath (13.6f, 13.6f); p.lineTo (20, 20); break;
        case Icon::speaker:   p.startNewSubPath (4, 9.5f); p.lineTo (8, 9.5f); p.lineTo (12.5f, 5.5f); p.lineTo (12.5f, 18.5f); p.lineTo (8, 14.5f); p.lineTo (4, 14.5f); p.closeSubPath();
                              p.addCentredArc (12.5f, 12, 4.2f, 4.2f, 0.0f, 0.75f, 2.4f, true); p.addCentredArc (12.5f, 12, 7.6f, 7.6f, 0.0f, 0.75f, 2.4f, true); break;
        case Icon::lyrics:    p.startNewSubPath (5, 7); p.lineTo (19, 7); p.startNewSubPath (5, 12); p.lineTo (16, 12); p.startNewSubPath (5, 17); p.lineTo (13, 17); break;
        case Icon::again:     p.addCentredArc (12, 12, 7.5f, 7.5f, 0.0f, -2.2f, 2.6f, true);
                              p.startNewSubPath (3.2f, 6.2f); p.lineTo (5.4f, 10.2f); p.lineTo (9.4f, 8.2f); break;
        case Icon::power:     p.addCentredArc (12, 12.8f, 7.2f, 7.2f, 0.0f, 0.65f, 5.63f, true); p.startNewSubPath (12, 3.5f); p.lineTo (12, 11.5f); break;
    }

    g.setColour (colour);
    if (! p.isEmpty())     g.strokePath (p, juce::PathStrokeType (thickness * 24.0f / side, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
    if (! solid.isEmpty()) g.fillPath (solid, t);
}

Look::Look()
{
    setColour (juce::ResizableWindow::backgroundColourId, black);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, control);
    setColour (juce::TextEditor::highlightColourId, yellow.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::CaretComponent::caretColourId, yellow);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, yellow);
    setColour (juce::PopupMenu::highlightedTextColourId, onYellow);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, lineHi);
    setColour (juce::AlertWindow::backgroundColourId, raised);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, lineHi);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, onYellow);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, lineHi);
}

void Look::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool danger = (bool) b.getProperties()["danger"];
    const bool primary = (bool) b.getProperties()["primary"] || b.getToggleState();
    const bool quiet = (bool) b.getProperties()["quiet"];
    const bool isSmall = (bool) b.getProperties()["small"];
    const float alpha = b.isEnabled() ? 1.0f : 0.4f;
    const float radius = isSmall ? r.getHeight() * 0.5f : ((bool) b.getProperties()["big"] ? 10.0f : 7.0f);

    if (danger)
    {
        g.setColour ((down ? bad.darker (0.25f) : over ? bad.brighter (0.12f) : bad).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, radius);
    }
    else if (primary)
    {
        g.setColour ((down ? yellow.darker (0.25f) : over ? yellow.brighter (0.15f) : yellow).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, radius);
    }
    else
    {
        g.setColour ((down ? control.brighter (0.12f) : over ? control.brighter (0.06f) : (quiet && ! isSmall ? juce::Colours::transparentBlack : control)).withMultipliedAlpha (alpha));
        g.fillRoundedRectangle (r, radius);
        if (! quiet || isSmall)
        {
            g.setColour ((over ? lineHi : line).withMultipliedAlpha (alpha));
            g.drawRoundedRectangle (r, radius, 1.0f);
        }
    }
}

void Look::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const bool danger = (bool) b.getProperties()["danger"];
    const bool primary = (bool) b.getProperties()["primary"] || b.getToggleState();
    const bool quiet = (bool) b.getProperties()["quiet"];
    const bool isSmall = (bool) b.getProperties()["small"];
    const bool big = (bool) b.getProperties()["big"];
    g.setFont (big ? headline (19.0f) : isSmall ? bodyBold (11.5f) : bodyBold (13.0f));
    g.setColour ((danger ? juce::Colours::white : primary ? onYellow : (quiet ? textDim : text)).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (8, 0), juce::Justification::centred, 1);
}

void Look::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (control);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 7.0f);
}

void Look::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? yellow : line);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 7.0f, 1.0f);
}

void Look::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    g.setColour (control);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 7.0f);
    g.setColour (box.hasKeyboardFocus (true) ? yellow : line);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 7.0f, 1.0f);

    juce::Path arrow;
    const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f); arrow.lineTo (cx, cy + 2.5f); arrow.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void Look::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (body (13.0f));
}

void Look::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto box = juce::Rectangle<float> (2.0f, (float) b.getHeight() * 0.5f - 9.0f, 18.0f, 18.0f);
    g.setColour (b.getToggleState() ? yellow : control);
    g.fillRoundedRectangle (box, 5.0f);
    g.setColour (b.getToggleState() ? yellow : (over ? lineHi : line));
    g.drawRoundedRectangle (box, 5.0f, 1.0f);
    if (b.getToggleState()) drawIcon (g, Icon::check, box.reduced (2.5f), onYellow, 2.2f);

    g.setColour (text.withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.45f));
    g.setFont (body (13.0f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (28), juce::Justification::centredLeft, 1);
}

void Look::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down)
{
    auto thumb = vertical ? juce::Rectangle<float> ((float) x + 2.0f, (float) thumbStart, (float) w - 4.0f, (float) thumbSize)
                          : juce::Rectangle<float> ((float) thumbStart, (float) y + 2.0f, (float) thumbSize, (float) h - 4.0f);
    g.setColour (over || down ? textMute : lineHi);
    g.fillRoundedRectangle (thumb, 3.0f);
}

void Look::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (raised);
    g.setColour (lineHi);
    g.drawRect (0, 0, w, h, 1);
}

void Look::drawProgressBar (juce::Graphics& g, juce::ProgressBar&, int w, int h, double progress, const juce::String&)
{
    g.setColour (control);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, (float) h * 0.5f);
    g.setColour (yellow);
    g.fillRoundedRectangle (0, 0, juce::jmax ((float) h, (float) w * (float) juce::jlimit (0.0, 1.0, progress)), (float) h, (float) h * 0.5f);
}

void styleEditor (juce::TextEditor& e, const juce::String& placeholder, bool multiLine)
{
    e.setMultiLine (multiLine, true);
    e.setReturnKeyStartsNewLine (multiLine);
    e.setFont (body (13.5f));
    e.setTextToShowWhenEmpty (placeholder, textMute);
    e.setIndents (10, multiLine ? 8 : 0);
    e.setJustification (multiLine ? juce::Justification::topLeft : juce::Justification::centredLeft);
    e.setSelectAllWhenFocused (false);
}

void styleLabel (juce::Label& l, const juce::String& caption, bool dim)
{
    l.setText (caption, juce::dontSendNotification);
    l.setFont (dim ? bodyBold (11.0f) : body (13.0f));
    l.setColour (juce::Label::textColourId, dim ? textDim : theme::text);
    l.setInterceptsMouseClicks (false, false);
    l.setBorderSize ({ 0, 1, 2, 0 });
}

static juce::uint32 hashOf (const juce::String& seedText)
{
    juce::uint32 h = 2166136261u;
    for (auto c : seedText) { h ^= (juce::uint32) c; h *= 16777619u; }
    return h;
}

void drawCover (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& seed, float radius)
{
    // two colours picked by the song's own id, laid corner to corner, with a few soft rings over the top
    const auto h = hashOf (seed);
    const float hueA = (float) (h % 360u) / 360.0f;
    const float hueB = std::fmod (hueA + 0.08f + (float) ((h >> 9) % 90u) / 360.0f, 1.0f);
    const auto a = juce::Colour::fromHSV (hueA, 0.72f, 0.80f, 1.0f);
    const auto b = juce::Colour::fromHSV (hueB, 0.85f, 0.26f, 1.0f);
    const bool flip = ((h >> 5) & 1u) != 0;

    juce::Path clip;
    clip.addRoundedRectangle (r, radius);
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (clip);

    g.setGradientFill (juce::ColourGradient (a, flip ? r.getTopRight() : r.getTopLeft(), b, flip ? r.getBottomLeft() : r.getBottomRight(), false));
    g.fillRect (r);

    const float cx = r.getX() + r.getWidth() * (0.25f + 0.5f * (float) ((h >> 12) % 100u) / 100.0f);
    const float cy = r.getY() + r.getHeight() * (0.25f + 0.5f * (float) ((h >> 19) % 100u) / 100.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (int i = 1; i <= 4; ++i)
    {
        const float d = r.getWidth() * 0.34f * (float) i;
        g.drawEllipse (cx - d * 0.5f, cy - d * 0.5f, d, d, juce::jmax (1.0f, r.getWidth() * 0.022f));
    }
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (r.withTop (r.getBottom() - r.getHeight() * 0.3f));
}

juce::Image coverImage (const juce::String& seed, int size)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics g (image);
    drawCover (g, image.getBounds().toFloat(), seed, (float) size * 0.12f);
    return image;
}

void drawChip (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& label, bool lit)
{
    g.setColour (lit ? yellow : control);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    if (! lit)
    {
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
    }
    g.setColour (lit ? onYellow : textDim);
    g.setFont (bodyBold (10.5f));
    g.drawFittedText (label, r.toNearestInt(), juce::Justification::centred, 1);
}
}
