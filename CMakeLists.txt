cmake_minimum_required(VERSION 3.22)

set(JUCE_PATH "" CACHE PATH "Path to local JUCE checkout")
if(NOT JUCE_PATH)
    message(FATAL_ERROR "JUCE_PATH is not set. Configure with -DJUCE_PATH=/path/to/JUCE")
endif()

# this builds the Microsoft C++ runtime into the plugin and app on Windows, instead of relying on
# a separate runtime installer that not every user's machine has.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# these two only matter on a Mac and are ignored on Windows. the first is the oldest macOS version
# the plugin runs on. the second builds one "universal" binary that holds code for both Apple
# Silicon (arm64) and Intel (x86_64) Macs. both must be set before project().
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Oldest macOS version the plugin runs on")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Mac processor types to build for")

project(JuicoliciousGrandPiano VERSION 0.2.0)

add_subdirectory(${JUCE_PATH} JUCE)

# IS_SYNTH makes DAWs list the plugin as an instrument instead of an effect. on a Mac this is what
# makes the AU show up as an instrument in Logic and GarageBand. NEEDS_MIDI_INPUT gives the plugin
# a MIDI input so the DAW sends it notes.
juce_add_plugin(JuicoliciousGrandPiano
    COMPANY_NAME "rayraldo"
    BUNDLE_ID com.rayhanmoraldo.juicoliciousgrandpiano
    PLUGIN_MANUFACTURER_CODE Raym
    PLUGIN_CODE Jgpn
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Juicolicious Grand"
)

target_sources(JuicoliciousGrandPiano PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/PianoKeyboardDisplay.cpp
    Source/PianoKeyboardDisplay.h
    Source/PianoDisplay.cpp
    Source/PianoDisplay.h
)

juce_generate_juce_header(JuicoliciousGrandPiano)

juce_add_binary_data(JuicoliciousGrandPianoAssets SOURCES Assets/logo.png Assets/titlefont.ttf)

target_link_libraries(JuicoliciousGrandPiano PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp
    JuicoliciousGrandPianoAssets
)

target_compile_definitions(JuicoliciousGrandPiano PUBLIC
    JUCE_FORCE_USE_LEGACY_PARAM_IDS=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
