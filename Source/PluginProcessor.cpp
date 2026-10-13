#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BoriyaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "WARMTH_DRIVE", 1 }, "Warmth", 0.0f, 1.0f, 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "SPACE_SIZE", 1 }, "Space Size", 0.0f, 1.0f, 0.4f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "SPACE_MIX", 1 }, "Space Mix", 0.0f, 1.0f, 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "FILTER_FREQ", 1 }, "Filter", 40.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "MASTER_LEVEL", 1 }, "Level", 0.0f, 2.0f, 1.0f));

    // Per-Layer Volume Parameters
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "LAYER1_VOL", 1 }, "Layer 1 Vol", 0.0f, 1.5f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "LAYER2_VOL", 1 }, "Layer 2 Vol", 0.0f, 1.5f, 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "LAYER3_VOL", 1 }, "Layer 3 Vol", 0.0f, 1.5f, 1.0f));

    return { params.begin(), params.end() };
}

BoriyaAudioProcessor::BoriyaAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "BoriyaParams", createParameterLayout())
{
    formatManager.registerBasicFormats();
    masterPreview.setSize(2, 44100 * 16);
    masterPreview.clear();

    for (int i = 0; i < 2; ++i)
    {
        voices[i].active = false;
        voices[i].gain = 0.0f;
    }
}

BoriyaAudioProcessor::~BoriyaAudioProcessor() {}

const juce::String BoriyaAudioProcessor::getName() const { return "BORIYA"; }
bool BoriyaAudioProcessor::acceptsMidi() const { return true; }
bool BoriyaAudioProcessor::producesMidi() const { return true; }
bool BoriyaAudioProcessor::isMidiEffect() const { return false; }
double BoriyaAudioProcessor::getTailLengthSeconds() const { return 0.0; }
int BoriyaAudioProcessor::getNumPrograms() { return 1; }
int BoriyaAudioProcessor::getCurrentProgram() { return 0; }
void BoriyaAudioProcessor::setCurrentProgram(int) {}
const juce::String BoriyaAudioProcessor::getProgramName(int) { return {}; }
void BoriyaAudioProcessor::changeProgramName(int, const juce::String&) {}

void BoriyaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;
    spec.maximumBlockSize = static_cast<juce::uint32>(std::max(32, samplesPerBlock));
    spec.numChannels = 2;

    spaceReverb.prepare(spec);
    masterFilter.prepare(spec);
    masterFilter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);

    double sSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    stepDuration = static_cast<int>(sSec * spec.sampleRate);

    for (int i = 0; i < 2; ++i)
    {
        voices[i].active = false;
        voices[i].gain = 0.0f;
    }
}

void BoriyaAudioProcessor::releaseResources()
{
    spaceReverb.reset();
    masterFilter.reset();
    for (int i = 0; i < 2; ++i) voices[i].active = false;
}

bool BoriyaAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void BoriyaAudioProcessor::calculateTransientSlices(int layerIdx)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& lyr = layers[layerIdx];
    lyr.transientCuts.clear();

    int total = lyr.audioData.getNumSamples();
    if (total == 0) return;

    int numCuts = 16;
    int stepSize = total / numCuts;

    for (int i = 0; i < numCuts; ++i)
        lyr.transientCuts.push_back(i * stepSize);

    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    lyr.profile = neuralEngine.analyzeAudioSignal(lyr.audioData.getReadPointer(0),
                                                   std::min(total, 44100 * 2),
                                                   sRate);
}

void BoriyaAudioProcessor::loadSampleIntoLayer(int layerIdx, const juce::File& audioFile)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& lyr = layers[layerIdx];

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (reader != nullptr)
    {
        lyr.audioData.setSize(2, static_cast<int>(reader->lengthInSamples));
        reader->read(&lyr.audioData, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        lyr.isLoaded = true;
        calculateTransientSlices(layerIdx);

        triggerMasterGeneration();
        playingState.store(false);
        for (int i = 0; i < 2; ++i) voices[i].active = false;
    }
}

void BoriyaAudioProcessor::triggerMasterGeneration()
{
    std::vector<int> loaded;
    for (int i = 0; i < 3; ++i)
        if (layers[i].isLoaded && !layers[i].transientCuts.empty())
            loaded.push_back(i);

    if (loaded.empty())
    {
        masterPreview.clear();
        return;
    }

    diceRollingState.store(true);
    juce::Random rng;
    diceValue = 1 + rng.nextInt(6);

    int leadLayer = loaded[0];
    const auto& lyr = layers[leadLayer];

    int seedVal = rng.nextInt();
    activeSteps = neuralEngine.generateInspiredCadence(lyr.profile, targetScaleMode, seedVal);

    renderMelodicPreview();
}

void BoriyaAudioProcessor::renderMelodicPreview()
{
    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    double stepSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    int curStepSamples = static_cast<int>(stepSec * sRate);
    int totalLen = curStepSamples * 64;

    juce::AudioBuffer<float> tempBuffer(2, totalLen);
    tempBuffer.clear();

    if (activeSteps.empty())
    {
        masterPreview.setSize(2, totalLen);
        masterPreview.clear();
        return;
    }

    int activeTrack = -1;
    for (int i = 0; i < 3; ++i)
    {
        if (layers[i].isLoaded && layers[i].audioData.getNumSamples() > 0)
        {
            activeTrack = i;
            break;
        }
    }

    if (activeTrack == -1) return;
    const auto& lyr = layers[activeTrack];

    juce::Random rng(juce::Time::currentTimeMillis() % 10000);

    for (int sIdx = 0; sIdx < 64; ++sIdx)
    {
        const auto& step = activeSteps[static_cast<size_t>(sIdx)];
        if (step.isTriggered && !lyr.transientCuts.empty())
        {
            int slice = rng.nextInt(static_cast<int>(lyr.transientCuts.size()));
            int startPt = lyr.transientCuts[static_cast<size_t>(slice)];
            int endPt = (slice + 1 < static_cast<int>(lyr.transientCuts.size())) 
                        ? lyr.transientCuts[static_cast<size_t>(slice + 1)] 
                        : lyr.audioData.getNumSamples();

            double pRatio = std::pow(2.0, step.semitoneOffset / 12.0);
            int writePos = sIdx * curStepSamples;
            int renderSamples = std::min(curStepSamples * 3, totalLen - writePos);
            double readPos = startPt;

            float panAngle = (step.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            float gL = std::cos(panAngle);
            float gR = std::sin(panAngle);

            for (int i = 0; i < renderSamples; ++i)
            {
                int rInt = static_cast<int>(readPos);
                if (rInt < endPt && rInt < lyr.audioData.getNumSamples() - 1 && writePos + i < totalLen)
                {
                    float env = 1.0f;
                    if (i < 120) env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * i / 120.0f));
                    else if (renderSamples - i < 600) env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (renderSamples - i) / 600.0f));

                    float sL = lyr.audioData.getSample(0, rInt);
                    float sR = (lyr.audioData.getNumChannels() > 1) ? lyr.audioData.getSample(1, rInt) : sL;
                    float mono = (sL + sR) * 0.5f * step.velocity * env * 0.85f;

                    tempBuffer.addSample(0, writePos + i, mono * gL);
                    tempBuffer.addSample(1, writePos + i, mono * gR);
                    readPos += pRatio;
                }
                else break;
            }
        }
    }

    masterPreview.setSize(2, totalLen, false, true, true);
    for (int ch = 0; ch < 2; ++ch)
        masterPreview.copyFrom(ch, 0, tempBuffer, ch, 0, totalLen);
}

void BoriyaAudioProcessor::triggerVoice(int layerIdx, int cutIdx, int pitchShift, float pan)
{
    if (layerIdx < 0 || layerIdx >= 3) return;
    auto& lyr = layers[layerIdx];
    if (!lyr.isLoaded || cutIdx >= static_cast<int>(lyr.transientCuts.size()) || lyr.audioData.getNumSamples() <= 0) return;

    int oldV = currentVoice;
    int newV = 1 - currentVoice;
    currentVoice = newV;

    if (voices[oldV].active)
    {
        voices[oldV].active = false;
        voices[oldV].gain = 0.0f;
    }

    int st = lyr.transientCuts[static_cast<size_t>(cutIdx)];
    int en = (cutIdx + 1 < static_cast<int>(lyr.transientCuts.size())) ? lyr.transientCuts[static_cast<size_t>(cutIdx + 1)] : lyr.audioData.getNumSamples();

    auto& v = voices[newV];
    v.layerIndex = layerIdx;
    v.currentSample = st;
    v.sampleEnd = en;
    v.pitchSpeed = std::pow(2.0, pitchShift / 12.0);

    float panAngle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
    v.panL = std::cos(panAngle);
    v.panR = std::sin(panAngle);

    v.active = true;
    v.gain = 1.0f;
}

void BoriyaAudioProcessor::toggleLoopPlayback()
{
    bool next = !playingState.load();
    playingState.store(next);

    for (int i = 0; i < 2; ++i)
    {
        voices[i].active = false;
        voices[i].gain = 0.0f;
    }
    stepClock = 0;
    currentPlayStep = 0;
}

juce::File BoriyaAudioProcessor::getExportWavFile()
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BORIYA_Theme.wav");
    renderMelodicPreview();

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> wr(wavFmt.createWriterFor(new juce::FileOutputStream(file), 44100.0, 2, 24, {}, 0));
    if (wr != nullptr && masterPreview.getNumSamples() > 0)
        wr->writeFromAudioSampleBuffer(masterPreview, 0, masterPreview.getNumSamples());

    return file;
}

juce::File BoriyaAudioProcessor::getExportMidiFile()
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BORIYA_Groove.mid");

    juce::MidiFile mf;
    const int tpq = 960;
    mf.setTicksPerQuarterNote(tpq);

    juce::MidiMessageSequence seq;
    double usPerQtr = 60000000.0 / std::max(60, targetBpm);
    seq.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(usPerQtr)), 0.0);
    seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0.0);

    const int tps = tpq / 4;

    for (size_t i = 0; i < activeSteps.size(); ++i)
    {
        const auto& s = activeSteps[i];
        if (s.isTriggered)
        {
            int note = 60 + targetKey + s.semitoneOffset;
            double sTick = static_cast<double>(i * tps);
            double eTick = sTick + (tps * 3);

            seq.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(s.velocity * 120.0f)), sTick);
            seq.addEvent(juce::MidiMessage::noteOff(1, note), eTick);
        }
    }

    seq.addEvent(juce::MidiMessage::endOfTrack(), static_cast<double>(64 * tps));
    seq.updateMatchedPairs();
    mf.addTrack(seq);

    juce::FileOutputStream fos(file);
    if (fos.openedOk()) mf.writeTo(fos);

    return file;
}

void BoriyaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (!playingState.load())
    {
        buffer.clear();
        for (int i = 0; i < 2; ++i) voices[i].active = false;
        return;
    }

    buffer.clear();

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (pos->getBpm().hasValue())
            {
                int hBpm = static_cast<int>(*pos->getBpm());
                if (hBpm >= 60 && hBpm <= 220)
                    targetBpm = hBpm;
            }
        }
    }

    double sRate = (getSampleRate() > 0.0) ? getSampleRate() : 44100.0;
    double sSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    stepDuration = static_cast<int>(sSec * sRate);

    const float l1Vol = apvts.getRawParameterValue("LAYER1_VOL")->load();
    const float l2Vol = apvts.getRawParameterValue("LAYER2_VOL")->load();
    const float l3Vol = apvts.getRawParameterValue("LAYER3_VOL")->load();
    const float vols[3] = { l1Vol, l2Vol, l3Vol };

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        stepClock++;
        if (stepClock >= stepDuration)
        {
            stepClock = 0;
            currentPlayStep = (currentPlayStep + 1) % 64;

            if (!activeSteps.empty() && static_cast<size_t>(currentPlayStep) < activeSteps.size())
            {
                const auto& stp = activeSteps[static_cast<size_t>(currentPlayStep)];
                if (stp.isTriggered)
                {
                    int mainL = -1;
                    for (int i = 0; i < 3; ++i) if (layers[i].isLoaded) { mainL = i; break; }
                    if (mainL != -1 && !layers[mainL].transientCuts.empty())
                    {
                        int cut = (currentPlayStep % static_cast<int>(layers[mainL].transientCuts.size()));
                        triggerVoice(mainL, cut, stp.semitoneOffset, stp.pan);
                    }
                }
            }
        }

        for (int vIdx = 0; vIdx < 2; ++vIdx)
        {
            auto& v = voices[vIdx];
            if (v.active && v.layerIndex >= 0 && v.layerIndex < 3)
            {
                auto& lyr = layers[v.layerIndex];
                int rPos = static_cast<int>(v.currentSample);

                if (rPos < v.sampleEnd && rPos < lyr.audioData.getNumSamples() - 1)
                {
                    float s0 = lyr.audioData.getSample(0, rPos);
                    float s1 = lyr.audioData.getSample(0, rPos + 1);
                    float frac = static_cast<float>(v.currentSample - rPos);
                    float sampleVal = (s0 + frac * (s1 - s0)) * 0.75f * vols[v.layerIndex];

                    buffer.addSample(0, s, sampleVal * v.panL);
                    buffer.addSample(1, s, sampleVal * v.panR);

                    v.currentSample += v.pitchSpeed;
                }
                else
                {
                    v.active = false;
                }
            }
        }
    }

    const float warmth = apvts.getRawParameterValue("WARMTH_DRIVE")->load();
    const float rvbMix = apvts.getRawParameterValue("SPACE_MIX")->load();
    const float rvbSz  = apvts.getRawParameterValue("SPACE_SIZE")->load();
    const float cutoff = apvts.getRawParameterValue("FILTER_FREQ")->load();
    const float master = apvts.getRawParameterValue("MASTER_LEVEL")->load();

    if (warmth > 0.01f)
    {
        float g = 1.0f + warmth * 1.5f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* d = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                d[i] = std::tanh(d[i] * g);
        }
    }

    masterFilter.setCutoffFrequency(cutoff);
    juce::dsp::AudioBlock<float> blk(buffer);
    juce::dsp::ProcessContextReplacing<float> pCtx(blk);
    masterFilter.process(pCtx);

    if (rvbMix > 0.01f)
    {
        reverbParams.roomSize = rvbSz * 0.75f;
        reverbParams.damping = 0.5f;
        reverbParams.wetLevel = rvbMix * 0.3f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.15f);
        spaceReverb.setParameters(reverbParams);
        spaceReverb.process(pCtx);
    }

    buffer.applyGain(master);
}

bool BoriyaAudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* BoriyaAudioProcessor::createEditor()
{
    return new BoriyaAudioProcessorEditor(*this);
}

void BoriyaAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BoriyaAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BoriyaAudioProcessor();
}
