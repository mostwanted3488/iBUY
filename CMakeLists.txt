cmake_minimum_required(VERSION 3.22)

# Universal macOS build (Apple Silicon + Intel), and a static Windows runtime so the
# plugin runs without a Visual C++ redistributable installed.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime")

project(iBuy VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

set(IBUY_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND IBUY_FORMATS AU)
endif()

juce_add_plugin(iBuy
    COMPANY_NAME "iBuy"
    BUNDLE_ID com.ibuy.plugin
    PRODUCT_NAME "iBuyorbealoser"
    PLUGIN_MANUFACTURER_CODE Ibuy
    PLUGIN_CODE Ibuy
    FORMATS ${IBUY_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(iBuyAssets SOURCES Assets/ipod.jpg)

target_sources(iBuy PRIVATE
    Source/Engine.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(iBuy PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(iBuy
    PRIVATE
        iBuyAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
