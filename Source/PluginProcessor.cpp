#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
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
            false);

        preparedOversamplingChannels = numChannels;
    }

    oversampling->initProcessing (static_cast<size_t> (samplesPerBlock));
    oversampling->reset();

    oversamplingLatencySamples = static_cast<int> (oversampling->getLatencyInSamples());
    setLatencySamples (oversamplingLatencySamples);

    const juce::dsp::ProcessSpec dryDelaySpec {
        getSampleRate(),
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (numChannels)
    };

    dryDelay.prepare (dryDelaySpec);
    dryDelay.setMaximumDelayInSamples (oversamplingLatencySamples + samplesPerBlock + 1);
    dryDelay.setDelay (static_cast<float> (oversamplingLatencySamples));
    dryDelay.reset();
}

void SCREAMERAudioProcessor::prepareProcessingFilters (double sampleRate, int samplesPerBlock)
{
    const auto numChannels = static_cast<size_t> (juce::jmax (1, getTotalNumOutputChannels()));
    preparedFilterChannels = numChannels;

    const juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32> (samplesPerBlock),
        static_cast<juce::uint32> (numChannels)
    };

    for (size_t channel = 0; channel < maxAudioChannels; ++channel)
    {
        channelFilters[channel].preHighPass.prepare (spec);
        channelFilters[channel].postLowPass.prepare (spec);
        channelFilters[channel].dcBlocker.prepare (spec);
    }

    dryInputBuffer.setSize (static_cast<int> (numChannels), samplesPerBlock, false, false, true);

    preparedFilterMode = -1;

    int mode = 1;
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode")))
        mode = modeParam->getIndex();

    updateProcessingFiltersForMode (mode, sampleRate);
    resetProcessingFilters();
}

void SCREAMERAudioProcessor::updateProcessingFiltersForMode (int mode, double sampleRate)
{
    float preHighPassHz = 85.0f;
    float postLowPassHz = 9500.0f;
    modeOutputGain = 0.58f;

    switch (mode)
    {
        case 0: // Warm
            preHighPassHz = 38.0f;
            postLowPassHz = 14000.0f;
            modeOutputGain = 0.82f;
            break;

        case 1: // Heavy
            preHighPassHz = 85.0f;
            postLowPassHz = 9500.0f;
            modeOutputGain = 0.58f;
            break;

        case 2: // Extreme
            preHighPassHz = 120.0f;
            postLowPassHz = 7500.0f;
            modeOutputGain = 0.48f;
            break;

        default:
            break;
    }

    const auto preHighPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, preHighPassHz, 0.707f);
    const auto postLowPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, postLowPassHz, 0.707f);
    const auto dcBlockerCoeffs   = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 8.0f, 0.707f);

    for (size_t channel = 0; channel < preparedFilterChannels; ++channel)
    {
        channelFilters[channel].preHighPass.coefficients = preHighPassCoeffs;
        channelFilters[channel].postLowPass.coefficients = postLowPassCoeffs;
        channelFilters[channel].dcBlocker.coefficients   = dcBlockerCoeffs;
        channelFilters[channel].preHighPass.reset();
        channelFilters[channel].postLowPass.reset();
        channelFilters[channel].dcBlocker.reset();
    }

    preparedFilterMode = mode;
}

void SCREAMERAudioProcessor::resetProcessingFilters()
{
    for (size_t channel = 0; channel < preparedFilterChannels; ++channel)
    {
        channelFilters[channel].preHighPass.reset();
        channelFilters[channel].postLowPass.reset();
        channelFilters[channel].dcBlocker.reset();
    }
}

void SCREAMERAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    outputFade.reset (sampleRate, fadeInLengthSeconds);
    outputFade.setCurrentAndTargetValue (0.0f);
    outputFade.setTargetValue (1.0f);
    wasSuspendedLastBlock = false;

    prepareOversampling (samplesPerBlock);
    prepareProcessingFilters (sampleRate, samplesPerBlock);
}

void SCREAMERAudioProcessor::releaseResources()
{
    outputFade.setCurrentAndTargetValue (0.0f);

    if (oversampling != nullptr)
        oversampling->reset();

    dryDelay.reset();
    resetProcessingFilters();
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
                                               float drive,
                                               int mode) const
{
    for (size_t channel = 0; channel < block.getNumChannels(); ++channel)
    {
        auto* samples = block.getChannelPointer (channel);

        for (size_t sample = 0; sample < block.getNumSamples(); ++sample)
        {
            const float input = samples[sample];

            if (mode == 0)
                samples[sample] = warmAsymmetricSaturation (input, drive);
            else if (mode == 1)
                samples[sample] = heavyTwoStageSoftClip (input, drive);
            else
                samples[sample] = extremeStagedSaturation (input, drive);
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

    int mode = 1; // default: Heavy
    if (auto* modeParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode")))
        mode = modeParam->getIndex();

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

    if (mode != preparedFilterMode)
        updateProcessingFiltersForMode (mode, getSampleRate());

    auto* driveParam = apvts.getRawParameterValue ("drive");
    const float drive = driveParam != nullptr ? driveParam->load() : 1.0f;

    auto* mixParam = apvts.getRawParameterValue ("mix");
    const float mix = mixParam != nullptr ? mixParam->load() : 1.0f;
    const float dryMix = 1.0f - mix;

    const int numSamples = buffer.getNumSamples();
    const float dryDelayInSamples = static_cast<float> (oversamplingLatencySamples);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
        dryInputBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto& filters = channelFilters[static_cast<size_t> (channel)];
        auto* wetData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
            wetData[sample] = filters.preHighPass.processSample (wetData[sample]);
    }

    {
        juce::dsp::AudioBlock<float> wetBlock (buffer);
        juce::dsp::AudioBlock<float> oversampledBlock = oversampling->processSamplesUp (wetBlock);
        processNonlinear (oversampledBlock, drive, mode);
        oversampling->processSamplesDown (wetBlock);
    }

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto& filters = channelFilters[static_cast<size_t> (channel)];
        auto* wetData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float wet = filters.postLowPass.processSample (wetData[sample]);
            wet = filters.dcBlocker.processSample (wet);
            wetData[sample] = wet * modeOutputGain;
        }
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float fade = outputFade.getNextValue();

        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            const float input = dryInputBuffer.getSample (channel, sample);
            dryDelay.pushSample (channel, input);

            const float delayedDry = dryDelay.popSample (channel, dryDelayInSamples);
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
