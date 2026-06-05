/*
 * Gateway Linux VST3 Plugin
 * Copyright (C) 2026 rations
 *
 * Based on NeuralAmpModelerPlugin by Steven Atkinson (MIT Licence).
 * See NOTICE for full attribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "NAM/get_dsp.h"

// Compatibility shims required by the iPlug2-derived LanczosResampler that
// AudioDSPTools' ResamplingContainer bundles.  These symbols are normally
// provided by iPlug2's IPlugPlatform.h which we don't include.
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE 128
#endif
namespace iplug {
static constexpr double PI = 3.14159265358979323846;
}

#include "ResamplingContainer/ResamplingContainer.h"
#include "slimmable.h"

#include <cmath>
#include <stdexcept>

// ---------------------------------------------------------------------------
// ResamplingNAM
//
// Wraps a nam::DSP model and provides transparent sample-rate conversion via
// AudioDSPTools' ResamplingContainer (Lanczos, A=12).  NAM models typically
// run at 48 kHz; the DAW may run at any rate.
// ---------------------------------------------------------------------------

static double GetNamEncapsulatedSampleRate(const std::unique_ptr<nam::DSP> &m) {
  const double reported = m->GetExpectedSampleRate();
  return reported > 0.0 ? reported : 48000.0;
}

class ResamplingNAM : public nam::DSP {
public:
  ResamplingNAM(std::unique_ptr<nam::DSP> enc, double externalSampleRate)
      : nam::DSP(1, 1, externalSampleRate), mEncapsulated(std::move(enc)),
        mResampler(GetNamEncapsulatedSampleRate(mEncapsulated)) {
    if (mEncapsulated->HasLoudness())
      SetLoudness(mEncapsulated->GetLoudness());
    if (mEncapsulated->HasInputLevel())
      SetInputLevel(mEncapsulated->GetInputLevel());
    if (mEncapsulated->HasOutputLevel())
      SetOutputLevel(mEncapsulated->GetOutputLevel());
    Reset(externalSampleRate, 2048);
  }

  void process(NAM_SAMPLE **input, NAM_SAMPLE **output, const int num_frames) override {
    const double encRate = GetNamEncapsulatedSampleRate(mEncapsulated);
    if (encRate == mExpectedSampleRate) {
      mEncapsulated->process(input, output, num_frames);
    } else {
      mResampler.ProcessBlock(input, output, num_frames,
                              [this](NAM_SAMPLE **in, NAM_SAMPLE **out, int n) {
                                mEncapsulated->process(in, out, n);
                              });
    }
  }

  void Reset(const double sampleRate, const int maxBlockSize) override {
    mExpectedSampleRate = sampleRate;
    mMaxBufferSize = maxBlockSize;
    mResampler.Reset(sampleRate, maxBlockSize);

    const double encRate = GetNamEncapsulatedSampleRate(mEncapsulated);
    const int encBlock = static_cast<int>(
        std::ceil(static_cast<double>(maxBlockSize) * encRate / sampleRate));
    mEncapsulated->ResetAndPrewarm(encRate, encBlock);
  }

  int GetLatency() const { return mResampler.GetLatency(); }

  bool IsSlimmable() const {
    return dynamic_cast<const nam::SlimmableModel *>(mEncapsulated.get()) != nullptr;
  }

  // Applies slimmable size if the encapsulated model supports the interface.
  // Thread-safe, not real-time safe — call from the audio thread only.
  void TrySetSlimmableSize(double val) {
    if (auto *s = dynamic_cast<nam::SlimmableModel *>(mEncapsulated.get()))
      s->SetSlimmableSize(val);
  }

private:
  std::unique_ptr<nam::DSP> mEncapsulated;
  dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> mResampler;
};

// ---------------------------------------------------------------------------
// Parameter IDs
// ---------------------------------------------------------------------------
namespace params {
static const juce::String kInputGain{"inputGain"};
static const juce::String kOutputGain{"outputGain"};
static const juce::String kNoiseGateThreshold{"noiseGateThreshold"};
static const juce::String kBass{"bass"};
static const juce::String kMiddle{"middle"};
static const juce::String kTreble{"treble"};
static const juce::String kToneStackOn{"toneStackOn"};
static const juce::String kNoiseGateOn{"noiseGateOn"};
} // namespace params

// ---------------------------------------------------------------------------
// Parameter layout
// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout
GatewayAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{params::kInputGain, 1}, "Input Gain",
      juce::NormalisableRange<float>(-40.0f, 40.0f, 0.1f), 0.0f,
      juce::AudioParameterFloatAttributes{}.withLabel("dB")));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{params::kOutputGain, 1}, "Output Gain",
      juce::NormalisableRange<float>(-40.0f, 40.0f, 0.1f), 0.0f,
      juce::AudioParameterFloatAttributes{}.withLabel("dB")));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{params::kNoiseGateThreshold, 1}, "Noise Gate",
      juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), -80.0f,
      juce::AudioParameterFloatAttributes{}.withLabel("dB")));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{params::kBass, 1}, "Bass",
      juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{params::kMiddle, 1}, "Middle",
      juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{params::kTreble, 1}, "Treble",
      juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{params::kToneStackOn, 1}, "Tone Stack", true));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{params::kNoiseGateOn, 1}, "Noise Gate On", true));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"slim", 1}, "Slim",
      juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.0f));

  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"outputMode", 1}, "Output Mode",
      juce::StringArray{"Raw", "Normalized", "Calibrated"}, 1));

  // Input calibration — matches original: range -60..60 dBu, default 12.0, step 0.1
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"inputCalibrationLevel", 1}, "Input Calibration Level",
      juce::NormalisableRange<float>(-60.0f, 60.0f, 0.1f), 12.0f,
      juce::AudioParameterFloatAttributes{}.withLabel("dBu")));

  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"calibrateInput", 1}, "Calibrate Input", false));

  return layout;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------
GatewayAudioProcessor::GatewayAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  // Wire the noise gate: trigger listens to the input and notifies the gain
  // module, which applies the computed envelope to the model output.
  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

  apvts.addParameterListener("slim", this);
}

GatewayAudioProcessor::~GatewayAudioProcessor() {
  apvts.removeParameterListener("slim", this);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void GatewayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  mSampleRate = sampleRate;
  mSamplesPerBlock = samplesPerBlock;

  mToneStack.Reset(sampleRate, samplesPerBlock);

  mWorkBufInput.assign(static_cast<size_t>(samplesPerBlock), 0.0);
  mWorkBufOutput.assign(static_cast<size_t>(samplesPerBlock), 0.0);
  mWorkPtrInput = mWorkBufInput.data();
  mWorkPtrOutput = mWorkBufOutput.data();

  if (mModel)
    mModel->Reset(sampleRate, samplesPerBlock);
}

void GatewayAudioProcessor::releaseResources() {}

bool GatewayAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
      layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  return true;
}

// ---------------------------------------------------------------------------
// Audio thread
// ---------------------------------------------------------------------------
void GatewayAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                         juce::MidiBuffer &) {
  juce::ScopedNoDenormals noDenormals;

  // Swap in any pending model/IR from the message thread.
  if (mModelPending.exchange(false)) {
    mModel = std::move(mPendingModel);
    if (mModel) {
      mModelHasInputLevel.store(mModel->HasInputLevel(), std::memory_order_relaxed);
      mModelHasOutputLevel.store(mModel->HasOutputLevel(), std::memory_order_relaxed);
      if (auto *r = dynamic_cast<ResamplingNAM *>(mModel.get())) {
        setLatencySamples(r->GetLatency());
        mModelIsSlimmable.store(r->IsSlimmable(), std::memory_order_relaxed);
      }
    } else {
      mModelHasInputLevel.store(false, std::memory_order_relaxed);
      mModelHasOutputLevel.store(false, std::memory_order_relaxed);
      mModelIsSlimmable.store(false, std::memory_order_relaxed);
      setLatencySamples(0);
    }
  }
  if (mIRPending.exchange(false))
    mIR = std::move(mPendingIR);

  // Apply slim size update requested from the message thread.
  if (mSlimDirty.exchange(false, std::memory_order_acq_rel)) {
    const float v = apvts.getRawParameterValue("slim")->load();
    if (auto *r = dynamic_cast<ResamplingNAM *>(mModel.get()))
      r->TrySetSlimmableSize(static_cast<double>(v));
  }

  applyDsp(buffer);
}

// ---------------------------------------------------------------------------
// DSP chain
// ---------------------------------------------------------------------------
void GatewayAudioProcessor::applyDsp(juce::AudioBuffer<float> &buffer) {
  const int numSamples = buffer.getNumSamples();

  float inputGainDb = apvts.getRawParameterValue(params::kInputGain)->load();

  // Input calibration — mirrors original: adjust input gain so the model
  // receives signal at its expected input level (in dBu).
  // kDefaultInputCalibrationLevel = 12.0; model's GetInputLevel() is also dBu.
  if (mModel && mModel->HasInputLevel()) {
    const bool calibrateInput =
        apvts.getRawParameterValue("calibrateInput")->load() > 0.5f;
    if (calibrateInput) {
      const float calibLevel =
          apvts.getRawParameterValue("inputCalibrationLevel")->load();
      inputGainDb += calibLevel - static_cast<float>(mModel->GetInputLevel());
    }
  }

  const float outputGainDb = apvts.getRawParameterValue(params::kOutputGain)->load();
  const float inputGainLinear = juce::Decibels::decibelsToGain(inputGainDb);
  const float outputGainLinear = juce::Decibels::decibelsToGain(outputGainDb);

  const bool ngOn = apvts.getRawParameterValue(params::kNoiseGateOn)->load() > 0.5f;
  const bool tsOn = apvts.getRawParameterValue(params::kToneStackOn)->load() > 0.5f;

  const float *floatIn = buffer.getReadPointer(0);

  // Measure raw input peak (before any gain) for the input level meter.
  {
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::abs(floatIn[i]));
    mInputLevel.store(peak, std::memory_order_relaxed);
  }

  // 1. Convert float → double, apply input gain → mWorkBufInput.
  for (int i = 0; i < numSamples; ++i)
    mWorkBufInput[static_cast<size_t>(i)] =
        static_cast<DSP_SAMPLE>(floatIn[i]) * inputGainLinear;

  // 2. Noise gate trigger (detects level, sets envelope on the Gain module,
  //    and returns a pointer to the gated signal that feeds into NAM).
  DSP_SAMPLE **processingInput = &mWorkPtrInput;
  if (ngOn) {
    const float ngThreshDb =
        apvts.getRawParameterValue(params::kNoiseGateThreshold)->load();
    const dsp::noise_gate::TriggerParams triggerParams(
        0.01,                            // time constant (s)
        static_cast<double>(ngThreshDb), // threshold (dB)
        0.1,                             // ratio
        0.005,                           // open time (s)
        0.01,                            // hold time (s)
        0.05                             // close time (s)
    );
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(mSampleRate);
    processingInput =
        mNoiseGateTrigger.Process(&mWorkPtrInput, 1, static_cast<size_t>(numSamples));
  }

  // 3. NAM model (mono, double-precision).
  // NAM_SAMPLE and DSP_SAMPLE are both double by default so the
  // reinterpret_cast is safe — it just satisfies the type system.
  DSP_SAMPLE **modelOutput = processingInput; // passthrough if no model
  if (mModel) {
    mModel->process(reinterpret_cast<NAM_SAMPLE **>(processingInput),
                    reinterpret_cast<NAM_SAMPLE **>(&mWorkPtrOutput), numSamples);
    modelOutput = &mWorkPtrOutput;
  } else {
    // No model: copy gated input to output buffer so the rest of the chain
    // still works with a consistent pointer.
    const DSP_SAMPLE *src = processingInput[0];
    for (int i = 0; i < numSamples; ++i)
      mWorkBufOutput[static_cast<size_t>(i)] = src[i];
    modelOutput = &mWorkPtrOutput;
  }

  // 4. Noise gate gain (applies the envelope computed by the trigger to the
  //    model output, suppressing tail noise).
  DSP_SAMPLE **gateOutput = modelOutput;
  if (ngOn)
    gateOutput = mNoiseGateGain.Process(modelOutput, 1, static_cast<size_t>(numSamples));

  // 5. Tone stack EQ (bass / middle / treble filters).
  DSP_SAMPLE **tsOutput = gateOutput;
  if (tsOn) {
    mToneStack.SetParam(
        "bass", static_cast<double>(apvts.getRawParameterValue(params::kBass)->load()));
    mToneStack.SetParam(
        "middle",
        static_cast<double>(apvts.getRawParameterValue(params::kMiddle)->load()));
    mToneStack.SetParam(
        "treble",
        static_cast<double>(apvts.getRawParameterValue(params::kTreble)->load()));
    tsOutput = mToneStack.Process(gateOutput, 1, numSamples);
  }

  // 6. IR convolution.
  DSP_SAMPLE **irOutput = tsOutput;
  if (mIR)
    irOutput = mIR->Process(tsOutput, 1, static_cast<size_t>(numSamples));

  // 7. Convert double → float, apply output gain, write back to channel 0.
  float *floatOut = buffer.getWritePointer(0);
  const DSP_SAMPLE *finalBuf = irOutput[0];
  for (int i = 0; i < numSamples; ++i)
    floatOut[i] = static_cast<float>(finalBuf[i]) * outputGainLinear;

  // Apply output mode — matches original _SetOutputGain() exactly.
  {
    const int outMode =
        static_cast<int>(apvts.getRawParameterValue("outputMode")->load());
    if (outMode == 1 && mModel && mModel->HasLoudness()) {
      // Normalized: gainDB += (targetLoudness - loudness), target = -18 dB.
      const float normGain = juce::Decibels::decibelsToGain(
          static_cast<float>(-18.0 - mModel->GetLoudness()));
      for (int i = 0; i < numSamples; ++i)
        floatOut[i] *= normGain;
    } else if (outMode == 2 && mModel && mModel->HasOutputLevel()) {
      // Calibrated: gainDB += (outputLevel - inputCalibrationLevel).
      const float inputLevel =
          apvts.getRawParameterValue("inputCalibrationLevel")->load();
      const float outputLevel = static_cast<float>(mModel->GetOutputLevel());
      const float calibGain = juce::Decibels::decibelsToGain(outputLevel - inputLevel);
      for (int i = 0; i < numSamples; ++i)
        floatOut[i] *= calibGain;
    }
  }

  // Update output level for the UI meter.
  {
    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
      peak = std::max(peak, std::abs(floatOut[i]));
    mOutputLevel.store(peak, std::memory_order_relaxed);
  }

  // 8. Copy mono channel 0 → all output channels (stereo).
  for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
    buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

// ---------------------------------------------------------------------------
// Model / IR loading (called from message thread)
// ---------------------------------------------------------------------------
bool GatewayAudioProcessor::loadModel(const juce::File &file) {
  try {
    auto raw = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
    if (raw->NumInputChannels() != 1 || raw->NumOutputChannels() != 1)
      throw std::runtime_error("Expected a 1-in / 1-out NAM model");

    auto wrapped = std::make_unique<ResamplingNAM>(std::move(raw), mSampleRate);
    wrapped->Reset(mSampleRate, mSamplesPerBlock);

    // Apply current slim setting immediately on this model before it goes live.
    wrapped->TrySetSlimmableSize(
        static_cast<double>(apvts.getRawParameterValue("slim")->load()));

    mPendingModel = std::move(wrapped);
    mModelPath = file.getFullPathName();
    // Pre-set capability flags so the UI can read them as soon as loadModel()
    // returns, without waiting for the next processBlock swap.
    mModelHasInputLevel.store(mPendingModel->HasInputLevel(), std::memory_order_relaxed);
    mModelHasOutputLevel.store(mPendingModel->HasOutputLevel(), std::memory_order_relaxed);
    mModelHasLoudness.store(mPendingModel->HasLoudness(), std::memory_order_relaxed);
    if (auto *r = dynamic_cast<ResamplingNAM *>(mPendingModel.get()))
      mModelIsSlimmable.store(r->IsSlimmable(), std::memory_order_relaxed);
    mModelPending.store(true);
    return true;
  } catch (const std::exception &e) {
    juce::Logger::writeToLog(juce::String("Gateway: model load failed: ") + e.what());
    return false;
  }
}

void GatewayAudioProcessor::clearModel() {
  mPendingModel.reset();
  mModelPath = {};
  mModelHasInputLevel.store(false, std::memory_order_relaxed);
  mModelHasOutputLevel.store(false, std::memory_order_relaxed);
  mModelHasLoudness.store(false, std::memory_order_relaxed);
  mModelIsSlimmable.store(false, std::memory_order_relaxed);
  mModelPending.store(true);
}

bool GatewayAudioProcessor::loadIR(const juce::File &file) {
  try {
    auto ir = std::make_unique<dsp::ImpulseResponse>(
        file.getFullPathName().toStdString().c_str(), mSampleRate);
    if (ir->GetWavState() != dsp::wav::LoadReturnCode::SUCCESS) {
      juce::Logger::writeToLog("Gateway: IR load failed: bad WAV file");
      return false;
    }
    mPendingIR = std::move(ir);
    mIRPath = file.getFullPathName();
    mIRPending.store(true);
    return true;
  } catch (const std::exception &e) {
    juce::Logger::writeToLog(juce::String("Gateway: IR load failed: ") + e.what());
    return false;
  }
}

void GatewayAudioProcessor::clearIR() {
  mPendingIR.reset();
  mIRPath = {};
  mIRPending.store(true);
}

// ---------------------------------------------------------------------------
// Parameter listener
// ---------------------------------------------------------------------------
void GatewayAudioProcessor::parameterChanged(const juce::String &parameterID,
                                             float /*newValue*/) {
  if (parameterID == "slim")
    mSlimDirty.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// State persistence
// ---------------------------------------------------------------------------
void GatewayAudioProcessor::getStateInformation(juce::MemoryBlock &dest) {
  auto state = apvts.copyState();
  state.setProperty("modelPath", mModelPath, nullptr);
  state.setProperty("irPath", mIRPath, nullptr);
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, dest);
}

void GatewayAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (!xml || !xml->hasTagName(apvts.state.getType()))
    return;

  auto newState = juce::ValueTree::fromXml(*xml);
  apvts.replaceState(newState);

  const juce::String mp = newState.getProperty("modelPath", "");
  const juce::String ip = newState.getProperty("irPath", "");

  if (mp.isNotEmpty()) {
    juce::File f(mp);
    if (f.existsAsFile())
      loadModel(f);
  }
  if (ip.isNotEmpty()) {
    juce::File f(ip);
    if (f.existsAsFile())
      loadIR(f);
  }
}

// ---------------------------------------------------------------------------
// Editor / plugin entry
// ---------------------------------------------------------------------------
juce::AudioProcessorEditor *GatewayAudioProcessor::createEditor() {
  return new GatewayAudioProcessorEditor(*this);
}

juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new GatewayAudioProcessor();
}
