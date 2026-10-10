cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

project(IRONCORE VERSION 0.8.1)

# Options
option(IRONCORE_COPY_AFTER_BUILD "Install the plug-in into the user's plug-in folder after building" ON)
option(IRONCORE_BUILD_TOOLS "Build the benchmark and stress-test tools" OFF)

add_subdirectory(JUCE)

if(APPLE)
    set(IRONCORE_FORMATS VST3 AU Standalone)
else()
    set(IRONCORE_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(IroncoreData SOURCES Resources/frequenzbetrieb-logo.png)

juce_add_plugin(IRONCORE
    COMPANY_NAME "Frequenzbetrieb n.e.V."
    COMPANY_WEBSITE "https://www.frequenzbetrieb.de"
    BUNDLE_ID "de.frequenzbetrieb.ironcore"
    PLUGIN_MANUFACTURER_CODE Irnc
    PLUGIN_CODE Ircr
    FORMATS ${IRONCORE_FORMATS}
    PRODUCT_NAME "IRONCORE"
    DESCRIPTION "Industrial Lead Synthesizer"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${IRONCORE_COPY_AFTER_BUILD})

set(IRONCORE_SOURCES
    Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Voice.cpp Source/Presets.cpp Source/Wavetables.cpp)

juce_generate_juce_header(IRONCORE)
target_sources(IRONCORE PRIVATE ${IRONCORE_SOURCES})
target_compile_features(IRONCORE PRIVATE cxx_std_17)
target_compile_definitions(IRONCORE PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0 IRONCORE_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(IRONCORE PRIVATE IroncoreData juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Developer tools (CPU benchmark + randomized stress tests), not shipped
if(IRONCORE_BUILD_TOOLS)
    foreach(tool IroncoreBench IroncoreStress)
        juce_add_console_app(${tool} PRODUCT_NAME "${tool}")
        juce_generate_juce_header(${tool})
        target_compile_features(${tool} PRIVATE cxx_std_17)
        target_compile_definitions(${tool} PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            IRONCORE_VERSION_STRING="${PROJECT_VERSION}" IRONCORE_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        target_link_libraries(${tool} PRIVATE IroncoreData juce::juce_audio_utils juce::juce_dsp
            PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags)
    endforeach()
    target_sources(IroncoreBench PRIVATE Tools/Bench.cpp ${IRONCORE_SOURCES})
    target_sources(IroncoreStress PRIVATE Tools/Stress.cpp ${IRONCORE_SOURCES})
endif()
