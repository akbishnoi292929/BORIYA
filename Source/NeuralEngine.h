#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <juce_audio_basics/juce_audio_basics.h>

class NeuralEngine
{
public:
    NeuralEngine() = default;

    struct AcousticProfile
    {
        float spectralCentroid = 250.0f;
        float warmthFactor = 0.6f;
        float transientDensity = 0.5f;
        int detectedRootPitch = 60;
        bool hasValidSignal = false;
    };

    struct MelodicStep
    {
        int step = 0;             // 0 to 63
        int semitoneOffset = 0;
        float velocity = 0.95f;
        float pan = 0.0f;
        bool isTriggered = false;
    };

    AcousticProfile analyzeAudioSignal(const float* channelData, int totalSamples, double sampleRate)
    {
        AcousticProfile profile;
        if (channelData == nullptr || totalSamples < 256)
            return profile;

        float energySum = 0.0f;
        float weightedFreqSum = 0.0f;
        int zeroCrossings = 0;

        for (int i = 0; i < totalSamples; ++i)
        {
            float smp = channelData[i];
            energySum += (smp * smp);

            if (i > 0 && ((smp >= 0.0f && channelData[i - 1] < 0.0f) || (smp < 0.0f && channelData[i - 1] >= 0.0f)))
                zeroCrossings++;

            float instantFreq = (static_cast<float>(i) / static_cast<float>(totalSamples)) * static_cast<float>(sampleRate * 0.5);
            weightedFreqSum += std::abs(smp) * instantFreq;
        }

        float rms = std::sqrt(energySum / static_cast<float>(totalSamples));
        if (rms > 0.003f)
        {
            profile.hasValidSignal = true;
            profile.spectralCentroid = weightedFreqSum / std::max(1.0f, energySum);
            profile.transientDensity = static_cast<float>(zeroCrossings) / static_cast<float>(totalSamples);
            profile.warmthFactor = std::clamp(1.0f - (profile.transientDensity * 6.0f), 0.15f, 0.95f);
        }

        return profile;
    }

    std::vector<MelodicStep> generateInspiredCadence(const AcousticProfile&, int scaleMode, int seed)
    {
        std::vector<MelodicStep> pattern(64);
        for (int i = 0; i < 64; ++i)
        {
            pattern[i].step = i;
            pattern[i].isTriggered = false;
        }

        juce::Random rng(seed);

        const int minorScale[7] = { 0, 2, 3, 5, 7, 8, 10 };
        const int majorScale[7] = { 0, 2, 4, 5, 7, 9, 11 };
        const int* scale = (scaleMode == 1) ? majorScale : minorScale;

        // 6 Different Archetypes for endless dynamic variety
        int grooveType = rng.nextInt(6);

        for (int bar = 0; bar < 4; ++bar)
        {
            int base = bar * 16;
            
            // Downbeat Foundation
            pattern[base].isTriggered = true;
            pattern[base].semitoneOffset = (bar == 3) ? scale[0] : scale[(bar * 2 + rng.nextInt(2)) % 7];
            pattern[base].velocity = 1.0f;
            pattern[base].pan = (bar % 2 == 0) ? -0.15f : 0.15f;

            if (grooveType == 0 || grooveType == 2)
            {
                // Syncopated Pluck Feel
                pattern[base + 3].isTriggered = true;
                pattern[base + 3].semitoneOffset = scale[(2 + bar) % 7];
                pattern[base + 3].velocity = 0.92f;
                pattern[base + 3].pan = 0.25f;

                pattern[base + 6].isTriggered = true;
                pattern[base + 6].semitoneOffset = scale[4];
                pattern[base + 6].velocity = 0.96f;
                pattern[base + 6].pan = -0.2f;

                pattern[base + 10].isTriggered = true;
                pattern[base + 10].semitoneOffset = (bar == 2) ? scale[4] + 12 : scale[(5 + bar) % 7];
                pattern[base + 10].velocity = 0.88f;
                pattern[base + 10].pan = 0.35f;
            }
            else if (grooveType == 1 || grooveType == 3)
            {
                // Soulful Expressive Flow
                pattern[base + 4].isTriggered = true;
                pattern[base + 4].semitoneOffset = scale[(3 + rng.nextInt(3)) % 7];
                pattern[base + 4].velocity = 0.94f;
                pattern[base + 4].pan = 0.2f;

                pattern[base + 8].isTriggered = true;
                pattern[base + 8].semitoneOffset = scale[4] + (bar == 2 ? 12 : 0);
                pattern[base + 8].velocity = 0.98f;
                pattern[base + 8].pan = -0.3f;

                pattern[base + 12].isTriggered = true;
                pattern[base + 12].semitoneOffset = (bar == 3) ? scale[0] : scale[(6 + rng.nextInt(2)) % 7];
                pattern[base + 12].velocity = 0.90f;
                pattern[base + 12].pan = 0.1f;
            }
            else
            {
                // Triplet/Afro Swing Vibe
                int hit1 = base + 4;
                int hit2 = base + 7;
                int hit3 = base + 11;

                pattern[hit1].isTriggered = true;
                pattern[hit1].semitoneOffset = scale[2];
                pattern[hit1].velocity = 0.91f;

                pattern[hit2].isTriggered = true;
                pattern[hit2].semitoneOffset = scale[(4 + bar) % 7];
                pattern[hit2].velocity = 0.95f;

                pattern[hit3].isTriggered = true;
                pattern[hit3].semitoneOffset = (bar == 3) ? scale[0] : scale[(1 + bar) % 7];
                pattern[hit3].velocity = 0.89f;
            }
        }

        return pattern;
    }
};
