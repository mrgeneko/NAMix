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

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"

class GatewayAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
  explicit GatewayAudioProcessorEditor(GatewayAudioProcessor&);
  ~GatewayAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  GatewayAudioProcessor& mProcessor;

  // Model file loading
  juce::TextButton mLoadModelButton{ "Load Model" };
  juce::TextButton mClearModelButton{ "Clear" };
  juce::Label mModelLabel;

  // IR file loading
  juce::TextButton mLoadIRButton{ "Load IR" };
  juce::TextButton mClearIRButton{ "Clear" };
  juce::Label mIRLabel;

  // Gain knobs
  juce::Slider mInputGainSlider;
  juce::Label mInputGainLabel{ {}, "Input" };
  juce::Slider mOutputGainSlider;
  juce::Label mOutputGainLabel{ {}, "Output" };

  // Tone stack controls
  juce::Slider mBassSlider;
  juce::Label mBassLabel{ {}, "Bass" };
  juce::Slider mMiddleSlider;
  juce::Label mMiddleLabel{ {}, "Middle" };
  juce::Slider mTrebleSlider;
  juce::Label mTrebleLabel{ {}, "Treble" };
  juce::ToggleButton mToneStackButton{ "EQ" };

  // Noise gate
  juce::Slider mNoiseGateSlider;
  juce::Label mNoiseGateLabel{ {}, "Gate" };
  juce::ToggleButton mNoiseGateButton{ "Gate" };

  // APVTS attachments keep sliders in sync with parameters.
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
    mInputGainAttachment, mOutputGainAttachment, mNoiseGateAttachment,
    mBassAttachment, mMiddleAttachment, mTrebleAttachment;

  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
    mToneStackAttachment, mNoiseGateButtonAttachment;

  void setupKnob(juce::Slider& slider, juce::Label& label);
  void chooseModelFile();
  void chooseIRFile();

  std::unique_ptr<juce::FileChooser> mFileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GatewayAudioProcessorEditor)
};
