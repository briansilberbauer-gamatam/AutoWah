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

        mixSlider = std::make_unique<GlassSlider> (apvts, mix, "MIX", formatPercent);
        outputSlider = std::make_unique<GlassSlider> (apvts, output, "OUTPUT", formatDb);
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

        paintBrandHeader (g);

        g.setColour (obsidianGrid.withAlpha (0.90f));
        g.fillRect (52.0f, 97.0f, 1076.0f, 1.0f);

        drawEngravedLabel (g, { 52.0f, 102.0f, 244.0f, 26.0f },
                           "ENVELOPE DETECTOR", "PROGRAM-DEPENDENT SWEEP");
        drawEngravedLabel (g, { 860.0f, 102.0f, 140.0f, 26.0f },
                           "LFO GENERATOR", "CYCLIC SWEEP");

        paintAnnotations (g);
        paintFooter (g);
    }

    void resized() override
    {
        pillBar->setBounds (386, 44, 408, 36);

        envelopeButton->setBounds (828, 48, 94, 30);
        lfoButton->setBounds (930, 48, 94, 30);
        bypassButton->setBounds (1032, 48, 96, 30);

        filterDisplay->setBounds (328, 110, 524, 320);

        sensitivityKnob->setBounds (54, 128, 240, 200);
        attackKnob->setBounds (62, 342, 108, 128);
        releaseKnob->setBounds (178, 342, 108, 128);
        modulationMeter->setBounds (60, 478, 228, 68);

        startKnob->setBounds (324, 442, 150, 124);
        sweepKnob->setBounds (482, 442, 150, 124);
        resonanceKnob->setBounds (640, 442, 150, 124);

        bandpassButton->setBounds (798, 460, 54, 26);
        lowpassButton->setBounds (798, 492, 54, 26);

        rateKnob->setBounds (860, 128, 140, 156);
        depthKnob->setBounds (860, 294, 140, 140);

        sineButton->setBounds (860, 458, 66, 28);
        triangleButton->setBounds (934, 458, 66, 28);
        squareButton->setBounds (860, 490, 66, 28);
        sawButton->setBounds (934, 490, 66, 28);

        mixSlider->setBounds (1008, 126, 58, 430);
        outputSlider->setBounds (1070, 126, 58, 430);

        outputMeter->setBounds (780, 608, 348, 42);
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
        g.drawText ("BLS AUDIO  /  SERIES WAH", 52, 36, 320, 9,
                    juce::Justification::centredLeft, false);

        g.setColour (textPrimary);
        g.setFont (font (24.0f, Weight::black, 0.035f));
        g.drawText ("AUTOWAH", 52, 44, 360, 32,
                    juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.6f, Weight::regular, 0.14f));
        g.drawText ("DUAL-SOURCE DYNAMIC FILTER  /  ENVELOPE + LFO",
                    52, 78, 380, 9, juce::Justification::centredLeft, false);

        g.setColour (crimsonAccent);
        g.fillRect (52.0f, 90.0f, 54.0f, 1.4f);

        // Control-source micro label above the mode selector.
        g.setColour (textMuted);
        g.setFont (font (5.8f, Weight::semibold, 0.18f));
        g.drawText ("CONTROL SOURCE", 828, 34, 196, 9,
                    juce::Justification::centred, false);
    }

    void paintAnnotations (juce::Graphics& g)
    {
        g.setFont (font (5.7f, Weight::regular, 0.10f));
        g.setColour (textMuted);

        g.drawText ("SENSITIVITY DRIVES THE DETECTOR",
                    54, 552, 244, 9, juce::Justification::centredLeft, false);
        g.drawText ("FAST ATTACK + LONG RELEASE = FUNK",
                    54, 563, 244, 9, juce::Justification::centredLeft, false);

        g.drawText ("FILTER TYPE", 798, 444, 54, 9,
                    juce::Justification::centred, false);

        g.setColour (textMuted);
        g.setFont (font (5.6f, Weight::semibold, 0.15f));
        g.drawText ("SHAPE", 860, 440, 140, 10,
                    juce::Justification::centredLeft, false);

        g.setFont (font (5.7f, Weight::regular, 0.10f));
        g.drawText ("FREE-RUNNING", 860, 530, 140, 9,
                    juce::Justification::centredLeft, false);
        g.drawText ("RESETS ON MODE ENTRY", 860, 541, 140, 9,
                    juce::Justification::centredLeft, false);

        // Engraved note under the filter core.
        g.setColour (textMuted.withAlpha (0.85f));
        g.setFont (font (5.7f, Weight::regular, 0.10f));
        g.drawText ("LOG-SWEPT 12 dB STATE VARIABLE FILTER",
                    324, 570, 528, 9, juce::Justification::centredLeft, false);
    }

    void paintFooter (juce::Graphics& g)
    {
        g.setColour (obsidianGrid);
        g.fillRect (52.0f, 598.0f, 1076.0f, 1.0f);

        g.setColour (textMuted);
        g.setFont (font (6.2f, Weight::semibold, 0.16f));
        g.drawText ("PRESET CHARACTER", 52, 606, 268, 9,
                    juce::Justification::centredLeft, false);

        const int preset = processor.getCurrentPresetIndex();
        const bool dirty = processor.isPresetDirty();

        g.setColour (textSecondary);
        g.setFont (font (7.4f, Weight::regular, 0.06f));
        g.drawFittedText (autowah::getFactoryPresetCharacter (preset),
                          juce::Rectangle<int> (52, 618, 268, 24),
                          juce::Justification::centredLeft, 2);

        drawCrimsonDot (g, { 55.0f, 660.0f }, 2.0f);

        g.setColour (dirty ? crimsonBright : textMuted);
        g.setFont (font (6.2f, Weight::semibold, 0.14f));
        g.drawText (dirty ? "EDITED / UNSAVED" : "FACTORY / CLEAN",
                    64, 655, 240, 9, juce::Justification::centredLeft, false);

        // Signal-path chips.
        const std::array<const char*, 4> stages { "DETECT", "SVF WAH", "BLEND", "OUT" };
        float chipX = 338.0f;
        constexpr float chipWidth = 80.0f;
        constexpr float chipGap = 12.0f;

        for (size_t i = 0; i < stages.size(); ++i)
        {
            const auto chip = juce::Rectangle<float> (chipX, 610.0f, chipWidth, 18.0f);
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
                    338, 636, 420, 9, juce::Justification::centredLeft, false);
        g.drawText ("ZERO-LATENCY BYPASS  //  ANALOGUE-MODEL SVF",
                    338, 648, 420, 9, juce::Justification::centredLeft, false);

        g.setColour (textSecondary);
        g.setFont (font (6.2f, Weight::semibold, 0.16f));
        g.drawText ("BLS AUDIO  /  VST3  /  64-BIT FLOAT",
                    780, 656, 348, 9, juce::Justification::centredRight, false);
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
