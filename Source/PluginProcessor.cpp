#include "PluginProcessor.h"

#include "Parameters.h"
#include "PluginEditor.h"

#include <cmath>

using namespace autowah::params;

AutoWahAudioProcessor::AutoWahAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    modeParam        = apvts.getRawParameterValue (mode);
    filterTypeParam  = apvts.getRawParameterValue (filterType);
    lfoShapeParam    = apvts.getRawParameterValue (lfoShape);
    startFreqParam   = apvts.getRawParameterValue (startFreq);
    sweepParam       = apvts.getRawParameterValue (sweep);
    resonanceParam   = apvts.getRawParameterValue (resonance);
    sensitivityParam = apvts.getRawParameterValue (sensitivity);
    attackParam      = apvts.getRawParameterValue (attack);
    releaseParam     = apvts.getRawParameterValue (release);
    lfoRateParam     = apvts.getRawParameterValue (lfoRate);
    lfoDepthParam    = apvts.getRawParameterValue (lfoDepth);
    mixParam         = apvts.getRawParameterValue (mix);
    outputParam      = apvts.getRawParameterValue (output);
    bypassParam      = apvts.getRawParameterValue (bypass);

    for (auto* parameter : getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            apvts.addParameterListener (identified->paramID, this);

    engine.reset();
}

AutoWahAudioProcessor::~AutoWahAudioProcessor()
{
    for (auto* parameter : getParameters())
        if (auto* identified = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            apvts.removeParameterListener (identified->paramID, this);
}

void AutoWahAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    preparedBlockSize = juce::jmax (1, samplesPerBlock);

    engine.prepare (currentSampleRate);
    updateParametersFromState();
    engine.reset();

    waveformSamplesPerColumn = juce::jmax (8, juce::roundToInt (currentSampleRate / 375.0));
    resetWaveformHistory();

    inputLevel.store (0.0f);
    outputLevel.store (0.0f);
    outputLevelLeft.store (0.0f);
    outputLevelRight.store (0.0f);
    cutoffHz.store (startFreqParam != nullptr ? startFreqParam->load() : 320.0f);
    controlAmount.store (0.0f);
    envelopeLevel.store (0.0f);
    lfoValue.store (0.0f);
    lfoPhase.store (0.0f);
    activeMode.store (modeParam != nullptr ? juce::roundToInt (modeParam->load()) : 0);
}

void AutoWahAudioProcessor::releaseResources() {}

void AutoWahAudioProcessor::updateParametersFromState() noexcept
{
    if (modeParam == nullptr)
        return;

    autowah::dsp::WahEngine::Parameters parameters;
    parameters.mode = juce::roundToInt (modeParam->load());
    parameters.filterType = juce::roundToInt (filterTypeParam->load());
    parameters.lfoShape = juce::roundToInt (lfoShapeParam->load());
    parameters.startHz = startFreqParam->load();
    parameters.sweepOctaves = sweepParam->load();
    parameters.resonance = resonanceParam->load();
    parameters.sensitivityDb = sensitivityParam->load();
    parameters.attackMs = attackParam->load();
    parameters.releaseMs = releaseParam->load();
    parameters.lfoRateHz = lfoRateParam->load();
    parameters.lfoDepth = lfoDepthParam->load() * 0.01f;
    parameters.mix = mixParam->load() * 0.01f;
    parameters.outputDb = outputParam->load();
    parameters.bypass = bypassParam->load() > 0.5f;

    engine.setParameters (parameters);
}

void AutoWahAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    const float inputLeft = buffer.getMagnitude (0, 0, numSamples);
    const float inputRight = numChannels > 1 ? buffer.getMagnitude (1, 0, numSamples) : inputLeft;

    updateParametersFromState();

    float* left = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    engine.process (left, right, numSamples);

    for (int channel = 2; channel < numChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    const float outputLeft = buffer.getMagnitude (0, 0, numSamples);
    const float outputRight = numChannels > 1 ? buffer.getMagnitude (1, 0, numSamples) : outputLeft;

    const float decay = std::exp (-static_cast<float> (numSamples)
                                  / static_cast<float> (currentSampleRate * 0.18));

    const auto follow = [decay] (std::atomic<float>& target, float peak)
    {
        const float previous = target.load (std::memory_order_relaxed) * decay;
        target.store (juce::jmax (peak, previous), std::memory_order_relaxed);
    };

    follow (inputLevel, juce::jmax (inputLeft, inputRight));
    follow (outputLevelLeft, outputLeft);
    follow (outputLevelRight, outputRight);
    follow (outputLevel, juce::jmax (outputLeft, outputRight));

    cutoffHz.store (engine.getCutoffHz(), std::memory_order_relaxed);
    controlAmount.store (engine.getControlAmount(), std::memory_order_relaxed);
    envelopeLevel.store (engine.getEnvelopeLevel(), std::memory_order_relaxed);
    lfoValue.store (engine.getLfoValue(), std::memory_order_relaxed);
    lfoPhase.store (engine.getLfoPhase(), std::memory_order_relaxed);
    activeMode.store (modeParam != nullptr ? juce::roundToInt (modeParam->load()) : 0,
                      std::memory_order_relaxed);

    captureWaveform (buffer, numSamples);
}

juce::AudioProcessorEditor* AutoWahAudioProcessor::createEditor()
{
    return new AutoWahAudioProcessorEditor (*this);
}

int AutoWahAudioProcessor::getCurrentProgram()
{
    return currentPreset.load (std::memory_order_relaxed);
}

void AutoWahAudioProcessor::setCurrentProgram (int index)
{
    loadFactoryPreset (index);
}

const juce::String AutoWahAudioProcessor::getProgramName (int index)
{
    return autowah::getFactoryPreset (index).name;
}

void AutoWahAudioProcessor::changeProgramName (int, const juce::String&) {}

void AutoWahAudioProcessor::parameterChanged (const juce::String&, float)
{
    if (! applyingPreset.load (std::memory_order_relaxed))
        presetDirty.store (true, std::memory_order_relaxed);
}

void AutoWahAudioProcessor::loadFactoryPreset (int index)
{
    applyPreset (juce::jlimit (0, autowah::numFactoryPresets - 1, index));
}

void AutoWahAudioProcessor::applyPreset (int index)
{
    const auto& preset = autowah::getFactoryPreset (index);
    applyingPreset.store (true, std::memory_order_relaxed);

    const auto setValue = [this] (const char* parameterID, float value)
    {
        if (auto* parameter = apvts.getParameter (parameterID))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    setValue (mode, static_cast<float> (preset.mode));
    setValue (filterType, static_cast<float> (preset.filterType));
    setValue (lfoShape, static_cast<float> (preset.lfoShape));
    setValue (startFreq, preset.startFreq);
    setValue (sweep, preset.sweep);
    setValue (resonance, preset.resonance);
    setValue (sensitivity, preset.sensitivity);
    setValue (attack, preset.attack);
    setValue (release, preset.release);
    setValue (lfoRate, preset.lfoRate);
    setValue (lfoDepth, preset.lfoDepth);
    setValue (mix, preset.mix);
    setValue (output, preset.output);
    setValue (bypass, 0.0f);

    applyingPreset.store (false, std::memory_order_relaxed);

    currentPreset.store (index, std::memory_order_relaxed);
    presetDirty.store (false, std::memory_order_relaxed);
}

void AutoWahAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", currentPreset.load (std::memory_order_relaxed), nullptr);
    state.setProperty ("presetDirty", presetDirty.load (std::memory_order_relaxed), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AutoWahAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    applyingPreset.store (true, std::memory_order_relaxed);
    apvts.replaceState (tree);
    applyingPreset.store (false, std::memory_order_relaxed);

    currentPreset.store (static_cast<int> (tree.getProperty ("presetIndex", 0)),
                         std::memory_order_relaxed);
    presetDirty.store (static_cast<bool> (tree.getProperty ("presetDirty", false)),
                       std::memory_order_relaxed);
}

juce::AudioProcessorParameter* AutoWahAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (bypass);
}

bool AutoWahAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    const auto outputSet = layouts.getMainOutputChannelSet();
    return outputSet == juce::AudioChannelSet::mono()
        || outputSet == juce::AudioChannelSet::stereo();
}

void AutoWahAudioProcessor::resetWaveformHistory() noexcept
{
    for (int i = 0; i < waveformCapacity; ++i)
    {
        waveformMin[static_cast<size_t> (i)].store (0.0f, std::memory_order_relaxed);
        waveformMax[static_cast<size_t> (i)].store (0.0f, std::memory_order_relaxed);
    }

    waveformWriteCount.store (0, std::memory_order_relaxed);
    waveformAccumulatedSamples = 0;
    waveformAccumulatedMin = 0.0f;
    waveformAccumulatedMax = 0.0f;
}

void AutoWahAudioProcessor::pushWaveformColumn (float minimum, float maximum) noexcept
{
    const auto writeIndex = waveformWriteCount.load (std::memory_order_relaxed) % waveformCapacity;
    waveformMin[static_cast<size_t> (writeIndex)].store (minimum, std::memory_order_relaxed);
    waveformMax[static_cast<size_t> (writeIndex)].store (maximum, std::memory_order_relaxed);
    waveformWriteCount.fetch_add (1, std::memory_order_release);
}

void AutoWahAudioProcessor::captureWaveform (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (buffer.getNumChannels() <= 0)
        return;

    const float* left = buffer.getReadPointer (0);
    const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sample = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];

        waveformAccumulatedMin = juce::jmin (waveformAccumulatedMin, sample);
        waveformAccumulatedMax = juce::jmax (waveformAccumulatedMax, sample);

        if (++waveformAccumulatedSamples >= waveformSamplesPerColumn)
        {
            pushWaveformColumn (waveformAccumulatedMin, waveformAccumulatedMax);
            waveformAccumulatedSamples = 0;
            waveformAccumulatedMin = 0.0f;
            waveformAccumulatedMax = 0.0f;
        }
    }
}

int AutoWahAudioProcessor::copyWaveform (float* minColumns, float* maxColumns, int maxColumnCount) const noexcept
{
    if (minColumns == nullptr || maxColumns == nullptr || maxColumnCount <= 0)
        return 0;

    const auto written = waveformWriteCount.load (std::memory_order_acquire);
    const int available = juce::jmin (static_cast<int> (juce::jmin<juce::uint32> (
                                          written, static_cast<juce::uint32> (waveformCapacity))),
                                      maxColumnCount);

    for (int i = 0; i < available; ++i)
    {
        const auto index = (written - static_cast<juce::uint32> (available)
                            + static_cast<juce::uint32> (i)) % waveformCapacity;

        minColumns[i] = waveformMin[static_cast<size_t> (index)].load (std::memory_order_relaxed);
        maxColumns[i] = waveformMax[static_cast<size_t> (index)].load (std::memory_order_relaxed);
    }

    return available;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoWahAudioProcessor();
}
