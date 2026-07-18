/*
 * NAMix Linux VST3 Plugin — parametric-knob layout/test additions
 * Copyright (C) 2026 Gene Ko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#include <juce_core/juce_core.h>

#include <cstdio>

int main() {
  juce::UnitTestRunner runner;
  runner.setAssertOnFailure(false);
  runner.runAllTests();

  int numFailures = 0;
  for (int i = 0; i < runner.getNumResults(); ++i) {
    const auto *result = runner.getResult(i);
    numFailures += result->failures;
  }

  std::printf("\n%d failure(s) across all tests.\n", numFailures);
  return numFailures == 0 ? 0 : 1;
}
