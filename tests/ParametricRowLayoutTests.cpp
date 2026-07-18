/*
 * NAMix Linux VST3 Plugin — parametric-knob layout/test additions
 * Copyright (C) 2026 Gene Ko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#include <juce_core/juce_core.h>

#include "ParametricKnobLayout.h"

// The Layout::PARAMETRIC_ROW_H / KNOB_AW values from PluginEditor.cpp, at
// the point ParametricRow::resized() is actually called with in practice.
namespace {
constexpr int kAvailableWidth = 500;
constexpr int kAvailableHeight = 154;
} // namespace

// Direct tests for the packing algorithm, including the regression a code
// review found: a model with more than 6 active knobs could wrap to a
// second row, but only knob *width* shrank to fit -- the reserved height
// only ever fit one row, so the second row's knobs were invisibly clipped
// and unclickable.
class ParametricRowLayoutTests : public juce::UnitTest {
public:
  ParametricRowLayoutTests() : juce::UnitTest("Parametric row layout packing") {}

  void runTest() override {
    beginTest("Zero active knobs produces an empty (no-op) layout");
    {
      const auto layout = computeParametricRowLayout(0, kAvailableWidth, kAvailableHeight);
      expectEquals(layout.rows, 0);
      expectEquals(layout.cols, 0);
    }

    beginTest("A small count (e.g. 2, matching real example models) fits in one row "
              "at full knob size, no shrinking needed");
    {
      const auto layout = computeParametricRowLayout(2, kAvailableWidth, kAvailableHeight);
      expectEquals(layout.rows, 1);
      expectEquals(layout.cols, 2);
      expectEquals(layout.knobWidth, 100); // kKnobMaxW
      expectEquals(layout.knobHeight, 110); // khMax
    }

    beginTest("Every knob position for every active count from 1 to 16 fits fully "
              "within the available bounds (the actual regression)");
    for (int activeCount = 1; activeCount <= 16; ++activeCount) {
      const auto layout =
          computeParametricRowLayout(activeCount, kAvailableWidth, kAvailableHeight);

      expect(layout.rows > 0, "rows must be positive for count " + juce::String(activeCount));
      expect(layout.cols > 0, "cols must be positive for count " + juce::String(activeCount));
      expect(layout.cols * layout.rows >= activeCount,
             "grid must have enough cells for count " + juce::String(activeCount));

      const int rowHeight = layout.knobHeight + layout.labelHeight;
      const int totalHeight = layout.rows * rowHeight + (layout.rows - 1) * layout.gap;
      const int totalWidth =
          layout.cols * layout.knobWidth + (layout.cols - 1) * layout.gap;

      expect(totalHeight <= kAvailableHeight,
             "count " + juce::String(activeCount) + " must fit within available height ("
                 + juce::String(totalHeight) + " vs " + juce::String(kAvailableHeight) + ")");
      expect(totalWidth <= kAvailableWidth,
             "count " + juce::String(activeCount) + " must fit within available width ("
                 + juce::String(totalWidth) + " vs " + juce::String(kAvailableWidth) + ")");
      expect(layout.knobWidth > 0 && layout.knobHeight > 0,
             "knob dimensions must stay positive for count " + juce::String(activeCount));
    }

    beginTest("More than 6 active knobs wraps to multiple rows (the case that used "
              "to clip)");
    {
      const auto layout = computeParametricRowLayout(8, kAvailableWidth, kAvailableHeight);
      expect(layout.rows >= 2, "8 active knobs should need more than one row");
    }
  }
};

static ParametricRowLayoutTests parametricRowLayoutTests;
