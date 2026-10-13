#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class BoriyaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::Timer,
                                   public juce::FileDragAndDropTarget
{
public:
    explicit BoriyaAudioProcessorEditor(BoriyaAudioProcessor&);
    ~BoriyaAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    BoriyaAudioProcessor& audioProcessor;

    juce::TextButton diceRollBtn { "ROLL BORIYA CADENCE" };
    juce::TextButton playBtn { "PLAY / STOP" };
    juce::TextButton exportWavBtn { "EXPORT AUDIO (WAV)" };
    juce::TextButton exportMidiBtn { "EXPORT GROOVE (MID)" };

    juce::Slider warmthSlider;
    juce::Slider spaceSlider;
    juce::Slider filterSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spaceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoriyaAudioProcessorEditor)
};
