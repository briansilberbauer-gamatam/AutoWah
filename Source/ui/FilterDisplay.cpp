#include "FilterDisplay.h"

#include "Parameters.h"
#include "Theme.h"

#include <cmath>
#include <utility>

namespace studio::ui
{
namespace
{
constexpr float minPlotHz = 60.0f;
constexpr float maxPlotHz = 12000.0f;
constexpr float maxPlotDb = 12.0f;
constexpr float minPlotDb = -36.0f;

const float logPlotRange = std::log (maxPlotHz / minPlotHz);
} // namespace

FilterDisplay::FilterDisplay (AutoWahAudioProcessor& p)
    : processor (p)
{
    setOpaque (false);
    setInterceptsMouseClicks (false, false);

    // Fill the history organically from the right; at 30 Hz the 90-slot buffer
    // covers three seconds, which is enough for at least one slow LFO cycle.
    modulationHistory.fill (0.0f);
    historyFilled = 0;

    startTimerHz (30);
}

void FilterDisplay::timerCallback()
{
    waveformColumns = processor.copyWaveform (waveformMin.data(), waveformMax.data(),
                                              maxWaveformColumns);

    // Auto-scale the display trace so quiet wah settings still show a readable
    // waveform while the output meter remains the honest level reference.
    float waveformPeak = 0.0f;

    for (int i = 0; i < waveformColumns; ++i)
        waveformPeak = juce::jmax (waveformPeak,
                                   std::abs (waveformMin[static_cast<size_t> (i)]),
                                   std::abs (waveformMax[static_cast<size_t> (i)]));

    const float targetGain = waveformPeak > 0.0005f
                                 ? juce::jlimit (1.0f, 4.0f, 0.34f / waveformPeak)
                                 : 1.0f;
    waveformGain += 0.18f * (targetGain - waveformGain);

    cutoffHz = processor.getCutoffHz();
    controlAmount = processor.getControlAmount();
    envelopeLevel = processor.getEnvelopeLevel();
    lfoValue = processor.getLfoValue();
    lfoPhase = processor.getLfoPhase();
    mode = processor.getActiveMode();

    auto& state = processor.getValueTreeState();

    if (auto* q = state.getRawParameterValue (autowah::params::resonance))
        resonance = q->load();

    if (auto* type = state.getRawParameterValue (autowah::params::filterType))
        filterType = juce::roundToInt (type->load());

    // On the first tick, back-fill the scope with the current control value so
    // the display opens as a settled trace instead of jumping up from zero.
    if (historyFilled == 0)
    {
        modulationHistory.fill (juce::jlimit (0.0f, 1.0f, controlAmount));
        historyFilled = historyLength;
    }

    modulationHistory[static_cast<size_t> (historyWrite)] = juce::jlimit (0.0f, 1.0f, controlAmount);
    historyWrite = (historyWrite + 1) % historyLength;

    repaint();
}

float FilterDisplay::frequencyToX (float hz, juce::Rectangle<float> plot) const
{
    const float clamped = juce::jlimit (minPlotHz, maxPlotHz, hz);
    const float proportion = std::log (clamped / minPlotHz) / logPlotRange;
    return plot.getX() + proportion * plot.getWidth();
}

float FilterDisplay::decibelsToY (float db, juce::Rectangle<float> plot) const
{
    const float clamped = juce::jlimit (minPlotDb, maxPlotDb, db);
    const float proportion = (clamped - minPlotDb) / (maxPlotDb - minPlotDb);
    return plot.getBottom() - proportion * plot.getHeight();
}

void FilterDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    constexpr float corner = 6.0f;

    g.setColour (obsidianScreen);
    g.fillRoundedRectangle (bounds, corner);

    const auto inner = bounds.reduced (2.0f);
    const auto header = juce::Rectangle<float> (14.0f, 9.0f, bounds.getWidth() - 28.0f, 27.0f);
    const auto plot = juce::Rectangle<float> (50.0f, 40.0f, bounds.getWidth() - 64.0f,
                                              bounds.getHeight() - 148.0f);
    const auto frequencyLabels = juce::Rectangle<float> (plot.getX(), plot.getBottom() + 1.0f,
                                                         plot.getWidth(), 13.0f);
    const auto strip = juce::Rectangle<float> (plot.getX(), plot.getBottom() + 25.0f,
                                               plot.getWidth(), 44.0f);
    const auto footer = juce::Rectangle<float> (plot.getX(), strip.getBottom() + 4.0f,
                                                plot.getWidth(), 14.0f);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path screenClip;
        screenClip.addRoundedRectangle (inner, corner - 1.0f);
        g.reduceClipRegion (screenClip);

        // Subtle upper-left glass reflection across the embedded screen.
        juce::Path sheen;
        sheen.startNewSubPath (inner.getX(), inner.getY());
        sheen.lineTo (inner.getX() + inner.getWidth() * 0.44f, inner.getY());
        sheen.lineTo (inner.getX() + inner.getWidth() * 0.16f, inner.getBottom());
        sheen.lineTo (inner.getX(), inner.getBottom());
        sheen.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.030f),
                                                inner.getTopLeft(),
                                                juce::Colours::transparentWhite,
                                                inner.getBottomRight(), false));
        g.fillPath (sheen);

        paintHeader (g, header);

        g.setColour (obsidianScreen);
        g.fillRoundedRectangle (plot, 2.0f);

        // The live waveform sits behind the grid and response so it reads as a
        // soft oscilloscope bed rather than competing with the filter curve.
        paintWaveform (g, plot);
        paintGrid (g, plot);
        paintResponse (g, plot);
        paintCutoffCursor (g, plot);
        paintModulationStrip (g, strip);

        // Frequency scale markers under the plot.
        g.setFont (font (6.4f, Weight::regular, 0.04f));
        const std::array<std::pair<float, const char*>, 7> markers {{
            { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" },
            { 1000.0f, "1k" }, { 2000.0f, "2k" }, { 5000.0f, "5k" },
            { 10000.0f, "10k" }
        }};

        for (const auto& marker : markers)
        {
            const float x = frequencyToX (marker.first, plot);
            g.setColour (textMuted.withAlpha (0.75f));
            g.drawText (marker.second,
                        juce::Rectangle<float> (x - 16.0f, frequencyLabels.getY(), 32.0f,
                                                frequencyLabels.getHeight()),
                        juce::Justification::centred, false);
        }

        g.setColour (textMuted.withAlpha (0.72f));
        g.setFont (font (5.7f, Weight::semibold, 0.13f));
        g.drawText ("Hz", juce::Rectangle<float> (inner.getX() + 12.0f,
                                                  frequencyLabels.getY(),
                                                  26.0f, frequencyLabels.getHeight()),
                    juce::Justification::centredLeft, false);

        // Footer readouts.
        auto& apvts = processor.getValueTreeState();
        const auto read = [&apvts] (const char* id, float fallback)
        {
            if (auto* value = apvts.getRawParameterValue (id))
                return value->load();

            return fallback;
        };

        g.setFont (font (6.2f, Weight::semibold, 0.11f));
        g.setColour (textSecondary.withAlpha (0.82f));

        const auto shapeNames = autowah::params::lfoShapeChoices();
        const int shapeIndex = juce::jlimit (0, shapeNames.size() - 1,
                                             juce::roundToInt (read (autowah::params::lfoShape, 0.0f)));

        const juce::String sourceText = mode == 0
            ? "ENVELOPE  /  SENS " + autowah::params::formatDb (read (autowah::params::sensitivity, 6.0f))
                  + "  /  " + autowah::params::formatMs (read (autowah::params::attack, 1.5f))
                  + " ATTACK"
            : "LFO  /  " + shapeNames[shapeIndex].toUpperCase()
                  + "  /  " + autowah::params::formatRate (read (autowah::params::lfoRate, 1.5f))
                  + "  /  DEPTH " + autowah::params::formatPercent (read (autowah::params::lfoDepth, 100.0f));

        g.drawFittedText (sourceText, footer.toNearestInt(),
                          juce::Justification::centredLeft, 1);

        g.setColour (textMuted);
        g.setFont (font (6.0f, Weight::regular, 0.06f));
        g.drawText (mode == 0 ? "ENV LEVEL " + juce::String (envelopeLevel, 2)
                              : "PHASE " + juce::String (lfoPhase, 2),
                    footer.toNearestInt(), juce::Justification::centredRight, false);
    }

    // Recessed glass bezel and crimson inner edge refraction.
    g.setColour (crimsonAccent.withAlpha (0.42f));
    g.drawRoundedRectangle (bounds, corner, 1.1f);
    g.setColour (glassRimHighlight.withAlpha (0.16f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), corner - 1.0f, 0.8f);
}

void FilterDisplay::paintHeader (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    drawCrimsonDot (g, { bounds.getX() + 2.0f, bounds.getCentreY() }, 2.1f);

    const auto cutoffArea = bounds.removeFromRight (96.0f);
    const auto modeChip = bounds.removeFromRight (76.0f).reduced (2.0f, 3.0f);
    const auto typeChip = bounds.removeFromRight (62.0f).reduced (2.0f, 3.0f);

    g.setColour (textSecondary);
    g.setFont (font (6.1f, Weight::semibold, 0.20f));
    g.drawText ("FILTER RESPONSE  /  LIVE",
                bounds.withTrimmedLeft (14.0f).withTrimmedRight (8.0f),
                juce::Justification::centredLeft, false);

    const auto drawChip = [&g] (juce::Rectangle<float> area, const juce::String& text,
                                bool active)
    {
        g.setColour (obsidianWell.withAlpha (0.95f));
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (active ? crimsonAccent.withAlpha (0.75f) : gunmetalLight.withAlpha (0.45f));
        g.drawRoundedRectangle (area, 3.0f, 0.8f);
        g.setColour (active ? textPrimary : textSecondary);
        g.setFont (font (6.3f, Weight::semibold, 0.13f));
        g.drawFittedText (text, area.toNearestInt(), juce::Justification::centred, 1);
    };

    drawChip (modeChip, mode == 0 ? "ENVELOPE" : "LFO", true);
    drawChip (typeChip, juce::String (filterType == 0 ? "BP" : "LP") + " / Q "
                            + juce::String (resonance, 1),
             false);

    g.setColour (textPrimary);
    g.setFont (font (11.5f, Weight::semibold, 0.05f));
    g.drawText (autowah::params::formatHz (cutoffHz), cutoffArea,
                juce::Justification::centredRight, false);

    g.setColour (textMuted);
    g.setFont (font (5.2f, Weight::semibold, 0.16f));
    g.drawText ("CUTOFF", cutoffArea.withTrimmedTop (14.0f),
                juce::Justification::centredRight, false);
}

void FilterDisplay::paintGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const std::array<float, 7> dbLines { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -30.0f };

    for (const float db : dbLines)
    {
        const float y = decibelsToY (db, plot);
        const bool isZero = juce::approximatelyEqual (db, 0.0f);

        g.setColour (isZero ? obsidianGrid.brighter (0.16f) : obsidianGrid);
        g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);

        g.setColour (textMuted.withAlpha (isZero ? 0.85f : 0.62f));
        g.setFont (font (6.2f, Weight::regular, 0.02f));
        g.drawText (db > 0.0f ? "+" + juce::String (juce::roundToInt (db)) : juce::String (juce::roundToInt (db)),
                    juce::Rectangle<float> (plot.getX() - 36.0f, y - 6.0f, 30.0f, 12.0f),
                    juce::Justification::centredRight, false);
    }

    const std::array<float, 7> hzLines { 100.0f, 200.0f, 500.0f, 1000.0f,
                                         2000.0f, 5000.0f, 10000.0f };
    for (const float hz : hzLines)
    {
        const float x = frequencyToX (hz, plot);
        g.setColour (obsidianGrid);
        g.fillRect (x, plot.getY(), 1.0f, plot.getHeight());
    }

    // Plot frame.
    g.setColour (obsidianGrid.brighter (0.08f));
    g.drawRect (plot, 1.0f);
}

void FilterDisplay::paintWaveform (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    if (waveformColumns < 2)
        return;

    const float midY = plot.getCentreY();
    const float halfHeight = plot.getHeight() * 0.40f;

    juce::Path top;
    juce::Path bottom;
    juce::Path fill;

    for (int i = 0; i < waveformColumns; ++i)
    {
        const float x = plot.getX() + (static_cast<float> (i) / static_cast<float> (waveformColumns - 1))
                                        * plot.getWidth();
        const float maximum = midY - juce::jlimit (-1.0f, 1.0f,
                                                   waveformMax[static_cast<size_t> (i)]
                                                       * waveformGain) * halfHeight;

        if (i == 0)
        {
            top.startNewSubPath (x, maximum);
            fill.startNewSubPath (x, maximum);
        }
        else
        {
            top.lineTo (x, maximum);
            fill.lineTo (x, maximum);
        }
    }

    for (int i = waveformColumns - 1; i >= 0; --i)
    {
        const float x = plot.getX() + (static_cast<float> (i) / static_cast<float> (waveformColumns - 1))
                                        * plot.getWidth();
        const float minimum = midY - juce::jlimit (-1.0f, 1.0f,
                                                   waveformMin[static_cast<size_t> (i)]
                                                       * waveformGain) * halfHeight;

        if (i == waveformColumns - 1)
            bottom.startNewSubPath (x, minimum);
        else
            bottom.lineTo (x, minimum);

        fill.lineTo (x, minimum);
    }

    fill.closeSubPath();

    juce::ColourGradient waveGradient (crimsonAccent.withAlpha (0.07f), plot.getCentreX(), midY - halfHeight,
                                       crimsonGlow.withAlpha (0.012f), plot.getCentreX(), midY + halfHeight,
                                       false);
    g.setGradientFill (waveGradient);
    g.fillPath (fill);

    g.setColour (crimsonGlow.withAlpha (0.12f));
    g.strokePath (top, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.strokePath (bottom, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    g.setColour (crimsonAccent.withAlpha (0.40f));
    g.strokePath (top, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.strokePath (bottom, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));
}

void FilterDisplay::paintResponse (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    constexpr int points = 200;
    const double w0 = 2.0 * juce::MathConstants<double>::pi
                      * juce::jlimit (minPlotHz, maxPlotHz, cutoffHz);
    const double q = juce::jmax (0.5, static_cast<double> (resonance));
    const float makeup = 1.0f / std::sqrt (static_cast<float> (juce::jmax (1.0, q)));
    const bool lowpass = filterType != 0;

    juce::Path curve;
    juce::Path fill;

    for (int i = 0; i < points; ++i)
    {
        const float proportion = static_cast<float> (i) / static_cast<float> (points - 1);
        const double frequency = static_cast<double> (minPlotHz)
                                 * std::pow (static_cast<double> (maxPlotHz / minPlotHz),
                                             static_cast<double> (proportion));
        const double w = 2.0 * juce::MathConstants<double>::pi * frequency;
        const double denominatorReal = w0 * w0 - w * w;
        const double denominatorImaginary = (w0 / q) * w;
        const double denominator = std::sqrt (denominatorReal * denominatorReal
                                              + denominatorImaginary * denominatorImaginary);
        const double magnitude = denominator > 1.0e-12
            ? (lowpass ? (w0 * w0) / denominator
                       : ((w0 / q) * w) / denominator) * makeup
            : 1.0e-6;
        const float db = static_cast<float> (20.0 * std::log10 (juce::jmax (1.0e-6, magnitude)));
        const float x = plot.getX() + proportion * plot.getWidth();
        const float y = decibelsToY (juce::jlimit (minPlotDb - 6.0f, maxPlotDb + 6.0f, db), plot);

        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            fill.startNewSubPath (x, plot.getBottom());
            fill.lineTo (x, y);
        }
        else
        {
            curve.lineTo (x, y);
            fill.lineTo (x, y);
        }
    }

    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    juce::ColourGradient fillGradient (crimsonAccent.withAlpha (0.16f), plot.getCentreX(), plot.getY(),
                                       crimsonGlow.withAlpha (0.015f), plot.getCentreX(), plot.getBottom(),
                                       false);
    g.setGradientFill (fillGradient);
    g.fillPath (fill);

    g.setColour (crimsonGlow.withAlpha (0.30f));
    g.strokePath (curve, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    g.setColour (crimsonAccent);
    g.strokePath (curve, juce::PathStrokeType (2.1f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    g.setColour (crimsonBright);
    g.strokePath (curve, juce::PathStrokeType (0.95f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void FilterDisplay::paintCutoffCursor (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    const float x = frequencyToX (cutoffHz, plot);

    g.setColour (crimsonAccent.withAlpha (0.34f));
    g.fillRect (x, plot.getY() + 2.0f, 1.0f, plot.getHeight() - 4.0f);

    // The resonant peak of both taps sits at approximately +20log10(sqrt(Q)) dB.
    const float peakDb = static_cast<float> (20.0 * std::log10 (std::sqrt (juce::jmax (0.5, static_cast<double> (resonance)))));
    const float y = decibelsToY (juce::jlimit (minPlotDb, maxPlotDb, peakDb), plot);

    g.setColour (crimsonGlow.withAlpha (0.30f));
    g.fillEllipse (x - 6.0f, y - 6.0f, 12.0f, 12.0f);
    g.setColour (crimsonAccent);
    g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    g.setColour (crimsonBright);
    g.fillEllipse (x - 1.2f, y - 1.2f, 2.4f, 2.4f);

    juce::Path topMarker;
    topMarker.startNewSubPath (x - 3.5f, plot.getY());
    topMarker.lineTo (x + 3.5f, plot.getY());
    topMarker.lineTo (x, plot.getY() + 5.0f);
    topMarker.closeSubPath();
    g.setColour (crimsonAccent);
    g.fillPath (topMarker);
}

void FilterDisplay::paintModulationStrip (juce::Graphics& g, juce::Rectangle<float> strip) const
{
    drawGlassWell (g, strip, 3.0f);

    const auto inner = strip.reduced (3.0f, 3.0f);

    g.setColour (obsidianGrid);
    for (int i = 1; i < 8; ++i)
    {
        const float x = inner.getX() + inner.getWidth() * static_cast<float> (i) / 8.0f;
        g.fillRect (x, inner.getY(), 1.0f, inner.getHeight());
    }

    constexpr float tracePadding = 4.0f;
    const float baseline = inner.getBottom() - 2.0f;

    if (historyFilled >= 2)
    {
        const float spacing = inner.getWidth() / static_cast<float> (historyLength - 1);
        juce::Path trace;
        juce::Path fill;
        bool started = false;

        for (int i = 0; i < historyFilled; ++i)
        {
            const int index = (historyWrite - historyFilled + i + historyLength) % historyLength;
            const float value = juce::jlimit (0.0f, 1.0f,
                                              modulationHistory[static_cast<size_t> (index)]);
            const float x = inner.getRight() - (static_cast<float> (historyFilled - 1 - i)) * spacing;
            const float y = inner.getBottom() - tracePadding
                            - value * (inner.getHeight() - tracePadding * 2.0f);

            if (! started)
            {
                trace.startNewSubPath (x, y);
                fill.startNewSubPath (x, baseline);
                fill.lineTo (x, y);
                started = true;
            }
            else
            {
                trace.lineTo (x, y);
                fill.lineTo (x, y);
            }
        }

        fill.lineTo (inner.getRight(), baseline);
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (crimsonAccent.withAlpha (0.24f),
                                                inner.getCentreX(), inner.getY(),
                                                crimsonGlow.withAlpha (0.02f),
                                                inner.getCentreX(), inner.getBottom(), false));
        g.fillPath (fill);

        g.setColour (crimsonGlow.withAlpha (0.35f));
        g.strokePath (trace, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (mode == 0 ? crimsonAccent : crimsonBright);
        g.strokePath (trace, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (crimsonBright);
        g.fillEllipse (inner.getRight() - 2.0f,
                       inner.getBottom() - tracePadding
                           - juce::jlimit (0.0f, 1.0f, controlAmount)
                                 * (inner.getHeight() - tracePadding * 2.0f) - 2.0f,
                       4.0f, 4.0f);
    }

    g.setColour (textMuted.withAlpha (0.85f));
    g.setFont (font (5.4f, Weight::semibold, 0.14f));
    g.drawText ("MODULATION HISTORY",
                juce::Rectangle<float> (inner.getX() + 4.0f, inner.getY() + 1.0f, 120.0f, 9.0f),
                juce::Justification::centredLeft, false);

    g.setColour (mode == 0 ? crimsonAccent : crimsonBright);
    g.drawText (mode == 0 ? "ENVELOPE TRACE" : "LFO TRACE",
                juce::Rectangle<float> (inner.getRight() - 90.0f, inner.getY() + 1.0f, 86.0f, 9.0f),
                juce::Justification::centredRight, false);
}
} // namespace studio::ui
