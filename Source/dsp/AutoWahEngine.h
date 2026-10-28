#pragma once

#include <algorithm>
#include <cmath>

namespace autowah::dsp
{
//==============================================================================
// Analogue-style envelope follower with independent attack and release times.
//==============================================================================
class EnvelopeFollower
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        updateCoefficients();
        reset();
    }

    void setTimes (float newAttackMs, float newReleaseMs) noexcept
    {
        attackMs = std::clamp (newAttackMs, 0.1f, 500.0f);
        releaseMs = std::clamp (newReleaseMs, 1.0f, 5000.0f);
        updateCoefficients();
    }

    void reset() noexcept { value = 0.0f; }

    // Feed a full-wave rectified (or otherwise positive) detector signal.
    float process (float rectified) noexcept
    {
        const float coefficient = rectified > value ? attackCoefficient : releaseCoefficient;
        value += coefficient * (rectified - value);

        if (! std::isfinite (value))
            value = 0.0f;

        return value;
    }

    float get() const noexcept { return value; }

private:
    void updateCoefficients() noexcept
    {
        const auto coefficientFor = [this] (float milliseconds)
        {
            const float samples = std::max (1.0f, milliseconds * 0.001f * static_cast<float> (sampleRate));
            return 1.0f - std::exp (-1.0f / samples);
        };

        attackCoefficient = std::clamp (coefficientFor (attackMs), 0.0f, 1.0f);
        releaseCoefficient = std::clamp (coefficientFor (releaseMs), 0.0f, 1.0f);
    }

    double sampleRate = 48000.0;
    float attackMs = 2.0f;
    float releaseMs = 150.0f;
    float attackCoefficient = 0.5f;
    float releaseCoefficient = 0.001f;
    float value = 0.0f;
};

//==============================================================================
// Topology-preserving-transform state variable filter (Cytomic / Zavalishin).
// Produces bandpass and lowpass taps with a genuinely stable resonant peak.
//==============================================================================
struct SvfCoefficients
{
    float a1 = 0.0f;
    float a2 = 0.0f;
    float a3 = 0.0f;
    float k = 1.0f;
};

inline SvfCoefficients makeSvfCoefficients (float cutoffHz, float q, double sampleRate) noexcept
{
    const float nyquist = static_cast<float> (sampleRate) * 0.5f;
    const float clampedCutoff = std::clamp (cutoffHz, 20.0f, nyquist * 0.92f);
    const float clampedQ = std::clamp (q, 0.1f, 40.0f);

    const float g = std::tan (3.14159265358979323846f * clampedCutoff / static_cast<float> (sampleRate));
    const float k = 1.0f / clampedQ;
    const float a1 = 1.0f / (1.0f + g * (g + k));

    SvfCoefficients coefficients;
    coefficients.a1 = a1;
    coefficients.a2 = g * a1;
    coefficients.a3 = g * coefficients.a2;
    coefficients.k = k;
    return coefficients;
}

class TptSvf
{
public:
    void setCoefficients (const SvfCoefficients& newCoefficients) noexcept
    {
        coefficients = newCoefficients;
    }

    void reset() noexcept
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    inline void process (float input, float& bandpass, float& lowpass) noexcept
    {
        const float v3 = input - ic2eq;
        const float v1 = coefficients.a1 * ic1eq + coefficients.a2 * v3;
        const float v2 = ic2eq + coefficients.a2 * ic1eq + coefficients.a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;

        bandpass = v1;
        lowpass = v2;
    }

private:
    SvfCoefficients coefficients;
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
};

//==============================================================================
// Free-running unipolar LFO: sine, triangle, square and up-saw.
//==============================================================================
class Lfo
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        reset();
    }

    void setRateAndShape (float hz, int newShape) noexcept
    {
        rateHz = std::clamp (hz, 0.001f, 100.0f);
        shape = std::clamp (newShape, 0, 3);
    }

    void reset() noexcept { phase = 0.0; }

    float process() noexcept
    {
        phase += static_cast<double> (rateHz) / sampleRate;

        if (phase >= 1.0)
            phase -= std::floor (phase);

        return valueForPhase (static_cast<float> (phase));
    }

    float valueForPhase (float p) const noexcept
    {
        switch (shape)
        {
            case 1: // Triangle: 0 -> 1 -> 0
                return 1.0f - std::abs (2.0f * p - 1.0f);

            case 2: // Square: open for the first half of the cycle
                return p < 0.5f ? 1.0f : 0.0f;

            case 3: // Up-saw: opens progressively, then snaps shut
                return p;

            case 0:
            default:
                return 0.5f + 0.5f * std::sin (6.28318530717958647692f * p);
        }
    }

    float getPhase() const noexcept { return static_cast<float> (phase); }

private:
    double sampleRate = 48000.0;
    double phase = 0.0;
    float rateHz = 1.5f;
    int shape = 0;
};

//==============================================================================
// Stereo dual-source auto-wah voice.
//
// The control source is either the programme envelope (attack/release shaped,
// sensitivity scaled) or a free-running LFO. Both sources modulate the same
// resonant state-variable filter in the log-frequency domain, so switching
// sources never produces a click or a jump in the host's audio timing.
//==============================================================================
class WahEngine
{
public:
    struct Parameters
    {
        int mode = 0;          // 0 = envelope, 1 = LFO
        int filterType = 0;    // 0 = bandpass, 1 = lowpass
        int lfoShape = 0;
        float startHz = 320.0f;
        float sweepOctaves = 2.4f;
        float resonance = 5.5f;
        float sensitivityDb = 6.0f;
        float attackMs = 1.5f;
        float releaseMs = 90.0f;
        float lfoRateHz = 1.5f;
        float lfoDepth = 1.0f;
        float mix = 1.0f;
        float outputDb = 0.0f;
        float inputDb = 0.0f;
        bool bypass = false;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
        envelope.prepare (sampleRate);
        lfo.prepare (sampleRate);

        const auto controlRate = sampleRate / static_cast<double> (controlInterval);
        cutoffGlide = 1.0f - std::exp (-1.0f / static_cast<float> (0.0035 * controlRate));
        qGlide      = 1.0f - std::exp (-1.0f / static_cast<float> (0.020 * controlRate));
        typeGlide   = 1.0f - std::exp (-1.0f / static_cast<float> (0.006 * controlRate));
        mixGlide    = 1.0f - std::exp (-1.0f / static_cast<float> (0.008 * controlRate));
        gainGlide   = 1.0f - std::exp (-1.0f / static_cast<float> (0.012 * controlRate));
        bypassGlide = 1.0f - std::exp (-1.0f / static_cast<float> (0.004 * controlRate));

        reset();
    }

    void reset() noexcept
    {
        filterLeft.reset();
        filterRight.reset();
        envelope.reset();
        lfo.reset();

        currentQ = std::clamp (parameters.resonance, 0.5f, 14.0f);
        typeMix = parameters.filterType == 0 ? 0.0f : 1.0f;
        bypassMix = parameters.bypass ? 1.0f : 0.0f;
        mixCurrent = std::clamp (parameters.mix, 0.0f, 1.0f);
        outputGainCurrent = decibelsToGain (parameters.outputDb);
        inputGainCurrent = decibelsToGain (parameters.inputDb);
        logCutoff = std::log2 (std::clamp (parameters.startHz, 20.0f, 20000.0f));
        lastControl = 0.0f;
        cutoffHz = parameters.startHz;
        inputPeak = 0.0f;
        outputPeak = 0.0f;
        envelopeLevel = 0.0f;
        lfoValue = 0.0f;
        lastMode = parameters.mode;
    }

    // Called once per block on the audio thread.
    void setParameters (const Parameters& newParameters) noexcept
    {
        if (newParameters.mode != lastMode)
        {
            if (newParameters.mode == 1)
                lfo.reset();

            lastMode = newParameters.mode;
        }

        parameters = newParameters;

        parameters.startHz = std::clamp (parameters.startHz, 20.0f, 20000.0f);
        parameters.sweepOctaves = std::clamp (parameters.sweepOctaves, 0.0f, 8.0f);
        parameters.resonance = std::clamp (parameters.resonance, 0.5f, 14.0f);
        parameters.lfoDepth = std::clamp (parameters.lfoDepth, 0.0f, 1.0f);
        parameters.mix = std::clamp (parameters.mix, 0.0f, 1.0f);

        logStart = std::log2 (parameters.startHz);
        sensitivityGain = decibelsToGain (parameters.sensitivityDb);
        targetInputGain = decibelsToGain (parameters.inputDb);
        targetOutputGain = decibelsToGain (parameters.outputDb);

        envelope.setTimes (parameters.attackMs, parameters.releaseMs);
        lfo.setRateAndShape (parameters.lfoRateHz, parameters.lfoShape);
    }

    void process (float* left, float* right, int numSamples) noexcept
    {
        if (left == nullptr || numSamples <= 0)
            return;

        const bool stereo = right != nullptr;
        float blockInputPeak = 0.0f;
        float blockOutputPeak = 0.0f;
        float blockEnvelope = 0.0f;
        float blockLfo = 0.0f;
        SvfCoefficients coefficients {};

        for (int i = 0; i < numSamples; ++i)
        {
            const float rawLeft = left[i];
            const float rawRight = stereo ? right[i] : rawLeft;
            const float dryLeft = rawLeft * inputGainCurrent;
            const float dryRight = rawRight * inputGainCurrent;

            blockInputPeak = std::max (blockInputPeak,
                                       std::max (std::abs (dryLeft), std::abs (dryRight)));

            const float rectified = std::max (std::abs (dryLeft), std::abs (dryRight))
                                    * sensitivityGain;
            const float envelopeValue = envelope.process (rectified);
            const float lfoSample = lfo.process();

            blockEnvelope = envelopeValue;
            blockLfo = lfoSample;

            if ((i % controlInterval) == 0)
            {
                float control;

                if (parameters.mode == 0)
                {
                    // Soft-saturating detector curve keeps low-level playing expressive.
                    control = 1.0f - std::exp (-envelopeValue * 2.4f);
                }
                else
                {
                    control = lfoSample * parameters.lfoDepth;
                }

                control = std::clamp (control, 0.0f, 1.0f);
                lastControl = control;

                const float targetLog = logStart + parameters.sweepOctaves * control;
                logCutoff += cutoffGlide * (targetLog - logCutoff);

                currentQ += qGlide * (parameters.resonance - currentQ);
                typeMix += typeGlide * ((parameters.filterType == 0 ? 0.0f : 1.0f) - typeMix);
                mixCurrent += mixGlide * (parameters.mix - mixCurrent);
                inputGainCurrent += gainGlide * (targetInputGain - inputGainCurrent);
                outputGainCurrent += gainGlide * (targetOutputGain - outputGainCurrent);
                bypassMix += bypassGlide * ((parameters.bypass ? 1.0f : 0.0f) - bypassMix);

                cutoffHz = std::exp2 (logCutoff);
                coefficients = makeSvfCoefficients (cutoffHz, currentQ, sampleRate);
                filterLeft.setCoefficients (coefficients);
                filterRight.setCoefficients (coefficients);
            }

            float bandLeft = 0.0f, lowLeft = 0.0f;
            float bandRight = 0.0f, lowRight = 0.0f;
            filterLeft.process (dryLeft, bandLeft, lowLeft);
            filterRight.process (dryRight, bandRight, lowRight);

            const float resonanceMakeup = 1.0f / std::sqrt (std::max (1.0f, currentQ));
            const float bandGain = resonanceMakeup;
            const float lowGain = resonanceMakeup;

            float wetLeft = bandLeft * bandGain * (1.0f - typeMix) + lowLeft * lowGain * typeMix;
            float wetRight = bandRight * bandGain * (1.0f - typeMix) + lowRight * lowGain * typeMix;

            // Gentle valve-like peak control on the resonant wet path.
            wetLeft = std::tanh (wetLeft * 1.1f) * 0.9090909f;
            wetRight = std::tanh (wetRight * 1.1f) * 0.9090909f;

            float outLeft = (dryLeft * (1.0f - mixCurrent) + wetLeft * mixCurrent) * outputGainCurrent;
            float outRight = (dryRight * (1.0f - mixCurrent) + wetRight * mixCurrent) * outputGainCurrent;

            outLeft = outLeft * (1.0f - bypassMix) + rawLeft * bypassMix;
            outRight = outRight * (1.0f - bypassMix) + rawRight * bypassMix;

            left[i] = outLeft;

            if (stereo)
                right[i] = outRight;

            blockOutputPeak = std::max (blockOutputPeak,
                                        std::max (std::abs (outLeft), std::abs (outRight)));
        }

        inputPeak = blockInputPeak;
        outputPeak = blockOutputPeak;
        envelopeLevel = std::clamp (1.0f - std::exp (-blockEnvelope * 2.4f), 0.0f, 1.0f);
        lfoValue = blockLfo;
    }

    // --- Telemetry consumed by the editor at 30-60 Hz ---
    float getCutoffHz() const noexcept { return cutoffHz; }
    float getControlAmount() const noexcept { return lastControl; }
    float getEnvelopeLevel() const noexcept { return envelopeLevel; }
    float getLfoValue() const noexcept { return lfoValue; }
    float getLfoPhase() const noexcept { return lfo.getPhase(); }
    float getInputPeak() const noexcept { return inputPeak; }
    float getOutputPeak() const noexcept { return outputPeak; }
    int getActiveMode() const noexcept { return parameters.mode; }

    static float decibelsToGain (float decibels) noexcept
    {
        return std::pow (10.0f, decibels / 20.0f);
    }

private:
    static constexpr int controlInterval = 8;

    Parameters parameters;
    double sampleRate = 48000.0;

    EnvelopeFollower envelope;
    Lfo lfo;

    TptSvf filterLeft;
    TptSvf filterRight;

    float sensitivityGain = 2.0f;
    float targetInputGain = 1.0f;
    float inputGainCurrent = 1.0f;
    float targetOutputGain = 1.0f;
    float outputGainCurrent = 1.0f;
    float mixCurrent = 1.0f;
    float currentQ = 5.5f;
    float typeMix = 0.0f;
    float bypassMix = 0.0f;
    float logStart = 8.32f;
    float logCutoff = 8.32f;
    float cutoffHz = 320.0f;
    float lastControl = 0.0f;

    float cutoffGlide = 0.01f;
    float qGlide = 0.002f;
    float typeGlide = 0.005f;
    float mixGlide = 0.005f;
    float gainGlide = 0.005f;
    float bypassGlide = 0.01f;

    float inputPeak = 0.0f;
    float outputPeak = 0.0f;
    float envelopeLevel = 0.0f;
    float lfoValue = 0.0f;
    int lastMode = 0;
};
} // namespace autowah::dsp
