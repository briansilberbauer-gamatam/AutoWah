#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace autowah::params
{
    // Stable parameter identifiers. Never rename these: hosts persist them.
    inline constexpr const char* mode         = "mode";         // 0 = envelope, 1 = LFO
    inline constexpr const char* filterType   = "filterType";   // 0 = bandpass, 1 = lowpass
    inline constexpr const char* lfoShape     = "lfoShape";     // 0 = sine, 1 = triangle, 2 = square, 3 = saw
    inline constexpr const char* startFreq    = "startFreq";
    inline constexpr const char* sweep        = "sweep";
    inline constexpr const char* resonance    = "resonance";
    inline constexpr const char* sensitivity  = "sensitivity";
    inline constexpr const char* attack       = "attack";
    inline constexpr const char* release      = "release";
    inline constexpr const char* lfoRate      = "lfoRate";
    inline constexpr const char* lfoDepth     = "lfoDepth";
    inline constexpr const char* input        = "input";
    inline constexpr const char* mix          = "mix";
    inline constexpr const char* output       = "output";
    inline constexpr const char* bypass       = "bypass";

    enum class Mode { envelope = 0, lfo = 1 };
    enum class FilterType { bandpass = 0, lowpass = 1 };
    enum class LfoShape { sine = 0, triangle = 1, square = 2, saw = 3 };

    inline constexpr int numParameters = 15;

    inline juce::String formatHz (float value)
    {
        if (value >= 1000.0f)
            return juce::String (value / 1000.0f, 2) + " kHz";

        return juce::String (juce::roundToInt (value)) + " Hz";
    }

    inline juce::String formatDb (float value)
    {
        if (std::abs (value) < 0.05f)
            return "0.0 dB";

        return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
    }

    inline juce::String formatPercent (float value)
    {
        return juce::String (juce::roundToInt (value)) + " %";
    }

    inline juce::String formatMs (float value)
    {
        if (value >= 1000.0f)
            return juce::String (value / 1000.0f, 2) + " s";

        if (value < 10.0f)
            return juce::String (value, 1) + " ms";

        return juce::String (juce::roundToInt (value)) + " ms";
    }

    inline juce::String formatOctaves (float value)
    {
        return juce::String (value, 1) + " oct";
    }

    inline juce::String formatQ (float value)
    {
        return juce::String (value, 1);
    }

    inline juce::String formatRate (float value)
    {
        return juce::String (value, 2) + " Hz";
    }

    inline juce::StringArray modeChoices()
    {
        return { "Envelope", "LFO" };
    }

    inline juce::StringArray filterTypeChoices()
    {
        return { "Bandpass", "Lowpass" };
    }

    inline juce::StringArray lfoShapeChoices()
    {
        return { "Sine", "Triangle", "Square", "Saw" };
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto floatParameter = [&layout] (const char* id, const juce::String& name,
                                         juce::NormalisableRange<float> range, float defaultValue,
                                         std::function<juce::String (float, int)> toText,
                                         std::function<float (const juce::String&)> fromText,
                                         const juce::String& group = {})
        {
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, 1 }, name, range, defaultValue,
                juce::AudioParameterFloatAttributes()
                    .withStringFromValueFunction (std::move (toText))
                    .withValueFromStringFunction (std::move (fromText))
                    .withLabel (group)));
        };

        auto choiceParameter = [&layout] (const char* id, const juce::String& name,
                                          const juce::StringArray& choices, int defaultIndex,
                                          const juce::String& group = {})
        {
            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { id, 1 }, name, choices, defaultIndex,
                juce::AudioParameterChoiceAttributes().withLabel (group)));
        };

        // --- Control source: the signature envelope / LFO switch ---
        choiceParameter (mode, "Control Source", modeChoices(), 0);

        // --- Filter core ---
        choiceParameter (filterType, "Filter Type", filterTypeChoices(), 0);

        juce::NormalisableRange<float> startRange { 80.0f, 6000.0f, 0.1f };
        startRange.setSkewForCentre (700.0f);
        floatParameter (startFreq, "Start Frequency", startRange, 320.0f,
                        [] (float v, int) { return formatHz (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Filter");

        floatParameter (sweep, "Sweep Range",
                        juce::NormalisableRange<float> { 0.0f, 6.0f, 0.01f }, 2.4f,
                        [] (float v, int) { return formatOctaves (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Filter");

        floatParameter (resonance, "Resonance",
                        juce::NormalisableRange<float> { 0.5f, 14.0f, 0.01f }, 5.5f,
                        [] (float v, int) { return formatQ (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Filter");

        // --- Envelope detector ---
        floatParameter (sensitivity, "Sensitivity",
                        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 6.0f,
                        [] (float v, int) { return formatDb (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Envelope");

        juce::NormalisableRange<float> attackRange { 0.5f, 100.0f, 0.01f };
        attackRange.setSkewForCentre (8.0f);
        floatParameter (attack, "Attack", attackRange, 1.5f,
                        [] (float v, int) { return formatMs (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Envelope");

        juce::NormalisableRange<float> releaseRange { 10.0f, 1200.0f, 0.1f };
        releaseRange.setSkewForCentre (180.0f);
        floatParameter (release, "Release", releaseRange, 90.0f,
                        [] (float v, int) { return formatMs (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Envelope");

        // --- LFO ---
        juce::NormalisableRange<float> rateRange { 0.05f, 12.0f, 0.001f };
        rateRange.setSkewForCentre (1.2f);
        floatParameter (lfoRate, "LFO Rate", rateRange, 1.5f,
                        [] (float v, int) { return formatRate (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "LFO");

        floatParameter (lfoDepth, "LFO Depth",
                        juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
                        [] (float v, int) { return formatPercent (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "LFO");

        choiceParameter (lfoShape, "LFO Shape", lfoShapeChoices(), 0, "LFO");

        // --- Mixer bay: input trim, wet/dry blend and output level ---
        floatParameter (input, "Input",
                        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f }, 0.0f,
                        [] (float v, int) { return formatDb (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Mixer");

        floatParameter (mix, "Mix", juce::NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, 100.0f,
                        [] (float v, int) { return formatPercent (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Output");

        floatParameter (output, "Output",
                        juce::NormalisableRange<float> { -24.0f, 12.0f, 0.1f }, 0.0f,
                        [] (float v, int) { return formatDb (v); },
                        [] (const juce::String& t) { return t.getFloatValue(); },
                        "Output");

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { bypass, 1 }, "Bypass", false,
            juce::AudioParameterBoolAttributes().withLabel ("Output")));

        return layout;
    }
} // namespace autowah::params
