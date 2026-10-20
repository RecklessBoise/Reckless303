cmake_minimum_required(VERSION 3.22)

project(RECKLESS303 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

add_subdirectory(JUCE)

option(RECKLESS303_COPY_AFTER_BUILD "Install the plugins into the user plugin folders after build" ON)

juce_add_plugin(Reckless303
    COMPANY_NAME                "RecklessBoise"
    COMPANY_WEBSITE             "https://github.com/RecklessBoise/Reckless303"
    BUNDLE_ID                   "com.recklessboise.reckless303"
    PLUGIN_MANUFACTURER_CODE    Rkbs
    PLUGIN_CODE                 Rk33
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Reckless303"
    DESCRIPTION                 "TB-303 style bassline synthesizer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Synth Mono
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD     ${RECKLESS303_COPY_AFTER_BUILD})

target_sources(Reckless303 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Pattern.cpp
    Source/presets/PresetManager.cpp
    Source/presets/FactoryPresets.cpp
    Source/gui/MetalTexture.cpp
    Source/gui/LookAndFeel303.cpp
    Source/gui/Widgets.cpp)

target_include_directories(Reckless303 PRIVATE Source)

target_compile_definitions(Reckless303 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Reckless303
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline check tool (UI snapshots + audio render), off by default.
option(RECKLESS303_BUILD_TOOLS "Build the offline render/snapshot tool" OFF)

if(RECKLESS303_BUILD_TOOLS)
    juce_add_console_app(Reckless303RenderTool PRODUCT_NAME "Reckless303RenderTool")

    target_sources(Reckless303RenderTool PRIVATE
        tools/RenderTool.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Pattern.cpp
        Source/presets/PresetManager.cpp
        Source/presets/FactoryPresets.cpp
        Source/gui/MetalTexture.cpp
        Source/gui/LookAndFeel303.cpp
        Source/gui/Widgets.cpp)

    target_include_directories(Reckless303RenderTool PRIVATE Source)

    target_compile_definitions(Reckless303RenderTool PRIVATE
        JucePlugin_Name="Reckless303"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(Reckless303RenderTool PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
