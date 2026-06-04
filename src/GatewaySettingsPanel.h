/*
 * Gateway Linux VST3 Plugin
 * Copyright (C) 2026 rations
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Full-size overlay panel shown when the gear icon is clicked.
// Provides output mode selection (Raw / Normalized / Calibrated) and about info.
class GatewaySettingsPanel : public juce::Component
{
public:
  std::function<void()> onClose;

  explicit GatewaySettingsPanel(juce::AudioProcessorValueTreeState& apvts)
    : mApvts(apvts)
  {
    mCloseButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc3\x97")));
    mCloseButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(mCloseButton);

    const char* labels[3] = { "Raw", "Normalized", "Calibrated" };
    for (int i = 0; i < 3; ++i)
    {
      mOutputButtons[i].setButtonText(labels[i]);
      mOutputButtons[i].setRadioGroupId(101, juce::dontSendNotification);
      const int idx = i; // capture by value
      mOutputButtons[i].onClick = [this, idx] { setOutputMode(idx); };
      addAndMakeVisible(mOutputButtons[i]);
    }

    refreshFromState();
  }

  // Sync radio buttons from the current APVTS parameter value.
  void refreshFromState()
  {
    const int mode = static_cast<int>(
      mApvts.getRawParameterValue("outputMode")->load());
    for (int i = 0; i < 3; ++i)
      mOutputButtons[i].setToggleState(i == mode, juce::dontSendNotification);
  }

  void visibilityChanged() override
  {
    if (isVisible()) refreshFromState();
  }

  void paint(juce::Graphics& g) override
  {
    // Opaque background so main UI is fully hidden
    g.fillAll(juce::Colour(0xff1a1a2e));

    // Title bar
    g.setColour(juce::Colour(0xff1f1f38));
    g.fillRect(0, 0, getWidth(), 40);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("SETTINGS", 0, 0, getWidth(), 40, juce::Justification::centred);

    // Output mode section header
    g.setFont(juce::Font(10.5f));
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawText("OUTPUT MODE", 40, 54, 200, 13, juce::Justification::centredLeft);

    // Divider below output mode
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(122, 30.0f, (float)(getWidth() - 30));

    // About header
    g.setFont(juce::Font(10.5f));
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawText("ABOUT", 40, 134, 200, 13, juce::Justification::centredLeft);

    // About body
    g.setFont(juce::Font(10.5f));
    g.setColour(juce::Colours::white.withAlpha(0.45f));
    g.drawText("Gateway v0.1  \xe2\x80\x94  GPL v3",
               40, 154, getWidth() - 80, 14, juce::Justification::centredLeft);
    g.drawText("Based on NeuralAmpModelerPlugin by Steven Atkinson  (MIT)",
               40, 170, getWidth() - 80, 14, juce::Justification::centredLeft);
  }

  void resized() override
  {
    // Close button top-right
    mCloseButton.setBounds(getWidth() - 32, 8, 24, 24);

    // Output mode radio buttons in a row
    const int btnY = 70;
    const int btnH = 36;
    mOutputButtons[0].setBounds(40,  btnY, 80,  btnH);
    mOutputButtons[1].setBounds(150, btnY, 120, btnH);
    mOutputButtons[2].setBounds(300, btnY, 120, btnH);
  }

private:
  void setOutputMode(int index)
  {
    // AudioParameterChoice normalized value: index / (numChoices - 1)
    if (auto* param = mApvts.getParameter("outputMode"))
      param->setValueNotifyingHost((float)index / 2.0f);
  }

  juce::AudioProcessorValueTreeState& mApvts;
  juce::TextButton   mCloseButton;
  juce::ToggleButton mOutputButtons[3];
};
