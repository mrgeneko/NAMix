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

#include "PluginEditor.h"

GatewayAudioProcessorEditor::GatewayAudioProcessorEditor(GatewayAudioProcessor& p)
  : AudioProcessorEditor(&p), mProcessor(p)
{
  setLookAndFeel(&mLookAndFeel);
  setSize(580, 320);

  // Knobs — label text set inside setupKnob
  setupKnob(mInputGainSlider,  mInputGainLabel,  "Input");
  setupKnob(mNoiseGateSlider,  mNoiseGateLabel,  "Threshold");
  setupKnob(mBassSlider,       mBassLabel,       "Bass");
  setupKnob(mMiddleSlider,     mMiddleLabel,      "Middle");
  setupKnob(mTrebleSlider,     mTrebleLabel,     "Treble");
  setupKnob(mOutputGainSlider, mOutputGainLabel, "Output");

  // APVTS attachments
  auto& apvts = mProcessor.apvts;
  mInputGainAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "inputGain", mInputGainSlider);
  mOutputGainAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "outputGain", mOutputGainSlider);
  mNoiseGateAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "noiseGateThreshold", mNoiseGateSlider);
  mBassAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "bass", mBassSlider);
  mMiddleAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "middle", mMiddleSlider);
  mTrebleAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "treble", mTrebleSlider);
  mToneStackAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::ButtonAttachment>(
    apvts, "toneStackOn", mToneStackButton);
  mNoiseGateButtonAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::ButtonAttachment>(
    apvts, "noiseGateOn", mNoiseGateButton);

  addAndMakeVisible(mToneStackButton);
  addAndMakeVisible(mNoiseGateButton);

  // Model file row
  mModelRow.onLoad  = [this] { chooseModelFile(); };
  mModelRow.onClear = [this] {
    mProcessor.clearModel();
    mModelRow.setFilename("");
  };
  {
    const juce::String p = mProcessor.getModelPath();
    mModelRow.setFilename(p.isNotEmpty() ? juce::File(p).getFileName() : "");
  }
  addAndMakeVisible(mModelRow);

  // IR file row
  mIRRow.onLoad  = [this] { chooseIRFile(); };
  mIRRow.onClear = [this] {
    mProcessor.clearIR();
    mIRRow.setFilename("");
  };
  {
    const juce::String p = mProcessor.getIRPath();
    mIRRow.setFilename(p.isNotEmpty() ? juce::File(p).getFileName() : "");
  }
  addAndMakeVisible(mIRRow);

  addAndMakeVisible(mLevelMeter);

  startTimerHz(30);
}

GatewayAudioProcessorEditor::~GatewayAudioProcessorEditor()
{
  stopTimer();
  setLookAndFeel(nullptr);
}

void GatewayAudioProcessorEditor::timerCallback()
{
  mLevelMeter.setLevel(mProcessor.getOutputLevel());
}

void GatewayAudioProcessorEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colour(0xff1a1a2e));

  // Title
  g.setColour(juce::Colours::white);
  g.setFont(juce::Font(20.0f, juce::Font::bold));
  g.drawText("GATEWAY",
             juce::Rectangle<int>(0, 0, getWidth() - 28, 32),
             juce::Justification::centred);

  // Settings icon (top right) — 8-tooth gear drawn with radial lines
  {
    const float gx = getWidth() - 16.0f;
    const float gy = 16.0f;
    const float outerR = 8.0f;
    const float innerR = 5.0f;
    const float holeR  = 2.5f;
    g.setColour(juce::Colours::white.withAlpha(0.55f));

    juce::Path gear;
    const int teeth = 8;
    for (int i = 0; i < teeth; ++i)
    {
      const float a0 = juce::MathConstants<float>::twoPi * i / teeth;
      const float a1 = juce::MathConstants<float>::twoPi * (i + 0.3f) / teeth;
      const float a2 = juce::MathConstants<float>::twoPi * (i + 0.7f) / teeth;
      const float a3 = juce::MathConstants<float>::twoPi * (i + 1.0f) / teeth;

      auto pt = [&](float r, float a) -> juce::Point<float> {
        return { gx + r * std::sin(a), gy - r * std::cos(a) };
      };

      if (i == 0)
        gear.startNewSubPath(pt(innerR, a0));
      else
        gear.lineTo(pt(innerR, a0));
      gear.lineTo(pt(outerR, a1));
      gear.lineTo(pt(outerR, a2));
      gear.lineTo(pt(innerR, a3));
    }
    gear.closeSubPath();
    g.fillPath(gear);

    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillEllipse(gx - holeR, gy - holeR, holeR * 2.0f, holeR * 2.0f);
  }

  // Separator line above file rows
  g.setColour(juce::Colours::white.withAlpha(0.08f));
  g.drawHorizontalLine(220, 0.0f, static_cast<float>(getWidth() - 22));
}

void GatewayAudioProcessorEditor::resized()
{
  const int meterW   = 12;
  const int meterGap = 6;
  const int contentW = getWidth() - meterW - meterGap;

  // Meter strip on the right
  mLevelMeter.setBounds(contentW + meterGap, 32, meterW, getHeight() - 40);

  const int knobW = contentW / 6;

  auto area = juce::Rectangle<int>(0, 0, contentW, getHeight());
  area.removeFromTop(32); // title strip

  // Knob area: label (16px) above, rotary slider fills remainder
  {
    auto knobArea = area.removeFromTop(150);
    auto placeKnob = [&](juce::Slider& s, juce::Label& l) {
      auto cell = knobArea.removeFromLeft(knobW);
      l.setBounds(cell.removeFromTop(16));
      s.setBounds(cell);
    };
    placeKnob(mInputGainSlider,  mInputGainLabel);
    placeKnob(mNoiseGateSlider,  mNoiseGateLabel);
    placeKnob(mBassSlider,       mBassLabel);
    placeKnob(mMiddleSlider,     mMiddleLabel);
    placeKnob(mTrebleSlider,     mTrebleLabel);
    placeKnob(mOutputGainSlider, mOutputGainLabel);
  }

  // Toggle strip: Noise Gate under Threshold (knob 1), EQ under Middle (knob 3)
  {
    auto toggleArea = area.removeFromTop(36);
    const int ty = toggleArea.getY();
    mNoiseGateButton.setBounds(knobW + (knobW - 90) / 2, ty, 90, 36);
    mToneStackButton.setBounds(knobW * 3 + (knobW - 60) / 2, ty, 60, 36);
  }

  area.removeFromTop(6);

  // File rows
  mModelRow.setBounds(area.removeFromTop(38));
  area.removeFromTop(4);
  mIRRow.setBounds(area.removeFromTop(38));
}

void GatewayAudioProcessorEditor::setupKnob(juce::Slider& slider,
                                              juce::Label& label,
                                              const juce::String& name)
{
  label.setText(name, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  label.setFont(juce::Font(11.0f));
  label.setColour(juce::Label::textColourId, juce::Colours::white);
  addAndMakeVisible(label);

  slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
  addAndMakeVisible(slider);
}

void GatewayAudioProcessorEditor::chooseModelFile()
{
  mFileChooser = std::make_unique<juce::FileChooser>(
    "Load NAM Model",
    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
    "*.nam");

  mFileChooser->launchAsync(
    juce::FileBrowserComponent::openMode
      | juce::FileBrowserComponent::canSelectFiles,
    [this](const juce::FileChooser& fc) {
      auto result = fc.getResult();
      if (result.existsAsFile() && mProcessor.loadModel(result))
        mModelRow.setFilename(result.getFileName());
    });
}

void GatewayAudioProcessorEditor::chooseIRFile()
{
  mFileChooser = std::make_unique<juce::FileChooser>(
    "Load Impulse Response",
    juce::File::getSpecialLocation(juce::File::userHomeDirectory),
    "*.wav");

  mFileChooser->launchAsync(
    juce::FileBrowserComponent::openMode
      | juce::FileBrowserComponent::canSelectFiles,
    [this](const juce::FileChooser& fc) {
      auto result = fc.getResult();
      if (result.existsAsFile() && mProcessor.loadIR(result))
        mIRRow.setFilename(result.getFileName());
    });
}
