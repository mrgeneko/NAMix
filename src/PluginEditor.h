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

#include <BinaryData.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "FileRow.h"
#include "GatewayLookAndFeel.h"
#include "GatewaySettingsPanel.h"
#include "LevelMeter.h"
#include "PluginProcessor.h"

class GatewayAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer {
public:
  explicit GatewayAudioProcessorEditor(GatewayAudioProcessor &);
  ~GatewayAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override;
  void setupKnob(juce::Slider &slider, juce::Label &label, const juce::String &name);
  void chooseModelFile();
  void chooseIRFile();

  GatewayAudioProcessor &mProcessor;
  GatewayLookAndFeel mLookAndFeel;

  // Fonts loaded from embedded binary data (Michroma-Regular, Roboto-Regular)
  juce::Font mMichromaFont;
  juce::Font mRobotoFont;

  // File loading rows
  FileRow mModelRow{"Select model..."};
  FileRow mIRRow{"Select IR..."};

  // Level meters — input on the left, output on the right (matches original)
  LevelMeter mInputLevelMeter;
  LevelMeter mLevelMeter;

  // Slim icon button (NAMSquareButtonControl equivalent).
  // Shown only when the loaded model supports slimming (SlimmableModel).
  // Clicking opens the slim overlay; hidden by default.
  juce::DrawableButton mSlimButton{"Slim", juce::DrawableButton::ImageStretched};

  // Full-editor overlay: dark backdrop + centred slim knob.
  // Matches original NAMSlimOverlayBackdropControl + NAMKnobControl.
  struct SlimOverlay : public juce::Component {
    SlimOverlay(juce::AudioProcessorValueTreeState &apvts, GatewayLookAndFeel &laf) {
      mKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
      mKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
      mKnob.setLookAndFeel(&laf);
      addAndMakeVisible(mKnob);
      mLabel.setText("Slim", juce::dontSendNotification);
      mLabel.setJustificationType(juce::Justification::centred);
      mLabel.setLookAndFeel(&laf);
      addAndMakeVisible(mLabel);
      mAttachment =
          std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
              apvts, "slim", mKnob);
      setInterceptsMouseClicks(true, true);
    }
    void paint(juce::Graphics &g) override {
      g.setColour(juce::Colours::black.withAlpha(0.45f));
      g.fillAll();
    }
    void resized() override {
      constexpr int kw = 100, kh = 120, lh = 20;
      const int kx = (getWidth() - kw) / 2;
      const int ky = (getHeight() - kh - lh) / 2;
      mLabel.setBounds(kx, ky, kw, lh);
      mKnob.setBounds(kx, ky + lh, kw, kh);
    }
    void mouseDown(const juce::MouseEvent &e) override {
      // Click anywhere outside the knob dismisses the overlay.
      if (!mKnob.getBounds().contains(e.getPosition()) &&
          !mLabel.getBounds().contains(e.getPosition()))
        setVisible(false);
    }
    juce::Slider mKnob;
    juce::Label mLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachment;
  } mSlimOverlay;


  // Gear / settings
  juce::TextButton mGearButton;
  GatewaySettingsPanel mSettingsPanel;

  // Knobs
  juce::Slider mInputGainSlider, mOutputGainSlider;
  juce::Label mInputGainLabel, mOutputGainLabel;

  juce::Slider mBassSlider, mMiddleSlider, mTrebleSlider;
  juce::Label mBassLabel, mMiddleLabel, mTrebleLabel;
  juce::ToggleButton mToneStackButton{"EQ"};

  juce::Slider mNoiseGateSlider;
  juce::Label mNoiseGateLabel;
  juce::ToggleButton mNoiseGateButton{"Noise Gate"};

  // APVTS attachments
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      mInputGainAttachment, mOutputGainAttachment, mNoiseGateAttachment, mBassAttachment,
      mMiddleAttachment, mTrebleAttachment;

  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      mToneStackAttachment, mNoiseGateButtonAttachment;

  juce::Image mBgImage;
  juce::Image mLinesImage;

  std::unique_ptr<juce::MenuBarModel> mMenuBarModel;
  std::unique_ptr<juce::FileChooser> mFileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GatewayAudioProcessorEditor)
};
