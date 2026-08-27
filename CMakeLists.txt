cmake_minimum_required(VERSION 3.24)  # $<LINK_LIBRARY:WHOLE_ARCHIVE,...> needs 3.24+

project(namix VERSION 0.6.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
add_subdirectory(JUCE)

# ---------------------------------------------------------------------------
# NeuralAmpModelerCore + AudioDSPTools — compiled as a static library and
# WHOLE_ARCHIVE-linked (see target_link_libraries below).
#
# Each architecture (WaveNet, LSTM, ConvNet) self-registers via a file-scope
# static ConfigParserHelper. Every plugin format here (VST3/AU shared library,
# Standalone executable) is built by a linker that, by default, drops object
# files whose only content is an unreferenced static initialiser -- forcing
# the whole archive in ensures all registration side-effects execute at
# plugin load time regardless of platform.
# ---------------------------------------------------------------------------
add_library(NamDsp STATIC
    NeuralAmpModelerCore/NAM/activations.cpp
    NeuralAmpModelerCore/NAM/container.cpp
    NeuralAmpModelerCore/NAM/conv1d.cpp
    NeuralAmpModelerCore/NAM/convnet.cpp
    NeuralAmpModelerCore/NAM/dsp.cpp
    NeuralAmpModelerCore/NAM/get_dsp.cpp
    NeuralAmpModelerCore/NAM/linear.cpp
    NeuralAmpModelerCore/NAM/lstm.cpp
    NeuralAmpModelerCore/NAM/ring_buffer.cpp
    NeuralAmpModelerCore/NAM/util.cpp
    NeuralAmpModelerCore/NAM/wavenet/model.cpp
    NeuralAmpModelerCore/NAM/wavenet/a2_fast.cpp
    NeuralAmpModelerCore/NAM/wavenet/parametric_wavenet.cpp
    NeuralAmpModelerCore/NAM/wavenet/slimmable.cpp
    AudioDSPTools/dsp/dsp.cpp
    AudioDSPTools/dsp/ImpulseResponse.cpp
    AudioDSPTools/dsp/NoiseGate.cpp
    AudioDSPTools/dsp/RecursiveLinearFilter.cpp
    AudioDSPTools/dsp/wav.cpp
)

# NOMINMAX/WIN32_LEAN_AND_MEAN, Windows-only: <windows.h>'s own min/max macros collide with
# any std::max/std::min call in scope for the rest of that translation unit (MSVC error
# C2589/C2760/C3878, "illegal token on right side of '::'" -- a std::max(...) call gets
# macro-expanded into garbage). Found on real Windows CI: AudioDSPTools's LanczosResampler.h
# (std::max(res + 1.0, 0.0)) and this repo's own PluginProcessor.cpp (std::max(peak, ...))
# both hit it. PUBLIC so it propagates to anything linking NamDsp (NAMix, NAMixTests), which
# both also have their own std::max/std::min calls needing the same protection.
target_compile_definitions(NamDsp PUBLIC
    NAM_ENABLE_A2_FAST
    $<$<PLATFORM_ID:Windows>:NOMINMAX;WIN32_LEAN_AND_MEAN>
)

target_include_directories(NamDsp PUBLIC
    NeuralAmpModelerCore
    NeuralAmpModelerCore/NAM
    NeuralAmpModelerCore/Dependencies/nlohmann
    AudioDSPTools/dsp
    eigen
)

# GNU/Clang-only flags, guarded on compiler ID (not just version): -fpermissive's own
# version check used to run unconditionally, so a Clang build whose (unrelated) version
# numbering happened to read < 13 would have picked up a GCC-only flag Clang doesn't
# support -- it never bit us here only because AppleClang reports a version far above 13.
# No MSVC equivalent is added for -ffast-math/-fPIC here: NamDsp is a small vendored DSP
# core, not the perf-critical audio-callback path (that's NAMix's own target below).
target_compile_options(NamDsp PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffast-math -Wno-unused-parameter -fPIC>
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,13>>:-fpermissive>
)

# ---------------------------------------------------------------------------
# Embedded font resources (Michroma-Regular and Roboto-Regular, copied from
# the original NeuralAmpModelerPlugin resources under MIT licence).
# ---------------------------------------------------------------------------
juce_add_binary_data(NAMixBinaryData SOURCES
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
# AU only makes sense (and only builds) on macOS -- Logic Pro/GarageBand support ONLY AU,
# not VST3 at all, so this is required for Mac feature parity with the iPlug2-based
# "Anti-Static" build, not just a nice-to-have.
set(NAMIX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND NAMIX_FORMATS AU)
endif()

juce_add_plugin(NAMix
    COMPANY_NAME               "Gene Ko"
    BUNDLE_ID                  "com.geneko.antistaticnam"
    PLUGIN_MANUFACTURER_CODE   NMIX
    PLUGIN_CODE                Nmix
    FORMATS                    ${NAMIX_FORMATS}
    NEEDS_STANDALONE           TRUE
    PRODUCT_NAME               "Anti-Static NAM"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            "Fx"
    AU_MAIN_TYPE               "kAudioUnitType_Effect"
)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
target_sources(NAMix PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ToneStack.cpp
)

# ---------------------------------------------------------------------------
# Compile definitions
# ---------------------------------------------------------------------------
target_compile_definitions(NAMix
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        NAM_ENABLE_A2_FAST
        # Belt-and-suspenders with NamDsp's own PUBLIC definition of these (see that
        # target's comment) -- PluginProcessor.cpp has its own std::max() call that needs
        # the same protection, independent of anything propagated from linking NamDsp.
        $<$<PLATFORM_ID:Windows>:NOMINMAX;WIN32_LEAN_AND_MEAN>
)

# NamDsp's PUBLIC include paths flow into NAMix automatically via the
# target_link_libraries below, so no need to repeat them here.

# ---------------------------------------------------------------------------
# Include paths
# ---------------------------------------------------------------------------
target_include_directories(NAMix PRIVATE
    src
)

# ---------------------------------------------------------------------------
# Link libraries
# ---------------------------------------------------------------------------
target_link_libraries(NAMix
    PRIVATE
        # WHOLE_ARCHIVE ensures every .o in NamDsp is linked into the plugin binary, so
        # that static-initialiser registrations (WaveNet, LSTM, etc.) always execute at
        # plugin load time regardless of LTO / --as-needed / dead-code stripping. The
        # $<LINK_LIBRARY:WHOLE_ARCHIVE,...> generator expression (CMake 3.24+) picks the
        # right underlying syntax per platform: -Wl,--whole-archive/--no-whole-archive
        # (GNU ld, Linux), -Wl,-force_load,<path> (Apple's linker, macOS), /WHOLEARCHIVE:
        # (MSVC, Windows) -- a hand-rolled single flag pair only ever worked on Linux.
        $<LINK_LIBRARY:WHOLE_ARCHIVE,NamDsp>
        NAMixBinaryData
        juce::juce_audio_utils
        juce::juce_opengl
        # GCC's <filesystem> needed a separate libstdc++fs.a before GCC 9; harmless to
        # keep for older Linux toolchains, but the library doesn't exist for AppleClang/
        # libc++ (macOS) or MSVC (Windows) and linking it there is a hard error.
        $<$<AND:$<PLATFORM_ID:Linux>,$<CXX_COMPILER_ID:GNU>>:stdc++fs>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
# See NamDsp's own target_compile_options above for why these are gated on compiler ID,
# not just version. MSVC gets /fp:fast as the -ffast-math equivalent -- this target IS the
# perf-critical audio-callback path, unlike NamDsp, so it's worth the explicit Windows case
# rather than silently dropping the optimization there.
target_compile_options(NAMix PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-ffast-math;-Wall;-Wno-unused-parameter>
    $<$<CXX_COMPILER_ID:MSVC>:/fp:fast>
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,13>>:-fpermissive>
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
