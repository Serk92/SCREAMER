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
    static constexpr double driveMixSmoothingSeconds = 0.03;
    static constexpr double modeOutputGainSmoothingSeconds = 0.0175;
    static constexpr double modeCrossfadeLengthSeconds = 0.0175;
    static constexpr size_t oversamplingFactorOrder = 2; // 2^2 = 4x
    static constexpr int maxAudioChannels = 2;
    static constexpr int numModes = 3;

    struct ChannelProcessingFilters
    {
        juce::dsp::IIR::Filter<float> preHighPass;
        juce::dsp::IIR::Filter<float> postLowPass;
        juce::dsp::IIR::Filter<float> dcBlocker;
    };

    struct ModeFilterCoefficients
    {
        juce::dsp::IIR::Filter<float>::CoefficientsPtr preHighPass;
        juce::dsp::IIR::Filter<float>::CoefficientsPtr postLowPass;
        juce::dsp::IIR::Filter<float>::CoefficientsPtr dcBlocker;
        float outputGain = 1.0f;
    };

    struct ModeFilterState
    {
        std::array<ChannelProcessingFilters, maxAudioChannels> channels {};
    };

    void prepareOversampling (int samplesPerBlock);
    void prepareModeProcessing (double sampleRate, int samplesPerBlock);
    void resetModeFilterStates();
    size_t getActiveChannelCount() const;
    void handleModeChange (int newMode);
    void processWetPath (int mode,
                         const juce::AudioBuffer<float>& input,
                         juce::AudioBuffer<float>& output,
                         const float* drivePerSample);
    void fillParameterSmoothedBuffers (int numSamples);
    void processNonlinear (juce::dsp::AudioBlock<float>& block,
                           const float* drivePerSample,
                           int numBaseSamples,
                           int mode) const;

    juce::SmoothedValue<float> outputFade;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDrive;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedModeOutputGain;
    bool wasSuspendedLastBlock = false;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay;

    std::array<ModeFilterCoefficients, numModes> preparedModeCoefficients {};
    std::array<ModeFilterState, numModes> modeFilterStates {};

    juce::AudioBuffer<float> dryInputBuffer;
    juce::AudioBuffer<float> crossfadePathBufferA;
    juce::AudioBuffer<float> crossfadePathBufferB;
    juce::AudioBuffer<float> smoothedDrivePerSample;
    juce::AudioBuffer<float> smoothedMixPerSample;
    juce::AudioBuffer<float> smoothedModeOutputGainPerSample;

    int oversamplingLatencySamples = 0;
    int activeMode = 1;
    int crossfadeFromMode = 1;
    int crossfadeToMode = 1;
    int modeCrossfadeSamplesRemaining = 0;
    int modeCrossfadeTotalSamples = 0;
    float modeCrossfadeGain = 0.0f;
    size_t preparedOversamplingChannels = 0;
    size_t preparedFilterChannels = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SCREAMERAudioProcessor)
};
