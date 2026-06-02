/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int defaultEditorWidth  = 720;
    constexpr int defaultEditorHeight = 320;
    constexpr int minEditorWidth      = 560;
    constexpr int minEditorHeight     = 240;

    constexpr float leftPanelWidthRatio  = 0.32f;
    constexpr float outerBorderThickness = 4.0f;
    constexpr float innerPadding         = 10.0f;
    constexpr float modeButtonRowRatio   = 0.16f;

    constexpr float gainSectionHeightRatio = 0.54f;
    constexpr float mixSectionHeightRatio  = 0.34f;
    constexpr float sectionGapHeightRatio  = 0.08f;
    constexpr float mixKnobScale           = 0.68f;

    const juce::Colour backgroundColour    { 0xff0e0e0e };
    const juce::Colour panelBorderColour   { 0xffe8a020 };
    const juce::Colour labelTextColour     { 0xffd8d8d8 };
    const juce::Colour displayGridColour   { 0xff2a2a2a };
    const juce::Colour displayCurveColour  { 0xffe8a020 };

    void setupRotarySlider (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
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
}

//==============================================================================
void ScreamerModeButtonLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                           juce::Button& button,
                                                           const juce::Colour&,
                                                           bool,
                                                           bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.5f, 1.0f);
    const float corner = 5.0f;
    const bool isOn = button.getToggleState();

    if (isOn)
    {
        for (int i = 3; i >= 1; --i)
        {
            const float expand = (float) i * 1.5f;
            g.setColour (panelBorderColour.withAlpha (0.07f * (float) i));
            g.drawRoundedRectangle (bounds.expanded (expand), corner + 1.0f, 1.2f);
        }
    }

    if (! isDown)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), corner);
    }

    const juce::Colour topFill = isDown ? juce::Colour (0xff161616)
                            : (isOn ? juce::Colour (0xff3a3020) : juce::Colour (0xff2c2c2c));
    const juce::Colour bottomFill = isDown ? juce::Colour (0xff080808)
                               : (isOn ? juce::Colour (0xff16120c) : juce::Colour (0xff121212));

    juce::ColourGradient faceGradient (topFill,
                                       bounds.getCentreX(), bounds.getY(),
                                       bottomFill,
                                       bounds.getCentreX(), bounds.getBottom(),
                                       false);
    g.setGradientFill (faceGradient);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (juce::Colours::white.withAlpha (isOn ? 0.10f : 0.06f));
    g.drawLine (bounds.getX() + corner,
                bounds.getY() + 1.0f,
                bounds.getRight() - corner,
                bounds.getY() + 1.0f,
                1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine (bounds.getX() + corner,
                bounds.getBottom() - 1.0f,
                bounds.getRight() - corner,
                bounds.getBottom() - 1.0f,
                1.0f);

    if (isOn)
    {
        g.setColour (panelBorderColour.withAlpha (0.95f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.8f);

        g.setColour (panelBorderColour.withAlpha (0.25f));
        g.drawRoundedRectangle (bounds.reduced (2.0f), corner - 1.0f, 1.0f);
    }
    else
    {
        g.setColour (juce::Colour (0xff2a2218).withAlpha (0.7f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
    }
}

void ScreamerModeButtonLookAndFeel::drawButtonText (juce::Graphics& g,
                                                    juce::TextButton& button,
                                                    bool,
                                                    bool)
{
    const bool isOn = button.getToggleState();
    const auto text = button.getButtonText();
    auto textArea = button.getLocalBounds().toFloat();

    g.setFont (juce::Font (juce::FontOptions().withHeight (13.0f).withStyle ("Bold")));

    if (isOn)
    {
        const juce::Point<float> glowOffsets[] {
            { 0.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f },
            { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -2.0f }, { 0.0f, 2.0f }
        };

        for (auto offset : glowOffsets)
        {
            g.setColour (juce::Colour (0xffff9020).withAlpha (0.22f));
            g.drawText (text, textArea.translated (offset), juce::Justification::centred, false);
        }

        g.setColour (juce::Colour (0xffffb040));
    }
    else
    {
        g.setColour (juce::Colour (0xff6a4828));
    }

    g.drawText (text, textArea, juce::Justification::centred, false);
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
    g.setColour (juce::Colour (0xff141414));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (panelBorderColour);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.5f);

    auto plotArea = bounds.reduced (14.0f, 12.0f);
    plotArea.removeFromBottom (16.0f);

    drawGrid (g, plotArea);
    drawTransferCurve (g, plotArea);
}

//==============================================================================
SCREAMERAudioProcessorEditor::SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setResizable (true, true);
    setResizeLimits (minEditorWidth, minEditorHeight, 1200, 520);
    setSize (defaultEditorWidth, defaultEditorHeight);

    setupRotarySlider (gainSlider);
    gainSlider.setRange (1.0, 20.0, 0.1);
    addAndMakeVisible (gainSlider);

    gainLabel.setText ("GAIN", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    gainLabel.setColour (juce::Label::textColourId, labelTextColour);
    gainLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
    addAndMakeVisible (gainLabel);

    driveAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts,
        "drive",
        gainSlider);

    setupRotarySlider (mixSlider);
    mixSlider.setRange (0.0, 100.0, 0.1);
    mixSlider.setValue (100.0, juce::dontSendNotification);
    mixSlider.setEnabled (false);
    addAndMakeVisible (mixSlider);

    mixLabel.setText ("MIX", juce::dontSendNotification);
    mixLabel.setJustificationType (juce::Justification::centred);
    mixLabel.setColour (juce::Label::textColourId, labelTextColour);
    mixLabel.setFont (juce::Font (juce::FontOptions().withHeight (12.0f).withStyle ("Bold")));
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

    warmButton.setLookAndFeel (nullptr);
    heavyButton.setLookAndFeel (nullptr);
    extremeButton.setLookAndFeel (nullptr);
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
    g.fillAll (backgroundColour);

    auto outer = getLocalBounds().toFloat().reduced (outerBorderThickness * 0.5f);
    g.setColour (panelBorderColour);
    g.drawRect (outer, outerBorderThickness);

    if (! leftPanelBounds.isEmpty())
    {
        g.setColour (panelBorderColour);
        g.drawRoundedRectangle (leftPanelBounds.toFloat().reduced (4.0f), 4.0f, 1.5f);
    }
}

void SCREAMERAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce (juce::roundToInt (outerBorderThickness), juce::roundToInt (outerBorderThickness));

    const int leftWidth = juce::roundToInt ((float) bounds.getWidth() * leftPanelWidthRatio);
    auto leftPanel  = bounds.removeFromLeft (leftWidth);
    auto rightPanel = bounds;

    layoutLeftPanel (leftPanel);
    layoutRightPanel (rightPanel);
}
