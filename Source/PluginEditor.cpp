/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // MAIN_PANEL.png is 1024 x 576 — layout coords match the image pixels.
    constexpr int panelDesignWidth  = 1024;
    constexpr int panelDesignHeight = 576;

    // Drive knob (matched to panel artwork)
    constexpr int driveCentreX = 205;
    constexpr int driveCentreY = 190;
    constexpr int driveRadius  = 65;

    // Dry/Wet knob — 0 = dry (clean), 1 = wet (100% effect)
    constexpr int mixCentreX = 203;
    constexpr int mixCentreY = 378;
    constexpr int mixRadius  = 57;

    // Mode buttons (1920-space coords scaled to 1024 x 576)
    constexpr int modeButtonY = 102;
    constexpr int modeButtonH = 52;

    constexpr int warmButtonX    = 398;
    constexpr int warmButtonW    = 122;
    constexpr int heavyButtonX   = 570;
    constexpr int heavyButtonW   = 121;
    constexpr int extremeButtonX = 743;
    constexpr int extremeButtonW = 121;

    juce::Image loadImageFromBinary (const char* data, int size)
    {
        if (auto image = juce::ImageFileFormat::loadFrom (data, (size_t) size); image.isValid())
            return image;

        return juce::ImageCache::getFromMemory (data, size);
    }

    juce::Rectangle<int> scaledRect (juce::Rectangle<int> area, int x, int y, int w, int h)
    {
        const float scaleX = (float) area.getWidth()  / (float) panelDesignWidth;
        const float scaleY = (float) area.getHeight() / (float) panelDesignHeight;

        return juce::Rectangle<int> (juce::roundToInt (x * scaleX),
                                     juce::roundToInt (y * scaleY),
                                     juce::roundToInt (w * scaleX),
                                     juce::roundToInt (h * scaleY));
    }

    juce::Rectangle<int> scaledKnobBounds (juce::Rectangle<int> area,
                                           int centreX,
                                           int centreY,
                                           int radius)
    {
        const float scaleX = (float) area.getWidth()  / (float) panelDesignWidth;
        const float scaleY = (float) area.getHeight() / (float) panelDesignHeight;
        const int size = juce::roundToInt (2 * radius * scaleX);
        const int x = juce::roundToInt (centreX * scaleX) - size / 2;
        const int y = juce::roundToInt (centreY * scaleY) - size / 2;

        return { x, y, size, size };
    }

    void setupInvisibleSlider (juce::Slider& slider, juce::LookAndFeel& lookAndFeel)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel (&lookAndFeel);
        slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::transparentBlack);
    }

    void setupInvisibleButton (juce::TextButton& button, juce::LookAndFeel& lookAndFeel)
    {
        button.setButtonText ({});
        button.setLookAndFeel (&lookAndFeel);
    }
}

//==============================================================================
void InvisibleControlLookAndFeel::drawRotarySlider (juce::Graphics&,
                                                    int, int, int, int,
                                                    float, float, float,
                                                    juce::Slider&)
{
}

void InvisibleControlLookAndFeel::drawButtonBackground (juce::Graphics&,
                                                        juce::Button&,
                                                        const juce::Colour&,
                                                        bool,
                                                        bool)
{
}

void InvisibleControlLookAndFeel::drawButtonText (juce::Graphics&,
                                                  juce::TextButton&,
                                                  bool,
                                                  bool)
{
}

//==============================================================================
SCREAMERAudioProcessorEditor::SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    loadUiAssets();

    const int editorWidth  = mainPanelImage.isValid() ? mainPanelImage.getWidth()  : panelDesignWidth;
    const int editorHeight = mainPanelImage.isValid() ? mainPanelImage.getHeight() : panelDesignHeight;

    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) editorWidth * 0.75f),
                     juce::roundToInt ((float) editorHeight * 0.75f),
                     editorWidth * 2,
                     editorHeight * 2);
    setSize (editorWidth, editorHeight);

    setupInvisibleSlider (driveSlider, invisibleLookAndFeel);
    driveSlider.setRange (1.0, 20.0, 0.1);
    addAndMakeVisible (driveSlider);

    driveAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts,
        "drive",
        driveSlider);

    setupInvisibleSlider (mixSlider, invisibleLookAndFeel);
    addAndMakeVisible (mixSlider);

    mixAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts,
        "mix",
        mixSlider);

    setupInvisibleButton (warmButton, invisibleLookAndFeel);
    warmButton.onClick = [this] { setMode (0); };
    addAndMakeVisible (warmButton);

    setupInvisibleButton (heavyButton, invisibleLookAndFeel);
    heavyButton.onClick = [this] { setMode (1); };
    addAndMakeVisible (heavyButton);

    setupInvisibleButton (extremeButton, invisibleLookAndFeel);
    extremeButton.onClick = [this] { setMode (2); };
    addAndMakeVisible (extremeButton);

    layoutControls();
}

SCREAMERAudioProcessorEditor::~SCREAMERAudioProcessorEditor()
{
    driveSlider.setLookAndFeel (nullptr);
    mixSlider.setLookAndFeel (nullptr);
    warmButton.setLookAndFeel (nullptr);
    heavyButton.setLookAndFeel (nullptr);
    extremeButton.setLookAndFeel (nullptr);
}

void SCREAMERAudioProcessorEditor::loadUiAssets()
{
    mainPanelImage = loadImageFromBinary (BinaryData::MAIN_PANEL_png, BinaryData::MAIN_PANEL_pngSize);
}

void SCREAMERAudioProcessorEditor::setMode (int index)
{
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter ("mode")))
        modeParam->setValueNotifyingHost (modeParam->convertTo0to1 (index));
}

void SCREAMERAudioProcessorEditor::layoutControls()
{
    const auto area = getLocalBounds();

    driveSlider.setBounds (scaledKnobBounds (area, driveCentreX, driveCentreY, driveRadius));
    mixSlider.setBounds (scaledKnobBounds (area, mixCentreX, mixCentreY, mixRadius));

    warmButton.setBounds (scaledRect (area, warmButtonX, modeButtonY, warmButtonW, modeButtonH));
    heavyButton.setBounds (scaledRect (area, heavyButtonX, modeButtonY, heavyButtonW, modeButtonH));
    extremeButton.setBounds (scaledRect (area, extremeButtonX, modeButtonY, extremeButtonW, modeButtonH));
}

void SCREAMERAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (! mainPanelImage.isValid())
    {
        g.fillAll (juce::Colours::black);
        return;
    }

    g.drawImage (mainPanelImage, getLocalBounds().toFloat());
}

void SCREAMERAudioProcessorEditor::resized()
{
    layoutControls();
}
