#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

#include <array>

namespace studio::ui
{
    // Large central obsidian screen: live audio waveform, computed resonant filter
    // response, animated cutoff cursor and a scrolling modulation-source history.
    class FilterDisplay final : public juce::Component,
                                private juce::Timer
    {
    public:
        explicit FilterDisplay (AutoWahAudioProcessor&);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        void paintHeader (juce::Graphics&, juce::Rectangle<float> bounds) const;
        void paintGrid (juce::Graphics&, juce::Rectangle<float> plot) const;
        void paintWaveform (juce::Graphics&, juce::Rectangle<float> plot) const;
        void paintResponse (juce::Graphics&, juce::Rectangle<float> plot) const;
        void paintCutoffCursor (juce::Graphics&, juce::Rectangle<float> plot) const;
        void paintModulationStrip (juce::Graphics&, juce::Rectangle<float> strip) const;

        float frequencyToX (float hz, juce::Rectangle<float> plot) const;
        float decibelsToY (float db, juce::Rectangle<float> plot) const;

        AutoWahAudioProcessor& processor;

        static constexpr int maxWaveformColumns = 96;
        static constexpr int historyLength = 90;

        std::array<float, maxWaveformColumns> waveformMin {};
        std::array<float, maxWaveformColumns> waveformMax {};
        int waveformColumns = 0;

        std::array<float, historyLength> modulationHistory {};
        int historyWrite = 0;
        int historyFilled = 0;

        float cutoffHz = 320.0f;
        float controlAmount = 0.0f;
        float waveformGain = 1.0f;
        float envelopeLevel = 0.0f;
        float lfoValue = 0.0f;
        float lfoPhase = 0.0f;
        float resonance = 5.5f;
        int mode = 0;
        int filterType = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilterDisplay)
    };
} // namespace studio::ui
