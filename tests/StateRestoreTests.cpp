/*
 * NAMix Linux VST3 Plugin — parametric-knob layout/test additions
 * Copyright (C) 2026 Gene Ko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

#include <juce_core/juce_core.h>

#include "PluginProcessor.h"

// Covers the bug a code review found: setStateInformation() used to call
// apvts.replaceState() *before* loadModel(), so the parametric-knob
// defaults that loadModel() resets every active slot to would immediately
// stomp the just-restored saved values. The fix reordered those two calls;
// this test loads a real parametric model (ParametricModels/
// muffinator_v4_optimal.param.nam), dials a knob away from its default,
// round-trips through get/setStateInformation on a fresh processor
// instance, and checks the dialed value -- not the model's default --
// survives.
class StateRestoreTests : public juce::UnitTest {
public:
  StateRestoreTests() : juce::UnitTest("Parametric knob state restore") {}

  void runTest() override {
    const juce::File modelFile(juce::String(NAMIX_TEST_MODELS_DIR)
                               + "/muffinator_v4_optimal.param.nam");

    beginTest("Test model file exists");
    expect(modelFile.existsAsFile(), "Expected " + modelFile.getFullPathName());
    if (!modelFile.existsAsFile())
      return;

    NAMixAudioProcessor processorA;
    processorA.prepareToPlay(48000.0, 512);

    beginTest("Loading the parametric model reports its real knob count");
    expect(processorA.loadModel(modelFile));
    expectEquals(processorA.getModelNumParams(), 2); // Sustain, Tone

    const auto paramId = NAMixAudioProcessor::getParametricParamId(0);
    auto *param = processorA.apvts.getParameter(paramId);
    expect(param != nullptr);

    beginTest("Dialing a knob away from its model default");
    const float defaultNormalised = param->getDefaultValue();
    const float dialedNormalised = 0.9f;
    // Sanity: make sure we're actually choosing a distinctly different value,
    // not accidentally landing back on the default.
    expect(std::abs(dialedNormalised - defaultNormalised) > 0.1f);
    param->setValueNotifyingHost(dialedNormalised);
    expectWithinAbsoluteError(param->getValue(), dialedNormalised, 1.0e-6f);

    beginTest("Saved state survives into a freshly constructed processor");
    juce::MemoryBlock savedState;
    processorA.getStateInformation(savedState);

    NAMixAudioProcessor processorB;
    processorB.prepareToPlay(48000.0, 512);
    processorB.setStateInformation(savedState.getData(),
                                    static_cast<int>(savedState.getSize()));

    expectEquals(processorB.getModelNumParams(), 2);
    auto *restoredParam = processorB.apvts.getParameter(paramId);
    expect(restoredParam != nullptr);
    expectWithinAbsoluteError(restoredParam->getValue(), dialedNormalised, 1.0e-4f);

    // The bug this guards against: loadModel(), called internally by
    // setStateInformation() to set up this model's knob ranges, used to
    // unconditionally reset every active slot back to its default *after*
    // the saved value had already been restored -- so a passing test here
    // and a value sitting at defaultNormalised would both be plausible
    // outcomes of a broken implementation. Assert it's specifically NOT
    // sitting at the default, to make sure this test would actually catch
    // that regression rather than passing by coincidence.
    expect(std::abs(restoredParam->getValue() - defaultNormalised) > 0.1f,
           "Restored value should not have fallen back to the model default");
  }
};

static StateRestoreTests stateRestoreTests;
