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
class SCREAMERAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor&);
    ~SCREAMERAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    SCREAMERAudioProcessor& audioProcessor;
    
    juce::Slider driveSlider;
    juce::Label driveLabel;
    
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCREAMERAudioProcessorEditor)
};
