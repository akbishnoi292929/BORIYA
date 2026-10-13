#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Custom Rajasthani Golden Ber (Boriya) Rotary LookAndFeel
class BoriyaBerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        auto centre = bounds.getCentre();

        // 1. Subtle Outer Dial Track
        g.setColour(juce::Colour(0xff091f1b));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        
        g.setColour(juce::Colour(0xff18443b));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        // Active Golden Fill Arc
        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(juce::Colour(0xffd4af37));
        g.strokePath(arc, juce::PathStrokeType(2.5f));

        // 2. Embossed Golden "Ber" (Boriya) Cap
        auto berRadius = radius * 0.72f;
        juce::ColourGradient berGrad(juce::Colour(0xfff3d078), centre.x - berRadius * 0.4f, centre.y - berRadius * 0.4f,
                                     juce::Colour(0xff8c6218), centre.x + berRadius, centre.y + berRadius, false);
        g.setGradientFill(berGrad);
        g.fillEllipse(centre.x - berRadius, centre.y - berRadius, berRadius * 2.0f, berRadius * 2.0f);

        // Ber Golden Rim Border
        g.setColour(juce::Colour(0xffffe699));
        g.drawEllipse(centre.x - berRadius, centre.y - berRadius, berRadius * 2.0f, berRadius * 2.0f, 1.2f);

        // 3. Clear Pointer Needle (Taaki ghumne ka andaza turant lage)
        juce::Path p;
        auto pointerLength = berRadius * 0.85f;
        auto pointerThickness = 2.2f;
        p.addRectangle(-pointerThickness * 0.5f, -berRadius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff122b27));
        g.fillPath(p);
    }
};

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
    BoriyaBerLookAndFeel berLookAndFeel;

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
