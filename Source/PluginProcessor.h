#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include "NeuralEngine.h"

struct LayerSlot
{
    juce::AudioSampleBuffer audioData;
    std::vector<int> transientCuts;
    NeuralEngine::AcousticProfile profile;
    int bpm = 120;
    int rootKey = 0;
    int scaleMode = 0;
    float volume = 1.0f;
    bool isLoaded = false;
    juce::String trackName = "EMPTY";
};

struct ActiveVoice
{
    int layerIndex = -1;
    double currentSample = 0.0;
    int sampleEnd = 0;
    double pitchSpeed = 1.0;
    float panL = 1.0f;
    float panR = 1.0f;
    bool active = false;
    float gain = 0.0f;
    float gainTarget = 0.0f;
    float gainRamp = 0.0f;
};

class BoriyaAudioProcessor : public juce::AudioProcessor
{
public:
    BoriyaAudioProcessor();
    ~BoriyaAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void loadSampleIntoLayer(int layerIdx, const juce::File& audioFile);
    void triggerMasterGeneration();
    void renderMelodicPreview();
    void toggleLoopPlayback();
    bool isLoopPlaying() const { return playingState.load(); }

    juce::File exportMixAsWav();
    juce::File exportGrooveAsMidi();

    LayerSlot layers[3];
    juce::AudioSampleBuffer masterPreview;
    std::vector<NeuralEngine::MelodicStep> activeSteps;

    int targetBpm = 130;
    int targetKey = 0;
    int targetScaleMode = 0;

    std::atomic<bool> diceRollingState { false };
    float diceSpinAmount = 0.0f;
    int diceValue = 6;

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void calculateTransientSlices(int layerIdx);
    void triggerVoice(int layerIdx, int cutIdx, int pitchShift, float pan);

    NeuralEngine neuralEngine;
    ActiveVoice voices[2];
    int currentVoice = 0;

    std::atomic<bool> playingState { false };
    int currentPlayStep = 0;
    int stepClock = 0;
    int stepDuration = 11025;

    juce::dsp::Reverb spaceReverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::dsp::StateVariableTPTFilter<float> masterFilter;

    juce::AudioBuffer<float> delayMemory;
    int delayWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoriyaAudioProcessor)
};
