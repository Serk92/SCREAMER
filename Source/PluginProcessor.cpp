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
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "mode",
        "Mode",
        juce::StringArray { "Warm", "Heavy", "Extreme" },
        1
    ));

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
}

const juce::String SCREAMERAudioProcessor::getProgramName (int index)
{
    return {};
}

void SCREAMERAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void SCREAMERAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
}

void SCREAMERAudioProcessor::releaseResources()
{
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

void SCREAMERAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    auto* modeParam = apvts.getRawParameterValue("mode");
    int mode = 1; //Default heavy
    if (modeParam != nullptr)
        mode = static_cast<int>(modeParam->load());

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* driveParam = apvts.getRawParameterValue ("drive");
    const float drive = driveParam != nullptr ? driveParam->load() : 1.0f;

    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            const float input = channelData[sample];

            float output = input;

            if (mode == 0) // Warm
            {
                const float preGain = drive * 3.0f;
                output = std::tanh(input * preGain) * 0.8f;
            }
            else if (mode == 1) // Heavy
            {
                const float preGain = drive * 10.0f;
                output = std::tanh(input * preGain) * 0.5f;
            }
            else if (mode == 2) // Extreme
            {
                const float preGain = drive * 25.0f;
                const float clipped = juce::jlimit(-1.0f, 1.0f, input * preGain);
                output = clipped * 0.3f;
            }

            channelData[sample] = output;
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
