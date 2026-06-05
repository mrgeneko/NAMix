cmake_minimum_required(VERSION 3.22)

project(gateway-linux VERSION 0.2.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
add_subdirectory(JUCE)

# ---------------------------------------------------------------------------
# NeuralAmpModelerCore + AudioDSPTools — compiled as a static library and
# linked with --whole-archive.
#
# Each architecture (WaveNet, LSTM, ConvNet) self-registers via a file-scope
# static ConfigParserHelper.  On Linux the VST3 is a shared library, and the
# linker drops object files whose only content is an unreferenced static
# initialiser.  --whole-archive forces every .o in the archive to be linked,
# ensuring all registration side-effects execute at plugin load time.
# ---------------------------------------------------------------------------
add_library(NamDsp STATIC
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
    AudioDSPTools/dsp/dsp.cpp
    AudioDSPTools/dsp/ImpulseResponse.cpp
    AudioDSPTools/dsp/NoiseGate.cpp
    AudioDSPTools/dsp/RecursiveLinearFilter.cpp
    AudioDSPTools/dsp/wav.cpp
)

target_compile_definitions(NamDsp PUBLIC NAM_ENABLE_A2_FAST)

target_include_directories(NamDsp PUBLIC
    NeuralAmpModelerCore
    NeuralAmpModelerCore/NAM
    NeuralAmpModelerCore/Dependencies/nlohmann
    AudioDSPTools/dsp
    eigen
)

target_compile_options(NamDsp PRIVATE -ffast-math -Wno-unused-parameter -fPIC)

# ---------------------------------------------------------------------------
# Embedded font resources (Michroma-Regular and Roboto-Regular, copied from
# the original NeuralAmpModelerPlugin resources under MIT licence).
# ---------------------------------------------------------------------------
juce_add_binary_data(GatewayBinaryData SOURCES
    resources/fonts/Michroma-Regular.ttf
    resources/fonts/Roboto-Regular.ttf
    resources/img/SlimmableIcon.svg
    resources/img/Background.jpg
    resources/img/KnobBackground.png
    resources/img/MeterBackground.png
    resources/img/FileBackground.png
    resources/img/InputLevelBackground.png
    resources/img/Lines.png
)

# ---------------------------------------------------------------------------
# Plugin definition
# ---------------------------------------------------------------------------
juce_add_plugin(GatewayLinux
    COMPANY_NAME               "Steven Atkinson"
    PLUGIN_MANUFACTURER_CODE   GwLx
    PLUGIN_CODE                Gwlx
    FORMATS                    VST3 Standalone
    NEEDS_STANDALONE           TRUE
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

# NamDsp's PUBLIC include paths flow into GatewayLinux automatically via the
# target_link_libraries below, so no need to repeat them here.

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(GatewayLinux PRIVATE
    src
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(GatewayLinux
    PRIVATE
        # --whole-archive ensures every .o in NamDsp is linked into the VST3
        # .so, so that static-initialiser registrations (WaveNet, LSTM, etc.)
        # always execute at plugin load time regardless of LTO / --as-needed.
        -Wl,--whole-archive NamDsp -Wl,--no-whole-archive
        GatewayBinaryData
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
