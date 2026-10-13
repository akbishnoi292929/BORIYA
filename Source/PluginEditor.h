#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class DragButton : public juce::TextButton
{
public:
    DragButton(const juce::String& name, std::function<juce::File()> getFileFunc)
        : juce::TextButton(name), getFile(getFileFunc) {}

    void mouseDrag(const juce::MouseEvent&) override
    {
        if (getFile != nullptr)
        {
            auto file = getFile();
            if (file.existsAsFile())
            {
                juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this);
                if (container != nullptr && !container->isDragAndDropActive())
                {
                    juce::StringArray files;
                    files.add(file.getFullPathName());
                    container->startDragging("audio/wav", this, juce::ScaledImage(), true, nullptr);
                    container->performExternalDragDropOfFiles(files, false);
                }
            }
        }
    }

private:
    std::function<juce::File()> getFile;
};

class BoriyaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::DragAndDropContainer,
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
    void drawWaveform(juce::Graphics& g, const juce::AudioSampleBuffer& buffer, juce::Rectangle<int> bounds);

    BoriyaAudioProcessor& audioProcessor;

    juce::TextButton diceRollBtn { "ROLL BORIYA" };
    juce::TextButton playBtn { "PLAY / STOP" };
    std::unique_ptr<DragButton> dragWavBtn;
    std::unique_ptr<DragButton> dragMidiBtn;

    juce::Slider warmthSlider;
    juce::Slider spaceSlider;
    juce::Slider filterSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spaceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttach;

    float pulsePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoriyaAudioProcessorEditor)
};
