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

    // DIRECT DRAG-AND-DROP TO PLAYLIST BUTTONS (ZERO POPUP DIALOGS)
    dragWavBtn = std::make_unique<DragButton>("DRAG TO PLAYLIST (WAV)", [this]() {
        return audioProcessor.exportMixAsWav();
    });
    dragWavBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3630));
    dragWavBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe8c35a));
    addAndMakeVisible(*dragWavBtn);

    dragMidiBtn = std::make_unique<DragButton>("DRAG GROOVE (MIDI)", [this]() {
        return audioProcessor.exportGrooveAsMidi();
    });
    dragMidiBtn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a3630));
    dragMidiBtn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe8c35a));
    addAndMakeVisible(*dragMidiBtn);

    // Luxury Knobs
    auto setupSlider = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 16);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd4af37));
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff0a1c19));
        s.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffd4af37));
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
    // Deep Emerald Marble Background
    juce::ColourGradient bgGradient(juce::Colour(0xff061614), 0, 0,
                                    juce::Colour(0xff0a2420), 0, (float) getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillAll();

    // Outer Vintage Gold Filigree Border
    g.setColour(juce::Colour(0xffc59b27));
    g.drawRoundedRectangle(10.0f, 10.0f, getWidth() - 20.0f, getHeight() - 20.0f, 12.0f, 1.8f);

    // Inner Subtle Frame
    g.setColour(juce::Colour(0xff0e352f));
    g.drawRoundedRectangle(14.0f, 14.0f, getWidth() - 28.0f, getHeight() - 28.0f, 10.0f, 1.0f);

    // Luxury Header Title (BORIYA - OG RJ HOOD)
    g.setColour(juce::Colour(0xffe8c35a));
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("BORIYA", 35, 20, 200, 32, juce::Justification::left);

    g.setColour(juce::Colour(0xff86ab9f));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("OG RJ HOOD  |  NEURAL GENERATIVE INSTRUMENT", 35, 48, 380, 20, juce::Justification::left);

    // Glowing Sacred Neural Lattice Centerpiece (Inspired by Image Artwork)
    float centerX = 430.0f;
    float centerY = 240.0f;
    pulsePhase += 0.04f;
    float glow = 0.7f + 0.3f * std::sin(pulsePhase);

    g.setColour(juce::Colour(0xffd4af37).withAlpha(0.25f * glow));
    for (int ring = 1; ring <= 3; ++ring)
        g.drawEllipse(centerX - ring * 22, centerY - ring * 22, ring * 44, ring * 44, 1.0f);

    g.setColour(juce::Colour(0xffe8c35a).withAlpha(glow));
    g.fillEllipse(centerX - 8, centerY - 8, 16, 16); // Golden Core Boriya Berry

    // Layer Slots with REAL WAVEFORM DRAWING
    for (int i = 0; i < 3; ++i)
    {
        int yPos = 85 + i * 72;
        juce::Rectangle<int> box(35, yPos, 340, 62);
        auto& lyr = audioProcessor.layers[i];

        // Frame
        g.setColour(juce::Colour(0xff091a18));
        g.fillRoundedRectangle(box.toFloat(), 6.0f);

        // Waveform Render inside Box
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

    // Knobs Golden Labels
    g.setColour(juce::Colour(0xffd4af37));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("WARMTH", 530, 270, 70, 16, juce::Justification::centred);
    g.drawText("SPACE", 610, 270, 70, 16, juce::Justification::centred);
    g.drawText("CUTOFF", 690, 270, 70, 16, juce::Justification::centred);
}

void BoriyaAudioProcessorEditor::resized()
{
    // Right Action Panel
    diceRollBtn.setBounds(525, 45, 220, 42);
    playBtn.setBounds(525, 95, 220, 36);

    // Direct Drag-and-Drop Buttons to FL Studio (Zero Popups)
    dragWavBtn->setBounds(525, 145, 220, 36);
    dragMidiBtn->setBounds(525, 189, 220, 36);

    // Knobs
    warmthSlider.setBounds(530, 290, 70, 70);
    spaceSlider.setBounds(610, 290, 70, 70);
    filterSlider.setBounds(690, 290, 70, 70);
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
