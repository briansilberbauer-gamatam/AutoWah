#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace autowah
{
struct FactoryPreset
{
    const char* name;
    const char* character;
    int mode;
    int filterType;
    int lfoShape;
    float startFreq;
    float sweep;
    float resonance;
    float sensitivity;
    float attack;
    float release;
    float lfoRate;
    float lfoDepth;
    float mix;
    float output;
    float inputDb = 0.0f;
};

inline constexpr int numFactoryPresets = 10;

inline const std::array<FactoryPreset, numFactoryPresets>& factoryPresets()
{
    static const std::array<FactoryPreset, numFactoryPresets> presets {{
        { "FUNK BUBBLE",
          "Tight percussive quack. Fast attack, quick release, vocal midrange.",
          0, 0, 0, 320.0f, 2.4f, 5.5f, 6.0f, 1.5f, 90.0f, 1.5f, 100.0f, 100.0f, 0.0f },

        { "MU-TRON SWEEP",
          "Deep retro bandpass sweep with a long, greasy release.",
          0, 0, 0, 220.0f, 3.2f, 7.5f, 2.0f, 6.0f, 320.0f, 1.5f, 100.0f, 100.0f, 0.0f },

        { "TALKING LEAD",
          "Resonant vowel-like lead voice. Dig in for the vowel shift.",
          0, 0, 0, 480.0f, 2.0f, 8.5f, 9.0f, 3.0f, 180.0f, 1.5f, 100.0f, 85.0f, 0.0f },

        { "SLAP BASS",
          "Sub-friendly bass wah; the harder you play, the higher it barks.",
          0, 0, 0, 180.0f, 3.6f, 4.5f, 10.0f, 0.8f, 60.0f, 1.5f, 100.0f, 100.0f, 0.0f },

        { "SLOW MORPH",
          "Slow ambient envelope bloom with a wide, gentle sweep.",
          0, 0, 0, 260.0f, 3.0f, 3.5f, 0.0f, 40.0f, 900.0f, 1.5f, 100.0f, 70.0f, 0.0f },

        { "WOBBLE SINE",
          "Classic dubstep wobble: sine LFO, deep and round.",
          1, 0, 0, 120.0f, 3.4f, 6.0f, 6.0f, 1.5f, 90.0f, 2.2f, 100.0f, 100.0f, 0.0f },

        { "TREM WAH",
          "Square-wave chop for rhythmic tremolo wah stabs.",
          1, 0, 2, 400.0f, 2.2f, 5.0f, 6.0f, 1.5f, 90.0f, 5.5f, 80.0f, 85.0f, 0.0f },

        { "PHASE RIDER",
          "Slow triangle sweep that rides just behind the groove.",
          1, 0, 1, 250.0f, 3.0f, 4.0f, 6.0f, 1.5f, 90.0f, 0.8f, 90.0f, 100.0f, 0.0f },

        { "DUB WOBBLE",
          "Lowpass LFO wobble with a long, dark resonant tail.",
          1, 1, 0, 90.0f, 4.0f, 9.0f, 6.0f, 1.5f, 90.0f, 0.45f, 100.0f, 100.0f, 0.0f },

        { "NEON PULSE",
          "Rising saw sweep, bright and synthetic for electronic pulses.",
          1, 0, 3, 600.0f, 2.6f, 10.0f, 6.0f, 1.5f, 90.0f, 3.2f, 75.0f, 90.0f, -3.0f }
    }};

    return presets;
}

inline const FactoryPreset& getFactoryPreset (int index)
{
    const auto& presets = factoryPresets();
    return presets[static_cast<size_t> (juce::jlimit (0, numFactoryPresets - 1, index))];
}

inline juce::String getFactoryPresetCharacter (int index)
{
    return getFactoryPreset (index).character;
}
} // namespace autowah
