/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    void styleModeButton (juce::TextButton& button, int radioGroupId)
    {
        button.setRadioGroupId (radioGroupId);
        button.setClickingTogglesState (true);
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::orange);
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }
}

//==============================================================================
SCREAMERAudioProcessorEditor::SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 340);

    driveSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    driveSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    driveSlider.setRange (1.0, 20.0, 0.1);
    addAndMakeVisible (driveSlider);

    driveLabel.setText ("Drive", juce::dontSendNotification);
    driveLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (driveLabel);

    driveAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.apvts,
        "drive",
        driveSlider);

    modeLabel.setText ("Mode", juce::dontSendNotification);
    modeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modeLabel);

    constexpr int modeRadioGroupId = 1;
    styleModeButton (warmButton, modeRadioGroupId);
    styleModeButton (heavyButton, modeRadioGroupId);
    styleModeButton (extremeButton, modeRadioGroupId);

    warmButton.onClick = [this] { setModeIndex (0); };
    heavyButton.onClick = [this] { setModeIndex (1); };
    extremeButton.onClick = [this] { setModeIndex (2); };

    addAndMakeVisible (warmButton);
    addAndMakeVisible (heavyButton);
    addAndMakeVisible (extremeButton);

    audioProcessor.apvts.addParameterListener ("mode", this);
    updateModeButtonStates();
}

SCREAMERAudioProcessorEditor::~SCREAMERAudioProcessorEditor()
{
    audioProcessor.apvts.removeParameterListener ("mode", this);
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
}

void SCREAMERAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour (juce::Colours::white);
    g.setFont (24.0f);
    g.drawFittedText ("SCREAMER", getLocalBounds().removeFromTop (40),
                      juce::Justification::centred, 1);
}

void SCREAMERAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (50);

    auto modeArea = area.removeFromBottom (70);
    modeLabel.setBounds (modeArea.removeFromTop (22));

    auto buttonRow = modeArea.reduced (20, 4);
    const int buttonWidth = buttonRow.getWidth() / 3;

    warmButton.setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (4, 0));
    heavyButton.setBounds (buttonRow.removeFromLeft (buttonWidth).reduced (4, 0));
    extremeButton.setBounds (buttonRow.reduced (4, 0));

    auto sliderArea = area.reduced (100, 10);
    driveLabel.setBounds (sliderArea.removeFromTop (30));
    driveSlider.setBounds (sliderArea);
}
