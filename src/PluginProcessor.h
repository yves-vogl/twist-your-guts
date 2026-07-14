#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/BandEQ.h"
#include "dsp/Crossover.h"
#include "dsp/IRLoader.h"
#include "dsp/NoiseGateStage.h"
#include "dsp/ParallelCompressor.h"
#include "dsp/Voicing.h"

// As of the M1 "DSP completion" milestone (issue #42), the full v1.0 signal
// path from the README is wired and live: input trim -> noise gate -> LR4
// crossover split -> [low: parallel compressor -> level] + [high: voicing
// (Gnaw/Wool/Razor, oversampled) -> level] -> delay-compensated sum ->
// 4-band EQ -> IR loader (cab sim) -> optional safety clip -> output trim.
// See docs/architecture.md for the full breakdown and docs/manual.md for the
// user-facing parameter reference.
class CryptaAudioProcessor final : public juce::AudioProcessor
{
public:
    CryptaAudioProcessor();
    ~CryptaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorParameter* getBypassParameter() const override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Loads a new cab-sim impulse response into the IR loader stage. Not
    // real-time safe by contract (see cryp::IRLoader) - call from the message
    // thread only (e.g. in response to a future GUI file picker or preset
    // load), never from processBlock() or any audio-thread callback.
    void loadImpulseResponse (juce::AudioBuffer<float> irBuffer, double irSampleRate);

private:
    //==============================================================================
    // Latency-compensation seam (issue #9): computes the plugin's total
    // reported latency (now the high-band voicing's oversampling latency,
    // issue #42) and re-arms the low-band compensation delay to match it.
    // Called once from prepareToPlay().
    int computeTotalLatencySamples() const noexcept;
    void updateLatencyCompensation();

    // Processes at most `preparedBlockSize` samples: gate, crossover split,
    // per-band dynamics/voicing, per-band level, sum, EQ, IR loader,
    // optional safety clip. Called once per chunk from processBlock() so
    // oversized host blocks (larger than prepareToPlay promised) are handled
    // defensively without ever resizing a buffer on the audio thread.
    void processChunk (juce::dsp::AudioBlock<float>& chunk) noexcept;

    //==============================================================================
    juce::dsp::Gain<float> inputGainProcessor;
    juce::dsp::Gain<float> outputGainProcessor;

    // Full-band input noise gate, ahead of the crossover split.
    cryp::NoiseGateStage gate;

    // Issue #8: LR4 crossover splitting the (input-trimmed, gated) signal
    // into low and high bands ahead of independent per-band processing.
    cryp::Crossover crossover;

    // Low band: parallel compressor, then level trim.
    cryp::ParallelCompressor lowCompressor;

    // High band: selectable oversampled distortion voicing (Gnaw/Wool/
    // Razor), then level trim.
    cryp::Voicing highVoicing;

    // Issue #10: independent per-band level trims applied after each band's
    // dynamics/voicing processing and before the bands are summed back
    // together.
    juce::dsp::Gain<float> lowGainProcessor;
    juce::dsp::Gain<float> highGainProcessor;

    // Post-sum 4-band EQ and cab-sim IR loader.
    cryp::BandEQ eq;
    cryp::IRLoader irLoader;

    // Issue #9: upper bound on the latency this plugin will ever need to
    // compensate for, i.e. the largest oversampling latency the high-band
    // voicing stage is expected to introduce. Generous headroom well above
    // the actual 4x-oversampling FIR latency at any supported sample rate,
    // at negligible memory cost.
    static constexpr int maxLatencyCompensationSamples = 4096;

    // Delays the low band so it stays time-aligned with the oversampled high
    // band. None-interpolation is correct here: latency compensation is
    // always an integer number of samples, never a fractionally modulated
    // delay.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lowBandLatencyDelay { maxLatencyCompensationSamples };

    // Pre-allocated band buffers, sized to `preparedBlockSize` in
    // prepareToPlay(). Never resized in processBlock().
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;
    int preparedBlockSize = 0;

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* bypassFlag = nullptr;
    std::atomic<float>* outputClipEnabled = nullptr;
    std::atomic<float>* crossoverFreqHz = nullptr;
    std::atomic<float>* lowLevelDb = nullptr;
    std::atomic<float>* highLevelDb = nullptr;

    std::atomic<float>* gateEnabled = nullptr;
    std::atomic<float>* gateThresholdDb = nullptr;
    std::atomic<float>* gateRatio = nullptr;
    std::atomic<float>* gateAttackMs = nullptr;
    std::atomic<float>* gateReleaseMs = nullptr;

    std::atomic<float>* lowCompThresholdDb = nullptr;
    std::atomic<float>* lowCompRatio = nullptr;
    std::atomic<float>* lowCompAttackMs = nullptr;
    std::atomic<float>* lowCompReleaseMs = nullptr;
    std::atomic<float>* lowCompMakeupDb = nullptr;
    std::atomic<float>* lowCompMixPercent = nullptr;

    std::atomic<float>* highVoicingChoice = nullptr;
    std::atomic<float>* highDrivePercent = nullptr;
    std::atomic<float>* highTonePercent = nullptr;
    std::atomic<float>* highBlendPercent = nullptr;

    std::atomic<float>* eqEnabled = nullptr;
    std::atomic<float>* eqLowShelfFreqHz = nullptr;
    std::atomic<float>* eqLowShelfGainDb = nullptr;
    std::atomic<float>* eqPeak1FreqHz = nullptr;
    std::atomic<float>* eqPeak1GainDb = nullptr;
    std::atomic<float>* eqPeak1Q = nullptr;
    std::atomic<float>* eqPeak2FreqHz = nullptr;
    std::atomic<float>* eqPeak2GainDb = nullptr;
    std::atomic<float>* eqPeak2Q = nullptr;
    std::atomic<float>* eqHighShelfFreqHz = nullptr;
    std::atomic<float>* eqHighShelfGainDb = nullptr;

    std::atomic<float>* irEnabled = nullptr;
    std::atomic<float>* irMixPercent = nullptr;

    // The actual parameter object handed back from getBypassParameter() so
    // hosts can offer their own bypass UI/automation for this parameter.
    juce::RangedAudioParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CryptaAudioProcessor)
};
