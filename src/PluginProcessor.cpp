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

namespace params
{
static const juce::String kInputGain{ "inputGain" };
static const juce::String kOutputGain{ "outputGain" };
static const juce::String kNoiseGateThreshold{ "noiseGateThreshold" };
static const juce::String kBass{ "bass" };
static const juce::String kMiddle{ "middle" };
static const juce::String kTreble{ "treble" };
static const juce::String kToneStackOn{ "toneStackOn" };
static const juce::String kNoiseGateOn{ "noiseGateOn" };
} // namespace params

juce::AudioProcessorValueTreeState::ParameterLayout
GatewayAudioProcessor::createParameterLayout()
{
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ params::kInputGain, 1 }, "Input Gain",
    juce::NormalisableRange<float>(-40.0f, 40.0f, 0.1f), 0.0f,
    juce::AudioParameterFloatAttributes{}.withLabel("dB")));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ params::kOutputGain, 1 }, "Output Gain",
    juce::NormalisableRange<float>(-40.0f, 40.0f, 0.1f), 0.0f,
    juce::AudioParameterFloatAttributes{}.withLabel("dB")));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ params::kNoiseGateThreshold, 1 }, "Noise Gate",
    juce::NormalisableRange<float>(-100.0f, 0.0f, 0.1f), -80.0f,
    juce::AudioParameterFloatAttributes{}.withLabel("dB")));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ params::kBass, 1 }, "Bass",
    juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ params::kMiddle, 1 }, "Middle",
    juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));

  layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID{ params::kTreble, 1 }, "Treble",
    juce::NormalisableRange<float>(0.0f, 10.0f, 0.1f), 5.0f));

  layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{ params::kToneStackOn, 1 }, "Tone Stack", true));

  layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID{ params::kNoiseGateOn, 1 }, "Noise Gate On", true));

  return layout;
}

GatewayAudioProcessor::GatewayAudioProcessor()
  : AudioProcessor(
      BusesProperties()
        .withInput("Input", juce::AudioChannelSet::mono(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
  , apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

GatewayAudioProcessor::~GatewayAudioProcessor() {}

void GatewayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
  mSampleRate = sampleRate;
  mSamplesPerBlock = samplesPerBlock;

  mToneStack.Reset(sampleRate);
}

void GatewayAudioProcessor::releaseResources() {}

bool GatewayAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  // Mono input, stereo output (matching original NAM)
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono()
      && layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
    return false;
  return true;
}

void GatewayAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
  juce::ScopedNoDenormals noDenormals;

  // Swap in any pending model or IR atomically.
  if (mModelPending.exchange(false))
    mModel = std::move(mPendingModel);
  if (mIRPending.exchange(false))
    mIR = std::move(mPendingIR);

  applyDsp(buffer);
}

void GatewayAudioProcessor::applyDsp(juce::AudioBuffer<float>& buffer)
{
  const int numSamples = buffer.getNumSamples();

  const float inputGainDb =
    apvts.getRawParameterValue(params::kInputGain)->load();
  const float outputGainDb =
    apvts.getRawParameterValue(params::kOutputGain)->load();
  const float inputGainLinear = juce::Decibels::decibelsToGain(inputGainDb);
  const float outputGainLinear = juce::Decibels::decibelsToGain(outputGainDb);

  // Work on the first (mono) input channel.
  float* const monoIn = buffer.getWritePointer(0);

  // Apply input gain.
  juce::FloatVectorOperations::multiply(monoIn, inputGainLinear, numSamples);

  // Noise gate trigger on the mono input.
  const bool ngOn =
    apvts.getRawParameterValue(params::kNoiseGateOn)->load() > 0.5f;
  if (ngOn)
  {
    const float ngThreshDb =
      apvts.getRawParameterValue(params::kNoiseGateThreshold)->load();
    // TODO: wire up dsp::noise_gate properly once parameter mapping is finalised.
    (void)ngThreshDb;
  }

  // Run NAM model (mono).
  if (mModel)
  {
    // nam::DSP::process expects std::vector<float>
    // TODO: avoid allocation by pre-allocating a work buffer in prepareToPlay.
    std::vector<float> inVec(monoIn, monoIn + numSamples);
    std::vector<float> outVec(numSamples);
    mModel->process(inVec.data(), outVec.data(), numSamples);
    std::copy(outVec.begin(), outVec.end(), monoIn);
  }

  // Tone stack (mono).
  const bool tsOn =
    apvts.getRawParameterValue(params::kToneStackOn)->load() > 0.5f;
  if (tsOn)
  {
    mToneStackParams.bass =
      apvts.getRawParameterValue(params::kBass)->load() / 10.0;
    mToneStackParams.middle =
      apvts.getRawParameterValue(params::kMiddle)->load() / 10.0;
    mToneStackParams.treble =
      apvts.getRawParameterValue(params::kTreble)->load() / 10.0;
    mToneStack.SetParams(mToneStackParams);
    mToneStack.Process(monoIn, numSamples);
  }

  // IR convolution (mono).
  if (mIR)
  {
    // TODO: wire up IR processing.
  }

  // Apply output gain then copy mono -> stereo.
  juce::FloatVectorOperations::multiply(monoIn, outputGainLinear, numSamples);

  // Copy mono channel to any additional output channels.
  for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
    buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

void GatewayAudioProcessor::loadModel(const juce::File& file)
{
  try
  {
    auto dsp = nam::get_dsp(file.getFullPathName().toStdString());
    mPendingModel = std::move(dsp);
    mModelPending.store(true);
  }
  catch (const std::exception& e)
  {
    // TODO: surface error to editor.
    juce::Logger::writeToLog(juce::String("Gateway: failed to load model: ") + e.what());
  }
}

void GatewayAudioProcessor::clearModel()
{
  mPendingModel.reset();
  mModelPending.store(true);
}

void GatewayAudioProcessor::loadIR(const juce::File& file)
{
  // TODO: implement IR loading via AudioDSPTools.
  (void)file;
}

void GatewayAudioProcessor::clearIR()
{
  mPendingIR.reset();
  mIRPending.store(true);
}

void GatewayAudioProcessor::getStateInformation(juce::MemoryBlock& dest)
{
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, dest);
}

void GatewayAudioProcessor::setStateInformation(const void* data,
                                                   int sizeInBytes)
{
  std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
  if (xml && xml->hasTagName(apvts.state.getType()))
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* GatewayAudioProcessor::createEditor()
{
  return new GatewayAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
  return new GatewayAudioProcessor();
}
