#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AutoWahEngine.h"
#include "Presets.h"

#include <array>
#include <atomic>
#include <memory>

class AutoWahAudioProcessor final : public juce::AudioProcessor,
                                    private juce::AudioProcessorValueTreeState::Listener
{
public:
    AutoWahAudioProcessor();
    ~AutoWahAudioProcessor() override;

    using juce::AudioProcessor::processBlock;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Auto-Wah"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return autowah::numFactoryPresets; }
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Let the host's native bypass control drive the same parameter as the UI.
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
    const juce::AudioProcessorValueTreeState& getValueTreeState() const noexcept { return apvts; }

    void loadFactoryPreset (int index);

    int getCurrentPresetIndex() const noexcept { return currentPreset.load (std::memory_order_relaxed); }
    bool isPresetDirty() const noexcept { return presetDirty.load (std::memory_order_relaxed); }

    // --- Live telemetry (lock-free, safe from the message thread) ---
    float getInputLevel() const noexcept { return inputLevel.load (std::memory_order_relaxed); }
    float getOutputLevel() const noexcept { return outputLevel.load (std::memory_order_relaxed); }
    float getOutputLevelLeft() const noexcept { return outputLevelLeft.load (std::memory_order_relaxed); }
    float getOutputLevelRight() const noexcept { return outputLevelRight.load (std::memory_order_relaxed); }
    float getCutoffHz() const noexcept { return cutoffHz.load (std::memory_order_relaxed); }
    float getControlAmount() const noexcept { return controlAmount.load (std::memory_order_relaxed); }
    float getEnvelopeLevel() const noexcept { return envelopeLevel.load (std::memory_order_relaxed); }
    float getLfoValue() const noexcept { return lfoValue.load (std::memory_order_relaxed); }
    float getLfoPhase() const noexcept { return lfoPhase.load (std::memory_order_relaxed); }
    int getActiveMode() const noexcept { return activeMode.load (std::memory_order_relaxed); }

    // Lock-free audio history for the central display. Columns are published by
    // the audio thread and copied in chronological order by the message thread.
    static constexpr int waveformCapacity = 512;
    int copyWaveform (float* minColumns, float* maxColumns, int maxColumnCount) const noexcept;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void applyPreset (int index);
    void resetWaveformHistory() noexcept;
    void captureWaveform (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
    void pushWaveformColumn (float minimum, float maximum) noexcept;
    void updateParametersFromState() noexcept;

    juce::AudioProcessorValueTreeState apvts;
    autowah::dsp::WahEngine engine;

    std::atomic<float>* modeParam = nullptr;
    std::atomic<float>* filterTypeParam = nullptr;
    std::atomic<float>* lfoShapeParam = nullptr;
    std::atomic<float>* startFreqParam = nullptr;
    std::atomic<float>* sweepParam = nullptr;
    std::atomic<float>* resonanceParam = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* attackParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    std::atomic<float>* lfoRateParam = nullptr;
    std::atomic<float>* lfoDepthParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* bypassParam = nullptr;

    std::atomic<int> currentPreset { 0 };
    std::atomic<bool> presetDirty { false };
    std::atomic<bool> applyingPreset { false };

    std::atomic<float> inputLevel { 0.0f };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> outputLevelLeft { 0.0f };
    std::atomic<float> outputLevelRight { 0.0f };
    std::atomic<float> cutoffHz { 320.0f };
    std::atomic<float> controlAmount { 0.0f };
    std::atomic<float> envelopeLevel { 0.0f };
    std::atomic<float> lfoValue { 0.0f };
    std::atomic<float> lfoPhase { 0.0f };
    std::atomic<int> activeMode { 0 };

    std::array<std::atomic<float>, waveformCapacity> waveformMin {};
    std::array<std::atomic<float>, waveformCapacity> waveformMax {};
    std::atomic<juce::uint32> waveformWriteCount { 0 };
    int waveformSamplesPerColumn = 128;
    int waveformAccumulatedSamples = 0;
    float waveformAccumulatedMin = 0.0f;
    float waveformAccumulatedMax = 0.0f;

    double currentSampleRate = 48000.0;
    int preparedBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoWahAudioProcessor)
};
