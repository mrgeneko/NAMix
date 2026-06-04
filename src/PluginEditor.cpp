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
  setSize(600, 400);

  // Knob setup
  setupKnob(mInputGainSlider, mInputGainLabel);
  setupKnob(mOutputGainSlider, mOutputGainLabel);
  setupKnob(mBassSlider, mBassLabel);
  setupKnob(mMiddleSlider, mMiddleLabel);
  setupKnob(mTrebleSlider, mTrebleLabel);
  setupKnob(mNoiseGateSlider, mNoiseGateLabel);

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

  // Model section
  addAndMakeVisible(mLoadModelButton);
  addAndMakeVisible(mClearModelButton);
  addAndMakeVisible(mModelLabel);
  mModelLabel.setText("No model loaded", juce::dontSendNotification);
  mModelLabel.setJustificationType(juce::Justification::centredLeft);
  mLoadModelButton.onClick = [this] { chooseModelFile(); };
  mClearModelButton.onClick = [this] {
    mProcessor.clearModel();
    mModelLabel.setText("No model loaded", juce::dontSendNotification);
  };

  // IR section
  addAndMakeVisible(mLoadIRButton);
  addAndMakeVisible(mClearIRButton);
  addAndMakeVisible(mIRLabel);
  mIRLabel.setText("No IR loaded", juce::dontSendNotification);
  mIRLabel.setJustificationType(juce::Justification::centredLeft);
  mLoadIRButton.onClick = [this] { chooseIRFile(); };
  mClearIRButton.onClick = [this] {
    mProcessor.clearIR();
    mIRLabel.setText("No IR loaded", juce::dontSendNotification);
  };

  addAndMakeVisible(mToneStackButton);
  addAndMakeVisible(mNoiseGateButton);
}

GatewayAudioProcessorEditor::~GatewayAudioProcessorEditor() {}

void GatewayAudioProcessorEditor::paint(juce::Graphics& g)
{
  g.fillAll(juce::Colour(0xff1a1a2e));

  g.setColour(juce::Colours::white);
  g.setFont(18.0f);
  g.drawText("Gateway", getLocalBounds().removeFromTop(30),
             juce::Justification::centred);
}

void GatewayAudioProcessorEditor::resized()
{
  auto area = getLocalBounds().reduced(10);
  area.removeFromTop(35); // title

  // Model row
  auto modelRow = area.removeFromTop(30);
  mLoadModelButton.setBounds(modelRow.removeFromLeft(100));
  modelRow.removeFromLeft(4);
  mClearModelButton.setBounds(modelRow.removeFromLeft(60));
  modelRow.removeFromLeft(4);
  mModelLabel.setBounds(modelRow);

  area.removeFromTop(6);

  // IR row
  auto irRow = area.removeFromTop(30);
  mLoadIRButton.setBounds(irRow.removeFromLeft(100));
  irRow.removeFromLeft(4);
  mClearIRButton.setBounds(irRow.removeFromLeft(60));
  irRow.removeFromLeft(4);
  mIRLabel.setBounds(irRow);

  area.removeFromTop(10);

  // Knobs row
  const int knobW = 80;
  const int knobH = 90;
  auto knobRow = area.removeFromTop(knobH);

  auto placeKnob = [&](juce::Slider& s, juce::Label& l) {
    auto cell = knobRow.removeFromLeft(knobW);
    knobRow.removeFromLeft(6);
    l.setBounds(cell.removeFromBottom(18));
    s.setBounds(cell);
  };

  placeKnob(mInputGainSlider, mInputGainLabel);
  placeKnob(mNoiseGateSlider, mNoiseGateLabel);
  placeKnob(mBassSlider, mBassLabel);
  placeKnob(mMiddleSlider, mMiddleLabel);
  placeKnob(mTrebleSlider, mTrebleLabel);
  placeKnob(mOutputGainSlider, mOutputGainLabel);

  area.removeFromTop(8);

  // Toggle buttons
  auto toggleRow = area.removeFromTop(24);
  mToneStackButton.setBounds(toggleRow.removeFromLeft(80));
  toggleRow.removeFromLeft(10);
  mNoiseGateButton.setBounds(toggleRow.removeFromLeft(80));
}

void GatewayAudioProcessorEditor::setupKnob(juce::Slider& slider,
                                              juce::Label& label)
{
  slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
  addAndMakeVisible(slider);
  label.setJustificationType(juce::Justification::centred);
  addAndMakeVisible(label);
}

void GatewayAudioProcessorEditor::chooseModelFile()
{
  mFileChooser = std::make_unique<juce::FileChooser>(
    "Load NAM Model", juce::File::getSpecialLocation(juce::File::userHomeDirectory),
    "*.nam");

  mFileChooser->launchAsync(
    juce::FileBrowserComponent::openMode
      | juce::FileBrowserComponent::canSelectFiles,
    [this](const juce::FileChooser& fc) {
      auto result = fc.getResult();
      if (result.existsAsFile())
      {
        mProcessor.loadModel(result);
        mModelLabel.setText(result.getFileName(), juce::dontSendNotification);
      }
    });
}

void GatewayAudioProcessorEditor::chooseIRFile()
{
  mFileChooser = std::make_unique<juce::FileChooser>(
    "Load Impulse Response",
    juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.wav");

  mFileChooser->launchAsync(
    juce::FileBrowserComponent::openMode
      | juce::FileBrowserComponent::canSelectFiles,
    [this](const juce::FileChooser& fc) {
      auto result = fc.getResult();
      if (result.existsAsFile())
      {
        mProcessor.loadIR(result);
        mIRLabel.setText(result.getFileName(), juce::dontSendNotification);
      }
    });
}
