/*
 * NAMix Linux VST3 Plugin — parametric-knob layout/test additions
 * Copyright (C) 2026 Gene Ko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#pragma once

#include <algorithm>

// Pure logic (no JUCE GUI/audio dependency) shared by PluginProcessor.cpp
// and PluginEditor.h, and exercised directly by tests/ -- see
// ParametricRangeTests.cpp and ParametricRowLayoutTests.cpp.

// A model-supplied DSPParamDef range is untrusted input (the model file
// could be malformed/hand-edited). Only min_val < max_val is a valid range;
// anything else (equal, or inverted) would make
// NormalisableRange::convertTo0to1() divide by zero and propagate NaN into
// the parameter and, from there, into the DSP via SetKnobValues().
inline bool isValidParametricRange(float minVal, float maxVal) {
  return maxVal > minVal;
}

// Packing result for one parametric-knob row: `rows` rows of up to `cols`
// knobs each, each knob `knobWidth` wide, with a `lh`-tall label above a
// `knobHeight`-tall control.
struct ParametricRowLayout {
  int cols = 0;
  int rows = 0;
  int knobWidth = 0;
  int knobHeight = 0;
  int labelHeight = 0;
  int gap = 0;
};

// Mirrors NeuralAmpModelerPlugin's packing: knob width shrinks from
// kKnobMaxW towards a kKnobMinW floor as more slots are active, wrapping to
// another row rather than going narrower than that. Then, whatever number
// of rows that produces, knob HEIGHT shrinks (not just width) as needed so
// that many rows always fit within `availableHeight` -- the caller's own
// component bounds are fixed, so nothing may ever be laid out past them
// (see the regression this fixed: a model with >6 params could wrap to a
// second row that got invisibly clipped when only knob width was ever
// adjusted).
inline ParametricRowLayout computeParametricRowLayout(int activeCount, int availableWidth,
                                                       int availableHeight) {
  constexpr int kKnobMaxW = 100, kKnobMinW = 70;
  constexpr int khMax = 110, khMin = 40, lh = 18, gap = 10;

  ParametricRowLayout out;
  out.labelHeight = lh;
  out.gap = gap;

  if (activeCount <= 0)
    return out;

  int cols = activeCount;
  int rows = 1;
  int kw = kKnobMaxW;
  while (true) {
    kw = (availableWidth - (cols - 1) * gap) / (cols > 0 ? cols : 1);
    if (kw >= kKnobMinW || cols <= 1)
      break;
    ++rows;
    cols = (activeCount + rows - 1) / rows;
  }
  kw = kw < kKnobMinW ? kKnobMinW : (kw > kKnobMaxW ? kKnobMaxW : kw);

  // Fitting within availableHeight is a hard constraint (violating it is
  // exactly the clipped/unclickable-second-row regression this exists to
  // prevent); khMin is only a soft preference for readability. At very high
  // active counts (13-16, near kMaxParametricParams) the two can conflict --
  // e.g. 16 knobs at kKnobMinW settles on 3 rows, which only leaves ~27px
  // per knob after the label, well under khMin's 40px "readable" floor. In
  // that case, still shrink below khMin rather than let the layout overflow
  // its bounds; a 1px floor only guards against a literally zero/negative
  // knob height, never against merely looking cramped.
  int kh = khMax;
  int rowH = kh + lh;
  const int neededH = rows * rowH + (rows - 1) * gap;
  if (neededH > availableHeight) {
    const int available = availableHeight - (rows - 1) * gap - rows * lh;
    const int fitted = available / (rows > 0 ? rows : 1);
    kh = fitted >= khMin ? khMin : std::max(1, fitted);
  }

  out.cols = cols;
  out.rows = rows;
  out.knobWidth = kw;
  out.knobHeight = kh;
  return out;
}
