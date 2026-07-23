#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr float dcBlockerHz = 8.0f;
    constexpr float filterQ = 0.707f;

    constexpr std::array<float, 3> modePreHighPassHz  { 38.0f,  85.0f, 120.0f };
    constexpr std::array<float, 3> modePostLowPassHz  { 14000.0f, 9500.0f, 7500.0f };
    constexpr std::array<float, 3> modeOutputGains    { 0.82f, 0.58f, 0.48f };

    float clampCutoffHz (float frequencyHz, double sampleRate)
    {
        const float nyquist = static_cast<float> (sampleRate * 0.49);
        return juce::jlimit (1.0f, nyquist, frequencyHz);
    }

    // Warm: soft asymmetric saturation — even harmonics, keeps pick attack readable.
    float warmAsymmetricSaturation (float x, float drive)
    {
        const float gain = drive * 2.8f;
        const float driven = x * gain;

        if (driven >= 0.0f)
            return std::tanh (driven * 0.72f);

        return std::tanh (driven * 1.08f) * 0.94f;
    }

    // Heavy: two moderate soft-clipping stages — tighter but still defined mutes.
    float heavyTwoStageSoftClip (float x, float drive)
    {
        const float stageOneGain = drive * 4.5f;
        const float stageTwoGain = 0.65f + drive * 0.08f;

        const float stageOne = std::tanh (x * stageOneGain);
        return std::tanh (stageOne * stageTwoGain);
    }

    // Extreme: staged saturation plus controlled soft clip — compressed but not brick-walled.
    float extremeStagedSaturation (float x, float drive)
    {
        const float gain = drive * 7.5f;
        float s = x * gain;

        s = s / (1.0f + std::abs (s));          // gentle rational soft saturate
        s = std::tanh (s * 1.35f);              // add controlled upper harmonics
        return s / (1.0f + 0.45f * s * s);      // final soft knee instead of hard clip
    }
}

SCREAMERAudioProcessor::SCREAMERAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
    , apvts (*this, nullptr, "PARAMETERS", createParameters())
{
}

SCREAMERAudioProcessor::~SCREAMERAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SCREAMERAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
                                                                  juce::ParameterID{"drive", 1},
                                                                  "Drive",
                                                                  juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f),
                                                                  1.0f));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 },
        "Mode",
        juce::StringArray { "Warm", "Heavy", "Extreme" },
        1));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
        1.0f));

    return { params.begin(), params.end() };
}

const juce::String SCREAMERAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SCREAMERAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool SCREAMERAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool SCREAMERAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double SCREAMERAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SCREAMERAudioProcessor::getNumPrograms()
{
    return 1;
}

int SCREAMERAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SCREAMERAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String SCREAMERAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void SCREAMERAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void SCREAMERAudioProcessor::prepareOversampling (int samplesPerBlock)
{
    const auto numChannels = static_cast<size_t> (juce::jmax (1, getTotalNumOutputChannels()));

    if (oversampling == nullptr || preparedOversamplingChannels != numChannels)
    {
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            numChannels,
            oversamplingFactorOrder,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            true);

        preparedOversamplingChannels = numChannels;
    }

    oversampling->initProcessing (static_cast<size_t> (samplesPerBlock));
    oversampling->reset();

    oversamplingLatencySamples = juce::roundToInt (oversampling->getLatencyInSamples());
    setLatencySamples (oversamplingLatencySamples);

    const juce::dsp::ProcessSpec dryDelaySpec {
        getSampleRate(),
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (numChannels)
    };

    const int dryDelayCapacity = oversamplingLatencySamples + samplesPerBlock + 8;

    dryDelay.prepare (dryDelaySpec);
    dryDelay.setMaximumDelayInSamples (dryDelayCapacity);
    dryDelay.setDelay (static_cast<float> (oversamplingLatencySamples));
    dryDelay.reset();
}

void SCREAMERAudioProcessor::prepareModeProcessing (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = static_cast<size_t> (juce::jmax (1, getTotalNumOutputChannels()));
    preparedFilterChannels = numChannels;

    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (numChannels)
    };

    const auto dcBlockerCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate,
        clampCutoffHz (dcBlockerHz, sampleRate),
        filterQ);

    for (int mode = 0; mode < numModes; ++mode)
    {
        auto& coeffs = preparedModeCoefficients[static_cast<size_t> (mode)];

        coeffs.preHighPass = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            sampleRate,
            clampCutoffHz (modePreHighPassHz[static_cast<size_t> (mode)], sampleRate),
            filterQ);

        coeffs.postLowPass = juce::dsp::IIR::Coefficients<float>::makeLowPass (
            sampleRate,
            clampCutoffHz (modePostLowPassHz[static_cast<size_t> (mode)], sampleRate),
            filterQ);

        coeffs.dcBlocker = dcBlockerCoeffs;
        coeffs.outputGain = modeOutputGains[static_cast<size_t> (mode)];

        for (size_t channel = 0; channel < maxAudioChannels; ++channel)
        {
            auto& filters = modeFilterStates[static_cast<size_t> (mode)].channels[channel];

            filters.preHighPass.prepare (spec);
            filters.postLowPass.prepare (spec);
            filters.dcBlocker.prepare (spec);

            filters.preHighPass.coefficients = coeffs.preHighPass;
            filters.postLowPass.coefficients = coeffs.postLowPass;
            filters.dcBlocker.coefficients   = coeffs.dcBlocker;
            filters.preHighPass.reset();
            filters.postLowPass.reset();
            filters.dcBlocker.reset();
        }
    }

    const int numChannelsInt = static_cast<int> (numChannels);

    dryInputBuffer.setSize (numChannelsInt, samplesPerBlock, false, false, true);
    crossfadePathBufferA.setSize (numChannelsInt, samplesPerBlock, false, false, true);
    crossfadePathBufferB.setSize (numChannelsInt, samplesPerBlock, false, false, true);
    smoothedDrivePerSample.setSize (1, samplesPerBlock, false, false, true);
    smoothedMixPerSample.setSize (1, samplesPerBlock, false, false, true);
    smoothedModeOutputGainPerSample.setSize (1, samplesPerBlock, false, false, true);

    modeCrossfadeTotalSamples = juce::jmax (1, juce::roundToInt (sampleRate * modeCrossfadeLengthSeconds));
    modeCrossfadeSamplesRemaining = 0;
    modeCrossfadeGain = 0.0f;

    activeMode = 1;
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode")))
        activeMode = modeParam->getIndex();

    crossfadeFromMode = activeMode;
    crossfadeToMode = activeMode;
}

void SCREAMERAudioProcessor::resetModeFilterStates()
{
    for (auto& modeState : modeFilterStates)
        for (size_t channel = 0; channel < preparedFilterChannels; ++channel)
        {
            modeState.channels[channel].preHighPass.reset();
            modeState.channels[channel].postLowPass.reset();
            modeState.channels[channel].dcBlocker.reset();
        }
}

void SCREAMERAudioProcessor::handleModeChange (int newMode)
{
    newMode = juce::jlimit (0, numModes - 1, newMode);

    if (modeCrossfadeSamplesRemaining > 0)
    {
        if (newMode == crossfadeToMode)
            return;

        crossfadeFromMode = crossfadeToMode;
    }
    else
    {
        if (newMode == activeMode)
            return;

        crossfadeFromMode = activeMode;
    }

    crossfadeToMode = newMode;
    modeCrossfadeGain = 0.0f;
    modeCrossfadeSamplesRemaining = modeCrossfadeTotalSamples;
}

void SCREAMERAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    outputFade.reset (sampleRate, fadeInLengthSeconds);
    outputFade.setCurrentAndTargetValue (0.0f);
    outputFade.setTargetValue (1.0f);
    wasSuspendedLastBlock = false;

    prepareOversampling (samplesPerBlock);
    prepareModeProcessing (sampleRate, samplesPerBlock);

    const float initialDrive = apvts.getRawParameterValue ("drive") != nullptr
                                   ? apvts.getRawParameterValue ("drive")->load()
                                   : 1.0f;
    const float initialMix = apvts.getRawParameterValue ("mix") != nullptr
                                 ? apvts.getRawParameterValue ("mix")->load()
                                 : 1.0f;
    const float initialOutputGain = preparedModeCoefficients[static_cast<size_t> (activeMode)].outputGain;

    smoothedDrive.reset (sampleRate, driveMixSmoothingSeconds);
    smoothedMix.reset (sampleRate, driveMixSmoothingSeconds);
    smoothedModeOutputGain.reset (sampleRate, modeOutputGainSmoothingSeconds);

    smoothedDrive.setCurrentAndTargetValue (initialDrive);
    smoothedMix.setCurrentAndTargetValue (initialMix);
    smoothedModeOutputGain.setCurrentAndTargetValue (initialOutputGain);
}

void SCREAMERAudioProcessor::releaseResources()
{
    outputFade.setCurrentAndTargetValue (0.0f);

    if (oversampling != nullptr)
        oversampling->reset();

    dryDelay.reset();
    resetModeFilterStates();

    modeCrossfadeSamplesRemaining = 0;
    modeCrossfadeGain = 0.0f;
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool SCREAMERAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
   #endif
}
#endif

void SCREAMERAudioProcessor::processNonlinear (juce::dsp::AudioBlock<float>& block,
                                               const float* drivePerSample,
                                               int numBaseSamples,
                                               int mode) const
{
    const size_t baseNumSamples = static_cast<size_t> (juce::jmax (0, numBaseSamples));
    const size_t osNumSamples = block.getNumSamples();
    const size_t osFactor = baseNumSamples > 0 ? osNumSamples / baseNumSamples : 1;

    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* samples = block.getChannelPointer (channel);

        for (size_t baseSample = 0; baseSample < baseNumSamples; ++baseSample)
        {
            const float drive = drivePerSample[baseSample];

            for (size_t os = 0; os < osFactor; ++os)
            {
                const size_t osIndex = baseSample * osFactor + os;
                const float input = samples[osIndex];

                if (mode == 0)
                    samples[osIndex] = warmAsymmetricSaturation (input, drive);
                else if (mode == 1)
                    samples[osIndex] = heavyTwoStageSoftClip (input, drive);
                else
                    samples[osIndex] = extremeStagedSaturation (input, drive);
            }
        }
    }
}

void SCREAMERAudioProcessor::fillParameterSmoothedBuffers (int numSamples)
{
    auto* driveValues = smoothedDrivePerSample.getWritePointer (0);
    auto* mixValues = smoothedMixPerSample.getWritePointer (0);
    auto* outputGainValues = smoothedModeOutputGainPerSample.getWritePointer (0);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        driveValues[sample] = smoothedDrive.getNextValue();
        mixValues[sample] = smoothedMix.getNextValue();
        outputGainValues[sample] = smoothedModeOutputGain.getNextValue();
    }
}

void SCREAMERAudioProcessor::processWetPath (int mode,
                                             const juce::AudioBuffer<float>& input,
                                             juce::AudioBuffer<float>& output,
                                             const float* drivePerSample)
{
    const int numSamples = input.getNumSamples();
    const auto numChannels = static_cast<size_t> (input.getNumChannels());

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        output.copyFrom (static_cast<int> (channel), 0,
                         input, static_cast<int> (channel), 0,
                         numSamples);
    }

    auto& modeFilters = modeFilterStates[static_cast<size_t> (mode)];

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto& filters = modeFilters.channels[channel];
        auto* wetData = output.getWritePointer (static_cast<int> (channel));

        for (int sample = 0; sample < numSamples; ++sample)
            wetData[sample] = filters.preHighPass.processSample (wetData[sample]);
    }

    {
        juce::dsp::AudioBlock<float> wetBlock (output);
        juce::dsp::AudioBlock<float> oversampledBlock = oversampling->processSamplesUp (wetBlock);
        processNonlinear (oversampledBlock, drivePerSample, numSamples, mode);
        oversampling->processSamplesDown (wetBlock);
    }

    for (size_t channel = 0; channel < numChannels; ++channel)
    {
        auto& filters = modeFilters.channels[channel];
        auto* wetData = output.getWritePointer (static_cast<int> (channel));

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float wet = filters.postLowPass.processSample (wetData[sample]);
            wet = filters.dcBlocker.processSample (wet);
            wetData[sample] = wet;
        }
    }
}

void SCREAMERAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    int requestedMode = 1;
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode")))
        requestedMode = modeParam->getIndex();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (isSuspended())
    {
        wasSuspendedLastBlock = true;
        return;
    }

    if (wasSuspendedLastBlock)
    {
        outputFade.setCurrentAndTargetValue (0.0f);
        outputFade.setTargetValue (1.0f);
        wasSuspendedLastBlock = false;
    }

    if (oversampling == nullptr)
        return;

    handleModeChange (requestedMode);

    auto* driveParam = apvts.getRawParameterValue ("drive");
    const float driveTarget = driveParam != nullptr ? driveParam->load() : 1.0f;

    auto* mixParam = apvts.getRawParameterValue ("mix");
    const float mixTarget = mixParam != nullptr ? mixParam->load() : 1.0f;

    smoothedDrive.setTargetValue (driveTarget);
    smoothedMix.setTargetValue (mixTarget);

    float outputGainTarget = preparedModeCoefficients[static_cast<size_t> (activeMode)].outputGain;

    if (modeCrossfadeSamplesRemaining > 0)
    {
        const float fromGain = preparedModeCoefficients[static_cast<size_t> (crossfadeFromMode)].outputGain;
        const float toGain = preparedModeCoefficients[static_cast<size_t> (crossfadeToMode)].outputGain;
        outputGainTarget = fromGain + modeCrossfadeGain * (toGain - fromGain);
    }

    smoothedModeOutputGain.setTargetValue (outputGainTarget);

    const int numSamples = buffer.getNumSamples();

    fillParameterSmoothedBuffers (numSamples);

    const float* drivePerSample = smoothedDrivePerSample.getReadPointer (0);
    const float* mixPerSample = smoothedMixPerSample.getReadPointer (0);
    const float* modeOutputGainPerSample = smoothedModeOutputGainPerSample.getReadPointer (0);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryInputBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    if (modeCrossfadeSamplesRemaining > 0)
    {
        processWetPath (crossfadeFromMode, dryInputBuffer, crossfadePathBufferA, drivePerSample);
        processWetPath (crossfadeToMode, dryInputBuffer, crossfadePathBufferB, drivePerSample);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float toGain = modeCrossfadeGain;
            const float fromGain = 1.0f - toGain;
            const float outputGain = modeOutputGainPerSample[sample];

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
            {
                const float wetFrom = crossfadePathBufferA.getSample (channel, sample);
                const float wetTo = crossfadePathBufferB.getSample (channel, sample);
                const float wet = (wetFrom * fromGain + wetTo * toGain) * outputGain;
                buffer.setSample (channel, sample, wet);
            }

            if (modeCrossfadeSamplesRemaining > 0)
            {
                --modeCrossfadeSamplesRemaining;
                modeCrossfadeGain = 1.0f - (static_cast<float> (modeCrossfadeSamplesRemaining)
                                              / static_cast<float> (modeCrossfadeTotalSamples));
            }
        }

        if (modeCrossfadeSamplesRemaining <= 0)
        {
            activeMode = crossfadeToMode;
            modeCrossfadeGain = 1.0f;
        }
    }
    else
    {
        processWetPath (activeMode, dryInputBuffer, buffer, drivePerSample);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float outputGain = modeOutputGainPerSample[sample];

            for (int channel = 0; channel < totalNumInputChannels; ++channel)
            {
                const float wet = buffer.getSample (channel, sample);
                buffer.setSample (channel, sample, wet * outputGain);
            }
        }
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float fade = outputFade.getNextValue();
        const float mix = mixPerSample[sample];
        const float dryMix = 1.0f - mix;

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            const float input = dryInputBuffer.getSample (channel, sample);
            dryDelay.pushSample (channel, input);

            const float delayedDry = dryDelay.popSample (channel);
            const float wet = buffer.getSample (channel, sample);
            const float mixed = delayedDry * dryMix + wet * mix;

            buffer.setSample (channel, sample, input + fade * (mixed - input));
        }
    }
}

juce::AudioProcessorEditor* SCREAMERAudioProcessor::createEditor()
{
    return new SCREAMERAudioProcessorEditor (*this);
}

void SCREAMERAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void SCREAMERAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr)
        if (xmlState->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SCREAMERAudioProcessor();
}
