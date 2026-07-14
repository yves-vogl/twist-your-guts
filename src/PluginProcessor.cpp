#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

#include <cmath>

namespace
{
    // ~20ms smoothing ramp for gain changes: fast enough to feel responsive,
    // slow enough to avoid zipper noise on parameter automation.
    constexpr double gainRampDurationSeconds = 0.02;
}

//==============================================================================
TwistYourGutsAudioProcessor::TwistYourGutsAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    inputGainDb = apvts.getRawParameterValue (ParamIDs::inputGain);
    outputGainDb = apvts.getRawParameterValue (ParamIDs::outputGain);
    bypassFlag = apvts.getRawParameterValue (ParamIDs::bypass);
    outputClipEnabled = apvts.getRawParameterValue (ParamIDs::outputClip);
    crossoverFreqHz = apvts.getRawParameterValue (ParamIDs::crossoverFreq);
    lowLevelDb = apvts.getRawParameterValue (ParamIDs::lowLevel);
    highLevelDb = apvts.getRawParameterValue (ParamIDs::highLevel);
    bypassParameter = apvts.getParameter (ParamIDs::bypass);

    gateEnabled = apvts.getRawParameterValue (ParamIDs::gateEnabled);
    gateThresholdDb = apvts.getRawParameterValue (ParamIDs::gateThreshold);
    gateRatio = apvts.getRawParameterValue (ParamIDs::gateRatio);
    gateAttackMs = apvts.getRawParameterValue (ParamIDs::gateAttack);
    gateReleaseMs = apvts.getRawParameterValue (ParamIDs::gateRelease);

    lowCompThresholdDb = apvts.getRawParameterValue (ParamIDs::lowCompThreshold);
    lowCompRatio = apvts.getRawParameterValue (ParamIDs::lowCompRatio);
    lowCompAttackMs = apvts.getRawParameterValue (ParamIDs::lowCompAttack);
    lowCompReleaseMs = apvts.getRawParameterValue (ParamIDs::lowCompRelease);
    lowCompMakeupDb = apvts.getRawParameterValue (ParamIDs::lowCompMakeup);
    lowCompMixPercent = apvts.getRawParameterValue (ParamIDs::lowCompMix);

    highVoicingChoice = apvts.getRawParameterValue (ParamIDs::highVoicing);
    highDrivePercent = apvts.getRawParameterValue (ParamIDs::highDrive);
    highTonePercent = apvts.getRawParameterValue (ParamIDs::highTone);
    highBlendPercent = apvts.getRawParameterValue (ParamIDs::highBlend);

    eqEnabled = apvts.getRawParameterValue (ParamIDs::eqEnabled);
    eqLowShelfFreqHz = apvts.getRawParameterValue (ParamIDs::eqLowShelfFreq);
    eqLowShelfGainDb = apvts.getRawParameterValue (ParamIDs::eqLowShelfGain);
    eqPeak1FreqHz = apvts.getRawParameterValue (ParamIDs::eqPeak1Freq);
    eqPeak1GainDb = apvts.getRawParameterValue (ParamIDs::eqPeak1Gain);
    eqPeak1Q = apvts.getRawParameterValue (ParamIDs::eqPeak1Q);
    eqPeak2FreqHz = apvts.getRawParameterValue (ParamIDs::eqPeak2Freq);
    eqPeak2GainDb = apvts.getRawParameterValue (ParamIDs::eqPeak2Gain);
    eqPeak2Q = apvts.getRawParameterValue (ParamIDs::eqPeak2Q);
    eqHighShelfFreqHz = apvts.getRawParameterValue (ParamIDs::eqHighShelfFreq);
    eqHighShelfGainDb = apvts.getRawParameterValue (ParamIDs::eqHighShelfGain);

    irEnabled = apvts.getRawParameterValue (ParamIDs::irEnabled);
    irMixPercent = apvts.getRawParameterValue (ParamIDs::irMix);

    jassert (inputGainDb != nullptr);
    jassert (outputGainDb != nullptr);
    jassert (bypassFlag != nullptr);
    jassert (outputClipEnabled != nullptr);
    jassert (crossoverFreqHz != nullptr);
    jassert (lowLevelDb != nullptr);
    jassert (highLevelDb != nullptr);
    jassert (bypassParameter != nullptr);

    jassert (gateEnabled != nullptr);
    jassert (gateThresholdDb != nullptr);
    jassert (gateRatio != nullptr);
    jassert (gateAttackMs != nullptr);
    jassert (gateReleaseMs != nullptr);

    jassert (lowCompThresholdDb != nullptr);
    jassert (lowCompRatio != nullptr);
    jassert (lowCompAttackMs != nullptr);
    jassert (lowCompReleaseMs != nullptr);
    jassert (lowCompMakeupDb != nullptr);
    jassert (lowCompMixPercent != nullptr);

    jassert (highVoicingChoice != nullptr);
    jassert (highDrivePercent != nullptr);
    jassert (highTonePercent != nullptr);
    jassert (highBlendPercent != nullptr);

    jassert (eqEnabled != nullptr);
    jassert (eqLowShelfFreqHz != nullptr);
    jassert (eqLowShelfGainDb != nullptr);
    jassert (eqPeak1FreqHz != nullptr);
    jassert (eqPeak1GainDb != nullptr);
    jassert (eqPeak1Q != nullptr);
    jassert (eqPeak2FreqHz != nullptr);
    jassert (eqPeak2GainDb != nullptr);
    jassert (eqPeak2Q != nullptr);
    jassert (eqHighShelfFreqHz != nullptr);
    jassert (eqHighShelfGainDb != nullptr);

    jassert (irEnabled != nullptr);
    jassert (irMixPercent != nullptr);
}

TwistYourGutsAudioProcessor::~TwistYourGutsAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TwistYourGutsAudioProcessor::createParameterLayout()
{
    return tyg::createParameterLayout();
}

//==============================================================================
const juce::String TwistYourGutsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TwistYourGutsAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TwistYourGutsAudioProcessor::producesMidi() const
{
    return false;
}

bool TwistYourGutsAudioProcessor::isMidiEffect() const
{
    return false;
}

double TwistYourGutsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TwistYourGutsAudioProcessor::getNumPrograms()
{
    return 1;
}

int TwistYourGutsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TwistYourGutsAudioProcessor::setCurrentProgram (int)
{
}

const juce::String TwistYourGutsAudioProcessor::getProgramName (int)
{
    return {};
}

void TwistYourGutsAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void TwistYourGutsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    inputGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    inputGainProcessor.prepare (spec);
    inputGainProcessor.setGainDecibels (inputGainDb->load (std::memory_order_relaxed));

    outputGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    outputGainProcessor.prepare (spec);
    outputGainProcessor.setGainDecibels (outputGainDb->load (std::memory_order_relaxed));

    // Full-band input noise gate.
    gate.prepare (spec);

    // Issue #8: LR4 crossover, prepared for the same spec as the gain
    // processors so its per-channel filter state matches the bus layout.
    crossover.prepare (spec);
    crossover.setCutoffFrequency (crossoverFreqHz->load (std::memory_order_relaxed));

    // Low-band parallel compressor. The DryWetMixer inside needs its mix
    // proportion primed *before* prepare() runs its internal reset() (JUCE
    // 8.0.14 gotcha - see docs/architecture.md), so the current lowCompMix
    // value is read and passed in here rather than set afterwards.
    lowCompressor.prepare (spec, lowCompMixPercent->load (std::memory_order_relaxed) / 100.0f);

    // High-band oversampled distortion voicing. Same DryWetMixer-priming
    // requirement for highBlend.
    highVoicing.prepare (spec, highBlendPercent->load (std::memory_order_relaxed) / 100.0f);

    // Issue #10: independent per-band level trims, smoothed the same way as
    // the input/output gains to avoid zipper noise on automation.
    lowGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    lowGainProcessor.prepare (spec);
    lowGainProcessor.setGainDecibels (lowLevelDb->load (std::memory_order_relaxed));

    highGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    highGainProcessor.prepare (spec);
    highGainProcessor.setGainDecibels (highLevelDb->load (std::memory_order_relaxed));

    // Post-sum 4-band EQ.
    eq.prepare (spec);

    // IR loader. Same DryWetMixer-priming requirement for irMix.
    irLoader.prepare (spec, irMixPercent->load (std::memory_order_relaxed) / 100.0f);

    // Issue #9: (re)allocate the low-band compensation delay line for the
    // new spec/max-delay bound. setMaximumDelayInSamples() may allocate, so
    // it must only ever be called here, never from processBlock().
    lowBandLatencyDelay.setMaximumDelayInSamples (maxLatencyCompensationSamples);
    lowBandLatencyDelay.prepare (spec);

    // Pre-allocate the band buffers to the promised block size so
    // processBlock() never resizes a buffer on the audio thread, even if a
    // host later sends an oversized block (handled defensively by chunking
    // in processBlock() - see processChunk()).
    preparedBlockSize = samplesPerBlock;
    lowBandBuffer.setSize (static_cast<int> (spec.numChannels), preparedBlockSize);
    highBandBuffer.setSize (static_cast<int> (spec.numChannels), preparedBlockSize);

    updateLatencyCompensation();
}

//==============================================================================
int TwistYourGutsAudioProcessor::computeTotalLatencySamples() const noexcept
{
    // Issue #42: the high band's oversampled voicing stage is the only
    // source of latency in the chain (the gate, low-band compressor, EQ and
    // IR loader - default zero-latency Convolution - are all zero-latency),
    // so the plugin's total reported latency is exactly the oversampling
    // latency, and that is exactly what the low-band compensation delay
    // needs to match to stay time-aligned with the high band at the sum.
    return highVoicing.getLatencySamples();
}

void TwistYourGutsAudioProcessor::updateLatencyCompensation()
{
    const auto totalLatencySamples = juce::jlimit (0, maxLatencyCompensationSamples, computeTotalLatencySamples());

    setLatencySamples (totalLatencySamples);

    // The low band bypasses oversampling entirely, so it must be delayed by
    // the same amount the high band's oversampling stage delays the high
    // band, keeping both bands time-aligned when they are summed back
    // together in processChunk().
    lowBandLatencyDelay.setDelay (static_cast<float> (totalLatencySamples));
}

void TwistYourGutsAudioProcessor::releaseResources()
{
}

bool TwistYourGutsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();

    if (mainOut != mono && mainOut != stereo)
        return false;

    if (mainOut != mainIn)
        return false;

    return true;
}

void TwistYourGutsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Buses are constrained to in == out (mono or stereo), so this is
    // normally a no-op, but it's cheap insurance against stray channels.
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (bypassFlag->load (std::memory_order_relaxed) >= 0.5f)
        return;

    // Parameters are read once per host block (not per chunk/sample): the
    // dsp::Gain smoothers and the various stages' own control-rate updates
    // recompute cheaply enough at control rate, and re-reading the same
    // atomic per chunk would buy nothing.
    inputGainProcessor.setGainDecibels (inputGainDb->load (std::memory_order_relaxed));
    outputGainProcessor.setGainDecibels (outputGainDb->load (std::memory_order_relaxed));
    lowGainProcessor.setGainDecibels (lowLevelDb->load (std::memory_order_relaxed));
    highGainProcessor.setGainDecibels (highLevelDb->load (std::memory_order_relaxed));
    crossover.setCutoffFrequency (crossoverFreqHz->load (std::memory_order_relaxed));

    gate.setEnabled (gateEnabled->load (std::memory_order_relaxed) >= 0.5f);
    gate.setThresholdDb (gateThresholdDb->load (std::memory_order_relaxed));
    gate.setRatio (gateRatio->load (std::memory_order_relaxed));
    gate.setAttackMs (gateAttackMs->load (std::memory_order_relaxed));
    gate.setReleaseMs (gateReleaseMs->load (std::memory_order_relaxed));

    lowCompressor.setThresholdDb (lowCompThresholdDb->load (std::memory_order_relaxed));
    lowCompressor.setRatio (lowCompRatio->load (std::memory_order_relaxed));
    lowCompressor.setAttackMs (lowCompAttackMs->load (std::memory_order_relaxed));
    lowCompressor.setReleaseMs (lowCompReleaseMs->load (std::memory_order_relaxed));
    lowCompressor.setMakeupGainDb (lowCompMakeupDb->load (std::memory_order_relaxed));
    lowCompressor.setWetMixProportion (lowCompMixPercent->load (std::memory_order_relaxed) / 100.0f);

    const auto voicingIndex = static_cast<int> (highVoicingChoice->load (std::memory_order_relaxed));
    highVoicing.setVoicing (static_cast<tyg::VoicingType> (juce::jlimit (0, 2, voicingIndex)));
    highVoicing.setDrive (highDrivePercent->load (std::memory_order_relaxed) / 100.0f);
    highVoicing.setTone (highTonePercent->load (std::memory_order_relaxed) / 100.0f);
    highVoicing.setWetMixProportion (highBlendPercent->load (std::memory_order_relaxed) / 100.0f);

    eq.setLowShelf (eqLowShelfFreqHz->load (std::memory_order_relaxed), eqLowShelfGainDb->load (std::memory_order_relaxed));
    eq.setPeak1 (eqPeak1FreqHz->load (std::memory_order_relaxed), eqPeak1GainDb->load (std::memory_order_relaxed), eqPeak1Q->load (std::memory_order_relaxed));
    eq.setPeak2 (eqPeak2FreqHz->load (std::memory_order_relaxed), eqPeak2GainDb->load (std::memory_order_relaxed), eqPeak2Q->load (std::memory_order_relaxed));
    eq.setHighShelf (eqHighShelfFreqHz->load (std::memory_order_relaxed), eqHighShelfGainDb->load (std::memory_order_relaxed));

    irLoader.setWetMixProportion (irMixPercent->load (std::memory_order_relaxed) / 100.0f);

    juce::dsp::AudioBlock<float> fullBlock (buffer);

    // Defensive chunking: hosts are expected to never exceed the block size
    // promised to prepareToPlay(), but if one ever did, indexing straight
    // into lowBandBuffer/highBandBuffer (sized to preparedBlockSize) would
    // be out of bounds. Processing in chunks of at most preparedBlockSize
    // handles that case safely without ever resizing a buffer here.
    const auto chunkLimit = preparedBlockSize > 0
                                 ? static_cast<size_t> (preparedBlockSize)
                                 : juce::jmax (static_cast<size_t> (1), fullBlock.getNumSamples());

    for (size_t offset = 0; offset < fullBlock.getNumSamples(); offset += chunkLimit)
    {
        const auto chunkLength = juce::jmin (chunkLimit, fullBlock.getNumSamples() - offset);
        auto chunk = fullBlock.getSubBlock (offset, chunkLength);
        processChunk (chunk);
    }
}

void TwistYourGutsAudioProcessor::processChunk (juce::dsp::AudioBlock<float>& chunk) noexcept
{
    inputGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (chunk));

    // Full-band noise gate, ahead of the crossover split.
    gate.process (chunk);

    const auto numChannels = chunk.getNumChannels();
    const auto numSamples = chunk.getNumSamples();

    auto lowBlock = juce::dsp::AudioBlock<float> (lowBandBuffer).getSubBlock (0, numSamples).getSubsetChannelBlock (0, numChannels);
    auto highBlock = juce::dsp::AudioBlock<float> (highBandBuffer).getSubBlock (0, numSamples).getSubsetChannelBlock (0, numChannels);

    // Issue #8: split the input-trimmed, gated signal into low/high bands.
    crossover.process (chunk, lowBlock, highBlock);

    // Low band: parallel compressor, then level trim.
    lowCompressor.process (lowBlock);

    // High band: oversampled distortion voicing (Gnaw/Wool/Razor), then
    // level trim. This is the only source of latency in the chain.
    highVoicing.process (highBlock);

    // Issue #9: time-align the low band with the latency the high band's
    // oversampling stage introduces.
    lowBandLatencyDelay.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));

    // Issue #10: independent per-band level trims, then sum the bands back
    // together in place of the pre-split signal.
    lowGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    highGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

    chunk.replaceWithSumOf (lowBlock, highBlock);

    // Post-sum 4-band EQ. Skipped entirely when disabled for a guaranteed
    // bit-exact bypass rather than relying on all-zero band gains.
    if (eqEnabled->load (std::memory_order_relaxed) >= 0.5f)
        eq.process (chunk);

    // Cab-sim IR loader. Skipped entirely when disabled for the same reason
    // (and safe-by-default even when enabled with no IR loaded yet - see
    // tyg::IRLoader).
    if (irEnabled->load (std::memory_order_relaxed) >= 0.5f)
        irLoader.process (chunk);

    // Optional safety clip (issue #10): a soft (tanh) limiter that only
    // engages when the user explicitly enables it, protecting against
    // accidental hard-clipped overs without colouring the signal at typical
    // playing levels (tanh(x) ~= x for |x| well below 1.0).
    if (outputClipEnabled->load (std::memory_order_relaxed) >= 0.5f)
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* data = chunk.getChannelPointer (channel);

            for (size_t sample = 0; sample < numSamples; ++sample)
                data[sample] = std::tanh (data[sample]);
        }
    }

    outputGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (chunk));
}

//==============================================================================
bool TwistYourGutsAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TwistYourGutsAudioProcessor::createEditor()
{
    return new TwistYourGutsAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessorParameter* TwistYourGutsAudioProcessor::getBypassParameter() const
{
    return bypassParameter;
}

//==============================================================================
void TwistYourGutsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TwistYourGutsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
void TwistYourGutsAudioProcessor::loadImpulseResponse (juce::AudioBuffer<float> irBuffer, double irSampleRate)
{
    irLoader.loadImpulseResponse (std::move (irBuffer), irSampleRate);
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TwistYourGutsAudioProcessor();
}
