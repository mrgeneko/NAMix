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
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

// A styled file-loading row with directory navigation.
//
// Layout (left→right):
//   [folder icon btn] [<] [>] [  filename label  ] [×]
//
// The folder icon is drawn in paint() under a transparent TextButton so the
// button handles clicks without overpainting the icon.
// Clicking the filename label shows a popup menu of all compatible files in
// the same directory as the currently loaded file.
// < / > cycle through those files in alphabetical order.
class FileRow : public juce::Component
{
public:
  // folder icon → tell editor to show file chooser dialog
  std::function<void()> onOpenChooser;
  // navigation or popup selected a specific file → editor should load it
  std::function<void(juce::File)> onLoad;
  // clear button pressed
  std::function<void()> onClear;

  explicit FileRow(const juce::String& placeholder)
    : mPlaceholder(placeholder)
  {
    mLoadButton.setButtonText("");
    mLoadButton.onClick = [this] { if (onOpenChooser) onOpenChooser(); };
    addAndMakeVisible(mLoadButton);

    mPrevButton.onClick = [this] { navigate(-1); };
    addAndMakeVisible(mPrevButton);

    mNextButton.onClick = [this] { navigate(+1); };
    addAndMakeVisible(mNextButton);

    // × U+00D7
    mClearButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc3\x97")));
    mClearButton.onClick = [this] { if (onClear) onClear(); };
    addAndMakeVisible(mClearButton);

    mFilenameLabel.setText(placeholder, juce::dontSendNotification);
    mFilenameLabel.setJustificationType(juce::Justification::centred);
    mFilenameLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xffa2b2bf).withAlpha(0.7f)); // NAM_3 dimmed
    mFilenameLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(mFilenameLabel);
  }

  // Called by the editor after a successful file load.
  // Scans the parent directory for all files with the same extension and
  // sets up the prev/next navigation state.
  void setLoadedFile(const juce::File& f, const juce::String& ext)
  {
    mCurrentFile = f;
    mExtension   = ext;
    scanDirectory();
    mCurrentIndex = mFiles.indexOf(f);
    if (mCurrentIndex < 0 && !mFiles.isEmpty())
      mCurrentIndex = 0;
    mFilenameLabel.setText(f.getFileName(), juce::dontSendNotification);
    mFilenameLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xfff2f2f2)); // NAM_THEMEFONTCOLOR
  }

  // Resets all state — called by the editor after clearing the file.
  void clearFile()
  {
    mCurrentFile = juce::File{};
    mExtension   = {};
    mFiles.clear();
    mCurrentIndex = -1;
    mFilenameLabel.setText(mPlaceholder, juce::dontSendNotification);
    mFilenameLabel.setColour(juce::Label::textColourId,
                             juce::Colour(0xffa2b2bf).withAlpha(0.7f));
  }

  void paint(juce::Graphics& g) override
  {
    // File row background — slightly lighter than NAM_1 main background,
    // matching the fileBackgroundBitmap tint in the original.
    g.fillAll(juce::Colour(0xff1a1720));

    // Folder icon centred in the load button area
    const auto lb = mLoadButton.getBounds().toFloat();
    const float cx = lb.getCentreX();
    const float cy = lb.getCentreY();
    constexpr float iw = 14.0f, ih = 11.0f;
    // Cadet Blue — matches NAM's dimmed icon colour
    g.setColour(juce::Colour(0xffa2b2bf).withAlpha(0.75f));
    juce::Path folder;
    folder.addRectangle(cx - iw * 0.5f, cy - ih * 0.5f + 2.5f, iw, ih - 2.5f);
    folder.addRectangle(cx - iw * 0.5f, cy - ih * 0.5f,         iw * 0.44f, 3.5f);
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

  // Mouse click on the label area opens the popup menu.
  void mouseDown(const juce::MouseEvent& e) override
  {
    if (mFilenameLabel.getBounds().contains(e.getPosition()))
      showPopupMenu();
  }

private:
  void scanDirectory()
  {
    mFiles.clear();
    if (!mCurrentFile.existsAsFile() || mExtension.isEmpty())
      return;

    mCurrentFile.getParentDirectory().findChildFiles(
      mFiles, juce::File::findFiles, false, "*." + mExtension);

    std::sort(mFiles.begin(), mFiles.end(),
              [](const juce::File& a, const juce::File& b) {
                return a.getFileName().compareIgnoreCase(b.getFileName()) < 0;
              });
  }

  void navigate(int delta)
  {
    if (mFiles.isEmpty()) return;
    const int n = mFiles.size();
    mCurrentIndex = ((mCurrentIndex + delta) % n + n) % n;
    if (onLoad) onLoad(mFiles[mCurrentIndex]);
  }

  void showPopupMenu()
  {
    if (mFiles.isEmpty()) return;

    juce::PopupMenu menu;
    for (int i = 0; i < mFiles.size(); ++i)
      menu.addItem(i + 1, mFiles[i].getFileName(), true, i == mCurrentIndex);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
      [this](int result) {
        if (result > 0 && result - 1 < mFiles.size())
        {
          mCurrentIndex = result - 1;
          if (onLoad) onLoad(mFiles[mCurrentIndex]);
        }
      });
  }

  juce::String           mPlaceholder;
  juce::String           mExtension;
  juce::File             mCurrentFile;
  juce::Array<juce::File> mFiles;
  int                    mCurrentIndex = -1;

  juce::TextButton mLoadButton, mPrevButton{ "<" }, mNextButton{ ">" }, mClearButton;
  juce::Label      mFilenameLabel;
};
