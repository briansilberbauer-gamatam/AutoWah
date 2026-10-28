#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/AutoWahEngine.h"

#include <array>
#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const char* message)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << message << '\n';

    if (! condition)
        ++failures;
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getReadPointer (channel)[i]))
                return false;

    return true;
}

void setParameter (AutoWahAudioProcessor& processor, const char* id, float value)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}

float getParameter (AutoWahAudioProcessor& processor, const char* id)
{
    if (auto* parameter = processor.getValueTreeState().getParameter (id))
        return parameter->convertFrom0to1 (parameter->getValue());

    return 0.0f;
}

struct ToneResult
{
    float rms = 0.0f;
    float finalCutoff = 0.0f;
    float minCutoff = 0.0f;
    float maxCutoff = 0.0f;
};

ToneResult renderTone (autowah::dsp::WahEngine& engine, double sampleRate,
                       float frequency, float amplitude, float seconds,
                       juce::int64& sampleCounter, bool useSilence = false)
{
    constexpr int blockSize = 512;
    juce::AudioBuffer<float> buffer (2, blockSize);

    const int numBlocks = juce::jmax (1, juce::roundToInt (sampleRate * seconds / blockSize));
    ToneResult result;
    result.minCutoff = 1.0e9f;
    result.maxCutoff = 0.0f;

    for (int block = 0; block < numBlocks; ++block)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            const auto n = static_cast<double> (sampleCounter + i);
            const float sample = useSilence
                ? 0.0f
                : amplitude * static_cast<float> (std::sin (juce::MathConstants<double>::twoPi
                                                            * frequency * n / sampleRate));
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, sample);
        }

        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), blockSize);

        const float cutoff = engine.getCutoffHz();
        result.minCutoff = juce::jmin (result.minCutoff, cutoff);
        result.maxCutoff = juce::jmax (result.maxCutoff, cutoff);
        result.rms = buffer.getRMSLevel (0, blockSize / 2, blockSize / 2);

        sampleCounter += blockSize;
    }

    result.finalCutoff = engine.getCutoffHz();
    return result;
}

autowah::dsp::WahEngine::Parameters baseParameters()
{
    autowah::dsp::WahEngine::Parameters parameters;
    parameters.mode = 0;
    parameters.filterType = 0;
    parameters.lfoShape = 0;
    parameters.startHz = 200.0f;
    parameters.sweepOctaves = 3.0f;
    parameters.resonance = 4.0f;
    parameters.sensitivityDb = 12.0f;
    parameters.attackMs = 2.0f;
    parameters.releaseMs = 100.0f;
    parameters.lfoRateHz = 2.0f;
    parameters.lfoDepth = 1.0f;
    parameters.mix = 1.0f;
    parameters.outputDb = 0.0f;
    return parameters;
}

void testSvf()
{
    const auto coefficients = autowah::dsp::makeSvfCoefficients (1000.0f, 5.0f, 48000.0);
    check (std::isfinite (coefficients.a1) && coefficients.a1 > 0.0f
               && coefficients.a1 < 1.0f,
           "SVF coefficients are finite and inside the stable range");
    check (std::abs (coefficients.k - 0.2f) < 1.0e-5f,
           "SVF damping equals 1/Q");

    autowah::dsp::TptSvf filter;
    filter.setCoefficients (coefficients);

    // A 1 kHz tone at the 1 kHz cutoff should resonate well above unity on the
    // raw bandpass tap (the engine applies its own resonance makeup afterwards).
    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float input = 0.5f * std::sin (juce::MathConstants<float>::twoPi * 1000.0f
                                             * static_cast<float> (i) / 48000.0f);
        float band = 0.0f;
        float low = 0.0f;
        filter.process (input, band, low);
        peak = juce::jmax (peak, std::abs (band));
    }

    check (peak > 1.2f, "resonant bandpass exhibits real gain at the cutoff");

    // Extreme settings must stay bounded.
    const auto extreme = autowah::dsp::makeSvfCoefficients (18000.0f, 14.0f, 48000.0);
    filter.reset();
    filter.setCoefficients (extreme);
    bool finite = true;
    float maxAbs = 0.0f;

    for (int i = 0; i < 24000; ++i)
    {
        const float input = (i % 2 == 0) ? 0.9f : -0.9f;
        float band = 0.0f;
        float low = 0.0f;
        filter.process (input, band, low);
        finite = finite && std::isfinite (band) && std::isfinite (low);
        maxAbs = juce::jmax (maxAbs, std::abs (band));
    }

    check (finite && maxAbs < 200.0f, "extremely resonant SVF stays finite and bounded");
}

void testEnvelopeMode()
{
    autowah::dsp::WahEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (baseParameters());

    juce::int64 counter = 0;

    // Hot input should open the filter, then release back toward the start.
    auto open = renderTone (engine, 48000.0, 220.0f, 0.5f, 0.6f, counter);
    check (open.finalCutoff > 700.0f,
           "envelope detector opens the filter on hot input");

    auto released = renderTone (engine, 48000.0, 220.0f, 0.5f, 1.2f, counter, true);
    check (released.finalCutoff < 320.0f,
           "envelope detector releases back toward the start frequency on silence");

    // Loud playing must sit far above quiet playing.
    auto quiet = renderTone (engine, 48000.0, 220.0f, 0.008f, 0.5f, counter);
    check (quiet.finalCutoff < open.finalCutoff * 0.5f,
           "quiet input produces a much lower envelope-controlled cutoff");

    // Sensitivity scales the same input into a higher cutoff.
    auto insensitiveParameters = baseParameters();
    insensitiveParameters.sensitivityDb = -18.0f;
    engine.setParameters (insensitiveParameters);
    auto insensitive = renderTone (engine, 48000.0, 220.0f, 0.35f, 0.5f, counter);

    auto sensitiveParameters = baseParameters();
    sensitiveParameters.sensitivityDb = 18.0f;
    engine.setParameters (sensitiveParameters);
    auto sensitive = renderTone (engine, 48000.0, 220.0f, 0.35f, 0.5f, counter);

    check (sensitive.finalCutoff > insensitive.finalCutoff * 1.5f,
           "sensitivity scales the envelope-controlled cutoff");

    // Attack and release must shape the ballistics in opposite directions.
    auto slowAttackParameters = baseParameters();
    slowAttackParameters.attackMs = 80.0f;
    engine.setParameters (slowAttackParameters);
    engine.reset();
    auto slowAttack = renderTone (engine, 48000.0, 220.0f, 0.5f, 0.03f, counter);

    auto fastAttackParameters = baseParameters();
    fastAttackParameters.attackMs = 0.5f;
    engine.setParameters (fastAttackParameters);
    engine.reset();
    auto fastAttack = renderTone (engine, 48000.0, 220.0f, 0.5f, 0.03f, counter);

    check (fastAttack.finalCutoff > slowAttack.finalCutoff,
           "fast attack opens the filter sooner than slow attack");
}

void testLfoMode()
{
    autowah::dsp::WahEngine engine;
    engine.prepare (48000.0);

    auto parameters = baseParameters();
    parameters.mode = 1;
    parameters.lfoRateHz = 2.0f;
    parameters.lfoDepth = 1.0f;
    engine.setParameters (parameters);

    juce::int64 counter = 0;
    auto sweep = renderTone (engine, 48000.0, 220.0f, 0.25f, 1.0f, counter);

    check (sweep.maxCutoff / juce::jmax (1.0f, sweep.minCutoff) > 4.0f,
           "LFO mode sweeps the cutoff over a wide range");
    check (sweep.maxCutoff > 1000.0f && sweep.minCutoff < 320.0f,
           "sine LFO reaches both ends of the programmed range");

    // LFO must keep running with silent input.
    auto silent = renderTone (engine, 48000.0, 220.0f, 0.0f, 1.0f, counter, true);
    check (silent.maxCutoff / juce::jmax (1.0f, silent.minCutoff) > 4.0f,
           "LFO modulation continues without input signal");

    // Depth at zero parks the filter at its start frequency.
    parameters.lfoDepth = 0.0f;
    engine.setParameters (parameters);
    auto parked = renderTone (engine, 48000.0, 220.0f, 0.25f, 0.4f, counter);
    check (std::abs (parked.finalCutoff - 200.0f) < 12.0f,
           "zero LFO depth parks the cutoff at the start frequency");

    // Square wave must produce distinct high and low plateaus.
    parameters.lfoDepth = 1.0f;
    parameters.lfoShape = 2;
    parameters.lfoRateHz = 1.0f;
    engine.reset();
    engine.setParameters (parameters);
    counter = 0;

    auto highPhase = renderTone (engine, 48000.0, 220.0f, 0.25f, 0.4f, counter);
    auto lowPhase = renderTone (engine, 48000.0, 220.0f, 0.25f, 0.45f, counter);

    check (highPhase.finalCutoff > 1100.0f,
           "square LFO opens the filter during its high plateau");
    check (lowPhase.finalCutoff < 320.0f,
           "square LFO closes the filter during its low plateau");

    // All shapes must remain finite and modulating.
    for (int shape = 0; shape < 4; ++shape)
    {
        parameters.lfoShape = shape;
        engine.reset();
        engine.setParameters (parameters);
        auto result = renderTone (engine, 48000.0, 220.0f, 0.25f, 0.6f, counter);
        const bool modulating = result.maxCutoff / juce::jmax (1.0f, result.minCutoff) > 2.0f;
        check (std::isfinite (result.finalCutoff) && modulating,
               shape == 0 ? "sine LFO shape is valid"
               : shape == 1 ? "triangle LFO shape is valid"
               : shape == 2 ? "square LFO shape is valid"
                            : "saw LFO shape is valid");
    }
}

void testFilterAttenuation()
{
    autowah::dsp::WahEngine engine;
    engine.prepare (48000.0);

    auto parameters = baseParameters();
    parameters.mode = 1;
    parameters.lfoDepth = 0.0f;
    parameters.sweepOctaves = 0.0f;
    parameters.startHz = 1000.0f;
    parameters.resonance = 8.0f;
    parameters.sensitivityDb = 0.0f;
    parameters.filterType = 0;
    engine.setParameters (parameters);

    juce::int64 counter = 0;
    renderTone (engine, 48000.0, 100.0f, 0.5f, 0.3f, counter); // warm up
    auto lowTone = renderTone (engine, 48000.0, 100.0f, 0.5f, 0.3f, counter);
    auto centreTone = renderTone (engine, 48000.0, 1000.0f, 0.5f, 0.3f, counter);

    check (lowTone.rms < centreTone.rms * 0.2f,
           "bandpass rejects content below the tuned frequency");

    // Lowpass tap: pass the low tone, reject the high tone.
    parameters.filterType = 1;
    parameters.resonance = 1.5f;
    engine.setParameters (parameters);

    renderTone (engine, 48000.0, 100.0f, 0.5f, 0.3f, counter);
    auto lowpassedLow = renderTone (engine, 48000.0, 100.0f, 0.5f, 0.3f, counter);
    renderTone (engine, 48000.0, 8000.0f, 0.5f, 0.3f, counter);
    auto lowpassedHigh = renderTone (engine, 48000.0, 8000.0f, 0.5f, 0.3f, counter);

    check (lowpassedLow.rms > lowpassedHigh.rms * 4.0f,
           "lowpass tap passes lows and rejects highs");
}

void testInputTrim()
{
    autowah::dsp::WahEngine engine;
    engine.prepare (48000.0);

    // With the blend fully dry the input trim is a pure gain stage, so +12 dB
    // must deliver four times the amplitude of unity trim.
    auto parameters = baseParameters();
    parameters.mix = 0.0f;
    parameters.inputDb = 0.0f;
    engine.setParameters (parameters);

    juce::int64 counter = 0;
    renderTone (engine, 48000.0, 440.0f, 0.1f, 0.25f, counter);
    const auto unity = renderTone (engine, 48000.0, 440.0f, 0.1f, 0.25f, counter);

    parameters.inputDb = 12.0f;
    engine.setParameters (parameters);
    renderTone (engine, 48000.0, 440.0f, 0.1f, 0.25f, counter);
    const auto boosted = renderTone (engine, 48000.0, 440.0f, 0.1f, 0.25f, counter);

    check (boosted.rms > unity.rms * 3.6f && boosted.rms < unity.rms * 4.4f,
           "input trim applies a calibrated gain to the dry path");
    check (std::isfinite (boosted.rms) && boosted.rms > 0.0f,
           "input trim stays finite and audible");
}

void testModeSwitchAndBypass()
{
    autowah::dsp::WahEngine engine;
    engine.prepare (48000.0);

    auto parameters = baseParameters();
    parameters.mode = 0;
    engine.setParameters (parameters);

    juce::int64 counter = 0;
    renderTone (engine, 48000.0, 220.0f, 0.4f, 0.5f, counter);

    parameters.mode = 1;
    parameters.lfoRateHz = 3.0f;
    engine.setParameters (parameters);
    const float envelopeControlBefore = engine.getControlAmount();
    auto switched = renderTone (engine, 48000.0, 220.0f, 0.4f, 0.5f, counter);
    const float lfoControlAfter = engine.getControlAmount();

    check (std::isfinite (switched.finalCutoff) && switched.rms > 0.0f,
           "switching from envelope to LFO keeps audio finite and audible");
    check (std::abs (lfoControlAfter - envelopeControlBefore) > 0.05f
               || switched.maxCutoff > switched.minCutoff * 2.0f,
           "switching control source changes the modulation behaviour");

    // Bypass must return the untouched dry input after its crossfade.
    auto bypassParameters = baseParameters();
    bypassParameters.bypass = true;
    engine.setParameters (bypassParameters);
    renderTone (engine, 48000.0, 220.0f, 0.4f, 0.5f, counter);

    juce::AudioBuffer<float> buffer (2, 512);
    juce::AudioBuffer<float> reference (2, 512);

    for (int i = 0; i < 512; ++i)
    {
        const float sample = 0.3f * std::sin (juce::MathConstants<float>::twoPi * 440.0f
                                              * static_cast<float> (i) / 48000.0f);
        buffer.setSample (0, i, sample);
        buffer.setSample (1, i, sample);
        reference.setSample (0, i, sample);
        reference.setSample (1, i, sample);
    }

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), 512);

    const float maxDifference = std::abs (buffer.getSample (0, 511) - reference.getSample (0, 511));
    check (maxDifference < 1.0e-5f, "bypass returns the exact dry signal");
}

void testProcessor()
{
    AutoWahAudioProcessor processor;

    check (processor.getParameters().size() == autowah::params::numParameters,
           "processor exposes all 15 parameters");
    check (processor.getNumPrograms() == autowah::numFactoryPresets,
           "ten factory programs are available");
    check (processor.getCurrentProgram() == 0, "processor starts on the first factory program");
    check (! processor.isPresetDirty(), "initial factory state is clean");

    // Every declared parameter must exist with a usable range.
    bool allParametersValid = true;
    for (const auto* id : { autowah::params::mode, autowah::params::filterType,
                            autowah::params::lfoShape, autowah::params::startFreq,
                            autowah::params::sweep, autowah::params::resonance,
                            autowah::params::sensitivity, autowah::params::attack,
                            autowah::params::release, autowah::params::lfoRate,
                            autowah::params::lfoDepth, autowah::params::input,
                            autowah::params::mix, autowah::params::output,
                            autowah::params::bypass })
    {
        auto* parameter = processor.getValueTreeState().getParameter (id);
        allParametersValid = allParametersValid && parameter != nullptr
                             && parameter->getNormalisableRange().end > parameter->getNormalisableRange().start;
    }

    check (allParametersValid, "all parameter identifiers resolve to valid ranges");

    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    check (processor.getLatencySamples() == 0, "zero-latency processing requires no host compensation");

    // Load the lowpass LFO preset and confirm the switch reached the parameters.
    processor.setCurrentProgram (8);
    check (processor.getCurrentProgram() == 8, "factory program can be selected");
    check (juce::roundToInt (getParameter (processor, autowah::params::mode)) == 1,
           "LFO preset loads the LFO control source");
    check (juce::roundToInt (getParameter (processor, autowah::params::filterType)) == 1,
           "lowpass preset loads the lowpass filter tap");
    check (! processor.isPresetDirty(), "loading a preset leaves the state clean");

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    juce::int64 sampleCounter = 0;
    bool finite = true;
    float peak = 0.0f;

    for (int block = 0; block < 90; ++block)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto n = static_cast<double> (sampleCounter + i);
            const auto fundamental = juce::MathConstants<double>::twoPi * 110.0 * n / 48000.0;
            const float sample = 0.4f * static_cast<float> (
                std::sin (fundamental) + 0.35 * std::sin (2.0 * fundamental)
                + 0.18 * std::sin (3.0 * fundamental));
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, sample * 0.92f);
        }

        sampleCounter += buffer.getNumSamples();
        processor.processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
        peak = juce::jmax (peak, buffer.getMagnitude (0, 0, buffer.getNumSamples()));
    }

    check (finite, "real-time processing produces finite output");
    check (peak > 0.01f, "processed signal remains audible");
    check (processor.getCutoffHz() >= 40.0f && processor.getCutoffHz() <= 20000.0f,
           "cutoff telemetry stays inside the audio band");
    check (processor.getEnvelopeLevel() > 0.05f,
           "envelope telemetry reports live detector activity");
    check (processor.getOutputLevel() > 0.001f, "output meter telemetry is live");
    check (processor.getActiveMode() == 1, "active-mode telemetry follows the control source");

    std::array<float, 64> waveformMin {};
    std::array<float, 64> waveformMax {};
    const int columns = processor.copyWaveform (waveformMin.data(), waveformMax.data(), 64);

    bool ordered = columns == 64;
    bool live = false;

    for (int i = 0; i < columns; ++i)
    {
        ordered = ordered && waveformMax[static_cast<size_t> (i)]
                                 >= waveformMin[static_cast<size_t> (i)];
        live = live || waveformMax[static_cast<size_t> (i)] > 0.001f
                    || waveformMin[static_cast<size_t> (i)] < -0.001f;
    }

    check (ordered, "waveform telemetry publishes ordered min/max columns");
    check (live, "waveform telemetry contains live audio");

    // Switch back to the envelope control source.
    setParameter (processor, autowah::params::mode, 0.0f);
    check (processor.isPresetDirty(), "editing a parameter marks the preset edited");

    for (int block = 0; block < 30; ++block)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const auto n = static_cast<double> (sampleCounter + i);
            buffer.setSample (0, i, 0.35f * static_cast<float> (
                std::sin (juce::MathConstants<double>::twoPi * 110.0 * n / 48000.0)));
            buffer.setSample (1, i, buffer.getSample (0, i));
        }

        sampleCounter += buffer.getNumSamples();
        processor.processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
    }

    check (finite, "envelope mode remains finite after a live control-source switch");
    check (processor.getActiveMode() == 0, "telemetry reports the envelope control source");

    // Bypass must stay finite and must not add latency.
    setParameter (processor, autowah::params::bypass, 1.0f);
    finite = true;

    for (int block = 0; block < 12; ++block)
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            buffer.setSample (0, i, buffer.getSample (1, i) * 0.5f);

        processor.processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
    }

    check (finite, "bypass produces finite output");
    check (processor.getLatencySamples() == 0, "bypass does not alter reported latency");

    // State round-trip.
    setParameter (processor, autowah::params::bypass, 0.0f);
    setParameter (processor, autowah::params::startFreq, 777.0f);
    setParameter (processor, autowah::params::mode, 1.0f);

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    check (state.getSize() > 0, "full processor state serialises");

    AutoWahAudioProcessor restored;
    restored.setPlayConfigDetails (2, 2, 48000.0, 512);
    restored.prepareToPlay (48000.0, 512);
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    check (std::abs (getParameter (restored, autowah::params::startFreq) - 777.0f) < 0.5f,
           "start frequency is restored from serialised state");
    check (juce::roundToInt (getParameter (restored, autowah::params::mode)) == 1,
           "control source is restored from serialised state");

    // Mono operation.
    AutoWahAudioProcessor monoProcessor;
    monoProcessor.setPlayConfigDetails (1, 1, 48000.0, 256);
    monoProcessor.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> monoBuffer (1, 256);
    for (int i = 0; i < monoBuffer.getNumSamples(); ++i)
        monoBuffer.setSample (0, i, 0.3f * std::sin (juce::MathConstants<float>::twoPi * 220.0f
                                                     * static_cast<float> (i) / 48000.0f));

    monoProcessor.processBlock (monoBuffer, midi);
    check (allFinite (monoBuffer), "mono bus processing remains finite");

    // High sample rate preparation.
    AutoWahAudioProcessor highRateProcessor;
    highRateProcessor.setPlayConfigDetails (2, 2, 96000.0, 512);
    highRateProcessor.prepareToPlay (96000.0, 512);
    juce::AudioBuffer<float> highRateBuffer (2, 512);
    highRateBuffer.clear();
    highRateBuffer.setSample (0, 0, 0.5f);
    highRateBuffer.setSample (1, 0, 0.5f);
    highRateProcessor.processBlock (highRateBuffer, midi);
    check (allFinite (highRateBuffer), "96 kHz preparation remains finite");

    processor.releaseResources();
    restored.releaseResources();
    monoProcessor.releaseResources();
    highRateProcessor.releaseResources();
}

void testPresets()
{
    AutoWahAudioProcessor processor;
    processor.setPlayConfigDetails (2, 2, 48000.0, 512);
    processor.prepareToPlay (48000.0, 512);

    bool bothSources = false;
    bool allValid = true;

    for (int index = 0; index < processor.getNumPrograms(); ++index)
    {
        processor.setCurrentProgram (index);
        const auto& preset = autowah::getFactoryPreset (index);

        allValid = allValid && processor.getProgramName (index).isNotEmpty()
                   && processor.getCurrentProgram() == index;

        const int mode = juce::roundToInt (getParameter (processor, autowah::params::mode));
        bothSources = bothSources || (mode == static_cast<int> (autowah::params::Mode::envelope));
        bothSources = bothSources || (mode == static_cast<int> (autowah::params::Mode::lfo));

        // Every preset must stay inside its declared parameter ranges.
        allValid = allValid
                   && std::abs (getParameter (processor, autowah::params::startFreq) - preset.startFreq)
                          < 1.0f
                   && std::abs (getParameter (processor, autowah::params::sweep) - preset.sweep) < 0.05f;
    }

    check (bothSources, "factory bank covers both envelope and LFO control sources");
    check (allValid, "all factory presets load their declared values");
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    std::cout << "Auto-Wah offline DSP tests\n\n";

    testSvf();
    testEnvelopeMode();
    testLfoMode();
    testFilterAttenuation();
    testInputTrim();
    testModeSwitchAndBypass();
    testProcessor();
    testPresets();

    std::cout << (failures == 0 ? "\nAll DSP tests passed.\n" : "\nDSP tests failed.\n");
    return failures == 0 ? 0 : 1;
}
