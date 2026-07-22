#include "PluginProcessor.h"
#include "PluginEditor.h"

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

void SCREAMERAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    outputFade.reset (sampleRate, fadeInLengthSeconds);
    outputFade.setCurrentAndTargetValue (0.0f);
    outputFade.setTargetValue (1.0f);
    wasSuspendedLastBlock = false;

    prepareOversampling (samplesPerBlock);
}

void SCREAMERAudioProcessor::releaseResources()
{
    outputFade.setCurrentAndTargetValue (0.0f);

    if (oversampling != nullptr)
        oversampling->reset();

    dryDelay.reset();
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
            float wet = input;

            if (mode == 0) // Warm
            {
                const float preGain = drive * 3.0f;
                wet = std::tanh (input * preGain) * 0.8f;
            }
            else if (mode == 1) // Heavy
            {
                const float preGain = drive * 10.0f;
                wet = std::tanh (input * preGain) * 0.5f;
            }
            else if (mode == 2) // Extreme
            {
                const float preGain = drive * 25.0f;
                const float clipped = juce::jlimit (-1.0f, 1.0f, input * preGain);
                wet = clipped * 0.3f;
            }

            samples[sample] = wet;
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

    auto* driveParam = apvts.getRawParameterValue ("drive");
    const float drive = driveParam != nullptr ? driveParam->load() : 1.0f;

    auto* mixParam = apvts.getRawParameterValue ("mix");
    const float mix = mixParam != nullptr ? mixParam->load() : 1.0f;
    const float dryMix = 1.0f - mix;

    const int numSamples = buffer.getNumSamples();
    const float dryDelayInSamples = static_cast<float> (oversamplingLatencySamples);

    juce::AudioBuffer<float> dryInputBuffer;
    dryInputBuffer.makeCopyOf (buffer, true);

    {
        juce::dsp::AudioBlock<float> wetBlock (buffer);
        juce::dsp::AudioBlock<float> oversampledBlock = oversampling->processSamplesUp (wetBlock);
        processNonlinear (oversampledBlock, drive, mode);
        oversampling->processSamplesDown (wetBlock);
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
