#include "PluginProcessor.h"
#include "PluginEditor.h"

BoriyaAudioProcessorEditor::BoriyaAudioProcessorEditor(BoriyaAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(800, 520);
    setWantsKeyboardFocus(true);
    addKeyListener(this);

    // Roll Button
    diceRollBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffc59b27));
    diceRollBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff091a18));
    diceRollBtn.onClick = [this]() { audioProcessor.triggerMasterGeneration(); };
    addAndMakeVisible(diceRollBtn);

    // Play/Stop Button
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff122b27));
    playBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd4af37));
    playBtn.onClick = [this]() { audioProcessor.toggleLoopPlayback(); };
    addAndMakeVisible(playBtn);

    // PURE DRAG LABELS (CLICK KARTE HI PLAYLIST PAR DROP HOTI HAI, EXPLORER NAHI KHULTA)
    dragWavBtn = std::make_unique<DirectDragLabel>("WAV", [this]() {
        return audioProcessor.getExportWavFile();
    });
    addAndMakeVisible(*dragWavBtn);

    dragMidiBtn = std::make_unique<DirectDragLabel>("MIDI", [this]() {
        return audioProcessor.getExportMidiFile();
    });
    addAndMakeVisible(*dragMidiBtn);

    // Master Knobs
    auto setupMasterSlider = [this](juce::Slider& s) {
        s.setLookAndFeel(&berLookAndFeel);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);
    };

    setupMasterSlider(warmthSlider);
    setupMasterSlider(spaceSlider);
    setupMasterSlider(filterSlider);

    warmthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "WARMTH_DRIVE", warmthSlider);
    spaceAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "SPACE_MIX", spaceSlider);
    filterAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "FILTER_FREQ", filterSlider);

    // Har Layer Ka Volume Slider
    const char* paramIds[3] = { "LAYER1_VOL", "LAYER2_VOL", "LAYER3_VOL" };
    for (int i = 0; i < 3; ++i)
    {
        auto& s = layerVolSliders[i];
        s.setLookAndFeel(&berLookAndFeel);
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(s);

        layerVolAttach[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.apvts, paramIds[i], s);
    }

    startTimerHz(30);
}

BoriyaAudioProcessorEditor::~BoriyaAudioProcessorEditor()
{
    stopTimer();
    removeKeyListener(this);
    warmthSlider.setLookAndFeel(nullptr);
    spaceSlider.setLookAndFeel(nullptr);
    filterSlider.setLookAndFeel(nullptr);
    for (int i = 0; i < 3; ++i)
        layerVolSliders[i].setLookAndFeel(nullptr);
}

bool BoriyaAudioProcessorEditor::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::spaceKey)
    {
        audioProcessor.toggleLoopPlayback();
        return true;
    }
    return false;
}

void BoriyaAudioProcessorEditor::drawWaveform(juce::Graphics& g, const juce::AudioSampleBuffer& buf, juce::Rectangle<int> bounds, juce::Colour waveCol)
{
    if (buf.getNumSamples() == 0) return;

    g.setColour(waveCol);
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
        float y = midY - (val * (h * 0.44f));
        p.lineTo(bounds.getX() + i, y);
    }

    g.strokePath(p, juce::PathStrokeType(1.4f));
}

void BoriyaAudioProcessorEditor::paint(juce::Graphics& g)
{
    juce::ColourGradient bgGradient(juce::Colour(0xff061614), 0, 0,
                                    juce::Colour(0xff0a2420), 0, (float) getHeight(), false);
    g.setGradientFill(bgGradient);
    g.fillAll();

    // Royal Borders
    g.setColour(juce::Colour(0xffc59b27));
    g.drawRoundedRectangle(10.0f, 10.0f, getWidth() - 20.0f, getHeight() - 20.0f, 12.0f, 1.8f);

    // Minimal Header (Sirf BORIYA & OG RJ HOOD)
    g.setColour(juce::Colour(0xffe8c35a));
    g.setFont(juce::Font(28.0f, juce::Font::bold));
    g.drawText("BORIYA", 35, 18, 200, 32, juce::Justification::left);

    g.setColour(juce::Colour(0xff86ab9f));
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("OG RJ HOOD", 35, 46, 200, 18, juce::Justification::left);

    // Sacred Center Glow
    float centerX = 445.0f;
    float centerY = 240.0f;
    pulsePhase += 0.04f;
    float glow = 0.7f + 0.3f * std::sin(pulsePhase);

    g.setColour(juce::Colour(0xffd4af37).withAlpha(0.2f * glow));
    for (int ring = 1; ring <= 3; ++ring)
        g.drawEllipse(centerX - ring * 20, centerY - ring * 20, ring * 40, ring * 40, 1.0f);

    g.setColour(juce::Colour(0xffe8c35a).withAlpha(glow));
    g.fillEllipse(centerX - 7, centerY - 7, 14, 14);

    // 3 INPUT LAYERS (NO SAMPLE NAME TEXT, SIRF CLEAN WAVEFORM & DROP HERE)
    for (int i = 0; i < 3; ++i)
    {
        int yPos = 78 + i * 78;
        juce::Rectangle<int> box(35, yPos, 330, 68);
        auto& lyr = audioProcessor.layers[i];

        g.setColour(juce::Colour(0xff091a18));
        g.fillRoundedRectangle(box.toFloat(), 6.0f);

        if (lyr.isLoaded && lyr.audioData.getNumSamples() > 0)
        {
            drawWaveform(g, lyr.audioData, box.reduced(6), juce::Colour(0xffd4af37));
        }
        else
        {
            g.setColour(juce::Colours::grey.withAlpha(0.5f));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText("DROP HERE", box, juce::Justification::centred);
        }

        g.setColour(lyr.isLoaded ? juce::Colour(0xffd4af37) : juce::Colour(0xff1e423b));
        g.drawRoundedRectangle(box.toFloat(), 6.0f, 1.2f);
    }

    // 4TH LAYER: FINAL GENERATED OUTPUT WAVEFORM LAYER
    int outY = 78 + 3 * 78 + 4;
    juce::Rectangle<int> outBox(35, outY, 395, 78);
    g.setColour(juce::Colour(0xff071513));
    g.fillRoundedRectangle(outBox.toFloat(), 6.0f);

    if (audioProcessor.masterPreview.getNumSamples() > 0)
    {
        drawWaveform(g, audioProcessor.masterPreview, outBox.reduced(6), juce::Colour(0xffffe082));
    }
    else
    {
        g.setColour(juce::Colour(0xffd4af37).withAlpha(0.35f));
        g.setFont(juce::Font(12.0f, juce::Font::plain));
        g.drawText("FINAL GENERATED THEME", outBox, juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xffc59b27));
    g.drawRoundedRectangle(outBox.toFloat(), 6.0f, 1.5f);

    // Master Knobs Labels
    g.setColour(juce::Colour(0xffd4af37));
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("WARMTH", 535, 290, 65, 14, juce::Justification::centred);
    g.drawText("SPACE", 615, 290, 65, 14, juce::Justification::centred);
    g.drawText("CUTOFF", 695, 290, 65, 14, juce::Justification::centred);
}

void BoriyaAudioProcessorEditor::resized()
{
    // Right Side Action Buttons
    diceRollBtn.setBounds(535, 45, 225, 44);
    playBtn.setBounds(535, 96, 225, 38);

    dragWavBtn->setBounds(535, 142, 108, 36);
    dragMidiBtn->setBounds(652, 142, 108, 36);

    // Layer Volume Knobs (Har layer box ke theek bagal mein)
    for (int i = 0; i < 3; ++i)
    {
        int yPos = 78 + i * 78 + 9;
        layerVolSliders[i].setBounds(372, yPos, 50, 50);
    }

    // Master Knobs
    warmthSlider.setBounds(535, 310, 65, 65);
    spaceSlider.setBounds(615, 310, 65, 65);
    filterSlider.setBounds(695, 310, 65, 65);
}

void BoriyaAudioProcessorEditor::timerCallback()
{
    playBtn.setButtonText(audioProcessor.isLoopPlaying() ? "STOP" : "PLAY");
    repaint();
}

void BoriyaAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int y)
{
    if (files.isEmpty()) return;

    int layer = -1;
    if (y >= 78 && y < 156)       layer = 0;
    else if (y >= 156 && y < 234) layer = 1;
    else if (y >= 234 && y < 312) layer = 2;

    if (layer != -1)
    {
        juce::File f(files[0]);
        if (f.existsAsFile())
            audioProcessor.loadSampleIntoLayer(layer, f);
    }
}
