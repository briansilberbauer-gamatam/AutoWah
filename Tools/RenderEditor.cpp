#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

#include <cmath>
#include <iostream>
#include <memory>

class AutoWahRenderApplication final : public juce::JUCEApplication,
                                       private juce::Timer
{
public:
    const juce::String getApplicationName() override { return "Auto-Wah Screenshot"; }
    const juce::String getApplicationVersion() override { return "1.1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}
    void systemRequestedQuit() override {}
    void shutdown() override {}

    void initialise (const juce::String& commandLine) override
    {
        outputFile = juce::File::getCurrentWorkingDirectory().getChildFile ("dist/autowah-ui.png");
        int presetIndex = 0;

        for (const auto& argument : juce::StringArray::fromTokens (commandLine, true))
        {
            if (argument.startsWith ("--out="))
            {
                const auto path = argument.substring (6);
                outputFile = juce::File::isAbsolutePath (path)
                                 ? juce::File (path)
                                 : juce::File::getCurrentWorkingDirectory().getChildFile (path);
            }
            else if (argument.startsWith ("--preset="))
            {
                presetIndex = argument.substring (9).getIntValue();
            }
        }

        outputFile = outputFile.getFullPathName();
        outputFile.getParentDirectory().createDirectory();

        processor = std::make_unique<AutoWahAudioProcessor>();
        processor->setPlayConfigDetails (2, 2, 48000.0, 1024);
        processor->prepareToPlay (48000.0, 1024);
        processor->setCurrentProgram (juce::jlimit (0, 9, presetIndex));

        // Park the pointer away from the unit so the snapshot never carries a
        // stale hover highlight.
        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 5.0f, 5.0f });

        editor = std::unique_ptr<juce::AudioProcessorEditor> (processor->createEditor());

        if (editor == nullptr)
        {
            std::cerr << "Could not create the plugin editor.\n";
            setApplicationReturnValue (1);
            quit();
            return;
        }

        editor->setSize (1180, 720);
        editor->addToDesktop (0);
        editor->setTopLeftPosition (30, 30);
        editor->setVisible (true);
        editor->toFront (true);

        testBuffer.setSize (2, 1024);
        startTimerHz (30);

        juce::Timer::callAfterDelay (3200, [this] { capture(); });
    }

private:
    void timerCallback() override
    {
        if (processor == nullptr)
            return;

        for (int i = 0; i < testBuffer.getNumSamples(); ++i)
        {
            const double t = static_cast<double> (sampleCounter + i) / 48000.0;

            // A repeating, harmonically rich funk pluck gives the display a clear
            // envelope contour plus enough midrange for the resonant wah to sing.
            const double notePhase = std::fmod (t, 0.5) / 0.5;
            const float pluck = static_cast<float> (std::exp (-3.0 * notePhase));
            const double fundamental = juce::MathConstants<double>::twoPi * 110.0 * t;

            float sample = 0.0f;

            for (int harmonic = 1; harmonic <= 7; ++harmonic)
                sample += (0.9f / static_cast<float> (harmonic))
                          * static_cast<float> (std::sin (fundamental * harmonic));

            sample *= pluck * 0.22f;

            testBuffer.setSample (0, i, sample);
            testBuffer.setSample (1, i, sample * 0.95f);
        }

        sampleCounter += testBuffer.getNumSamples();
        juce::MidiBuffer midi;
        processor->processBlock (testBuffer, midi);
    }

    void capture()
    {
        if (editor == nullptr)
            return;

        stopTimer();

        if (auto* autoWahEditor = dynamic_cast<AutoWahAudioProcessorEditor*> (editor.get()))
            autoWahEditor->setTooltipsVisible (false);

        juce::Desktop::getInstance().getMainMouseSource().setScreenPosition ({ 5.0f, 5.0f });

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);

        if (image.isValid())
        {
            outputFile.deleteFile();

            if (auto stream = std::unique_ptr<juce::FileOutputStream> (outputFile.createOutputStream()))
            {
                juce::PNGImageFormat png;
                const bool written = png.writeImageToStream (image, *stream);
                stream->flush();

                if (written)
                    std::cout << "Rendered " << outputFile.getFullPathName() << '\n';
                else
                    std::cerr << "Failed to encode PNG.\n";
            }
            else
            {
                std::cerr << "Could not open " << outputFile.getFullPathName() << '\n';
            }
        }
        else
        {
            std::cerr << "Editor snapshot was invalid.\n";
        }

        processor->releaseResources();
        editor->removeFromDesktop();
        editor.reset();
        processor.reset();
        quit();
    }

    std::unique_ptr<AutoWahAudioProcessor> processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    juce::File outputFile;
    juce::AudioBuffer<float> testBuffer;
    juce::int64 sampleCounter = 0;
};

START_JUCE_APPLICATION (AutoWahRenderApplication)
