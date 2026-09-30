cmake_minimum_required(VERSION 3.22)

set(JUCE_PATH "" CACHE PATH "Path to local JUCE checkout")
if(NOT JUCE_PATH)
    message(FATAL_ERROR "JUCE_PATH is not set. Configure with -DJUCE_PATH=/path/to/JUCE")
endif()

project(JuicoliciousGrandPiano VERSION 0.1.0)

add_subdirectory(${JUCE_PATH} JUCE)

juce_add_plugin(JuicoliciousGrandPiano
    COMPANY_NAME "rayraldo"
    BUNDLE_ID com.rayhanmoraldo.juicoliciousgrandpiano
    PLUGIN_MANUFACTURER_CODE Raym
    PLUGIN_CODE Jgpn
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