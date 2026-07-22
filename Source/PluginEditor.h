/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class InvisibleControlLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&,
                           int, int, int, int,
                           float, float, float,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&,
                               juce::Button&,
                               const juce::Colour&,
                               bool,
                               bool) override;

    void drawButtonText (juce::Graphics&,
                         juce::TextButton&,
                         bool,
                         bool) override;
};

//==============================================================================
class SCREAMERAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor&);
    ~SCREAMERAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void loadUiAssets();
    void layoutControls();
    void setMode (int index);

    SCREAMERAudioProcessor& audioProcessor;

    juce::Image mainPanelImage;

    InvisibleControlLookAndFeel invisibleLookAndFeel;

    juce::Slider driveSlider;
    juce::Slider mixSlider;
    juce::TextButton warmButton;
    juce::TextButton heavyButton;
    juce::TextButton extremeButton;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCREAMERAudioProcessorEditor)
};
