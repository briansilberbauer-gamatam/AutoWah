#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "ui/StudioLookAndFeel.h"

#include <memory>

class AutoWahEditorContent;

class AutoWahAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AutoWahAudioProcessorEditor (AutoWahAudioProcessor&);
    ~AutoWahAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Used by the headless screenshot renderer so no hover tooltip is captured.
    void setTooltipsVisible (bool shouldBeVisible);

private:
    AutoWahAudioProcessor& processor;
    studio::ui::StudioLookAndFeel lookAndFeel;
    std::unique_ptr<AutoWahEditorContent> content;
    juce::TooltipWindow tooltipWindow { this, 650 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoWahAudioProcessorEditor)
};
