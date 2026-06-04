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
#include <BinaryData.h>

#include "PluginProcessor.h"
#include "GatewayLookAndFeel.h"
#include "FileRow.h"
#include "LevelMeter.h"
#include "GatewaySettingsPanel.h"

class GatewayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
  explicit GatewayAudioProcessorEditor(GatewayAudioProcessor&);
  ~GatewayAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  void timerCallback() override;
  void setupKnob(juce::Slider& slider, juce::Label& label, const juce::String& name);
  void chooseModelFile();
  void chooseIRFile();

  GatewayAudioProcessor& mProcessor;
  GatewayLookAndFeel     mLookAndFeel;

  // Fonts loaded from embedded binary data (Michroma-Regular, Roboto-Regular)
  juce::Font mMichromaFont;
  juce::Font mRobotoFont;

  // File loading rows
  FileRow    mModelRow { "Select model..." };
  FileRow    mIRRow    { "Select IR..." };

  // Level meters — input on the left, output on the right (matches original)
  LevelMeter mInputLevelMeter;
  LevelMeter mLevelMeter;

  // Slim slider (next to NAM model row, matches original slimIconArea)
  juce::Slider mSlimSlider;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
    mSlimAttachment;

  // Gear / settings
  juce::TextButton      mGearButton;
  GatewaySettingsPanel  mSettingsPanel;

  // Knobs
  juce::Slider mInputGainSlider, mOutputGainSlider;
  juce::Label  mInputGainLabel,  mOutputGainLabel;

  juce::Slider mBassSlider, mMiddleSlider, mTrebleSlider;
  juce::Label  mBassLabel,  mMiddleLabel,  mTrebleLabel;
  juce::ToggleButton mToneStackButton { "EQ" };

  juce::Slider mNoiseGateSlider;
  juce::Label  mNoiseGateLabel;
  juce::ToggleButton mNoiseGateButton { "Noise Gate" };

  // APVTS attachments
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
    mInputGainAttachment, mOutputGainAttachment, mNoiseGateAttachment,
    mBassAttachment, mMiddleAttachment, mTrebleAttachment;

  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
    mToneStackAttachment, mNoiseGateButtonAttachment;

  std::unique_ptr<juce::FileChooser> mFileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GatewayAudioProcessorEditor)
};
