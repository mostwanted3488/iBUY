#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine.h"

class IBuyProcessor : public juce::AudioProcessor
{
public:
    IBuyProcessor();
    ~IBuyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "iBuyorbealoser"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    ibuy::Engine engine;
    std::atomic<int> theme { 0 };          // 0 = Light, 1 = Dark (saved with the session)
    std::atomic<double> hostBpm { 120.0 };

private:
    std::atomic<float>* pEq = nullptr;
    std::atomic<float>* pLowBattery = nullptr;
    std::atomic<float>* pNextRoom = nullptr;
    std::atomic<float>* pEarbuds = nullptr;
    std::atomic<float>* pReverb = nullptr;
    std::atomic<float>* pDelay = nullptr;
    std::atomic<float>* pChorus = nullptr;
    std::atomic<float>* pFilter = nullptr;
    std::atomic<float>* pBitcrush = nullptr;
    std::atomic<float>* pPower = nullptr;
    std::atomic<float>* pLoopDiv = nullptr;
    std::atomic<float>* pLoopHold = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IBuyProcessor)
};
