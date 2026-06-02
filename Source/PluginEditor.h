/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/** Custom paint for WARM / HEAVY / EXTREME mode buttons. */
class ScreamerModeButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;
};

//==============================================================================
class DisplayPanel : public juce::Component
{
public:
    void paint (juce::Graphics& g) override;

private:
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> plotArea) const;
    void drawTransferCurve (juce::Graphics& g, juce::Rectangle<float> plotArea) const;
};

//==============================================================================
class SCREAMERAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                      private juce::AudioProcessorValueTreeState::Listener
{
public:
    SCREAMERAudioProcessorEditor (SCREAMERAudioProcessor&);
    ~SCREAMERAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void setModeIndex (int index);
    void updateModeButtonStates();

    void layoutLeftPanel (juce::Rectangle<int> area);
    void layoutRightPanel (juce::Rectangle<int> area);

    SCREAMERAudioProcessor& audioProcessor;

    juce::Rectangle<int> leftPanelBounds;

    ScreamerModeButtonLookAndFeel modeButtonLookAndFeel;

    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::Slider mixSlider;
    juce::Label mixLabel;

    juce::TextButton warmButton;
    juce::TextButton heavyButton;
    juce::TextButton extremeButton;

    DisplayPanel displayPanel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCREAMERAudioProcessorEditor)
};
