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

#pragma once
#include <BinaryData.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Full-size overlay panel (600x400) matching NAMSettingsPageControl exactly.
//
// Geometry (all derived from original NAM iPlug2 layout, pad=20):
//
//   titleArea       (30,  30, 540,  50)  "SETTINGS" Michroma 30px centred
//   inputOutputArea (30,  80, 540, 180)
//     inputArea     (30,  80, 270, 180)  left half — input calibration
//       inputLevelArea  (121, 175,  87,  25)  editable dBu value
//       inputSwitchArea (121, 210,  87,  50)  "Calibrate Input" pill toggle
//     outputArea    (300,  80, 270, 180) right half — output mode
//       outputRadioArea (300, 177, 270,  83)  Raw/Normalized/Calibrated vertical
//   bottomArea      (20,  282, 560,  78)  lineHeight=15
//     modelInfo     (20,  282, 280,  60)  left  — model sample rate
//     about         (300, 282, 280,  75)  right — 5 lines white Roboto 14px
//   closeButton     (545,  35,  20,  20)

class NAMixSettingsPanel : public juce::Component {
public:
  std::function<void()> onClose;

  explicit NAMixSettingsPanel(juce::AudioProcessorValueTreeState &apvts)
      : mApvts(apvts) {
    auto michromaTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
    mMichromaFont = juce::Font(michromaTypeface).withHeight(30.0f);

    auto robotoTypeface = juce::Typeface::createSystemTypefaceFor(
        BinaryData::RobotoRegular_ttf, BinaryData::RobotoRegular_ttfSize);
    mRobotoFont = juce::Font(robotoTypeface).withHeight(14.0f);

    mInputLevelBg = juce::ImageCache::getFromMemory(
        BinaryData::InputLevelBackground_png, BinaryData::InputLevelBackground_pngSize);

    // Close button — CornerButtonArea: (545, 35, 20, 20)
    mCloseButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc3\x97")));
    mCloseButton.onClick = [this] {
      if (onClose)
        onClose();
    };
    addAndMakeVisible(mCloseButton);

    // -----------------------------------------------------------------------
    // Left side — Input calibration (inputArea = (30, 80, 270, 180))
    // -----------------------------------------------------------------------
    // Matches original InputLevelControl (IEditableTextControl): a plain
    // editable label showing "12.0 dBu". The bitmap background is drawn in
    // paint(). Clicking opens a text editor; on commit the value is clamped
    // to [-60, 60] and written back to the APVTS parameter.
    {
      const float val = apvts.getRawParameterValue("inputCalibrationLevel")->load();
      mInputCalibLabel.setText(juce::String(val, 1) + " dBu", juce::dontSendNotification);
    }
    mInputCalibLabel.setEditable(true);
    mInputCalibLabel.setJustificationType(juce::Justification::centred);
    mInputCalibLabel.setColour(juce::Label::textColourId, NAMColours::FONT);
    mInputCalibLabel.setColour(juce::Label::backgroundColourId,
                               juce::Colours::transparentBlack);
    mInputCalibLabel.setColour(juce::Label::outlineWhenEditingColourId,
                               NAMColours::BLUE.withAlpha(0.5f));
    mInputCalibLabel.setTooltip("Analog reference level in dBu RMS corresponding "
                                "to 0 dBFS peak in the host.");
    mInputCalibLabel.onTextChange = [this] {
      const float newVal = juce::jlimit(-60.0f, 60.0f,
                                        mInputCalibLabel.getText()
                                            .upToFirstOccurrenceOf(" ", false, false)
                                            .getFloatValue());
      if (auto *param = mApvts.getParameter("inputCalibrationLevel"))
        param->setValueNotifyingHost(
            juce::NormalisableRange<float>(-60.0f, 60.0f, 0.1f).convertTo0to1(newVal));
      const float actual = mApvts.getRawParameterValue("inputCalibrationLevel")->load();
      mInputCalibLabel.setText(juce::String(actual, 1) + " dBu",
                               juce::dontSendNotification);
    };
    addAndMakeVisible(mInputCalibLabel);

    // "Calibrate Input" pill toggle — inputSwitchArea (121, 210, 87, 50)
    mCalibrateInputButton.setButtonText("Calibrate Input");
    addAndMakeVisible(mCalibrateInputButton);
    mCalibrateInputAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, "calibrateInput", mCalibrateInputButton);

    // -----------------------------------------------------------------------
    // Right side — Output mode (outputRadioArea = (300, 177, 270, 83))
    // -----------------------------------------------------------------------
    const char *outputLabels[3] = {"Raw", "Normalized", "Calibrated"};
    for (int i = 0; i < 3; ++i) {
      mOutputButtons[i].setButtonText(outputLabels[i]);
      mOutputButtons[i].setRadioGroupId(101, juce::dontSendNotification);
      const int idx = i;
      mOutputButtons[i].onClick = [this, idx] { setOutputMode(idx); };
      addAndMakeVisible(mOutputButtons[i]);
    }

    refreshFromState();
  }

  // Enable/disable input calibration controls based on whether the loaded model
  // carries input level metadata (HasInputLevel). Mirrors original behaviour
  // where kCtrlTagCalibrateInput and kCtrlTagInputCalibrationLevel are disabled
  // when !mModel->HasInputLevel().
  void setInputCalibrationEnabled(bool enabled) {
    mInputCalibLabel.setEnabled(enabled);
    mCalibrateInputButton.setEnabled(enabled);
  }

  // Called from the editor's timer to keep the label in sync with the parameter
  // (e.g. after a preset load). Skips the update while the user is typing.
  void refreshCalibrationDisplay() {
    if (!mInputCalibLabel.isBeingEdited()) {
      const float val = mApvts.getRawParameterValue("inputCalibrationLevel")->load();
      mInputCalibLabel.setText(juce::String(val, 1) + " dBu", juce::dontSendNotification);
    }
  }

  // Called by the editor after a model loads.
  void setModelSampleRate(double sampleRate) {
    mModelSampleRate = juce::String(static_cast<int>(sampleRate)) + " Hz";
    mHasModelInfo = true;
    repaint();
  }

  void clearModelInfo() {
    mModelSampleRate = {};
    mHasModelInfo = false;
    repaint();
  }

  void setOutputModeSupport(bool hasLoudness, bool hasOutputLevel) {
    mOutputButtons[1].setButtonText(hasLoudness ? "Normalized"
                                                : "Normalized [Not supported by model]");
    mOutputButtons[2].setButtonText(
        hasOutputLevel ? "Calibrated" : "Calibrated [Not supported by model]");
    repaint();
  }

  void clearOutputModeSupport() {
    mOutputButtons[1].setButtonText("Normalized");
    mOutputButtons[2].setButtonText("Calibrated");
    repaint();
  }

  void refreshFromState() {
    const int mode = static_cast<int>(mApvts.getRawParameterValue("outputMode")->load());
    for (int i = 0; i < 3; ++i)
      mOutputButtons[i].setToggleState(i == mode, juce::dontSendNotification);
  }

  void visibilityChanged() override {
    if (isVisible()) {
      refreshFromState();
      refreshCalibrationDisplay();
    }
  }

  void paint(juce::Graphics &g) override {
    // Opaque NAM_1 background — covers the entire plugin window
    g.fillAll(juce::Colour(0xff1d1a1f));

    // Title — Michroma-Regular 30px, white, centred in titleArea (30,30,540,50)
    g.setFont(mMichromaFont);
    g.setColour(juce::Colours::white);
    g.drawText("SETTINGS", juce::Rectangle<int>(30, 30, 540, 50),
               juce::Justification::centred);

    // -----------------------------------------------------------------------
    // Left-side header (above input calibration controls)
    // -----------------------------------------------------------------------
    g.setFont(mRobotoFont.withHeight(11.0f));
    g.setColour(juce::Colour(0xffa2b2bf)); // NAM_3 Cadet Blue for section headers
    g.drawText("INPUT CALIBRATION", juce::Rectangle<int>(30, 85, 270, 14),
               juce::Justification::centredLeft);

    // Recessed background bitmap behind the dBu input level text box
    // inputLevelArea: (121, 175, 87, 25)
    if (mInputLevelBg.isValid())
      g.drawImage(mInputLevelBg, 121, 175, 87, 25, 0, 0, mInputLevelBg.getWidth(),
                  mInputLevelBg.getHeight(), false);

    // -----------------------------------------------------------------------
    // Right-side header (above output mode radio buttons)
    // -----------------------------------------------------------------------
    g.drawText("OUTPUT MODE", juce::Rectangle<int>(300, 160, 270, 14),
               juce::Justification::centredLeft);

    // Separator between middle section and bottom
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(275, 20.0f, 580.0f);

    // -----------------------------------------------------------------------
    // Bottom area — (20, 282, 560, 78), lineHeight=15
    // Original: IText(DEFAULT_TEXT_SIZE=14, EAlign::Near, HELP_TEXT=COLOR_WHITE)
    // -----------------------------------------------------------------------
    constexpr int lh = 15;

    // Left half — model info (mirrors ModelInfoControl, 4 lines x 15px)
    if (mHasModelInfo) {
      g.setFont(mRobotoFont.withHeight(14.0f));
      g.setColour(juce::Colours::white);
      g.drawText("Model information:", juce::Rectangle<int>(20, 282 + 0 * lh, 280, lh),
                 juce::Justification::centredLeft);
      g.drawText("Sample rate: " + mModelSampleRate,
                 juce::Rectangle<int>(20, 282 + 1 * lh, 280, lh),
                 juce::Justification::centredLeft);
    }

    // Right half — about (mirrors AboutControl, 5 lines x 15px)
    g.setFont(mRobotoFont.withHeight(14.0f));
    g.setColour(juce::Colours::white);
    g.drawText("NAMix", juce::Rectangle<int>(300, 282 + 0 * lh, 280, lh),
               juce::Justification::centredLeft);
    g.drawText("Based on NeuralAmpModelerPlugin by Steven Atkinson",
               juce::Rectangle<int>(300, 282 + 1 * lh, 280, lh),
               juce::Justification::centredLeft);
    g.drawText("MIT licence - GPL v3 required by JUCE",
               juce::Rectangle<int>(300, 282 + 2 * lh, 280, lh),
               juce::Justification::centredLeft);
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawText("github.com/rations/namix-linux",
               juce::Rectangle<int>(300, 282 + 3 * lh, 280, lh),
               juce::Justification::centredLeft);
  }

  void resized() override {
    // Close button — CornerButtonArea from original: (545, 35, 20, 20)
    mCloseButton.setBounds(545, 35, 20, 20);

    // -----------------------------------------------------------------------
    // Left side — input calibration
    //   inputLevelArea  (121, 175, 87, 25) — in original this is the editable
    //                   text showing the dBu value. We use a compact rotary
    //                   centred in the left half above it.
    //   inputSwitchArea (121, 210, 87, 50) — "Calibrate Input" pill toggle
    // -----------------------------------------------------------------------
    // Rotary centred in left inputArea (30,80,270,180): centre x=165
    // Position it so textbox lines up near inputLevelArea y=175
    // inputLevelArea from original: (121, 175, 87, 25)
    mInputCalibLabel.setBounds(121, 175, 87, 25);

    // "Calibrate Input" toggle in inputSwitchArea (121, 210, 87, 50)
    mCalibrateInputButton.setBounds(121, 210, 87, 50);

    // -----------------------------------------------------------------------
    // Right side — output mode radio buttons
    //   outputRadioArea (300, 177, 270, 83) split into 3 x ~28px vertical slots
    // -----------------------------------------------------------------------
    constexpr int ox = 300, oy = 177, ow = 270, oh = 28;
    mOutputButtons[0].setBounds(ox, oy + 0 * oh, ow, oh);
    mOutputButtons[1].setBounds(ox, oy + 1 * oh, ow, oh);
    mOutputButtons[2].setBounds(ox, oy + 2 * oh, ow, oh);
  }

private:
  void setOutputMode(int index) {
    if (auto *param = mApvts.getParameter("outputMode"))
      param->setValueNotifyingHost((float)index / 2.0f);
  }

  juce::AudioProcessorValueTreeState &mApvts;
  juce::Font mMichromaFont;
  juce::Font mRobotoFont;
  juce::Image mInputLevelBg;
  juce::String mModelSampleRate;
  bool mHasModelInfo = false;

  juce::TextButton mCloseButton;

  // Left side — input calibration
  juce::Label mInputCalibLabel;
  juce::ToggleButton mCalibrateInputButton;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      mCalibrateInputAttachment;

  // Right side — output mode
  juce::ToggleButton mOutputButtons[3];
};
