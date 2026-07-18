# NAMix unit tests. Built via juce_add_console_app() rather than
# juce_add_plugin(): PluginProcessor.cpp/.h were refactored (see
# createEditor()'s move to PluginEditor.cpp) so the processor and the
# parametric-knob logic it owns don't depend on the *GUI-rendering* module
# (BinaryData, fonts, juce_gui_basics) and can be run headlessly, without a
# display, in CI. juce_add_console_app() is still needed (over a plain
# add_executable()) because AudioProcessorValueTreeState only exists in the
# full juce_audio_processors module, which itself declares a hard
# dependency on juce_gui_extra (for plugin-hosting/scanning UI code we never
# call) -- juce_add_console_app runs the same module-initialisation
# machinery juce_add_plugin does, which resolves that dependency's own
# system requirements (e.g. GTK pkg-config) correctly; a raw add_executable
# does not.
juce_add_console_app(NAMixTests
    PRODUCT_NAME "NAMixTests"
)

target_sources(NAMixTests PRIVATE
    TestMain.cpp
    TestEditorStub.cpp
    ParametricRangeTests.cpp
    ParametricRowLayoutTests.cpp
    StateRestoreTests.cpp
    ../src/PluginProcessor.cpp
    ../src/ToneStack.cpp
)

target_include_directories(NAMixTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# getName() is the processor's only reference to a JucePlugin_* macro
# (normally supplied by juce_add_plugin()); a placeholder here is enough
# since these tests never call it.
#
# JUCE_WEB_BROWSER=0 matches NAMixLinux's own setting (root CMakeLists.txt):
# without it, juce_gui_extra.cpp's #include <gtk/gtk.h> (behind
# `#elif (JUCE_LINUX || JUCE_BSD) && JUCE_WEB_BROWSER`) is compiled in, which
# needs webkit2gtk's GTK dev headers -- unrelated to anything these tests
# actually use.
target_compile_definitions(NAMixTests PRIVATE
    JucePlugin_Name="NAMixTests"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    NAMIX_TEST_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../ParametricModels"
)

target_compile_options(NAMixTests PRIVATE
    $<$<VERSION_LESS:$<CXX_COMPILER_VERSION>,13>:-fpermissive>)

target_link_libraries(NAMixTests PRIVATE
    juce::juce_core
    juce::juce_events
    juce::juce_data_structures
    juce::juce_audio_basics
    juce::juce_audio_processors
    # --whole-archive: StateRestoreTests.cpp calls nam::get_dsp() on a real
    # .nam file, which needs the architecture self-registration static
    # initialisers (see the matching comment on NAMixLinux's own link line
    # in the root CMakeLists.txt) to have actually run.
    -Wl,--whole-archive NamDsp -Wl,--no-whole-archive
)

add_test(NAME NAMixTests COMMAND NAMixTests)
