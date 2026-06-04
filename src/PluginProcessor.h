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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "NAM/dsp.h"
#include "dsp.h"
#include "ImpulseResponse.h"
#include "NoiseGate.h"
#include "ToneStack.h"

class GatewayAudioProcessor : public juce::AudioProcessor
{
public:
  GatewayAudioProcessor();
  ~GatewayAudioProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return JucePlugin_Name; }

  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "Default"; }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& dest) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  // Model and IR loading — called from the editor on the message thread.
  // The processor takes ownership and swaps on the next processBlock call.
  void loadModel(const juce::File& file);
  void clearModel();
  void loadIR(const juce::File& file);
  void clearIR();

  juce::AudioProcessorValueTreeState apvts;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
  void applyDsp(juce::AudioBuffer<float>& buffer);

  double mSampleRate{ 48000.0 };
  int mSamplesPerBlock{ 512 };

  // Pending model/IR loaded from the editor thread, swapped in on audio thread.
  std::unique_ptr<nam::DSP> mPendingModel;
  std::unique_ptr<nam::DSP> mModel;
  std::atomic<bool> mModelPending{ false };

  std::unique_ptr<dsp::ImpulseResponse> mPendingIR;
  std::unique_ptr<dsp::ImpulseResponse> mIR;
  std::atomic<bool> mIRPending{ false };

  // Noise gate: two-stage (trigger detects signal, gain applies attenuation).
  // Trigger is wired to Gain via AddListener in the constructor.
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain    mNoiseGateGain;

  dsp::tone_stack::BasicNamToneStack mToneStack;

  // Pre-allocated double-precision work buffers used in applyDsp().
  // Avoids any audio-thread allocation. mWorkPtrInput/Output point into these.
  std::vector<DSP_SAMPLE> mWorkBufInput;
  std::vector<DSP_SAMPLE> mWorkBufOutput;
  DSP_SAMPLE* mWorkPtrInput  = nullptr;
  DSP_SAMPLE* mWorkPtrOutput = nullptr;

  // Persisted file paths — stored in plugin state so DAW projects can reload.
  juce::String mModelPath;
  juce::String mIRPath;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GatewayAudioProcessor)
};
