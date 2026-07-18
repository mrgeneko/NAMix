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

#include <array>

#include "FileRow.h"
#include "NAMixLookAndFeel.h"
#include "NAMixSettingsPanel.h"
#include "LevelMeter.h"
#include "PluginProcessor.h"

class NAMixAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer {
public:
  explicit NAMixAudioProcessorEditor(NAMixAudioProcessor &);
  ~NAMixAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void timerCallback() override;
  void setupKnob(juce::Slider &slider, juce::Label &label, const juce::String &name);
  void chooseModelFile();
  void chooseIRFile();

  NAMixAudioProcessor &mProcessor;
  NAMixLookAndFeel mLookAndFeel;

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
    SlimOverlay(juce::AudioProcessorValueTreeState &apvts, NAMixLookAndFeel &laf) {
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

  // Inline parametric-knob row, shown permanently below the toggle strip
  // (matches NeuralAmpModelerPlugin's inline row, added in place of an
  // earlier click-to-reveal icon+overlay design). Built dynamically from
  // the loaded model's own DSPParamDef metadata (name/min/max/default/
  // steps) whenever a model loads; empty (all children hidden) when the
  // loaded model isn't parametric. Sized to the same
  // NAMixAudioProcessor::kMaxParametricParams cap as the DSP/APVTS side —
  // resized() shrinks row height (not just knob width) as needed so that
  // however many are active, none ever get clipped by our fixed bounds.
  struct ParametricRow : public juce::Component {
    ParametricRow(NAMixAudioProcessor &proc, NAMixLookAndFeel &laf)
        : mProcessor(proc), mLaf(laf) {
      for (auto &lbl : mLabels) {
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setLookAndFeel(&laf);
        addChildComponent(lbl);
      }
      for (auto &sld : mKnobs) {
        sld.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sld.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 17);
        sld.setNumDecimalPlacesToDisplay(2);
        sld.setLookAndFeel(&laf);
        addChildComponent(sld);
      }
    }
    ~ParametricRow() override {
      for (auto &lbl : mLabels)
        lbl.setLookAndFeel(nullptr);
      for (auto &sld : mKnobs)
        sld.setLookAndFeel(nullptr);
    }
    // Rebuilds knobs/attachments from the processor's current model. Call
    // whenever a model finishes loading (or is cleared) — the set of
    // active knobs changes with it.
    void refresh() {
      mAttachments.clear();
      auto defs = mProcessor.getModelParamDefs();
      mActiveCount = juce::jmin(static_cast<int>(defs.size()),
                                NAMixAudioProcessor::kMaxParametricParams);
      for (int i = 0; i < NAMixAudioProcessor::kMaxParametricParams; ++i) {
        const bool active = i < mActiveCount;
        mKnobs[static_cast<size_t>(i)].setVisible(active);
        mLabels[static_cast<size_t>(i)].setVisible(active);
        if (active) {
          mLabels[static_cast<size_t>(i)].setText(defs[static_cast<size_t>(i)].name,
                                                  juce::dontSendNotification);
          mAttachments.push_back(
              std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                  mProcessor.apvts, NAMixAudioProcessor::getParametricParamId(i),
                  mKnobs[static_cast<size_t>(i)]));
        }
      }
      resized();
    }
    void resized() override {
      if (mActiveCount <= 0)
        return;
      // Mirrors NeuralAmpModelerPlugin's packing: knobs shrink from 100px
      // towards a 70px floor as more slots are active, wrapping to another
      // row rather than going narrower than that.
      constexpr int kKnobMaxW = 100, kKnobMinW = 70;
      constexpr int khMax = 110, khMin = 40, lh = 18, gap = 10;
      int cols = mActiveCount;
      int rows = 1;
      int kw = kKnobMaxW;
      while (true) {
        kw = (getWidth() - (cols - 1) * gap) / juce::jmax(cols, 1);
        if (kw >= kKnobMinW || cols <= 1)
          break;
        ++rows;
        cols = (mActiveCount + rows - 1) / rows;
      }
      kw = juce::jlimit(kKnobMinW, kKnobMaxW, kw);

      // However many rows the width-shrink above settled on, shrink knob
      // HEIGHT (not just width) so that many rows always fit within our
      // own bounds — this component's height is fixed by the caller
      // (Layout::PARAMETRIC_ROW_H), so nothing may extend past it.
      int kh = khMax;
      int rowH = kh + lh;
      const int neededH = rows * rowH + (rows - 1) * gap;
      if (neededH > getHeight()) {
        const int available = getHeight() - (rows - 1) * gap - rows * lh;
        kh = juce::jmax(khMin, available / juce::jmax(rows, 1));
        rowH = kh + lh;
      }

      const int totalW = cols * kw + (cols - 1) * gap;
      const int totalH = rows * rowH + (rows - 1) * gap;
      const int x0 = (getWidth() - totalW) / 2;
      const int y0 = juce::jmax(0, (getHeight() - totalH) / 2);
      for (int i = 0; i < mActiveCount; ++i) {
        const int col = i % cols;
        const int row = i / cols;
        const int x = x0 + col * (kw + gap);
        const int y = y0 + row * (rowH + gap);
        mLabels[static_cast<size_t>(i)].setBounds(x, y, kw, lh);
        mKnobs[static_cast<size_t>(i)].setBounds(x, y + lh, kw, kh);
      }
    }
    NAMixAudioProcessor &mProcessor;
    NAMixLookAndFeel &mLaf;
    int mActiveCount = 0;
    std::array<juce::Slider, NAMixAudioProcessor::kMaxParametricParams> mKnobs;
    std::array<juce::Label, NAMixAudioProcessor::kMaxParametricParams> mLabels;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>
        mAttachments;
  } mParametricRow;

  // Gear / settings
  juce::TextButton mGearButton;
  NAMixSettingsPanel mSettingsPanel;

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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NAMixAudioProcessorEditor)
};
