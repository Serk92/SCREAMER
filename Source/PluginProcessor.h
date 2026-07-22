#pragma once

#include <JuceHeader.h>

class SCREAMERAudioProcessor : public juce::AudioProcessor
{
public:
    SCREAMERAudioProcessor();
    ~SCREAMERAudioProcessor() override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr double fadeInLengthSeconds = 0.03;
    static constexpr size_t oversamplingFactorOrder = 2; // 2^2 = 4x
    static constexpr int maxAudioChannels = 2;

    struct ChannelProcessingFilters
    {
        juce::dsp::IIR::Filter<float> preHighPass;
        juce::dsp::IIR::Filter<float> postLowPass;
        juce::dsp::IIR::Filter<float> dcBlocker;
    };

    void prepareOversampling (int samplesPerBlock);
    void prepareProcessingFilters (double sampleRate, int samplesPerBlock);
    void updateProcessingFiltersForMode (int mode, double sampleRate);
    void resetProcessingFilters();
    void processNonlinear (juce::dsp::AudioBlock<float>& block, float drive, int mode) const;

    juce::SmoothedValue<float> outputFade;
    bool wasSuspendedLastBlock = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;
    std::array<ChannelProcessingFilters, maxAudioChannels> channelFilters {};
    juce::AudioBuffer<float> dryInputBuffer;

    int oversamplingLatencySamples = 0;
    int preparedFilterMode = -1;
    float modeOutputGain = 1.0f;
    size_t preparedOversamplingChannels = 0;
    size_t preparedFilterChannels = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCREAMERAudioProcessor)
};
