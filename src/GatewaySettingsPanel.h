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
// Colours and layout match the original NAMSettingsPageControl.
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
      const int idx = i;
      mOutputButtons[i].onClick = [this, idx] { setOutputMode(idx); };
      addAndMakeVisible(mOutputButtons[i]);
    }

    refreshFromState();
  }

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
    // Opaque NAM_1 background
    g.fillAll(juce::Colour(0xff1d1a1f));

    // Title bar
    g.setColour(juce::Colour(0xff252230));
    g.fillRect(0, 0, getWidth(), 50);

    // Title — Michroma-Regular 22px (original uses 30px; scaled for our panel)
    g.setColour(juce::Colour(0xfff2f2f2));
    g.setFont(juce::Font("Michroma-Regular", 22.0f, juce::Font::plain));
    g.drawText("SETTINGS", 0, 0, getWidth(), 50, juce::Justification::centred);

    // Section headers — Roboto, Cadet Blue
    g.setFont(juce::Font("Roboto-Regular", 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffa2b2bf).withAlpha(0.7f));
    g.drawText("OUTPUT MODE", 40, 65, 200, 13, juce::Justification::centredLeft);

    // Divider
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.drawHorizontalLine(132, 30.0f, (float)(getWidth() - 30));

    g.setFont(juce::Font("Roboto-Regular", 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xffa2b2bf).withAlpha(0.7f));
    g.drawText("ABOUT", 40, 148, 200, 13, juce::Justification::centredLeft);

    g.setFont(juce::Font("Roboto-Regular", 11.0f, juce::Font::plain));
    g.setColour(juce::Colour(0xfff2f2f2).withAlpha(0.5f));
    g.drawText("Gateway v0.1  \xe2\x80\x94  GPL v3",
               40, 168, getWidth() - 80, 14, juce::Justification::centredLeft);
    g.drawText("Based on NeuralAmpModelerPlugin by Steven Atkinson (MIT)",
               40, 186, getWidth() - 80, 14, juce::Justification::centredLeft);
  }

  void resized() override
  {
    mCloseButton.setBounds(getWidth() - 36, 13, 24, 24);

    // Three radio buttons in a row beneath "OUTPUT MODE"
    const int btnY = 80;
    const int btnH = 40;
    mOutputButtons[0].setBounds(40,  btnY,  80, btnH);
    mOutputButtons[1].setBounds(150, btnY, 130, btnH);
    mOutputButtons[2].setBounds(310, btnY, 130, btnH);
  }

private:
  void setOutputMode(int index)
  {
    if (auto* param = mApvts.getParameter("outputMode"))
      param->setValueNotifyingHost((float)index / 2.0f);
  }

  juce::AudioProcessorValueTreeState& mApvts;
  juce::TextButton   mCloseButton;
  juce::ToggleButton mOutputButtons[3];
};
