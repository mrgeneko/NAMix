/*
 * Gateway Linux VST3 Plugin
 * Copyright (C) 2026 rations
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// A styled file-loading row: folder icon | < | > | filename label | ×
// The folder icon is drawn by paint() under a transparent button so click events
// are captured without the button chrome overpainting the icon.
class FileRow : public juce::Component
{
public:
  std::function<void()> onLoad;
  std::function<void()> onClear;

  explicit FileRow(const juce::String& placeholder)
    : mPlaceholder(placeholder)
  {
    mLoadButton.setButtonText("");
    mLoadButton.onClick = [this] { if (onLoad) onLoad(); };
    addAndMakeVisible(mLoadButton);

    addAndMakeVisible(mPrevButton);
    addAndMakeVisible(mNextButton);

    // × (U+00D7) as UTF-8
    mClearButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc3\x97")));
    mClearButton.onClick = [this] { if (onClear) onClear(); };
    addAndMakeVisible(mClearButton);

    mFilenameLabel.setText(placeholder, juce::dontSendNotification);
    mFilenameLabel.setJustificationType(juce::Justification::centredLeft);
    mFilenameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff666688));
    mFilenameLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(mFilenameLabel);
  }

  void setFilename(const juce::String& name)
  {
    const bool empty = name.isEmpty();
    mFilenameLabel.setText(empty ? mPlaceholder : name, juce::dontSendNotification);
    mFilenameLabel.setColour(juce::Label::textColourId,
      empty ? juce::Colour(0xff666688) : juce::Colours::white);
  }

  void paint(juce::Graphics& g) override
  {
    g.fillAll(juce::Colour(0xff141428));

    // Folder icon centred in the load button area
    const auto lb = mLoadButton.getBounds().toFloat();
    const float cx = lb.getCentreX();
    const float cy = lb.getCentreY();
    constexpr float iw = 14.0f, ih = 11.0f;

    g.setColour(juce::Colour(0xffaaaacc));
    juce::Path folder;
    // Body
    folder.addRectangle(cx - iw * 0.5f, cy - ih * 0.5f + 2.5f, iw, ih - 2.5f);
    // Tab
    folder.addRectangle(cx - iw * 0.5f, cy - ih * 0.5f, iw * 0.44f, 3.5f);
    g.fillPath(folder);
  }

  void resized() override
  {
    auto r = getLocalBounds().reduced(4, 3);
    mLoadButton.setBounds(r.removeFromLeft(26));
    r.removeFromLeft(3);
    mPrevButton.setBounds(r.removeFromLeft(20));
    mNextButton.setBounds(r.removeFromLeft(20));
    r.removeFromLeft(5);
    mClearButton.setBounds(r.removeFromRight(22));
    r.removeFromRight(4);
    mFilenameLabel.setBounds(r);
  }

private:
  juce::String mPlaceholder;
  juce::TextButton mLoadButton, mPrevButton{ "<" }, mNextButton{ ">" }, mClearButton;
  juce::Label mFilenameLabel;
};
