#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout BoriyaAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "WARMTH_DRIVE", 1 }, "Warmth Drive", 0.0f, 1.0f, 0.15f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "SPACE_SIZE", 1 }, "Space Size", 0.0f, 1.0f, 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "SPACE_MIX", 1 }, "Space Mix", 0.0f, 1.0f, 0.18f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "ANALOG_DELAY", 1 }, "Delay Mix", 0.0f, 1.0f, 0.10f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "FILTER_FREQ", 1 }, "Filter Freq", 40.0f, 20000.0f, 20000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ "MASTER_LEVEL", 1 }, "Master Level", 0.0f, 2.5f, 1.35f));

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

    delayMemory.setSize(2, static_cast<int>(spec.sampleRate * 2.0));
    delayMemory.clear();
    delayWritePos = 0;

    double sSec = (60.0 / std::max(60, targetBpm)) * 0.25;
    stepDuration = static_cast<int>(sSec * spec.sampleRate);
}

void BoriyaAudioProcessor::releaseResources()
{
    spaceReverb.reset();
    masterFilter.reset();
    delayMemory.clear();
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
        lyr.trackName = audioFile.getFileNameWithoutExtension().substring(0, 12);

        calculateTransientSlices(layerIdx);

        if (layerIdx == 0)
        {
            targetBpm = lyr.bpm;
            targetKey = lyr.rootKey;
            targetScaleMode = lyr.scaleMode;
        }

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
    diceSpinAmount = 0.0f;
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
            int renderSamples = std::min(curStepSamples * 5, totalLen - writePos);
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
                    if (i < 140) env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * i / 140.0f));
                    else if (renderSamples - i < 900) env = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (renderSamples - i) / 900.0f));

                    float sL = lyr.audioData.getSample(0, rInt);
                    float sR = (lyr.audioData.getNumChannels() > 1) ? lyr.audioData.getSample(1, rInt) : sL;
                    // Boosted Level (Loud & Punchy)
                    float mono = (sL + sR) * 0.85f * step.velocity * env;

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
    if (!lyr.isLoaded || cutIdx >= static_cast<int>(lyr.transientCuts.size())) return;

    int oldV = currentVoice;
    int newV = 1 - currentVoice;
    currentVoice = newV;

    if (voices[oldV].active)
    {
        voices[oldV].gainTarget = 0.0f;
        voices[oldV].gainRamp = -(voices[oldV].gain / 128.0f);
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
    v.gain = 0.0f;
    v.gainTarget = 1.0f;
    v.gainRamp = 1.0f / 128.0f;
}

void BoriyaAudioProcessor::toggleLoopPlayback()
{
    bool next = !playingState.load();
    playingState.store(next);

    if (!next)
    {
        for (int i = 0; i < 2; ++i) voices[i].active = false;
        stepClock = 0;
    }
    else
    {
        currentPlayStep = 0;
        stepClock = 0;
    }
}

juce::File BoriyaAudioProcessor::exportMixAsWav()
{
    auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("BORIYA_Theme.wav");
    renderMelodicPreview();

    juce::WavAudioFormat wavFmt;
    std::unique_ptr<juce::AudioFormatWriter> wr(wavFmt.createWriterFor(new juce::FileOutputStream(file), 44100.0, 2, 24, {}, 0));
    if (wr != nullptr && masterPreview.getNumSamples() > 0)
        wr->writeFromAudioSampleBuffer(masterPreview, 0, masterPreview.getNumSamples());

    return file;
}

juce::File BoriyaAudioProcessor::exportGrooveAsMidi()
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

            seq.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(s.velocity * 124.0f)), sTick);
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

void BoriyaAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

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

    for (const auto meta : midiMessages)
    {
        auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            int n = msg.getNoteNumber() - 60;
            int trk = juce::jlimit(0, 2, n / 12);
            int cut = n % 12;
            if (layers[trk].isLoaded && cut < static_cast<int>(layers[trk].transientCuts.size()))
                triggerVoice(trk, cut, 0, 0.0f);
        }
    }

    for (int s = 0; s < buffer.getNumSamples(); ++s)
    {
        if (playingState.load() && !activeSteps.empty())
        {
            stepClock++;
            if (stepClock >= stepDuration)
            {
                stepClock = 0;
                currentPlayStep = (currentPlayStep + 1) % static_cast<int>(activeSteps.size());

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
                    v.gain += v.gainRamp;
                    if (v.gainRamp > 0.0f && v.gain >= v.gainTarget) { v.gain = v.gainTarget; v.gainRamp = 0.0f; }
                    else if (v.gainRamp < 0.0f && v.gain <= v.gainTarget) { v.gain = 0.0f; v.active = false; v.gainRamp = 0.0f; }

                    float s0 = lyr.audioData.getSample(0, rPos);
                    float s1 = lyr.audioData.getSample(0, rPos + 1);
                    float frac = static_cast<float>(v.currentSample - rPos);
                    float val = (s0 + frac * (s1 - s0)) * v.gain * 1.15f;

                    buffer.addSample(0, s, val * v.panL);
                    buffer.addSample(1, s, val * v.panR);

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
        float g = 1.0f + warmth * 1.8f;
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
        reverbParams.damping = 0.45f;
        reverbParams.wetLevel = rvbMix * 0.35f;
        reverbParams.dryLevel = 1.0f - (rvbMix * 0.15f);
        spaceReverb.setParameters(reverbParams);
        spaceReverb.process(pCtx);
    }

    buffer.applyGain(master);

    // Warm Analog Limiting
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* d = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float v = d[i];
            if (std::abs(v) > 0.95f)
                d[i] = std::copysign(0.95f + 0.04f * std::tanh(std::abs(v) - 0.95f), v);
        }
    }
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
