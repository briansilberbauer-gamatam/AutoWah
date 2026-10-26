#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Theme.h"

#include <functional>
#include <memory>

namespace studio::ui
{
    //==================================================================================
    // Low-profile machined knob with caption and value readout.
    //==================================================================================
    class ParamKnob final : public juce::Component,
                            private juce::Timer
    {
    public:
        ParamKnob (juce::AudioProcessorValueTreeState&,
                   const juce::String& parameterID,
                   juce::String caption,
                   std::function<juce::String (float)> formatter);

        ~ParamKnob() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        void refreshValueText();

        juce::AudioProcessorValueTreeState& apvts;
        juce::String parameterID;
        juce::String captionText;
        std::function<juce::String (float)> formatValue;

        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String valueText;
        bool hover = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
    };

    //==================================================================================
    // Ultra-thin vertical glass fader inset into the panel.
    //==================================================================================
    class GlassSlider final : public juce::Component,
                              private juce::Timer
    {
    public:
        GlassSlider (juce::AudioProcessorValueTreeState&,
                     const juce::String& parameterID,
                     juce::String caption,
                     std::function<juce::String (float)> formatter);

        ~GlassSlider() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        void timerCallback() override;
        void refreshValueText();

        juce::AudioProcessorValueTreeState& apvts;
        juce::String parameterID;
        juce::String captionText;
        std::function<juce::String (float)> formatValue;

        juce::Slider slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
        juce::String valueText;
        bool hover = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlassSlider)
    };

    //==================================================================================
    // Low-profile tactile button attached to a boolean parameter.
    //==================================================================================
    class TactileButton final : public juce::ToggleButton
    {
    public:
        TactileButton (juce::AudioProcessorValueTreeState&,
                       const juce::String& parameterID,
                       const juce::String& labelText);

    private:
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TactileButton)
    };

    //==================================================================================
    // One position of a multi-way choice selector (ENV / LFO, BP / LP, shapes).
    //==================================================================================
    class ChoiceButton final : public juce::ToggleButton,
                               private juce::Timer
    {
    public:
        ChoiceButton (juce::AudioProcessorValueTreeState&,
                      const juce::String& parameterID,
                      int choiceIndex,
                      const juce::String& labelText);

        using juce::ToggleButton::clicked;
        void clicked() override;

    private:
        void timerCallback() override;

        juce::AudioProcessorValueTreeState& apvts;
        juce::String parameterID;
        int choiceIndex;
        juce::RangedAudioParameter* parameter = nullptr;
        std::atomic<float>* rawValue = nullptr;
        bool lastState = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceButton)
    };

    //==================================================================================
    // Slender top-centre pill readout with preset name and chevron navigation.
    //==================================================================================
    class HeaderPillBar final : public juce::Component,
                                private juce::Timer
    {
    public:
        explicit HeaderPillBar (AutoWahAudioProcessor&);

        void paint (juce::Graphics&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void()> onPresetChanged;

    private:
        enum class Zone { none, previous, next, readout };

        void timerCallback() override;
        void step (int delta);
        void showPresetMenu();
        Zone zoneAt (float x) const;

        AutoWahAudioProcessor& processor;
        int lastPreset = -1;
        bool lastDirty = false;
        Zone hoverZone = Zone::none;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderPillBar)
    };

    //==================================================================================
    // Horizontal detector / LFO activity strip for the modulation source zone.
    //==================================================================================
    class ModulationMeter final : public juce::Component,
                                  private juce::Timer
    {
    public:
        explicit ModulationMeter (AutoWahAudioProcessor&);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        AutoWahAudioProcessor& processor;
        float displayValue = 0.0f;
        float peakValue = 0.0f;
        float phase = 0.0f;
        int peakHold = 0;
        int mode = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModulationMeter)
    };

    //==================================================================================
    // Compact stereo output meter with peak hold and clip bead.
    //==================================================================================
    class OutputMeter final : public juce::Component,
                              private juce::Timer
    {
    public:
        explicit OutputMeter (AutoWahAudioProcessor&);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;
        void drawChannel (juce::Graphics&, juce::Rectangle<float> bounds,
                          const juce::String& label, float normalisedLevel,
                          float normalisedPeak) const;

        AutoWahAudioProcessor& processor;
        float levelLeft = 0.0f;
        float levelRight = 0.0f;
        float peakLeft = 0.0f;
        float peakRight = 0.0f;
        int peakHoldLeft = 0;
        int peakHoldRight = 0;
        int clipHold = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputMeter)
    };
} // namespace studio::ui
