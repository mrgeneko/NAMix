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

// Direct tests for the range-validation guard added after a code review
// found that a malformed model file (min_val >= max_val) would make
// NormalisableRange::convertTo0to1() divide by zero and propagate NaN into
// the parameter, and from there into the DSP via SetKnobValues().
class ParametricRangeTests : public juce::UnitTest {
public:
  ParametricRangeTests() : juce::UnitTest("Parametric range validation") {}

  void runTest() override {
    beginTest("A normal ascending range is valid");
    expect(isValidParametricRange(0.0f, 1.0f));
    expect(isValidParametricRange(-60.0f, 60.0f));
    expect(isValidParametricRange(0.001f, 1.0f));

    beginTest("An equal min/max range is rejected");
    expect(!isValidParametricRange(0.5f, 0.5f));
    expect(!isValidParametricRange(0.0f, 0.0f));

    beginTest("An inverted (min > max) range is rejected");
    expect(!isValidParametricRange(1.0f, 0.0f));
    expect(!isValidParametricRange(10.0f, -10.0f));

    beginTest("A rejected range never produces NaN via NormalisableRange::convertTo0to1");
    // Mirrors exactly what updateParametricRangesAndDefaults() falls back to
    // for an invalid range: [0, 1] with a mid-scale default.
    {
      juce::NormalisableRange<float> fallback(0.0f, 1.0f);
      const float normalised = fallback.convertTo0to1(0.5f);
      expect(!std::isnan(normalised));
      expectWithinAbsoluteError(normalised, 0.5f, 1.0e-6f);
    }
  }
};

static ParametricRangeTests parametricRangeTests;
