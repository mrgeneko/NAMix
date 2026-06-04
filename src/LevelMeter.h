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
#include <atomic>
#include <algorithm>
#include <cmath>

// Vertical peak-hold level meter. The audio thread writes via setLevel();
// a 30 Hz timer on the message thread applies decay and repaints.
class LevelMeter : public juce::Component, public juce::Timer
{
public:
  LevelMeter()  { startTimerHz(30); }
  ~LevelMeter() override { stopTimer(); }

  // Called from the editor's timer callback with the processor's output peak.
  void setLevel(float linear)
  {
    const float v = std::max(0.0f, linear);
    float current = mIncoming.load(std::memory_order_relaxed);
    while (v > current
           && !mIncoming.compare_exchange_weak(current, v,
                                               std::memory_order_relaxed))
    {}
  }

  void timerCallback() override
  {
    // Pull incoming peak and reset the slot.
    const float next = mIncoming.exchange(0.0f, std::memory_order_relaxed);

    // Smooth level: take max of incoming vs decayed stored level.
    // Decay ~15 dB/s at 30 Hz: multiply by 10^(-15/(20*30)) ≈ 0.944 per tick.
    mLevel = std::max(next, mLevel * 0.944f);
    if (mLevel < 1e-5f) mLevel = 0.0f;

    // Peak hold: hold for 1 s (30 ticks) then decay at the same rate.
    if (mLevel >= mPeak)
    {
      mPeak     = mLevel;
      mPeakHold = 30;
    }
    else if (mPeakHold > 0)
    {
      --mPeakHold;
    }
    else
    {
      mPeak *= 0.944f;
      if (mPeak < 1e-5f) mPeak = 0.0f;
    }

    repaint();
  }

  void paint(juce::Graphics& g) override
  {
    const auto b = getLocalBounds().toFloat();

    // Track background — dark, close to NAM_1
    g.setColour(juce::Colour(0xff131116));
    g.fillRect(b);

    // Level fill from bottom — Azure (NAM_THEMECOLOR)
    if (mLevel > 0.0f)
    {
      const float h = b.getHeight() * std::min(mLevel, 1.0f);
      g.setColour(juce::Colour(0xff5085e8));
      g.fillRect(b.getX(), b.getBottom() - h, b.getWidth(), h);
    }

    // Peak hold tick — brighter Azure (kX3 = THEMECOLOR.WithContrast(0.1))
    if (mPeak > 0.001f)
    {
      const float py = b.getBottom() - b.getHeight() * std::min(mPeak, 1.0f);
      g.setColour(juce::Colour(0xff6a9ff0));
      g.fillRect(b.getX(), py - 1.0f, b.getWidth(), 2.0f);

      // Grid lines (NAM draws grid on meter)
      g.setColour(juce::Colours::black.withAlpha(0.3f));
      const float gridStep = b.getHeight() / 7.0f;
      for (int i = 1; i < 7; ++i)
        g.fillRect(b.getX(), b.getY() + i * gridStep - 0.5f, b.getWidth(), 1.0f);
    }
  }

private:
  std::atomic<float> mIncoming { 0.0f };
  float mLevel    = 0.0f;
  float mPeak     = 0.0f;
  int   mPeakHold = 0;
};
