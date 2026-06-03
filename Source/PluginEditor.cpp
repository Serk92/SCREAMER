/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int backgroundNativeWidth  = 693;
    constexpr int backgroundNativeHeight = 374;
    constexpr float leftPanelWidthRatio  = 172.0f / 693.0f;

    constexpr float innerPadding         = 10.0f;
    constexpr float modeButtonRowRatio   = 0.16f;

    constexpr float gainSectionHeightRatio = 0.54f;
    constexpr float mixSectionHeightRatio  = 0.34f;
    constexpr float sectionGapHeightRatio  = 0.08f;
    constexpr float mixKnobScale           = 0.748f;

    const juce::Colour labelTextColour    { 0xffd8d8d8 };
    const juce::Colour displayGridColour  { 0xff2a2a2a };
    const juce::Colour displayCurveColour { 0xffe8a020 };
    const juce::Colour activeButtonText   { 0xffffb040 };
    const juce::Colour inactiveButtonText { 0xff8a6030 };

    juce::Image loadImageFromBinary (const char* data, int size)
    {
        return juce::ImageCache::getFromMemory (data, size);
    }

    void setupRotarySlider (juce::Slider& slider, juce::LookAndFeel& lookAndFeel)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel (&lookAndFeel);
        slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::transparentBlack);
    }

    void setupModeButton (juce::TextButton& button,
                          int radioGroupId,
                          juce::LookAndFeel& lookAndFeel)
    {
        button.setRadioGroupId (radioGroupId);
        button.setClickingTogglesState (true);
        button.setLookAndFeel (&lookAndFeel);
        button.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    }

    void layoutKnobColumn (juce::Rectangle<int> section,
                           juce::Slider& knob,
                           juce::Label& label,
                           float knobDiameterScale)
    {
        const int labelHeight = 20;
        auto labelBounds = section.removeFromBottom (labelHeight);
        label.setBounds (labelBounds);

        const int maxDiameter = juce::jmin (section.getWidth(), section.getHeight());
        const int diameter = juce::jmax (32, juce::roundToInt ((float) maxDiameter * knobDiameterScale));
        knob.setBounds (section.withSizeKeepingCentre (diameter, diameter));
    }

    juce::Font knobLabelFont (float height)
    {
        return juce::Font (juce::FontOptions().withHeight (height).withStyle ("Bold"));
    }
}

//==============================================================================
void ScreamerImageKnobLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                                      int x,
                                                      int y,
                                                      int width,
                                                      int height,
                                                      float sliderPosProportional,
                                                      float rotaryStartAngle,
                                                      float rotaryEndAngle,
                                                      juce::Slider&)
{
    if (! knobImage.isValid())
        return;

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();

    const float sliderAngle = rotaryStartAngle
                              + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // GAIN.png indicator is at 12 o'clock; align it to the slider arc angle.
    const float rotation = sliderAngle + juce::MathConstants<float>::halfPi;

    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (rotation, centre.x, centre.y));

    g.drawImage (knobImage,
                 bounds.getX(), bounds.getY(),
                 bounds.getWidth(), bounds.getHeight(),
                 0, 0, knobImage.getWidth(), knobImage.getHeight());

    g.restoreState();
}

//==============================================================================
void ScreamerImageButtonLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                            juce::Button& button,
                                                            const juce::Colour&,
                                                            bool,
                                                            bool isDown)
{
    if (! buttonImage.isValid())
        return;

    auto bounds = button.getLocalBounds().toFloat();
    const bool isOn = button.getToggleState();

    g.setOpacity (isOn ? 1.0f : (isDown ? 0.75f : 0.55f));
    g.drawImage (buttonImage, bounds);
    g.setOpacity (1.0f);

    if (isOn)
    {
        g.setColour (juce::Colour (0xffff9020).withAlpha (0.12f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }
}

void ScreamerImageButtonLookAndFeel::drawButtonText (juce::Graphics& g,
                                                      juce::TextButton& button,
                                                      bool,
                                                      bool)
{
    const bool isOn = button.getToggleState();
    g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));
    g.setColour (isOn ? activeButtonText : inactiveButtonText);
    g.drawText (button.getButtonText(),
                button.getLocalBounds(),
                juce::Justification::centred,
                false);
}

//==============================================================================
void DisplayPanel::drawGrid (juce::Graphics& g, juce::Rectangle<float> plotArea) const
{
    g.setColour (displayGridColour);

    constexpr int numVerticalLines   = 8;
    constexpr int numHorizontalLines = 6;

    for (int i = 0; i <= numVerticalLines; ++i)
    {
        const float x = plotArea.getX() + plotArea.getWidth() * (float) i / (float) numVerticalLines;
        g.drawVerticalLine (juce::roundToInt (x), plotArea.getY(), plotArea.getBottom());
    }

    for (int i = 0; i <= numHorizontalLines; ++i)
    {
        const float y = plotArea.getY() + plotArea.getHeight() * (float) i / (float) numHorizontalLines;
        g.drawHorizontalLine (juce::roundToInt (y), plotArea.getX(), plotArea.getRight());
    }
}

void DisplayPanel::drawTransferCurve (juce::Graphics& g, juce::Rectangle<float> plotArea) const
{
    juce::Path curve;
    const int numPoints = 128;

    for (int i = 0; i < numPoints; ++i)
    {
        const float t = (float) i / (float) (numPoints - 1);
        const float xNorm = t * 2.0f - 1.0f;
        const float yNorm = std::tanh (xNorm * 2.5f);

        const float x = plotArea.getX() + (t * plotArea.getWidth());
        const float y = plotArea.getCentreY() - yNorm * plotArea.getHeight() * 0.42f;

        if (i == 0)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);
    }

    g.setColour (displayCurveColour);
    g.strokePath (curve, juce::PathStrokeType (2.5f));

    g.setColour (juce::Colours::grey.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions().withHeight (10.0f)));
    const char* axisLabels[] = { "-24", "-12", "0", "+12", "+24" };

    for (int i = 0; i < 5; ++i)
    {
        const float t = (float) i / 4.0f;
        const float x = plotArea.getX() + t * plotArea.getWidth();
        g.drawText (axisLabels[i],
                    juce::Rectangle<float> (x - 18.0f, plotArea.getBottom() + 2.0f, 36.0f, 14.0f),
                    juce::Justification::centred);
    }
}

void DisplayPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0x88000000));
    g.fillRoundedRectangle (bounds, 4.0f);

    auto plotArea = bounds.reduced (14.0f, 12.0f);
    plotArea.removeFromBottom (16.0f);

    drawGrid (g, plotArea);
    drawTransferCurve (g, plotArea);
}

//==============================================================================
SCREAMERAudioProcessorEditor::SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    loadUiAssets();

    gainKnobLookAndFeel.setKnobImage (knobImage);
    mixKnobLookAndFeel.setKnobImage (knobImage);
    modeButtonLookAndFeel.setButtonImage (buttonImage);

    const int editorWidth  = backgroundImage.isValid() ? backgroundImage.getWidth()  : backgroundNativeWidth;
    const int editorHeight = backgroundImage.isValid() ? backgroundImage.getHeight() : backgroundNativeHeight;

    setResizable (true, true);
    setResizeLimits (juce::roundToInt ((float) editorWidth * 0.8f),
                     juce::roundToInt ((float) editorHeight * 0.8f),
                     editorWidth * 2,
                     editorHeight * 2);
    setSize (editorWidth, editorHeight);

    setupRotarySlider (gainSlider, gainKnobLookAndFeel);
    gainSlider.setRange (1.0, 20.0, 0.1);
    addAndMakeVisible (gainSlider);

    gainLabel.setText ("GAIN", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setColour (juce::Label::textColourId, labelTextColour);
    gainLabel.setFont (knobLabelFont (12.0f));
    addAndMakeVisible (gainLabel);

    driveAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts,
        "drive",
        gainSlider);

    setupRotarySlider (mixSlider, mixKnobLookAndFeel);
    addAndMakeVisible (mixSlider);

    mixAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts,
        "mix",
        mixSlider);

    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    mixLabel.setColour (juce::Label::textColourId, labelTextColour);
    mixLabel.setFont (knobLabelFont (11.0f));
    addAndMakeVisible (mixLabel);

    constexpr int modeRadioGroupId = 1;
    setupModeButton (warmButton, modeRadioGroupId, modeButtonLookAndFeel);
    setupModeButton (heavyButton, modeRadioGroupId, modeButtonLookAndFeel);
    setupModeButton (extremeButton, modeRadioGroupId, modeButtonLookAndFeel);

    warmButton.setButtonText ("WARM");
    heavyButton.setButtonText ("HEAVY");
    extremeButton.setButtonText ("EXTREME");

    warmButton.onClick = [this] { setModeIndex (0); };
    heavyButton.onClick = [this] { setModeIndex (1); };
    extremeButton.onClick = [this] { setModeIndex (2); };

    addAndMakeVisible (warmButton);
    addAndMakeVisible (heavyButton);
    addAndMakeVisible (extremeButton);

    addAndMakeVisible (displayPanel);

    audioProcessor.apvts.addParameterListener ("mode", this);
    updateModeButtonStates();
}

SCREAMERAudioProcessorEditor::~SCREAMERAudioProcessorEditor()
{
    audioProcessor.apvts.removeParameterListener ("mode", this);

    gainSlider.setLookAndFeel (nullptr);
    mixSlider.setLookAndFeel (nullptr);
    warmButton.setLookAndFeel (nullptr);
    heavyButton.setLookAndFeel (nullptr);
    extremeButton.setLookAndFeel (nullptr);
}

void SCREAMERAudioProcessorEditor::loadUiAssets()
{
    backgroundImage = loadImageFromBinary (BinaryData::BACKGROUND_png, BinaryData::BACKGROUND_pngSize);
    leftPanelImage  = loadImageFromBinary (BinaryData::LEFT_PANEL_png, BinaryData::LEFT_PANEL_pngSize);
    buttonImage     = loadImageFromBinary (BinaryData::BUTTON_png, BinaryData::BUTTON_pngSize);
    knobImage       = loadImageFromBinary (BinaryData::GAIN_png, BinaryData::GAIN_pngSize);
}

//==============================================================================
void SCREAMERAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "mode")
        updateModeButtonStates();
}

void SCREAMERAudioProcessorEditor::setModeIndex (int index)
{
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter ("mode")))
        modeParam->setValueNotifyingHost (modeParam->convertTo0to1 (index));
}

void SCREAMERAudioProcessorEditor::updateModeButtonStates()
{
    int index = 1;

    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (audioProcessor.apvts.getParameter ("mode")))
        index = modeParam->getIndex();

    warmButton.setToggleState (index == 0, juce::dontSendNotification);
    heavyButton.setToggleState (index == 1, juce::dontSendNotification);
    extremeButton.setToggleState (index == 2, juce::dontSendNotification);

    warmButton.repaint();
    heavyButton.repaint();
    extremeButton.repaint();
}

void SCREAMERAudioProcessorEditor::layoutLeftPanel (juce::Rectangle<int> area)
{
    leftPanelBounds = area;

    area = area.reduced (juce::roundToInt (innerPadding));

    const int totalHeight = area.getHeight();
    const int sectionGap  = juce::jmax (12, juce::roundToInt (totalHeight * sectionGapHeightRatio));
    const int gainHeight  = juce::roundToInt (totalHeight * gainSectionHeightRatio);
    const int mixHeight   = totalHeight - gainHeight - sectionGap;

    layoutKnobColumn (area.removeFromTop (gainHeight), gainSlider, gainLabel, 0.92f);

    area.removeFromTop (sectionGap);

    layoutKnobColumn (area.removeFromTop (mixHeight), mixSlider, mixLabel, mixKnobScale);
}

void SCREAMERAudioProcessorEditor::layoutRightPanel (juce::Rectangle<int> area)
{
    area = area.reduced (juce::roundToInt (innerPadding));

    const int buttonRowHeight = juce::jmax (40, juce::roundToInt (area.getHeight() * modeButtonRowRatio));
    auto buttonRow = area.removeFromTop (buttonRowHeight);

    juce::FlexBox buttonFlex;
    buttonFlex.flexDirection = juce::FlexBox::Direction::row;
    buttonFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    const float buttonMargin = 6.0f;
    buttonFlex.items.add (juce::FlexItem (warmButton).withFlex (1.0f).withMargin (buttonMargin));
    buttonFlex.items.add (juce::FlexItem (heavyButton).withFlex (1.0f).withMargin (buttonMargin));
    buttonFlex.items.add (juce::FlexItem (extremeButton).withFlex (1.0f).withMargin (buttonMargin));
    buttonFlex.performLayout (buttonRow);

    area.removeFromTop (juce::roundToInt (innerPadding));
    displayPanel.setBounds (area);
}

void SCREAMERAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, bounds);
    else
        g.fillAll (juce::Colours::black);

    if (leftPanelImage.isValid() && ! leftPanelBounds.isEmpty())
        g.drawImage (leftPanelImage, leftPanelBounds.toFloat());
}

void SCREAMERAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    const int leftWidth = juce::roundToInt ((float) bounds.getWidth() * leftPanelWidthRatio);
    auto leftPanel  = bounds.removeFromLeft (leftWidth);
    auto rightPanel = bounds;

    layoutLeftPanel (leftPanel);
    layoutRightPanel (rightPanel);
}
