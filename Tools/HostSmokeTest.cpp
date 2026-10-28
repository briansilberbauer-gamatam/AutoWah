#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

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

void fillSignal (juce::AudioBuffer<float>& buffer, int offset)
{
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto n = static_cast<double> (offset + i);
        const auto fundamental = juce::MathConstants<double>::twoPi * 98.0 * n / 48000.0;
        const float pluck = 0.55f + 0.45f * static_cast<float> (
            std::sin (juce::MathConstants<double>::twoPi * 1.6 * n / 48000.0));
        const float left = pluck * static_cast<float> (
            0.34 * std::sin (fundamental) + 0.10 * std::sin (2.0 * fundamental)
            + 0.05 * std::sin (3.0 * fundamental));
        const float right = pluck * static_cast<float> (
            0.31 * std::sin (fundamental) + 0.08 * std::sin (2.0 * fundamental));

        buffer.setSample (0, i, left);
        buffer.setSample (1, i, right);
    }
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getReadPointer (channel)[i]))
                return false;

    return true;
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    if (argc < 2)
    {
        std::cerr << "Usage: AutoWahHostSmokeTest /path/to/Auto-Wah.vst3\n";
        return 2;
    }

    const juce::File pluginFile { juce::String (argv[1]) };

    if (! pluginFile.exists())
    {
        std::cerr << "Plugin bundle not found: " << pluginFile.getFullPathName() << '\n';
        return 2;
    }

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    juce::OwnedArray<juce::PluginDescription> descriptions;

    for (auto* format : formatManager.getFormats())
        if (auto* vst3 = dynamic_cast<juce::VST3PluginFormat*> (format))
            vst3->findAllTypesForFile (descriptions, pluginFile.getFullPathName());

    check (! descriptions.isEmpty(), "VST3 scanner discovers the plugin");

    if (descriptions.isEmpty())
        return 1;

    auto& description = *descriptions[0];

    std::cout << "  INFO  " << description.name << " / " << description.manufacturerName
              << " / " << description.version << '\n';

    juce::String error;
    auto instance = formatManager.createPluginInstance (description, 48000.0, 512, error);

    check (instance != nullptr, "plugin instance can be created");

    if (instance == nullptr)
    {
        std::cerr << "  ERROR " << error << '\n';
        return 1;
    }

    std::cout << "  INFO  parameter count: " << instance->getParameters().size() << "\n";

    int parameterIndex = 0;
    for (auto* parameter : instance->getParameters())
        std::cout << "  INFO    " << parameterIndex++ << "  " << parameter->getName (48) << "\n";

    check (instance->getParameters().size() >= 15,
           "all 15 user parameters are exposed to the host");
    check (instance->getNumPrograms() == 10,
           "all ten factory programs are exposed to the host");

    instance->setPlayConfigDetails (2, 2, 48000.0, 512);
    instance->prepareToPlay (48000.0, 512);

    check (instance->getLatencySamples() == 0,
           "zero-latency design reports no host latency");

    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    bool finite = true;

    for (int block = 0; block < 80; ++block)
    {
        fillSignal (buffer, block * 512);
        instance->processBlock (buffer, midi);
        finite = finite && allFinite (buffer);
    }

    check (finite, "audio thread produces finite output through the VST3 wrapper");
    check (buffer.getMagnitude (0, 0, buffer.getNumSamples()) > 0.0001f,
           "processed signal remains audible");

    // Locate the signature parameters by their host-visible names. The VST3
    // wrapper assigns its own numeric IDs, so the APVTS IDs are not visible here.
    juce::AudioProcessorParameter* startFrequency = nullptr;
    juce::AudioProcessorParameter* controlSource = nullptr;

    for (auto* parameter : instance->getParameters())
    {
        if (parameter->getName (48).containsIgnoreCase ("Start Frequency"))
            startFrequency = parameter;

        if (parameter->getName (48).containsIgnoreCase ("Control Source"))
            controlSource = parameter;
    }

    check (startFrequency != nullptr, "start frequency is exposed to the host");
    check (controlSource != nullptr, "control source is exposed to the host");

    if (startFrequency != nullptr)
    {
        const float markerValue = 0.37f;
        startFrequency->setValueNotifyingHost (markerValue);

        juce::MemoryBlock state;
        instance->getStateInformation (state);
        check (state.getSize() > 0, "state can be serialised for the host");

        startFrequency->setValueNotifyingHost (0.83f);
        instance->setStateInformation (state.getData(), static_cast<int> (state.getSize()));

        check (std::abs (startFrequency->getValue() - markerValue) < 0.01f,
               "serialised parameter state is restored");
    }

    // Exercise the signature control-source switch through the host parameter.
    if (controlSource != nullptr)
    {
        controlSource->setValueNotifyingHost (1.0f);
        instance->prepareToPlay (48000.0, 512);

        for (int block = 0; block < 40; ++block)
        {
            fillSignal (buffer, (80 + block) * 512);
            instance->processBlock (buffer, midi);
            finite = finite && allFinite (buffer);
        }

        check (finite, "control-source switch remains finite through the VST3 wrapper");
    }

    instance->releaseResources();

    std::cout << (failures == 0 ? "\nHost smoke test passed.\n" : "\nHost smoke test failed.\n");
    return failures == 0 ? 0 : 1;
}
