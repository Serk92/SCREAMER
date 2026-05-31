/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class SCREAMERAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor&);
    ~SCREAMERAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void setModeIndex (int index);
    void updateModeButtonStates();

    SCREAMERAudioProcessor& audioProcessor;

    juce::Slider driveSlider;
    juce::Label driveLabel;

    juce::Label modeLabel;
    juce::TextButton warmButton   { "Warm" };
    juce::TextButton heavyButton  { "Heavy" };
    juce::TextButton extremeButton { "Extreme" };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCREAMERAudioProcessorEditor)
};
