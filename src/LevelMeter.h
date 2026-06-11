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
#include <algorithm>
#include <atomic>
#include <cmath>
#include <juce_gui_basics/juce_gui_basics.h>

// Vertical level meter. The audio thread writes via setLevel(); a 30 Hz timer
// on the message thread applies decay and repaints.
class LevelMeter : public juce::Component, public juce::Timer {
public:
  LevelMeter() {
    mBg = juce::ImageCache::getFromMemory(BinaryData::MeterBackground_png,
                                          BinaryData::MeterBackground_pngSize);
    startTimerHz(30);
  }
  ~LevelMeter() override { stopTimer(); }

  // Called from the editor's timer callback with the processor's output peak.
  void setLevel(float linear) {
    const float v = std::max(0.0f, linear);
    float current = mIncoming.load(std::memory_order_relaxed);
    while (v > current &&
           !mIncoming.compare_exchange_weak(current, v, std::memory_order_relaxed)) {
    }
  }

  void timerCallback() override {
    // Pull incoming peak and reset the slot.
    const float next = mIncoming.exchange(0.0f, std::memory_order_relaxed);

    // Smooth level: take max of incoming vs decayed stored level.
    // Decay ~15 dB/s at 30 Hz: multiply by 10^(-15/(20*30)) ≈ 0.944 per tick.
    // This falloff replicates the original meter's averaged response; the
    // original draws no separate held-peak indicator.
    mLevel = std::max(next, mLevel * 0.944f);
    if (mLevel < 1e-5f)
      mLevel = 0.0f;

    repaint();
  }

  void paint(juce::Graphics &g) override {
    const auto b = getLocalBounds().toFloat();

    // Exact replica of the original NAMMeterControl rendering (iPlug2).
    //
    // Draw order: DrawBackground(g, mRECT) -> DrawTrackHandle (fill) ->
    // DrawPeak (grid + tick). The style is WithShowValue(false),
    // WithDrawFrame(false), WithWidgetFrac(0.8), so there is NO frame.

    // 1. Bitmap at full control bounds.
    if (mBg.isValid())
      g.drawImage(mBg, (int)b.getX(), (int)b.getY(), (int)b.getWidth(),
                  (int)b.getHeight(), 0, 0, mBg.getWidth(), mBg.getHeight(), false);
    else {
      g.setColour(juce::Colour(0xff131116));
      g.fillRect(b);
    }

    // 2. Fill track. Replicates NAMMeterControl::OnResize():
    //      widget = mRECT.GetScaledAboutCentre(0.8)
    //      widget = widget.GetMidHPadded(5).GetVPadded(10)
    //    => 10 px wide, horizontally centred; top/bottom margin = 0.1*H - 10.
    const float topMargin = 0.1f * b.getHeight() - 10.0f;
    const juce::Rectangle<float> track(b.getCentreX() - 5.0f, b.getY() + topMargin, 10.0f,
                                       b.getHeight() - 2.0f * topMargin);

    const float level = std::min(std::max(mLevel, 0.0f), 1.0f);

    // 3. Average level fill from the bottom (DrawTrackHandle, kX1).
    juce::Rectangle<float> fill;
    if (level > 0.0f) {
      const float h = track.getHeight() * level;
      fill = {track.getX(), track.getBottom() - h, track.getWidth(), h};
      g.setColour(juce::Colour(0xff5085e8));
      g.fillRect(fill);
    }

    // 4. Black grid over the track (DrawGrid(COLOR_BLACK, track, 10, 2)):
    //    10 px horizontal spacing > 10 px width => no vertical lines;
    //    2 px vertical spacing => a horizontal line every 2 px (segmented look).
    g.setColour(juce::Colours::black);
    for (float y = track.getY() + 2.0f; y < track.getBottom(); y += 2.0f)
      g.fillRect(track.getX(), y, track.getWidth(), 1.0f);

    // 5. Bright tick at the top edge of the fill (DrawPeak, kX3, mPeakSize=1).
    if (level > 0.0f) {
      g.setColour(juce::Colour(0xff6a9ff0));
      g.fillRect(fill.getX(), fill.getY(), fill.getWidth(), 1.0f);
    }
  }

private:
  juce::Image mBg;
  std::atomic<float> mIncoming{0.0f};
  float mLevel = 0.0f;
};
