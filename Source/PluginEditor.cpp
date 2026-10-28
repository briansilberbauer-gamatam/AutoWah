#include "PluginEditor.h"

#include "Parameters.h"
#include "Presets.h"
#include "ui/Controls.h"
#include "ui/FilterDisplay.h"
#include "ui/Theme.h"

#include <array>

using namespace studio::ui;
using namespace autowah::params;

namespace
{
constexpr int designWidth = 1180;
constexpr int designHeight = 720;
} // namespace

class AutoWahEditorContent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit AutoWahEditorContent (AutoWahAudioProcessor& p)
        : processor (p),
          apvts (p.getValueTreeState())
    {
        setOpaque (true);

        sensitivityKnob = std::make_unique<ParamKnob> (apvts, sensitivity, "SENSITIVITY", formatDb);
        attackKnob = std::make_unique<ParamKnob> (apvts, attack, "ATTACK", formatMs);
        releaseKnob = std::make_unique<ParamKnob> (apvts, release, "RELEASE", formatMs);

        startKnob = std::make_unique<ParamKnob> (apvts, startFreq, "START FREQ", formatHz);
        sweepKnob = std::make_unique<ParamKnob> (apvts, sweep, "SWEEP", formatOctaves);
        resonanceKnob = std::make_unique<ParamKnob> (apvts, resonance, "RESONANCE", formatQ);

        rateKnob = std::make_unique<ParamKnob> (apvts, lfoRate, "RATE", formatRate);
        depthKnob = std::make_unique<ParamKnob> (apvts, lfoDepth, "DEPTH", formatPercent);

        for (auto* knob : { sensitivityKnob.get(), attackKnob.get(), releaseKnob.get(),
                            startKnob.get(), sweepKnob.get(), resonanceKnob.get(),
                            rateKnob.get(), depthKnob.get() })
        {
            addAndMakeVisible (*knob);
        }

        inputSlider = std::make_unique<GlassSlider> (apvts, input, "INPUT", formatDb);
        mixSlider = std::make_unique<GlassSlider> (apvts, mix, "MIX", formatPercent);
        outputSlider = std::make_unique<GlassSlider> (apvts, output, "OUTPUT", formatDb);
        addAndMakeVisible (*inputSlider);
        addAndMakeVisible (*mixSlider);
        addAndMakeVisible (*outputSlider);

        envelopeButton = std::make_unique<ChoiceButton> (apvts, mode,
                                                         static_cast<int> (Mode::envelope), "ENV");
        lfoButton = std::make_unique<ChoiceButton> (apvts, mode,
                                                    static_cast<int> (Mode::lfo), "LFO");
        bandpassButton = std::make_unique<ChoiceButton> (apvts, filterType,
                                                         static_cast<int> (FilterType::bandpass), "BP");
        lowpassButton = std::make_unique<ChoiceButton> (apvts, filterType,
                                                        static_cast<int> (FilterType::lowpass), "LP");

        sineButton = std::make_unique<ChoiceButton> (apvts, lfoShape,
                                                     static_cast<int> (LfoShape::sine), "SINE");
        triangleButton = std::make_unique<ChoiceButton> (apvts, lfoShape,
                                                         static_cast<int> (LfoShape::triangle), "TRI");
        squareButton = std::make_unique<ChoiceButton> (apvts, lfoShape,
                                                       static_cast<int> (LfoShape::square), "SQR");
        sawButton = std::make_unique<ChoiceButton> (apvts, lfoShape,
                                                    static_cast<int> (LfoShape::saw), "SAW");

        bypassButton = std::make_unique<TactileButton> (apvts, bypass, "BYPASS");

        const auto addToggle = [this] (juce::ToggleButton& button) { addAndMakeVisible (button); };
        addToggle (*envelopeButton);
        addToggle (*lfoButton);
        addToggle (*bandpassButton);
        addToggle (*lowpassButton);
        addToggle (*sineButton);
        addToggle (*triangleButton);
        addToggle (*squareButton);
        addToggle (*sawButton);
        addToggle (*bypassButton);

        filterDisplay = std::make_unique<FilterDisplay> (processor);
        addAndMakeVisible (*filterDisplay);

        modulationMeter = std::make_unique<ModulationMeter> (processor);
        addAndMakeVisible (*modulationMeter);

        outputMeter = std::make_unique<OutputMeter> (processor);
        addAndMakeVisible (*outputMeter);

        pillBar = std::make_unique<HeaderPillBar> (processor);
        pillBar->onPresetChanged = [this] { repaint(); };
        addAndMakeVisible (*pillBar);

        lastBypassed = isBypassed();
        lastPreset = processor.getCurrentPresetIndex();
        lastDirty = processor.isPresetDirty();
        updateModeDimming();
        startTimerHz (12);
    }

    ~AutoWahEditorContent() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        drawDesktopHardwareChassis (g, getLocalBounds().toFloat());
        drawMixerBay (g, mixerBayBounds);

        paintBrandHeader (g);

        g.setColour (obsidianGrid.withAlpha (0.90f));
        g.fillRect (46.0f, 91.0f, 870.0f, 1.0f);

        drawEngravedLabel (g, { 46.0f, 97.0f, 232.0f, 26.0f },
                           "ENVELOPE DETECTOR", "PROGRAM-DEPENDENT SWEEP");
        drawEngravedLabel (g, { 306.0f, 97.0f, 394.0f, 26.0f },
                           "FILTER CORE", "LOG-SWEPT 12 dB STATE VARIABLE");
        drawEngravedLabel (g, { 716.0f, 97.0f, 200.0f, 26.0f },
                           "LFO GENERATOR", "CYCLIC SWEEP");

        paintAnnotations (g);
        paintFooter (g);
    }

    void resized() override
    {
        auto unitBounds = getLocalBounds().reduced (18, 16);
        auto metalBase = unitBounds.reduced (5);
        auto glassPanel = metalBase.reduced (6);
        auto innerGlass = glassPanel.reduced (18, 16);

        // 1. Carve out the right-side mixer bay FIRST so it spans the full
        //    interior height of the unit.
        auto rightMixerBay = innerGlass.removeFromRight (204);
        mixerBayBounds = rightMixerBay.toFloat();
        innerGlass.removeFromRight (14);

        // 1a. Horizontal Output meter lives vertically BELOW the three faders.
        auto outputMeterArea = rightMixerBay.removeFromBottom (72);
        outputMeter->setBounds (outputMeterArea.reduced (6, 4));

        rightMixerBay.removeFromBottom (8);

        // 1b. Three vertical mixer columns: Input, Wet/Dry Mix, Output.
        const std::array<GlassSlider*, 3> mixerSliders { inputSlider.get(),
                                                         mixSlider.get(),
                                                         outputSlider.get() };
        const int columnWidth = rightMixerBay.getWidth() / 3;

        for (size_t i = 0; i < mixerSliders.size(); ++i)
        {
            auto column = rightMixerBay.removeFromLeft (i == mixerSliders.size() - 1
                                                            ? rightMixerBay.getWidth()
                                                            : columnWidth);
            mixerSliders[i]->setBounds (column.reduced (2, 2));
        }

        // 2. Left / main workspace: header pill, filter screen, knobs & buttons.
        auto topRow = innerGlass.removeFromTop (38);
        pillBar->setBounds (topRow.withSizeKeepingCentre (360, 32));

        envelopeButton->setBounds (702, 48, 64, 30);
        lfoButton->setBounds (770, 48, 64, 30);
        bypassButton->setBounds (838, 48, 78, 30);

        filterDisplay->setBounds (306, 126, 394, 318);

        sensitivityKnob->setBounds (46, 122, 232, 200);
        attackKnob->setBounds (52, 328, 104, 128);
        releaseKnob->setBounds (168, 328, 104, 128);
        modulationMeter->setBounds (52, 464, 220, 68);

        startKnob->setBounds (306, 452, 130, 132);
        sweepKnob->setBounds (438, 452, 130, 132);
        resonanceKnob->setBounds (570, 452, 130, 132);

        bandpassButton->setBounds (580, 96, 52, 24);
        lowpassButton->setBounds (638, 96, 52, 24);

        rateKnob->setBounds (726, 122, 160, 150);
        depthKnob->setBounds (726, 276, 160, 140);

        sineButton->setBounds (730, 442, 78, 28);
        triangleButton->setBounds (814, 442, 78, 28);
        squareButton->setBounds (730, 476, 78, 28);
        sawButton->setBounds (814, 476, 78, 28);
    }

private:
    void timerCallback() override
    {
        const bool bypassed = isBypassed();
        const int preset = processor.getCurrentPresetIndex();
        const bool dirty = processor.isPresetDirty();

        if (bypassed != lastBypassed || preset != lastPreset || dirty != lastDirty)
        {
            lastBypassed = bypassed;
            lastPreset = preset;
            lastDirty = dirty;
            repaint();
        }

        updateModeDimming();
    }

    // The inactive modulation section is dimmed (but still editable) so the
    // active control source reads at a glance.
    void updateModeDimming()
    {
        if (auto* rawMode = apvts.getRawParameterValue (mode))
        {
            const int currentMode = juce::roundToInt (rawMode->load());

            if (currentMode == dimmedMode)
                return;

            dimmedMode = currentMode;

            const float envelopeAlpha = currentMode == 0 ? 1.0f : 0.68f;
            const float lfoAlpha = currentMode == 1 ? 1.0f : 0.68f;

            for (auto* knob : { sensitivityKnob.get(), attackKnob.get(), releaseKnob.get() })
                knob->setAlpha (envelopeAlpha);

            modulationMeter->setAlpha (envelopeAlpha);

            for (auto* knob : { rateKnob.get(), depthKnob.get() })
                knob->setAlpha (lfoAlpha);

            for (auto* button : { sineButton.get(), triangleButton.get(),
                                  squareButton.get(), sawButton.get() })
                button->setAlpha (lfoAlpha);
        }
    }

    bool isBypassed() const noexcept
    {
        if (auto* value = apvts.getRawParameterValue (bypass))
            return value->load() > 0.5f;

        return false;
    }

    void paintBrandHeader (juce::Graphics& g)
    {
        g.setColour (textSecondary.withAlpha (0.85f));
        g.setFont (font (6.3f, Weight::semibold, 0.22f));
        g.drawText ("BLS AUDIO  /  SERIES WAH", 46, 36, 250, 9,
                    juce::Justification::centredLeft, false);

        g.setColour (textPrimary);
        g.setFont (font (23.0f, Weight::black, 0.035f));
        g.drawText ("AUTOWAH", 46, 44, 250, 32,
                    juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.4f, Weight::regular, 0.12f));
        g.drawText ("DUAL-SOURCE DYNAMIC FILTER  /  ENVELOPE + LFO",
                    46, 78, 250, 9, juce::Justification::centredLeft, false);

        g.setColour (crimsonAccent);
        g.fillRect (46.0f, 89.0f, 54.0f, 1.4f);

        // Control-source micro label above the mode selector.
        g.setColour (textMuted);
        g.setFont (font (5.8f, Weight::semibold, 0.18f));
        g.drawText ("CONTROL SOURCE", 702, 34, 214, 9,
                    juce::Justification::centredRight, false);
    }

    void paintAnnotations (juce::Graphics& g)
    {
        g.setFont (font (5.7f, Weight::regular, 0.10f));
        g.setColour (textMuted);

        g.drawText ("SENSITIVITY DRIVES THE DETECTOR",
                    46, 538, 232, 9, juce::Justification::centredLeft, false);
        g.drawText ("FAST ATTACK + LONG RELEASE = FUNK",
                    46, 550, 232, 9, juce::Justification::centredLeft, false);

        g.setFont (font (5.6f, Weight::semibold, 0.15f));
        g.drawText ("TYPE", 546, 102, 30, 10,
                    juce::Justification::centredRight, false);

        g.setColour (textMuted);
        g.setFont (font (5.6f, Weight::semibold, 0.15f));
        g.drawText ("SHAPE", 726, 426, 160, 10,
                    juce::Justification::centredLeft, false);

        g.setFont (font (5.7f, Weight::regular, 0.10f));
        g.drawText ("FREE-RUNNING", 726, 512, 160, 9,
                    juce::Justification::centredLeft, false);
        g.drawText ("RESETS ON MODE ENTRY", 726, 524, 160, 9,
                    juce::Justification::centredLeft, false);
    }

    void paintFooter (juce::Graphics& g)
    {
        g.setColour (obsidianGrid);
        g.fillRect (46.0f, 596.0f, 870.0f, 1.0f);

        g.setColour (textMuted);
        g.setFont (font (6.2f, Weight::semibold, 0.16f));
        g.drawText ("PRESET CHARACTER", 46, 604, 250, 9,
                    juce::Justification::centredLeft, false);

        const int preset = processor.getCurrentPresetIndex();
        const bool dirty = processor.isPresetDirty();

        g.setColour (textSecondary);
        g.setFont (font (7.2f, Weight::regular, 0.05f));
        g.drawFittedText (autowah::getFactoryPresetCharacter (preset),
                          juce::Rectangle<int> (46, 616, 250, 24),
                          juce::Justification::centredLeft, 2);

        drawCrimsonDot (g, { 49.0f, 652.0f }, 2.0f);

        g.setColour (dirty ? crimsonBright : textMuted);
        g.setFont (font (6.2f, Weight::semibold, 0.14f));
        g.drawText (dirty ? "EDITED / UNSAVED" : "FACTORY / CLEAN",
                    56, 647, 240, 9, juce::Justification::centredLeft, false);

        // Signal-path chips.
        const std::array<const char*, 4> stages { "DETECT", "SVF WAH", "BLEND", "OUT" };
        float chipX = 306.0f;
        constexpr float chipWidth = 80.0f;
        constexpr float chipGap = 12.0f;

        for (size_t i = 0; i < stages.size(); ++i)
        {
            const auto chip = juce::Rectangle<float> (chipX, 608.0f, chipWidth, 18.0f);
            drawSignalChip (g, chip, stages[i]);

            if (i + 1 < stages.size())
            {
                g.setColour (crimsonAccent.withAlpha (0.80f));
                g.setFont (font (7.5f, Weight::semibold, 0.0f));
                g.drawText (">", juce::Rectangle<float> (chip.getRight(), chip.getY(),
                                                          chipGap, chip.getHeight()),
                            juce::Justification::centred, false);
            }

            chipX += chipWidth + chipGap;
        }

        g.setColour (textMuted);
        g.setFont (font (5.8f, Weight::semibold, 0.12f));
        g.drawText ("ENVELOPE / LFO DUAL CONTROL SOURCE  //  LINKED STEREO DETECTOR",
                    306, 634, 356, 9, juce::Justification::centredLeft, false);
        g.drawText ("ZERO-LATENCY BYPASS  //  ANALOGUE-MODEL SVF",
                    306, 646, 356, 9, juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.2f, Weight::semibold, 0.16f));
        g.drawText ("BLS AUDIO  /  VST3  /  64-BIT FLOAT",
                    670, 652, 246, 9, juce::Justification::centredRight, false);
    }

    AutoWahAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    std::unique_ptr<ParamKnob> sensitivityKnob;
    std::unique_ptr<ParamKnob> attackKnob;
    std::unique_ptr<ParamKnob> releaseKnob;
    std::unique_ptr<ParamKnob> startKnob;
    std::unique_ptr<ParamKnob> sweepKnob;
    std::unique_ptr<ParamKnob> resonanceKnob;
    std::unique_ptr<ParamKnob> rateKnob;
    std::unique_ptr<ParamKnob> depthKnob;

    std::unique_ptr<GlassSlider> inputSlider;
    std::unique_ptr<GlassSlider> mixSlider;
    std::unique_ptr<GlassSlider> outputSlider;

    std::unique_ptr<ChoiceButton> envelopeButton;
    std::unique_ptr<ChoiceButton> lfoButton;
    std::unique_ptr<ChoiceButton> bandpassButton;
    std::unique_ptr<ChoiceButton> lowpassButton;
    std::unique_ptr<ChoiceButton> sineButton;
    std::unique_ptr<ChoiceButton> triangleButton;
    std::unique_ptr<ChoiceButton> squareButton;
    std::unique_ptr<ChoiceButton> sawButton;
    std::unique_ptr<TactileButton> bypassButton;

    std::unique_ptr<FilterDisplay> filterDisplay;
    std::unique_ptr<ModulationMeter> modulationMeter;
    std::unique_ptr<OutputMeter> outputMeter;
    std::unique_ptr<HeaderPillBar> pillBar;
    juce::Rectangle<float> mixerBayBounds;

    bool lastBypassed = false;
    int lastPreset = 0;
    bool lastDirty = false;
    int dimmedMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoWahEditorContent)
};

//======================================================================================
AutoWahAudioProcessorEditor::AutoWahAudioProcessorEditor (AutoWahAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    tooltipWindow.setLookAndFeel (&lookAndFeel);

    content = std::make_unique<AutoWahEditorContent> (processor);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (885, 540, 1770, 1080);

    if (auto* boundsConstrainer = getConstrainer())
        boundsConstrainer->setFixedAspectRatio (static_cast<double> (designWidth)
                                                / static_cast<double> (designHeight));

    setSize (designWidth, designHeight);
}

AutoWahAudioProcessorEditor::~AutoWahAudioProcessorEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void AutoWahAudioProcessorEditor::setTooltipsVisible (bool shouldBeVisible)
{
    tooltipWindow.setVisible (shouldBeVisible);
}

void AutoWahAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (studioSurfaceDark);
}

void AutoWahAudioProcessorEditor::resized()
{
    if (content == nullptr)
        return;

    const float scale = juce::jmin (static_cast<float> (getWidth()) / static_cast<float> (designWidth),
                                    static_cast<float> (getHeight()) / static_cast<float> (designHeight));
    const float offsetX = (static_cast<float> (getWidth()) - static_cast<float> (designWidth) * scale) * 0.5f;
    const float offsetY = (static_cast<float> (getHeight()) - static_cast<float> (designHeight) * scale) * 0.5f;

    content->setBounds (0, 0, designWidth, designHeight);
    content->setTransform (juce::AffineTransform::scale (scale).translated (offsetX, offsetY));
}
