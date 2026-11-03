#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout IBuyProcessor::createLayout()
{
    using juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { "eq", 1 }, "EQ Preset",
        juce::StringArray { "Flat", "Bass Booster", "Treble Booster", "Lounge", "Small Speakers", "Spoken Word" }, 0));

    auto addBool = [&layout] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    addBool ("lowBattery", "Low Battery", false);
    addBool ("nextRoom", "Next Room", false);
    addBool ("earbuds", "Earbuds", false);
    addBool ("reverb", "Reverb", false);
    addBool ("delay", "Delay", false);
    addBool ("chorus", "Chorus", false);
    addBool ("filter", "Filter", false);
    addBool ("bitcrush", "Bitcrush", false);
    addBool ("power", "Power", true);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterID { "loopDiv", 1 }, "Looper Length",
        juce::StringArray { "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" }, 2));

    addBool ("loopHold", "Looper Hold", false);

    return layout;
}

IBuyProcessor::IBuyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "iBuy", createLayout())
{
    pEq = apvts.getRawParameterValue ("eq");
    pLowBattery = apvts.getRawParameterValue ("lowBattery");
    pNextRoom = apvts.getRawParameterValue ("nextRoom");
    pEarbuds = apvts.getRawParameterValue ("earbuds");
    pReverb = apvts.getRawParameterValue ("reverb");
    pDelay = apvts.getRawParameterValue ("delay");
    pChorus = apvts.getRawParameterValue ("chorus");
    pFilter = apvts.getRawParameterValue ("filter");
    pBitcrush = apvts.getRawParameterValue ("bitcrush");
    pPower = apvts.getRawParameterValue ("power");
    pLoopDiv = apvts.getRawParameterValue ("loopDiv");
    pLoopHold = apvts.getRawParameterValue ("loopHold");
}

void IBuyProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool IBuyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void IBuyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    double bpm = 120.0;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    hostBpm.store (bpm);

    ibuy::Params p;
    p.eq = (int) std::lround (pEq->load());
    p.lowBattery = pLowBattery->load() > 0.5f;
    p.nextRoom = pNextRoom->load() > 0.5f;
    p.earbuds = pEarbuds->load() > 0.5f;
    p.reverb = pReverb->load() > 0.5f;
    p.delay = pDelay->load() > 0.5f;
    p.chorus = pChorus->load() > 0.5f;
    p.filter = pFilter->load() > 0.5f;
    p.bitcrush = pBitcrush->load() > 0.5f;
    p.power = pPower->load() > 0.5f;
    p.loopDiv = (int) std::lround (pLoopDiv->load());
    p.loopHold = pLoopHold->load() > 0.5f;
    p.bpm = bpm;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    engine.process (buffer.getArrayOfWritePointers(), numCh, numSamples, p);
}

juce::AudioProcessorEditor* IBuyProcessor::createEditor()
{
    return new IBuyEditor (*this);
}

void IBuyProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("theme", theme.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void IBuyProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            theme.store ((int) tree.getProperty ("theme", 0));
            apvts.replaceState (tree);
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new IBuyProcessor();
}
