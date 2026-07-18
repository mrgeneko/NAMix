/*
 * NAMix Linux VST3 Plugin
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

#include "NAM/get_dsp.h"
#include "ParametricKnobLayout.h"

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

  // Parametric-model knob interface — forwarded straight to the encapsulated
  // model. Non-parametric models inherit no-op defaults from nam::DSP, so
  // these are safe to call unconditionally regardless of model type.
  int GetNumParams() const override { return mEncapsulated->GetNumParams(); }
  std::vector<nam::DSPParamDef> GetParameterDefs() const override {
    return mEncapsulated->GetParameterDefs();
  }
  void SetKnobValues(const std::vector<float> &values) override {
    mEncapsulated->SetKnobValues(values);
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
NAMixAudioProcessor::createParameterLayout() {
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

  // Fixed-size bank of generic parametric-knob slots. Range/name/default are
  // placeholders until a parametric model loads and
  // updateParametricRangesAndDefaults() rewrites them to match its real
  // per-knob metadata (NAM/dsp.h: DSPParamDef).
  //
  // AudioParameterFloat derives its displayed-text decimal count from the
  // range's *interval* once, in its constructor -- it never recomputes that
  // when updateParametricRangesAndDefaults() later mutates `range` at
  // runtime. Left to the default, a continuous (interval == 0) param falls
  // back to 7 decimal places, which is what actually governs the knob's
  // text box (SliderAttachment reads the parameter's own getText(), not
  // the Slider's local setNumDecimalPlacesToDisplay()). Supplying our own
  // stringFromValueFunction here bypasses that interval-based lookup
  // entirely, so the display stays at 2 decimal places regardless of range.
  for (int i = 0; i < NAMixAudioProcessor::kMaxParametricParams; ++i) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{NAMixAudioProcessor::getParametricParamId(i), 1},
        "Model Param " + juce::String(i + 1),
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 2); })));
  }

  return layout;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------
NAMixAudioProcessor::NAMixAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  // Pre-size once so processBlock() can reuse this without reallocating.
  mKnobScratch.resize(static_cast<size_t>(kMaxParametricParams), 0.0f);

  // Wire the noise gate: trigger listens to the input and notifies the gain
  // module, which applies the computed envelope to the model output.
  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

  apvts.addParameterListener("slim", this);
  for (int i = 0; i < kMaxParametricParams; ++i)
    apvts.addParameterListener(getParametricParamId(i), this);
}

NAMixAudioProcessor::~NAMixAudioProcessor() {
  apvts.removeParameterListener("slim", this);
  for (int i = 0; i < kMaxParametricParams; ++i)
    apvts.removeParameterListener(getParametricParamId(i), this);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void NAMixAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
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

void NAMixAudioProcessor::releaseResources() {}

bool NAMixAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const {
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
void NAMixAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
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

        // Cache parametric metadata for the UI. Ranges/defaults on the APVTS
        // parameters themselves were already set in loadModel() (message
        // thread); this just mirrors the count/defs for getModelNumParams()/
        // getModelParamDefs() once the model is actually live.
        //
        // try_lock, not a blocking lock: this runs on the audio thread, and
        // getModelParamDefs() (message thread, e.g. opening the Params
        // overlay) can hold this same mutex. If it's contended just skip
        // the cache update this buffer -- getModelParamDefs() will see the
        // fresh values as soon as the lock is free, typically the very next
        // callback, and nothing here needs it to be exactly synchronous.
        auto defs = r->GetParameterDefs();
        if (static_cast<int>(defs.size()) > kMaxParametricParams)
          defs.resize(static_cast<size_t>(kMaxParametricParams));
        {
          std::unique_lock<std::mutex> lock(mParamDefsMutex, std::try_to_lock);
          if (lock.owns_lock())
            mModelParamDefs = defs;
        }
        mModelNumParams.store(static_cast<int>(defs.size()), std::memory_order_relaxed);
      }
    } else {
      mModelHasInputLevel.store(false, std::memory_order_relaxed);
      mModelHasOutputLevel.store(false, std::memory_order_relaxed);
      mModelIsSlimmable.store(false, std::memory_order_relaxed);
      mModelNumParams.store(0, std::memory_order_relaxed);
      {
        std::unique_lock<std::mutex> lock(mParamDefsMutex, std::try_to_lock);
        if (lock.owns_lock())
          mModelParamDefs.clear();
      }
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

  // Apply parametric knob updates requested from the message thread.
  //
  // Deliberately re-query the live model's own GetNumParams() here rather
  // than trust the cached mModelNumParams: that cache is updated as soon as
  // loadModel() resolves on the message thread, which can run ahead of the
  // model actually swapping in above. Using the count from whichever model
  // is *actually* live right now avoids sizing a knob-values call for a
  // different (new, not-yet-swapped or already-replaced) model.
  if (mParamsDirty.exchange(false, std::memory_order_acq_rel)) {
    if (auto *r = dynamic_cast<ResamplingNAM *>(mModel.get())) {
      const int n = r->GetNumParams();
      if (n > 0) {
        readKnobValuesFromApvtsInto(n, mKnobScratch);
        r->SetKnobValues(mKnobScratch);
      }
    }
  }

  applyDsp(buffer);
}

// ---------------------------------------------------------------------------
// DSP chain
// ---------------------------------------------------------------------------
void NAMixAudioProcessor::applyDsp(juce::AudioBuffer<float> &buffer) {
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
bool NAMixAudioProcessor::loadModel(const juce::File &file) {
  try {
    auto raw = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
    if (raw->NumInputChannels() != 1 || raw->NumOutputChannels() != 1)
      throw std::runtime_error("Expected a 1-in / 1-out NAM model");

    auto wrapped = std::make_unique<ResamplingNAM>(std::move(raw), mSampleRate);
    wrapped->Reset(mSampleRate, mSamplesPerBlock);

    // Apply current slim setting immediately on this model before it goes live.
    wrapped->TrySetSlimmableSize(
        static_cast<double>(apvts.getRawParameterValue("slim")->load()));

    // Set up the fixed parametric-knob slots for this model (range/name/
    // default per real DSPParamDef) and apply their (now-default) values to
    // the model before it goes live. Message thread only — safe to call
    // setValueNotifyingHost() here.
    {
      auto defs = wrapped->GetParameterDefs();
      if (static_cast<int>(defs.size()) > kMaxParametricParams)
        defs.resize(static_cast<size_t>(kMaxParametricParams));
      updateParametricRangesAndDefaults(defs);
      {
        std::lock_guard<std::mutex> lock(mParamDefsMutex);
        mModelParamDefs = defs;
      }
      mModelNumParams.store(static_cast<int>(defs.size()), std::memory_order_relaxed);
      if (!defs.empty()) {
        std::vector<float> initial;
        readKnobValuesFromApvtsInto(static_cast<int>(defs.size()), initial);
        wrapped->SetKnobValues(initial);
      }
    }

    mPendingModel = std::move(wrapped);
    mModelPath = file.getFullPathName();
    // Pre-set capability flags so the UI can read them as soon as loadModel()
    // returns, without waiting for the next processBlock swap.
    mModelHasInputLevel.store(mPendingModel->HasInputLevel(), std::memory_order_relaxed);
    mModelHasOutputLevel.store(mPendingModel->HasOutputLevel(),
                               std::memory_order_relaxed);
    mModelHasLoudness.store(mPendingModel->HasLoudness(), std::memory_order_relaxed);
    if (auto *r = dynamic_cast<ResamplingNAM *>(mPendingModel.get()))
      mModelIsSlimmable.store(r->IsSlimmable(), std::memory_order_relaxed);
    mModelPending.store(true);
    return true;
  } catch (const std::exception &e) {
    juce::Logger::writeToLog(juce::String("NAMix: model load failed:") + e.what());
    return false;
  }
}

void NAMixAudioProcessor::clearModel() {
  mPendingModel.reset();
  mModelPath = {};
  mModelHasInputLevel.store(false, std::memory_order_relaxed);
  mModelHasOutputLevel.store(false, std::memory_order_relaxed);
  mModelHasLoudness.store(false, std::memory_order_relaxed);
  mModelIsSlimmable.store(false, std::memory_order_relaxed);
  mModelNumParams.store(0, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(mParamDefsMutex);
    mModelParamDefs.clear();
  }
  mModelPending.store(true);
}

bool NAMixAudioProcessor::loadIR(const juce::File &file) {
  try {
    auto ir = std::make_unique<dsp::ImpulseResponse>(
        file.getFullPathName().toStdString().c_str(), mSampleRate);
    if (ir->GetWavState() != dsp::wav::LoadReturnCode::SUCCESS) {
      juce::Logger::writeToLog("NAMix: IR load failed: bad WAV file");
      return false;
    }
    mPendingIR = std::move(ir);
    mIRPath = file.getFullPathName();
    mIRPending.store(true);
    return true;
  } catch (const std::exception &e) {
    juce::Logger::writeToLog(juce::String("NAMix: IR load failed:") + e.what());
    return false;
  }
}

void NAMixAudioProcessor::clearIR() {
  mPendingIR.reset();
  mIRPath = {};
  mIRPending.store(true);
}

// ---------------------------------------------------------------------------
// Parameter listener
// ---------------------------------------------------------------------------
void NAMixAudioProcessor::parameterChanged(const juce::String &parameterID,
                                             float /*newValue*/) {
  if (parameterID == "slim")
    mSlimDirty.store(true, std::memory_order_release);
  else if (parameterID.startsWith("nam_param_"))
    mParamsDirty.store(true, std::memory_order_release);
}

// ---------------------------------------------------------------------------
// Parametric-model knob helpers
// ---------------------------------------------------------------------------
void NAMixAudioProcessor::updateParametricRangesAndDefaults(
    const std::vector<nam::DSPParamDef> &defs) {
  const int numParams = static_cast<int>(defs.size());
  for (int i = 0; i < kMaxParametricParams; ++i) {
    auto *fp = dynamic_cast<juce::AudioParameterFloat *>(
        apvts.getParameter(getParametricParamId(i)));
    if (!fp)
      continue;
    // A malformed/degenerate model-supplied range (min >= max) would make
    // convertTo0to1() divide by zero and propagate NaN into the parameter
    // (and from there into the DSP via SetKnobValues). Fall back to a safe
    // placeholder range instead of trusting the model file blindly. See
    // ParametricRangeTests.cpp for the direct test of this condition.
    if (i < numParams && isValidParametricRange(defs[static_cast<size_t>(i)].min_val,
                                                defs[static_cast<size_t>(i)].max_val)) {
      const auto &d = defs[static_cast<size_t>(i)];
      const float interval =
          d.steps > 1 ? (d.max_val - d.min_val) / static_cast<float>(d.steps - 1) : 0.0f;
      fp->range = juce::NormalisableRange<float>(d.min_val, d.max_val, interval);
      fp->setValueNotifyingHost(fp->convertTo0to1(d.default_val));
    } else {
      // Inactive slot, or an invalid def for an active one: reset both the
      // range and the stored value together so nothing is left pointing
      // outside the range it's actually in (host generic-parameter views
      // and saved state would otherwise see a stale, now out-of-bounds value).
      fp->range = juce::NormalisableRange<float>(0.0f, 1.0f);
      fp->setValueNotifyingHost(0.5f);
    }
  }
}

void NAMixAudioProcessor::readKnobValuesFromApvtsInto(int numParams,
                                                        std::vector<float> &out) const {
  out.resize(static_cast<size_t>(numParams));
  for (int i = 0; i < numParams; ++i) {
    auto *raw = apvts.getRawParameterValue(getParametricParamId(i));
    out[static_cast<size_t>(i)] = raw ? raw->load() : 0.0f;
  }
}

// ---------------------------------------------------------------------------
// State persistence
// ---------------------------------------------------------------------------
void NAMixAudioProcessor::getStateInformation(juce::MemoryBlock &dest) {
  auto state = apvts.copyState();
  state.setProperty("modelPath", mModelPath, nullptr);
  state.setProperty("irPath", mIRPath, nullptr);
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, dest);
}

void NAMixAudioProcessor::setStateInformation(const void *data, int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (!xml || !xml->hasTagName(apvts.state.getType()))
    return;

  auto newState = juce::ValueTree::fromXml(*xml);
  const juce::String mp = newState.getProperty("modelPath", "");
  const juce::String ip = newState.getProperty("irPath", "");

  // Load the model (and IR) BEFORE restoring the saved ValueTree below.
  // loadModel() sets each parametric-knob slot's range to match this model
  // and resets its value to the model's own default; if the saved state
  // were restored first, this would immediately stomp the user's saved
  // knob positions back to defaults. Loading first means replaceState()
  // restores the real saved values on top of correctly-ranged parameters,
  // and the parameterChanged() callbacks it fires for every value that
  // differs from the fresh default mark mParamsDirty/mSlimDirty, so
  // processBlock() re-applies the actual saved values to the live model
  // (the same mechanism already relied on for the "slim" parameter).
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

  apvts.replaceState(newState);
}

// ---------------------------------------------------------------------------
// Plugin entry
// ---------------------------------------------------------------------------
// createEditor() lives in PluginEditor.cpp: it's the only thing in this file
// that would otherwise pull in the GUI module (BinaryData, fonts,
// juce_gui_basics) as a dependency of the processor, which is undesirable
// since it means the processor -- and the parametric-knob logic it owns --
// can't be built or tested headlessly without the whole GUI stack.
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new NAMixAudioProcessor();
}
