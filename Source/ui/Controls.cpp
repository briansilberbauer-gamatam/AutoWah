#include "Controls.h"

#include <algorithm>
#include <cmath>

namespace studio::ui
{
namespace
{
float normaliseDb (float linear, float floorDb = -48.0f)
{
    if (! std::isfinite (linear) || linear <= 0.0f)
        return 0.0f;

    const float db = juce::Decibels::gainToDecibels (linear, floorDb);
    return juce::jlimit (0.0f, 1.0f, (db - floorDb) / -floorDb);
}

void drawChevron (juce::Graphics& g, juce::Rectangle<float> area, bool pointLeft, bool highlighted)
{
    const auto centre = area.getCentre();
    const float s = 4.2f;
    const float direction = pointLeft ? -1.0f : 1.0f;

    juce::Path path;
    path.startNewSubPath (centre.x - direction * s * 0.5f, centre.y - s);
    path.lineTo (centre.x + direction * s * 0.6f, centre.y);
    path.lineTo (centre.x - direction * s * 0.5f, centre.y + s);

    g.setColour (highlighted ? crimsonBright : textSecondary);
    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}
} // namespace

//======================================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& state,
                      const juce::String& id,
                      juce::String caption,
                      std::function<juce::String (float)> formatter)
    : apvts (state),
      parameterID (id),
      captionText (std::move (caption)),
      formatValue (std::move (formatter))
{
    setOpaque (false);
    setInterceptsMouseClicks (true, true);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setMouseDragSensitivity (180);
    slider.setScrollWheelEnabled (true);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, parameterID, slider);

    if (auto* parameter = apvts.getParameter (parameterID))
    {
        const float defaultValue = parameter->getNormalisableRange().convertFrom0to1 (
            parameter->getDefaultValue());
        slider.setDoubleClickReturnValue (true, defaultValue);
        slider.setTooltip (parameter->getName (64));
    }

    slider.onValueChange = [this] { refreshValueText(); };

    refreshValueText();
    startTimerHz (24);
}

ParamKnob::~ParamKnob()
{
    stopTimer();
}

void ParamKnob::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const auto captionArea = bounds.removeFromTop (15.0f);
    g.setColour (hover ? textPrimary : textSecondary);
    g.setFont (font (7.9f, Weight::semibold, 0.14f));
    g.drawFittedText (captionText, captionArea.toNearestInt(),
                      juce::Justification::centred, 1);

    const auto valueArea = bounds.removeFromBottom (15.0f);
    g.setColour (hover ? textPrimary : textSecondary.brighter (0.25f));
    g.setFont (font (8.3f, Weight::semibold, 0.05f));
    g.drawFittedText (valueText, valueArea.toNearestInt(),
                      juce::Justification::centred, 1);
}

void ParamKnob::resized()
{
    auto area = getLocalBounds().withTrimmedTop (15).withTrimmedBottom (15);
    const int diameter = juce::jmin (area.getWidth(), area.getHeight());
    slider.setBounds (area.withSizeKeepingCentre (diameter, diameter));
}

void ParamKnob::mouseEnter (const juce::MouseEvent&)
{
    hover = true;
    repaint();
}

void ParamKnob::mouseExit (const juce::MouseEvent&)
{
    hover = false;
    repaint();
}

void ParamKnob::timerCallback()
{
    refreshValueText();
}

void ParamKnob::refreshValueText()
{
    if (formatValue == nullptr)
        return;

    const auto text = formatValue (static_cast<float> (slider.getValue()));

    if (text != valueText)
    {
        valueText = text;
        repaint();
    }
}

//======================================================================================
GlassSlider::GlassSlider (juce::AudioProcessorValueTreeState& state,
                          const juce::String& id,
                          juce::String caption,
                          std::function<juce::String (float)> formatter)
    : apvts (state),
      parameterID (id),
      captionText (std::move (caption)),
      formatValue (std::move (formatter))
{
    setOpaque (false);
    setInterceptsMouseClicks (true, true);

    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (200);
    slider.setScrollWheelEnabled (true);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, parameterID, slider);

    if (auto* parameter = apvts.getParameter (parameterID))
    {
        const float defaultValue = parameter->getNormalisableRange().convertFrom0to1 (
            parameter->getDefaultValue());
        slider.setDoubleClickReturnValue (true, defaultValue);
        slider.setTooltip (parameter->getName (64));
    }

    slider.onValueChange = [this] { refreshValueText(); };

    refreshValueText();
    startTimerHz (24);
}

GlassSlider::~GlassSlider()
{
    stopTimer();
}

void GlassSlider::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const auto captionArea = bounds.removeFromTop (18.0f);
    g.setColour (hover ? textPrimary : textSecondary);
    g.setFont (font (7.6f, Weight::semibold, 0.14f));
    g.drawFittedText (captionText, captionArea.toNearestInt(),
                      juce::Justification::centred, 1);

    const auto valueArea = bounds.removeFromBottom (18.0f);
    g.setColour (hover ? textPrimary : textSecondary.brighter (0.25f));
    g.setFont (font (8.3f, Weight::semibold, 0.05f));
    g.drawFittedText (valueText, valueArea.toNearestInt(),
                      juce::Justification::centred, 1);

    // Recessed slim glass channel behind the fader.
    const auto channel = bounds.withSizeKeepingCentre (30.0f, juce::jmax (20.0f, bounds.getHeight() - 8.0f));
    drawGlassWell (g, channel, 7.0f);

    // Tiny end-stop index ticks.
    g.setColour (textMuted.withAlpha (0.65f));
    g.fillRect (channel.getCentreX() - 13.0f, channel.getY() + 5.0f, 26.0f, 1.0f);
    g.fillRect (channel.getCentreX() - 13.0f, channel.getBottom() - 6.0f, 26.0f, 1.0f);
}

void GlassSlider::resized()
{
    auto area = getLocalBounds().withTrimmedTop (18).withTrimmedBottom (18);
    slider.setBounds (area.reduced (6, 4));
}

void GlassSlider::mouseEnter (const juce::MouseEvent&)
{
    hover = true;
    repaint();
}

void GlassSlider::mouseExit (const juce::MouseEvent&)
{
    hover = false;
    repaint();
}

void GlassSlider::timerCallback()
{
    refreshValueText();
}

void GlassSlider::refreshValueText()
{
    if (formatValue == nullptr)
        return;

    const auto text = formatValue (static_cast<float> (slider.getValue()));

    if (text != valueText)
    {
        valueText = text;
        repaint();
    }
}

//======================================================================================
TactileButton::TactileButton (juce::AudioProcessorValueTreeState& state,
                              const juce::String& id,
                              const juce::String& labelText)
{
    setButtonText (labelText);
    setClickingTogglesState (true);
    setTooltip (labelText);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, id, *this);
}

//======================================================================================
ChoiceButton::ChoiceButton (juce::AudioProcessorValueTreeState& state,
                            const juce::String& id,
                            int index,
                            const juce::String& labelText)
    : apvts (state),
      parameterID (id),
      choiceIndex (index)
{
    setButtonText (labelText);
    setClickingTogglesState (false);

    parameter = apvts.getParameter (parameterID);
    rawValue = apvts.getRawParameterValue (parameterID);

    if (parameter != nullptr)
        setTooltip (parameter->getName (64));

    lastState = rawValue != nullptr
                && juce::roundToInt (rawValue->load (std::memory_order_relaxed)) == choiceIndex;
    setToggleState (lastState, juce::dontSendNotification);

    startTimerHz (12);
}

void ChoiceButton::clicked()
{
    if (parameter == nullptr)
        return;

    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (choiceIndex)));
    parameter->endChangeGesture();
}

void ChoiceButton::timerCallback()
{
    if (rawValue == nullptr)
        return;

    const bool state = juce::roundToInt (rawValue->load (std::memory_order_relaxed)) == choiceIndex;

    if (state != lastState)
    {
        lastState = state;
        setToggleState (state, juce::dontSendNotification);
        repaint();
    }
}

//======================================================================================
HeaderPillBar::HeaderPillBar (AutoWahAudioProcessor& p)
    : processor (p)
{
    setOpaque (false);
    setInterceptsMouseClicks (true, true);
    lastPreset = processor.getCurrentPresetIndex();
    lastDirty = processor.isPresetDirty();
    startTimerHz (10);
}

void HeaderPillBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float radius = bounds.getHeight() * 0.5f;

    // Subtle crimson edge refraction around the pill cutout.
    g.setColour (crimsonGlow.withAlpha (0.28f));
    g.drawRoundedRectangle (bounds.expanded (1.2f), radius + 1.2f, 2.0f);

    // Recessed obsidian digital screen.
    g.setColour (obsidianScreen);
    g.fillRoundedRectangle (bounds, radius);

    g.setColour (crimsonAccent.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, radius, 1.0f);
    g.setColour (glassRimHighlight.withAlpha (0.18f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), radius - 1.0f, 0.75f);

    const float navWidth = bounds.getHeight() * 1.15f;
    const auto previousArea = bounds.withWidth (navWidth);
    const auto nextArea = bounds.withLeft (bounds.getRight() - navWidth);

    drawChevron (g, previousArea, true, hoverZone == Zone::previous);
    drawChevron (g, nextArea, false, hoverZone == Zone::next);

    // Divider hairlines.
    g.setColour (obsidianGrid);
    g.fillRect (previousArea.getRight() + 1.0f, bounds.getY() + 7.0f,
                1.0f, bounds.getHeight() - 14.0f);
    g.fillRect (nextArea.getX() - 1.0f, bounds.getY() + 7.0f,
                1.0f, bounds.getHeight() - 14.0f);

    // Preset readout.
    auto textArea = bounds.withTrimmedLeft (navWidth + 10.0f).withTrimmedRight (navWidth + 10.0f);
    const auto statusArea = textArea.removeFromRight (54.0f);

    drawCrimsonDot (g, { textArea.getX() + 2.0f, textArea.getCentreY() }, 2.0f);

    const auto nameArea = textArea.withTrimmedLeft (12.0f);
    const auto presetName = lastPreset >= 0 ? processor.getProgramName (lastPreset)
                                            : juce::String ("INIT");
    const auto readout = juce::String (lastPreset + 1).paddedLeft ('0', 2)
                         + "  //  " + presetName.toUpperCase();

    g.setColour (textPrimary);
    g.setFont (font (10.8f, Weight::semibold, 0.16f));
    g.drawFittedText (readout, nameArea.toNearestInt(), juce::Justification::centred, 1);

    g.setColour (lastDirty ? crimsonBright : textSecondary.withAlpha (0.78f));
    g.setFont (font (6.2f, Weight::semibold, 0.14f));
    g.drawText (lastDirty ? "EDITED" : "FACTORY", statusArea,
                juce::Justification::centredRight, false);
}

void HeaderPillBar::mouseMove (const juce::MouseEvent& event)
{
    const auto newZone = zoneAt (static_cast<float> (event.x));

    if (newZone != hoverZone)
    {
        hoverZone = newZone;
        repaint();
    }
}

void HeaderPillBar::mouseExit (const juce::MouseEvent&)
{
    if (hoverZone != Zone::none)
    {
        hoverZone = Zone::none;
        repaint();
    }
}

void HeaderPillBar::mouseUp (const juce::MouseEvent& event)
{
    switch (zoneAt (static_cast<float> (event.x)))
    {
        case Zone::previous: step (-1); break;
        case Zone::next:     step (1);  break;
        case Zone::readout:  showPresetMenu(); break;
        case Zone::none:     break;
    }
}

HeaderPillBar::Zone HeaderPillBar::zoneAt (float x) const
{
    const float navWidth = static_cast<float> (getHeight()) * 1.15f;

    if (x < navWidth)
        return Zone::previous;

    if (x > static_cast<float> (getWidth()) - navWidth)
        return Zone::next;

    return Zone::readout;
}

void HeaderPillBar::timerCallback()
{
    const int preset = processor.getCurrentPresetIndex();
    const bool dirty = processor.isPresetDirty();

    if (preset != lastPreset || dirty != lastDirty)
    {
        lastPreset = preset;
        lastDirty = dirty;
        repaint();
    }
}

void HeaderPillBar::step (int delta)
{
    const int count = processor.getNumPrograms();

    if (count <= 0)
        return;

    int next = (processor.getCurrentPresetIndex() + delta) % count;

    if (next < 0)
        next += count;

    processor.loadFactoryPreset (next);
    lastPreset = next;
    lastDirty = processor.isPresetDirty();
    repaint();

    if (onPresetChanged)
        onPresetChanged();
}

void HeaderPillBar::showPresetMenu()
{
    juce::PopupMenu menu;

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        menu.addItem (i + 1,
                      juce::String (i + 1).paddedLeft ('0', 2) + "   " + processor.getProgramName (i),
                      true, i == lastPreset);

    juce::Component::SafePointer<HeaderPillBar> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (this)
                            .withMinimumWidth (getWidth()),
                        [safeThis] (int result) mutable
                        {
                            if (safeThis == nullptr || result <= 0)
                                return;

                            safeThis->processor.loadFactoryPreset (result - 1);
                            safeThis->lastPreset = result - 1;
                            safeThis->lastDirty = safeThis->processor.isPresetDirty();
                            safeThis->repaint();

                            if (safeThis->onPresetChanged)
                                safeThis->onPresetChanged();
                        });
}

//======================================================================================
ModulationMeter::ModulationMeter (AutoWahAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void ModulationMeter::timerCallback()
{
    mode = processor.getActiveMode();

    const float target = mode == 0 ? processor.getEnvelopeLevel() : processor.getLfoValue();
    displayValue = juce::jmax (target, displayValue * 0.80f);
    phase = processor.getLfoPhase();

    if (displayValue >= peakValue)
    {
        peakValue = displayValue;
        peakHold = 48;
    }
    else if (--peakHold <= 0)
    {
        peakValue *= 0.955f;

        if (peakValue < 0.002f)
            peakValue = 0.0f;
    }

    repaint();
}

void ModulationMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const auto header = bounds.removeFromTop (12.0f);
    g.setColour (textSecondary);
    g.setFont (font (6.1f, Weight::semibold, 0.16f));
    g.drawText (mode == 0 ? "DETECTOR ACTIVITY" : "LFO MODULATION",
                header.withWidth (header.getWidth() - 46.0f),
                juce::Justification::centredLeft, false);

    g.setColour (mode == 0 ? crimsonAccent : crimsonBright);
    g.setFont (font (6.2f, Weight::semibold, 0.14f));
    g.drawText (mode == 0 ? "ENV" : "LFO", header,
                juce::Justification::centredRight, false);

    auto barArea = bounds.removeFromTop (16.0f).reduced (0.0f, 1.0f);
    drawGlassWell (g, barArea, 2.5f);

    const auto inner = barArea.reduced (2.5f, 2.5f);
    constexpr int segments = 30;
    const float gap = 1.6f;
    const float segmentWidth = (inner.getWidth() - gap * static_cast<float> (segments - 1))
                               / static_cast<float> (segments);
    const float lit = juce::jlimit (0.0f, 1.0f, displayValue) * static_cast<float> (segments);

    for (int i = 0; i < segments; ++i)
    {
        const float x = inner.getX() + static_cast<float> (i) * (segmentWidth + gap);
        const juce::Rectangle<float> segment (x, inner.getY(), segmentWidth, inner.getHeight());

        if (static_cast<float> (i) + 1.0f <= lit)
        {
            g.setColour (i >= segments - 3 ? crimsonBright : crimsonAccent);
            g.fillRoundedRectangle (segment, 1.0f);
        }
        else if (static_cast<float> (i) < lit)
        {
            g.setColour (crimsonAccent.withAlpha (0.55f));
            g.fillRoundedRectangle (segment, 1.0f);
        }
        else
        {
            g.setColour (obsidianGrid.withAlpha (0.85f));
            g.fillRoundedRectangle (segment, 1.0f);
        }
    }

    if (mode == 1)
    {
        // Phase playhead: shows where the free-running LFO is in its cycle.
        const float phaseX = barArea.getX() + phase * barArea.getWidth();
        g.setColour (crimsonBright.withAlpha (0.85f));
        g.fillRect (juce::jlimit (barArea.getX(), barArea.getRight() - 1.0f, phaseX),
                    barArea.getY() - 1.0f, 1.0f, barArea.getHeight() + 2.0f);
    }
    else if (peakValue > 0.001f)
    {
        const float peakX = inner.getX() + peakValue * inner.getWidth();
        g.setColour (textPrimary.withAlpha (0.8f));
        g.fillRect (juce::jlimit (inner.getX(), inner.getRight() - 1.0f, peakX),
                    inner.getY() - 0.5f, 1.0f, inner.getHeight() + 1.0f);
    }

    const auto footer = bounds.removeFromTop (13.0f);
    g.setColour (textMuted);
    g.setFont (font (5.7f, Weight::regular, 0.10f));
    g.drawText (mode == 0 ? "STEREO-LINKED PEAK DETECTOR"
                          : "FREE-RUNNING / RESETS ON MODE ENTRY",
                footer, juce::Justification::centredLeft, false);
}

//======================================================================================
OutputMeter::OutputMeter (AutoWahAudioProcessor& p)
    : processor (p)
{
    startTimerHz (30);
}

void OutputMeter::timerCallback()
{
    const float left = processor.getOutputLevelLeft();
    const float right = processor.getOutputLevelRight();

    levelLeft = juce::jmax (left, levelLeft * 0.80f);
    levelRight = juce::jmax (right, levelRight * 0.80f);

    const auto updatePeak = [this] (float level, float& peak, int& hold)
    {
        if (level >= peak)
        {
            peak = level;
            hold = 42;
        }
        else if (--hold <= 0)
        {
            peak *= 0.94f;

            if (peak < 0.0005f)
                peak = 0.0f;
        }
    };

    updatePeak (levelLeft, peakLeft, peakHoldLeft);
    updatePeak (levelRight, peakRight, peakHoldRight);

    if (juce::jmax (left, right) >= 0.999f)
        clipHold = 55;
    else if (--clipHold <= 0)
        clipHold = 0;

    repaint();
}

void OutputMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    const auto header = bounds.removeFromTop (10.0f).withTrimmedLeft (2.0f);
    g.setColour (textMuted);
    g.setFont (font (5.8f, Weight::semibold, 0.16f));
    g.drawText ("OUTPUT LEVEL", header, juce::Justification::centredLeft, false);

    drawChannel (g, bounds.removeFromTop (13.0f), "L", normaliseDb (levelLeft), normaliseDb (peakLeft));
    bounds.removeFromTop (2.0f);
    drawChannel (g, bounds.removeFromTop (13.0f), "R", normaliseDb (levelRight), normaliseDb (peakRight));

    if (clipHold > 0)
    {
        g.setColour (crimsonBright);
        g.fillEllipse (getWidth() - 7.0f, 4.0f, 4.0f, 4.0f);
    }
}

void OutputMeter::drawChannel (juce::Graphics& g, juce::Rectangle<float> bounds,
                               const juce::String& label, float normalisedLevel,
                               float normalisedPeak) const
{
    auto area = bounds;
    const auto labelArea = area.removeFromLeft (12.0f);

    g.setColour (textSecondary);
    g.setFont (font (7.0f, Weight::semibold, 0.10f));
    g.drawText (label, labelArea, juce::Justification::centredLeft, false);

    const auto readoutArea = area.removeFromRight (40.0f);
    const auto meterArea = area.reduced (2.0f, 1.5f);

    drawGlassWell (g, meterArea, 2.0f);

    const auto inner = meterArea.reduced (1.5f, 1.5f);
    constexpr int segments = 44;
    const float gap = 1.4f;
    const float segmentWidth = (inner.getWidth() - gap * static_cast<float> (segments - 1))
                               / static_cast<float> (segments);
    const float lit = juce::jlimit (0.0f, 1.0f, normalisedLevel) * static_cast<float> (segments);

    for (int i = 0; i < segments; ++i)
    {
        const juce::Rectangle<float> segment (inner.getX() + static_cast<float> (i) * (segmentWidth + gap),
                                              inner.getY(), segmentWidth, inner.getHeight());

        if (static_cast<float> (i) < lit)
        {
            const float proportion = static_cast<float> (i) / static_cast<float> (segments);
            g.setColour (proportion > 0.92f ? crimsonBright
                                            : crimsonAccent.interpolatedWith (crimsonGlow, 0.35f));
            g.fillRoundedRectangle (segment, 0.8f);
        }
        else
        {
            g.setColour (obsidianGrid.withAlpha (0.75f));
            g.fillRoundedRectangle (segment, 0.8f);
        }
    }

    if (normalisedPeak > 0.001f)
    {
        const float peakX = inner.getX() + normalisedPeak * inner.getWidth();
        g.setColour (textPrimary.withAlpha (0.75f));
        g.fillRect (juce::jlimit (inner.getX(), inner.getRight() - 1.0f, peakX),
                    inner.getY() - 0.5f, 1.0f, inner.getHeight() + 1.0f);
    }

    const float db = normalisedLevel <= 0.0001f
                         ? -48.0f
                         : juce::jmap (normalisedLevel, 0.0f, 1.0f, -48.0f, 0.0f);
    g.setColour (normalisedLevel > 0.92f ? crimsonBright : textSecondary);
    g.setFont (font (6.4f, Weight::regular, 0.04f));
    g.drawText (juce::String (db, 1), readoutArea, juce::Justification::centredRight, false);
}
} // namespace studio::ui
