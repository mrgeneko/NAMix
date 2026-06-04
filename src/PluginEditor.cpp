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
  : AudioProcessorEditor(&p)
  , mProcessor(p)
  , mSettingsPanel(p.apvts)
{
  setLookAndFeel(&mLookAndFeel);
  setSize(580, 320);

  // --- Knobs ---
  setupKnob(mInputGainSlider,  mInputGainLabel,  "Input");
  setupKnob(mNoiseGateSlider,  mNoiseGateLabel,  "Threshold");
  setupKnob(mBassSlider,       mBassLabel,       "Bass");
  setupKnob(mMiddleSlider,     mMiddleLabel,      "Middle");
  setupKnob(mTrebleSlider,     mTrebleLabel,     "Treble");
  setupKnob(mOutputGainSlider, mOutputGainLabel, "Output");

  // --- APVTS attachments ---
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

  // --- Slim slider ---
  mSlimSlider.setSliderStyle(juce::Slider::LinearHorizontal);
  mSlimSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
  mSlimSlider.setTooltip("Model size (0=smallest, 1=largest). Applies to slimmable"
                          " WaveNet models only.");
  addAndMakeVisible(mSlimSlider);
  mSlimAttachment = std::make_unique<
    juce::AudioProcessorValueTreeState::SliderAttachment>(
    apvts, "slim", mSlimSlider);

  // --- Gear / settings button ---
  mGearButton.setButtonText("");
  mGearButton.setTooltip("Settings");
  mGearButton.onClick = [this] {
    mSettingsPanel.setVisible(!mSettingsPanel.isVisible());
  };
  addAndMakeVisible(mGearButton);

  // --- Settings panel (full-size overlay, hidden by default) ---
  mSettingsPanel.onClose = [this] { mSettingsPanel.setVisible(false); };
  addChildComponent(mSettingsPanel); // invisible until gear clicked

  // --- Model file row ---
  mModelRow.onOpenChooser = [this] { chooseModelFile(); };
  mModelRow.onLoad = [this](juce::File f) {
    if (mProcessor.loadModel(f))
      mModelRow.setLoadedFile(f, "nam");
  };
  mModelRow.onClear = [this] {
    mProcessor.clearModel();
    mModelRow.clearFile();
  };
  {
    const juce::String mp = mProcessor.getModelPath();
    if (mp.isNotEmpty())
      mModelRow.setLoadedFile(juce::File(mp), "nam");
  }
  addAndMakeVisible(mModelRow);

  // --- IR file row ---
  mIRRow.onOpenChooser = [this] { chooseIRFile(); };
  mIRRow.onLoad = [this](juce::File f) {
    if (mProcessor.loadIR(f))
      mIRRow.setLoadedFile(f, "wav");
  };
  mIRRow.onClear = [this] {
    mProcessor.clearIR();
    mIRRow.clearFile();
  };
  {
    const juce::String ip = mProcessor.getIRPath();
    if (ip.isNotEmpty())
      mIRRow.setLoadedFile(juce::File(ip), "wav");
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

  // Gear icon drawn behind the transparent mGearButton
  {
    const float gx     = getWidth() - 16.0f;
    const float gy     = 16.0f;
    const float outer  = 8.0f;
    const float inner  = 5.0f;
    const float holeR  = 2.5f;
    const int   teeth  = 8;
    g.setColour(juce::Colours::white.withAlpha(0.55f));

    juce::Path gear;
    for (int i = 0; i < teeth; ++i)
    {
      const float a0 = juce::MathConstants<float>::twoPi * i / teeth;
      const float a1 = juce::MathConstants<float>::twoPi * (i + 0.3f) / teeth;
      const float a2 = juce::MathConstants<float>::twoPi * (i + 0.7f) / teeth;
      const float a3 = juce::MathConstants<float>::twoPi * (i + 1.0f) / teeth;
      auto pt = [&](float r, float a) -> juce::Point<float> {
        return { gx + r * std::sin(a), gy - r * std::cos(a) };
      };
      if (i == 0) gear.startNewSubPath(pt(inner, a0));
      else         gear.lineTo(pt(inner, a0));
      gear.lineTo(pt(outer, a1));
      gear.lineTo(pt(outer, a2));
      gear.lineTo(pt(inner, a3));
    }
    gear.closeSubPath();
    g.fillPath(gear);
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillEllipse(gx - holeR, gy - holeR, holeR * 2.0f, holeR * 2.0f);
  }

  // Separator line above file rows
  g.setColour(juce::Colours::white.withAlpha(0.08f));
  g.drawHorizontalLine(184, 0.0f, static_cast<float>(getWidth() - 22));
}

void GatewayAudioProcessorEditor::resized()
{
  const int meterW   = 12;
  const int meterGap = 6;
  const int contentW = getWidth() - meterW - meterGap;
  const int knobW    = contentW / 6;

  // Meter strip
  mLevelMeter.setBounds(contentW + meterGap, 32, meterW, getHeight() - 40);

  // Gear button overlays the drawn gear icon
  mGearButton.setBounds(getWidth() - 26, 4, 24, 24);

  // Settings panel covers full editor
  mSettingsPanel.setBounds(getLocalBounds());

  auto area = juce::Rectangle<int>(0, 0, contentW, getHeight());
  area.removeFromTop(32); // title

  // Knob area — smaller than before to reduce knob diameter
  {
    auto knobArea = area.removeFromTop(100);
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

  area.removeFromTop(12); // gap between knobs and toggles

  // Toggle strip — centred under Threshold (knob 1) and Middle (knob 3)
  {
    auto toggleArea = area.removeFromTop(36);
    const int ty = toggleArea.getY();
    mNoiseGateButton.setBounds(knobW + (knobW - 90) / 2, ty, 90, 36);
    mToneStackButton.setBounds(knobW * 3 + (knobW - 60) / 2, ty, 60, 36);
  }

  area.removeFromTop(8); // gap before file rows

  // Model row + Slim slider
  {
    constexpr int slimW = 84;
    const int modelY = area.getY();
    mModelRow.setBounds(0, modelY, contentW - slimW - 4, 38);
    mSlimSlider.setBounds(contentW - slimW, modelY + 8, slimW, 22);
    area.removeFromTop(38);
  }

  area.removeFromTop(4);

  // IR row
  mIRRow.setBounds(area.removeFromTop(38));
}

void GatewayAudioProcessorEditor::setupKnob(juce::Slider& slider,
                                              juce::Label&  label,
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
        mModelRow.setLoadedFile(result, "nam");
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
        mIRRow.setLoadedFile(result, "wav");
    });
}
