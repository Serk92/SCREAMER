/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
SCREAMERAudioProcessorEditor::SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    
    driveSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    driveSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    driveSlider.setRange (1.0, 20.0, 0.1);
    addAndMakeVisible (driveSlider);

    driveLabel.setText ("Drive", juce::dontSendNotification);
    driveLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (driveLabel);

    driveAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.apvts,
        "drive",
        driveSlider);
}

SCREAMERAudioProcessorEditor::~SCREAMERAudioProcessorEditor()
{
}

//==============================================================================
void SCREAMERAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
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

        auto sliderArea = area.reduced (100, 20);
        driveLabel.setBounds (sliderArea.removeFromTop (30));
        driveSlider.setBounds (sliderArea.removeFromTop (180));
}
