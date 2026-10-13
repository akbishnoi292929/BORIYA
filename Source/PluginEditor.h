#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

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

        g.setColour(juce::Colour(0xff091f1b));
        g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        g.setColour(juce::Colour(0xff18443b));
        g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

        juce::Path arc;
        arc.addCentredArc(centre.x, centre.y, radius - 2.0f, radius - 2.0f, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(juce::Colour(0xffd4af37));
        g.strokePath(arc, juce::PathStrokeType(2.5f));

        auto berRadius = radius * 0.72f;
        juce::ColourGradient berGrad(juce::Colour(0xfff3d078), centre.x - berRadius * 0.4f, centre.y - berRadius * 0.4f,
                                     juce::Colour(0xff8c6218), centre.x + berRadius, centre.y + berRadius, false);
        g.setGradientFill(berGrad);
        g.fillEllipse(centre.x - berRadius, centre.y - berRadius, berRadius * 2.0f, berRadius * 2.0f);

        g.setColour(juce::Colour(0xffffe699));
        g.drawEllipse(centre.x - berRadius, centre.y - berRadius, berRadius * 2.0f, berRadius * 2.0f, 1.2f);

        juce::Path p;
        auto pointerLength = berRadius * 0.85f;
        auto pointerThickness = 2.2f;
        p.addRectangle(-pointerThickness * 0.5f, -berRadius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(centre.x, centre.y));

        g.setColour(juce::Colour(0xff122b27));
        g.fillPath(p);
    }
};

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

    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void drawWaveform(juce::Graphics& g, const juce::AudioSampleBuffer& buffer, juce::Rectangle<int> bounds);

    BoriyaAudioProcessor& audioProcessor;
    BoriyaBerLookAndFeel berLookAndFeel;

    juce::TextButton diceRollBtn { "ROLL BORIYA" };
    juce::TextButton playBtn { "PLAY / STOP" };
    juce::TextButton exportWavBtn { "DRAG / SAVE WAV" };
    juce::TextButton exportMidiBtn { "DRAG / SAVE MIDI" };

    juce::Slider warmthSlider;
    juce::Slider spaceSlider;
    juce::Slider filterSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> warmthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spaceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> filterAttach;

    float pulsePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BoriyaAudioProcessorEditor)
};
