cmake_minimum_required(VERSION 3.22)

project(gateway-linux VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
add_subdirectory(JUCE)

# ---------------------------------------------------------------------------
# NeuralAmpModelerCore sources (compiled directly; avoids its top-level
# CMakeLists assumptions about being the root project)
# ---------------------------------------------------------------------------
set(NAM_SOURCES
    NeuralAmpModelerCore/NAM/activations.cpp
    NeuralAmpModelerCore/NAM/container.cpp
    NeuralAmpModelerCore/NAM/conv1d.cpp
    NeuralAmpModelerCore/NAM/convnet.cpp
    NeuralAmpModelerCore/NAM/dsp.cpp
    NeuralAmpModelerCore/NAM/get_dsp.cpp
    NeuralAmpModelerCore/NAM/lstm.cpp
    NeuralAmpModelerCore/NAM/ring_buffer.cpp
    NeuralAmpModelerCore/NAM/util.cpp
    NeuralAmpModelerCore/NAM/wavenet/model.cpp
    NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp
    NeuralAmpModelerCore/NAM/wavenet/slimmable.cpp
)

# ---------------------------------------------------------------------------
# AudioDSPTools sources
# ---------------------------------------------------------------------------
set(DSP_SOURCES
    AudioDSPTools/dsp/dsp.cpp
    AudioDSPTools/dsp/ImpulseResponse.cpp
    AudioDSPTools/dsp/NoiseGate.cpp
    AudioDSPTools/dsp/RecursiveLinearFilter.cpp
    AudioDSPTools/dsp/wav.cpp
)

# ---------------------------------------------------------------------------
# Plugin definition
# ---------------------------------------------------------------------------
juce_add_plugin(GatewayLinux
    COMPANY_NAME               "Gateway"
    PLUGIN_MANUFACTURER_CODE   GwLx
    PLUGIN_CODE                Gwlx
    FORMATS                    VST3
    PRODUCT_NAME               "Gateway"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            "Fx"
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
target_sources(GatewayLinux PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ToneStack.cpp
    ${NAM_SOURCES}
    ${DSP_SOURCES}
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(GatewayLinux
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        NAM_ENABLE_A2_FAST
)

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(GatewayLinux PRIVATE
    src
    NeuralAmpModelerCore
    NeuralAmpModelerCore/NAM
    NeuralAmpModelerCore/Dependencies/nlohmann
    AudioDSPTools/dsp
    eigen
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(GatewayLinux
    PRIVATE
        juce::juce_audio_utils
        juce::juce_opengl
        stdc++fs
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
target_compile_options(GatewayLinux PRIVATE
    -ffast-math
    -Wall
    -Wno-unused-parameter
)
