#include "StudioLookAndFeel.h"

#include <cmath>

namespace studio::ui
{
StudioLookAndFeel::StudioLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId, obsidianScreen);
    setColour (juce::PopupMenu::textColourId, textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, crimsonGlow.withAlpha (0.32f));
    setColour (juce::PopupMenu::highlightedTextColourId, textPrimary);
    setColour (juce::TooltipWindow::backgroundColourId, obsidianScreen);
    setColour (juce::TooltipWindow::textColourId, textPrimary);
}

void StudioLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height))
                            .reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;

    if (radius < 10.0f)
        return;

    const auto centre = bounds.getCentre();
    const float prop  = juce::jlimit (0.0f, 1.0f, sliderPosProportional);
    const float angle = rotaryStartAngle + prop * (rotaryEndAngle - rotaryStartAngle);

    // 1. Minimalist perimeter tick hairlines etched into the glass.
    const float scaleR = radius - 1.5f;
    for (int i = 0; i <= 24; ++i)
    {
        const float tProp = static_cast<float> (i) / 24.0f;
        const float a     = rotaryStartAngle + tProp * (rotaryEndAngle - rotaryStartAngle);
        const bool isMajor = (i % 4 == 0);
        const auto p1 = centre.getPointOnCircumference (scaleR - (isMajor ? 4.0f : 2.2f), a);
        const auto p2 = centre.getPointOnCircumference (scaleR, a);

        const bool lit = (tProp <= prop);
        g.setColour (lit ? crimsonAccent.withAlpha (isMajor ? 0.85f : 0.55f)
                         : textMuted.withAlpha (isMajor ? 0.55f : 0.28f));
        g.drawLine ({ p1, p2 }, isMajor ? 1.3f : 0.8f);
    }

    // 2. Recessed well + crimson LED under-glow beneath the machined skirt.
    const float wellR = radius * 0.86f;
    const juce::Rectangle<float> wellBounds (centre.x - wellR, centre.y - wellR, wellR * 2.0f, wellR * 2.0f);
    g.setColour (obsidianWell);
    g.fillEllipse (wellBounds);

    {
        juce::ColourGradient underGlow (crimsonAccent.withAlpha (0.18f + 0.28f * prop), centre.x, centre.y,
                                        juce::Colours::transparentBlack, centre.x + wellR, centre.y, true);
        g.setGradientFill (underGlow);
        g.fillEllipse (wellBounds.expanded (2.0f));

        juce::Path activeUnderArc;
        activeUnderArc.addCentredArc (centre.x, centre.y, wellR - 1.2f, wellR - 1.2f,
                                      0.0f, rotaryStartAngle, angle, true);
        g.setColour (crimsonGlow.withAlpha (0.45f));
        g.strokePath (activeUnderArc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
        g.setColour (crimsonAccent.withAlpha (0.85f));
        g.strokePath (activeUnderArc, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                            juce::PathStrokeType::rounded));
    }

    // 3. Low-profile recessed height: soft cast shadow to the lower right.
    const float rimR = radius * 0.80f;
    const juce::Rectangle<float> rimBounds (centre.x - rimR, centre.y - rimR, rimR * 2.0f, rimR * 2.0f);
    g.setColour (dropShadowDeep.withAlpha (0.65f));
    g.fillEllipse (rimBounds.translated (1.5f, 2.2f));

    // 4. Heavy-machined metal outer rim with fine vertical knurled ridges.
    juce::ColourGradient rimGrad (gunmetalLight.brighter (0.15f), rimBounds.getTopLeft(),
                                  obsidianScreen, rimBounds.getBottomRight(), false);
    g.setGradientFill (rimGrad);
    g.fillEllipse (rimBounds);

    constexpr int numKnurls = 64;
    for (int i = 0; i < numKnurls; ++i)
    {
        const float kAngle = angle + static_cast<float> (i)
                                        * (juce::MathConstants<float>::twoPi
                                           / static_cast<float> (numKnurls));
        const auto k1 = centre.getPointOnCircumference (rimR * 0.86f, kAngle);
        const auto k2 = centre.getPointOnCircumference (rimR * 0.99f, kAngle);

        // Upper-left soft-box lighting modulates knurl specular intensity.
        const float lightBias = 0.5f - 0.5f * std::sin (kAngle + juce::MathConstants<float>::pi * 0.25f);
        g.setColour (i % 2 == 0
                         ? juce::Colours::white.withAlpha (0.06f + 0.16f * lightBias)
                         : juce::Colours::black.withAlpha (0.65f));
        g.drawLine ({ k1, k2 }, 1.0f);
    }

    // 5. Flat, low-profile machined metal top cap.
    const float capR = rimR * 0.84f;
    const juce::Rectangle<float> capBounds (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);
    juce::ColourGradient capGrad (gunmetalLight.brighter (0.08f), capBounds.getTopLeft(),
                                  gunmetalDeep, capBounds.getBottomRight(), false);
    capGrad.addColour (0.5, gunmetalBase);
    g.setGradientFill (capGrad);
    g.fillEllipse (capBounds);

    // Fine horizontal brushed grain on the flat metal cap.
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path capClip;
        capClip.addEllipse (capBounds);
        g.reduceClipRegion (capClip);

        for (int gy = static_cast<int> (capBounds.getY());
             gy < static_cast<int> (capBounds.getBottom()); gy += 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.022f));
            g.fillRect (capBounds.getX(), static_cast<float> (gy), capBounds.getWidth(), 1.0f);
        }
    }

    // Chamfered specular ring on the flat cap edge.
    g.setColour (glassRimHighlight.withAlpha (0.24f));
    g.drawEllipse (capBounds, 0.9f);
    g.setColour (obsidianScreen.withAlpha (0.7f));
    g.drawEllipse (capBounds.expanded (0.8f), 0.8f);

    // 6. Tiny surgical tick mark on the cap for position reference.
    const auto tickInner = centre.getPointOnCircumference (capR * 0.64f, angle);
    const auto tickOuter = centre.getPointOnCircumference (capR * 0.88f, angle);
    g.setColour (crimsonGlow.withAlpha (0.45f));
    g.drawLine ({ tickInner, tickOuter }, 3.0f);
    g.setColour (textPrimary);
    g.drawLine ({ tickInner, tickOuter }, 1.5f);
}

void StudioLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float,
                                          const juce::Slider::SliderStyle style, juce::Slider&)
{
    if (style != juce::Slider::LinearVertical && style != juce::Slider::LinearBarVertical)
        return;

    const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                static_cast<float> (width), static_cast<float> (height));
    const float centerX = bounds.getCentreX();
    const float trackTop = bounds.getY() + 8.0f;
    const float trackBot = bounds.getBottom() - 8.0f;
    const float trackH   = juce::jmax (10.0f, trackBot - trackTop);
    const float clampedY = juce::jlimit (trackTop, trackBot, sliderPos);

    // 1. Minimalist horizontal tick marks alongside both sides of the track.
    constexpr int numTicks = 21;
    for (int i = 0; i < numTicks; ++i)
    {
        const float prop  = static_cast<float> (i) / static_cast<float> (numTicks - 1);
        const float tickY = trackTop + prop * trackH;
        const bool isMajor = (i % 5 == 0);
        const float tickLen = isMajor ? 7.0f : 4.0f;
        constexpr float gap = 6.5f;

        g.setColour (isMajor ? textSecondary.withAlpha (0.55f) : textMuted.withAlpha (0.35f));
        g.fillRect (centerX - gap - tickLen, tickY - 0.5f, tickLen, 1.0f);
        g.fillRect (centerX + gap,           tickY - 0.5f, tickLen, 1.0f);
    }

    // 2. Subtly recessed ultra-thin linear track in the glass surface.
    const juce::Rectangle<float> trackRect (centerX - 2.0f, trackTop, 4.0f, trackH);
    g.setColour (obsidianWell);
    g.fillRoundedRectangle (trackRect, 2.0f);
    g.setColour (glassRimHighlight.withAlpha (0.14f));
    g.drawRoundedRectangle (trackRect.expanded (0.5f), 2.2f, 0.7f);

    // 3. Muted crimson active level fill inside the recessed track.
    const float fillH = trackBot - clampedY;
    if (fillH > 0.5f)
    {
        const juce::Rectangle<float> activeRect (centerX - 1.5f, clampedY, 3.0f, fillH);
        g.setColour (crimsonGlow.withAlpha (0.38f));
        g.fillRoundedRectangle (activeRect.expanded (1.5f, 0.5f), 2.0f);
        g.setColour (crimsonAccent);
        g.fillRoundedRectangle (activeRect, 1.5f);
    }

    // 4. Low-profile flush machined fader cap.
    const juce::Rectangle<float> thumb (centerX - 12.0f, clampedY - 4.5f, 24.0f, 9.0f);
    g.setColour (dropShadowDeep.withAlpha (0.55f));
    g.fillRoundedRectangle (thumb.translated (0.8f, 1.4f), 2.0f);

    juce::ColourGradient thumbGrad (gunmetalLight.brighter (0.18f), thumb.getTopLeft(),
                                    gunmetalDeep, thumb.getBottomRight(), false);
    g.setGradientFill (thumbGrad);
    g.fillRoundedRectangle (thumb, 2.0f);

    g.setColour (glassRimHighlight.withAlpha (0.28f));
    g.drawRoundedRectangle (thumb, 2.0f, 0.8f);

    // Crisp minimalist crimson reference line across the thumb centre.
    g.setColour (crimsonBright);
    g.fillRect (thumb.getX() + 3.0f, clampedY - 0.5f, thumb.getWidth() - 6.0f, 1.2f);
}

void StudioLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
    constexpr float corner = 3.5f;
    const bool active = button.getToggleState();

    // 1. Subtle recessed seat in the glass surface.
    g.setColour (obsidianWell);
    g.fillRoundedRectangle (bounds.expanded (1.0f), corner + 0.8f);

    // 2. Low-profile flush button surface.
    juce::ColourGradient capGrad (active ? gunmetalLight.interpolatedWith (crimsonGlow, 0.14f)
                                         : gunmetalBase,
                                  bounds.getTopLeft(),
                                  gunmetalDeep,
                                  bounds.getBottomRight(),
                                  false);
    g.setGradientFill (capGrad);
    g.fillRoundedRectangle (bounds, corner);

    // 3. Thin red glowing border when active; matte dark rim when inactive.
    if (active)
    {
        g.setColour (crimsonGlow.withAlpha (0.36f));
        g.drawRoundedRectangle (bounds.expanded (1.0f), corner + 0.5f, 2.8f);

        g.setColour (crimsonAccent);
        g.drawRoundedRectangle (bounds, corner, 1.2f);

        g.setColour (crimsonBright.withAlpha (0.75f));
        g.drawRoundedRectangle (bounds.reduced (0.4f), corner, 0.6f);
    }
    else
    {
        g.setColour (shouldDrawButtonAsHighlighted ? chamferLight : gunmetalLight.withAlpha (0.65f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    // 4. Crisp sans-serif typography.
    g.setColour (active ? textPrimary : textSecondary);
    g.setFont (font (10.5f, active ? Weight::semibold : Weight::regular, 0.10f));
    g.drawText (button.getButtonText().toUpperCase(), bounds, juce::Justification::centred, false);
}

void StudioLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool isHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (isHighlighted ? gunmetalLight : gunmetalBase);
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (crimsonAccent.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

void StudioLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                        bool, bool)
{
    g.setColour (button.findColour (juce::TextButton::textColourOffId));
    g.setFont (font (10.0f, Weight::semibold, 0.10f));
    g.drawText (button.getButtonText().toUpperCase(), button.getLocalBounds(),
                juce::Justification::centred, false);
}

juce::Font StudioLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return font (10.0f, Weight::semibold, 0.10f);
}

void StudioLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                                static_cast<float> (width), static_cast<float> (height));
    g.setColour (obsidianScreen);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (crimsonAccent.withAlpha (0.45f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (glassRimHighlight.withAlpha (0.12f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 4.0f, 0.7f);
}

void StudioLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                           bool isSeparator, bool isActive, bool isHighlighted,
                                           bool isTicked, bool hasSubMenu, const juce::String& text,
                                           const juce::String& shortcutKeyText,
                                           const juce::Drawable*, const juce::Colour*)
{
    auto bounds = area.toFloat().reduced (4.0f, 1.0f);

    if (isSeparator)
    {
        g.setColour (obsidianGrid);
        g.fillRect (bounds.getX(), bounds.getCentreY(), bounds.getWidth(), 1.0f);
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (crimsonGlow.withAlpha (0.30f));
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (crimsonAccent.withAlpha (0.55f));
        g.drawRoundedRectangle (bounds, 3.0f, 0.8f);
    }

    auto textArea = bounds.reduced (8.0f, 0.0f);

    if (isTicked)
    {
        g.setColour (crimsonAccent);
        g.fillEllipse (textArea.getX(), textArea.getCentreY() - 2.0f, 4.0f, 4.0f);
    }

    textArea.removeFromLeft (12.0f);

    g.setColour (isActive ? textPrimary : textMuted);
    g.setFont (font (10.0f, isHighlighted ? Weight::semibold : Weight::regular, 0.06f));
    g.drawFittedText (text, textArea.toNearestInt(), juce::Justification::centredLeft, 1);

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (textMuted);
        g.setFont (font (8.0f, Weight::regular, 0.04f));
        g.drawText (shortcutKeyText, textArea.toNearestInt(), juce::Justification::centredRight, false);
    }

    if (hasSubMenu)
    {
        const auto centre = juce::Point<float> (bounds.getRight() - 10.0f, bounds.getCentreY());
        juce::Path arrow;
        arrow.startNewSubPath (centre.x - 2.0f, centre.y - 3.0f);
        arrow.lineTo (centre.x + 2.0f, centre.y);
        arrow.lineTo (centre.x - 2.0f, centre.y + 3.0f);
        g.setColour (textSecondary);
        g.strokePath (arrow, juce::PathStrokeType (1.2f));
    }
}

juce::Font StudioLookAndFeel::getPopupMenuFont()
{
    return font (10.0f, Weight::regular, 0.06f);
}

void StudioLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text,
                                     int width, int height)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f,
                                                static_cast<float> (width), static_cast<float> (height));

    g.setColour (obsidianScreen.withAlpha (0.97f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (crimsonAccent.withAlpha (0.5f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 0.9f);

    g.setColour (textPrimary);
    g.setFont (font (9.5f, Weight::regular, 0.04f));
    g.drawFittedText (text, bounds.reduced (6.0f, 3.0f).toNearestInt(),
                      juce::Justification::centredLeft, 2);
}

void StudioLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                                   int standardMenuItemHeight, int& idealWidth,
                                                   int& idealHeight)
{
    if (isSeparator)
    {
        idealWidth = 80;
        idealHeight = 8;
        return;
    }

    idealHeight = juce::jmax (24, standardMenuItemHeight);
    idealWidth = juce::jmax (150, juce::GlyphArrangement::getStringWidthInt (
                                       font (10.0f, Weight::regular, 0.06f), text) + 56);
}
} // namespace studio::ui
