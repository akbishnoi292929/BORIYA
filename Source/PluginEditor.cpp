#include "PluginProcessor.h"
#include "PluginEditor.h"

BoriyaAudioProcessorEditor::BoriyaAudioProcessorEditor(BoriyaAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(780, 480);

    // Roll Dice Button
    diceRollBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffc59b27));
    diceRollBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff091a18));
    diceRollBtn.onClick = [this]() { audioProcessor.triggerMasterGeneration(); };
    addAndMakeVisible(diceRollBtn);

    // Playback Button
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff122b27));
    playBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd4af37));
    playBtn.onClick = [this]() { audioProcessor.toggleLoopPlayback(); };
    addAndMakeVisible(playBtn);

    // Drag / Export Buttons
    exportWavBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3630));
    exportWavBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe8c35a));
    exportWavBtn.onClick = [this]() {
        auto file = audioProcessor.exportMixAsWav();
        if (file.existsAsFile())
            file.revealToUser();
    };
    addAndMakeVisible(exportWavBtn);

    exportMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3630));
    exportMidiBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe8c35a));
    exportMidiBtn.onClick = [this]() {
        auto file = audioProcessor.exportGrooveAsMidi();
        if (file.existsAsFile())
            file.revealToUser();
    };
    addAndMakeVisible(exportMidiBtn);

    // Knobs
    auto setupSlider = [this](juce::Slider& s) {
        s.setLookAndFeel(&berLookAndFeel);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 15);
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe8c35a));
        s.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };

    setupSlider(warmthSlider);
    setupSlider(spaceSlider);
    setupSlider(filterSlider);

    warmthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "WARMTH_DRIVE", warmthSlider);
    spaceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "SPACE_MIX", spaceSlider);
    filterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILTER_FREQ", filterSlider);

    startTimerHz(30);
}

BoriyaAudioProcessorEditor::~BoriyaAudioProcessorEditor()
{
    stopTimer();
    warmthSlider.setLookAndFeel(nullptr);
    spaceSlider.setLookAndFeel(nullptr);
    filterSlider.setLookAndFeel(nullptr);
}

void BoriyaAudioProcessorEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (exportWavBtn.getBounds().contains(e.getPosition()))
    {
        auto file = audioProcessor.exportMixAsWav();
        if (file.existsAsFile())
        {
            juce::DragAndDropContainer* ddc = juce::DragAndDropContainer::findParentDragContainerFor(this);
            if (ddc != nullptr)
                ddc->performExternalDragDropOfFiles({ file.getFullPathName() }, false);
        }
    }
}

void BoriyaAudioProcessorEditor::drawWaveform(juce::Graphics& g, const juce::AudioSampleBuffer& buf, juce::Rectangle<int> bounds)
{
    if (buf.getNumSamples() == 0) return;

    g.setColour(juce::Colour(0xff081a17).withAlpha(0.6f));
    g.fillRect(bounds);

    g.setColour(juce::Colour(0xffd4af37));
    juce::Path p;
    int w = bounds.getWidth();
    int h = bounds.getHeight();
    int midY = bounds.getY() + h / 2;
    int numSamples = buf.getNumSamples();
    int step = std::max(1, numSamples / w);
    const float* data = buf.getReadPointer(0);

    p.startNewSubPath(bounds.getX(), midY);
    for (int i = 0; i < w; ++i)
    {
        int sIdx = std::min(numSamples - 1, i * step);
        float val = data[sIdx];
        float y = midY - (val * (h * 0.42f));
        p.lineTo(bounds.getX() + i, y);
    }

    g.strokePath(p, juce::PathStrokeType(1.2f));
}

void BoriyaAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bgGradient(juce::Colour(0xff061614), 0, 0,
                                    juce::Colour(0xff0a2420), 0, (float) getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillAll();

    g.setColour(juce::Colour(0xffc59b27));
    g.drawRoundedRectangle(10.0f, 10.0f, getWidth() - 20.0f, getHeight() - 20.0f, 12.0f, 1.8f);

    g.setColour(juce::Colour(0xff0e352f));
    g.drawRoundedRectangle(14.0f, 14.0f, getWidth() - 28.0f, getHeight() - 28.0f, 10.0f, 1.0f);

    g.setColour(juce::Colour(0xffe8c35a));
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("BORIYA", 35, 20, 200, 32, juce::Justification::left);

    g.setColour(juce::Colour(0xff86ab9f));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("OG RJ HOOD  |  NEURAL GENERATIVE INSTRUMENT", 35, 48, 380, 20, juce::Justification::left);

    float centerX = 430.0f;
    float centerY = 240.0f;
    pulsePhase += 0.04f;
    float glow = 0.7f + 0.3f * std::sin(pulsePhase);

    g.setColour(juce::Colour(0xffd4af37).withAlpha(0.25f * glow));
    for (int ring = 1; ring <= 3; ++ring)
        g.drawEllipse(centerX - ring * 22, centerY - ring * 22, ring * 44, ring * 44, 1.0f);

    g.setColour(juce::Colour(0xffe8c35a).withAlpha(glow));
    g.fillEllipse(centerX - 8, centerY - 8, 16, 16);

    for (int i = 0; i < 3; ++i)
    {
        int yPos = 85 + i * 72;
        juce::Rectangle<int> box(35, yPos, 340, 62);
        auto& lyr = audioProcessor.layers[i];

        g.setColour(juce::Colour(0xff091a18));
        g.fillRoundedRectangle(box.toFloat(), 6.0f);

        if (lyr.isLoaded && lyr.audioData.getNumSamples() > 0)
        {
            drawWaveform(g, lyr.audioData, box.reduced(4));
        }

        g.setColour(lyr.isLoaded ? juce::Colour(0xffd4af37) : juce::Colour(0xff1e423b));
        g.drawRoundedRectangle(box.toFloat(), 6.0f, 1.2f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        juce::String title = "LAYER " + juce::String(i + 1) + ": " + (lyr.isLoaded ? lyr.trackName : "DRAG & DROP ACOUSTIC AUDIO");
        g.drawText(title, 45, yPos + 6, 320, 18, juce::Justification::left);
    }

    g.setColour(juce::Colour(0xffd4af37));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("WARMTH", 530, 266, 70, 16, juce::Justification::centred);
    g.drawText("SPACE", 610, 266, 70, 16, juce::Justification::centred);
    g.drawText("CUTOFF", 690, 266, 70, 16, juce::Justification::centred);
}

void BoriyaAudioProcessorEditor::resized()
{
    diceRollBtn.setBounds(525, 45, 220, 42);
    playBtn.setBounds(525, 95, 220, 36);

    exportWavBtn.setBounds(525, 145, 220, 36);
    exportMidiBtn.setBounds(525, 189, 220, 36);

    warmthSlider.setBounds(530, 285, 70, 75);
    spaceSlider.setBounds(610, 285, 70, 75);
    filterSlider.setBounds(690, 285, 70, 75);
}

void BoriyaAudioProcessorEditor::timerCallback()
{
    playBtn.setButtonText(audioProcessor.isLoopPlaying() ? "PAUSE" : "PLAY CADENCE");
    repaint();
}

void BoriyaAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int y)
{
    if (files.isEmpty()) return;

    int layer = -1;
    if (y >= 85 && y < 155)       layer = 0;
    else if (y >= 155 && y < 225) layer = 1;
    else if (y >= 225 && y < 295) layer = 2;

    if (layer != -1)
    {
        juce::File f(files[0]);
        if (f.existsAsFile())
            audioProcessor.loadSampleIntoLayer(layer, f);
    }
}
