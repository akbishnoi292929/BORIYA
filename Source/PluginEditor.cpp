#include "PluginProcessor.h"
#include "PluginEditor.h"

BoriyaAudioProcessorEditor::BoriyaAudioProcessorEditor(BoriyaAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(720, 460);

    // Dice Button
    diceRollBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffe65100));
    diceRollBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    diceRollBtn.onClick = [this]() { audioProcessor.triggerMasterGeneration(); };
    addAndMakeVisible(diceRollBtn);

    // Playback
    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff212121));
    playBtn.onClick = [this]() { audioProcessor.toggleLoopPlayback(); };
    addAndMakeVisible(playBtn);

    // Exports
    exportWavBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff37474f));
    exportWavBtn.onClick = [this]() {
        auto file = audioProcessor.exportMixAsWav();
        if (file.existsAsFile())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "BORIYA Export", "Exported WAV to:\n" + file.getFullPathName());
    };
    addAndMakeVisible(exportWavBtn);

    exportMidiBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff37474f));
    exportMidiBtn.onClick = [this]() {
        auto file = audioProcessor.exportGrooveAsMidi();
        if (file.existsAsFile())
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "BORIYA MIDI", "Exported MIDI Groove to:\n" + file.getFullPathName());
    };
    addAndMakeVisible(exportMidiBtn);

    // Sliders
    auto setupSlider = [this](juce::Slider& s) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff9800));
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

void BoriyaAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff121212));

    // Brand Title Header
    g.setColour(juce::Colour(0xff1f1f1f));
    g.fillRect(0, 0, getWidth(), 60);

    g.setColour(juce::Colour(0xffff9800));
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("BORIYA", 25, 8, 200, 30, juce::Justification::left);

    g.setColour(juce::Colours::grey);
    g.setFont(juce::Font(13.0f, juce::Font::plain));
    g.drawText("BY OG RJ HOOD  |  NEURAL GENERATIVE INSTRUMENT", 25, 34, 400, 20, juce::Justification::left);

    // Layer Slots Drag & Drop Zone
    for (int i = 0; i < 3; ++i)
    {
        int yPos = 80 + i * 65;
        auto& lyr = audioProcessor.layers[i];

        g.setColour(lyr.isLoaded ? juce::Colour(0xff263238) : juce::Colour(0xff1a1a1a));
        g.fillRoundedRectangle(25.0f, static_cast<float>(yPos), 380.0f, 55.0f, 6.0f);

        g.setColour(lyr.isLoaded ? juce::Colour(0xffff9800) : juce::Colours::darkgrey);
        g.drawRoundedRectangle(25.0f, static_cast<float>(yPos), 380.0f, 55.0f, 6.0f, 1.2f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        juce::String label = "LAYER " + juce::String(i + 1) + ": " + (lyr.isLoaded ? lyr.trackName : "DRAG & DROP ACOUSTIC AUDIO");
        g.drawText(label, 40, yPos + 18, 350, 20, juce::Justification::left);
    }

    // Knobs Labels
    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("WARMTH", 440, 260, 75, 18, juce::Justification::centred);
    g.drawText("SPACE", 525, 260, 75, 18, juce::Justification::centred);
    g.drawText("CUTOFF", 610, 260, 75, 18, juce::Justification::centred);
}

void BoriyaAudioProcessorEditor::resized()
{
    diceRollBtn.setBounds(440, 80, 250, 48);
    playBtn.setBounds(440, 138, 250, 36);

    exportWavBtn.setBounds(440, 184, 120, 32);
    exportMidiBtn.setBounds(570, 184, 120, 32);

    warmthSlider.setBounds(440, 280, 75, 75);
    spaceSlider.setBounds(525, 280, 75, 75);
    filterSlider.setBounds(610, 280, 75, 75);
}

void BoriyaAudioProcessorEditor::timerCallback()
{
    playBtn.setButtonText(audioProcessor.isLoopPlaying() ? "STOPPING..." : "PLAY CADENCE");
    repaint();
}

void BoriyaAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int y)
{
    if (files.isEmpty()) return;

    int layer = -1;
    if (y >= 80 && y < 145)       layer = 0;
    else if (y >= 145 && y < 210) layer = 1;
    else if (y >= 210 && y < 275) layer = 2;

    if (layer != -1)
    {
        juce::File f(files[0]);
        if (f.existsAsFile())
            audioProcessor.loadSampleIntoLayer(layer, f);
    }
}
