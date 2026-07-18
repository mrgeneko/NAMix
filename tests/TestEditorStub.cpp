/*
 * NAMix Linux VST3 Plugin — parametric-knob layout/test additions
 * Copyright (C) 2026 Gene Ko
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */

// Real createEditor() lives in PluginEditor.cpp, which this test target
// deliberately never compiles (that's the whole point of moving it out of
// PluginProcessor.cpp -- see the comment there). But NAMixAudioProcessor's
// vtable still needs a concrete symbol for every virtual function, even
// one the tests never call. This satisfies the linker without pulling in
// BinaryData/fonts/juce_gui_basics.
#include "PluginProcessor.h"

juce::AudioProcessorEditor *NAMixAudioProcessor::createEditor() { return nullptr; }
