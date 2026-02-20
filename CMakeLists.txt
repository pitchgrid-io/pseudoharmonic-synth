cmake_minimum_required(VERSION 3.15)

project(PseudoHarmonicSynth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# JUCE (git submodule)
add_subdirectory(JUCE)

# Scalatrix
include(FetchContent)
FetchContent_Declare(
    scalatrix
    GIT_REPOSITORY https://github.com/pitchgrid-io/scalatrix.git
    GIT_TAG feature/consonance
)
FetchContent_MakeAvailable(scalatrix)

# nlohmann/json (header-only)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

juce_add_plugin(PseudoHarmonicSynth
    VERSION ${PROJECT_VERSION}
    FORMATS Standalone VST3
    PRODUCT_NAME "PseudoHarmonic"
    COMPANY_NAME "PitchGrid"
    BUNDLE_ID "io.pitchgrid.pseudoharmonic"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PLUGIN_MANUFACTURER_CODE PtGd
    PLUGIN_CODE PhSy
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
)

target_sources(PseudoHarmonicSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/PseudoHarmonicVoice.cpp
    Source/DSP/PseudoHarmonicVoice.h
    Source/DSP/PseudoHarmonicEngine.cpp
    Source/DSP/PseudoHarmonicEngine.h
    Source/Network/SimpleWebSocketServer.h
    Source/Network/WSBridge.cpp
    Source/Network/WSBridge.h
    Source/Visualization/ConsonanceCurve.cpp
    Source/Visualization/ConsonanceCurve.h
)

target_compile_definitions(PseudoHarmonicSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)

target_link_libraries(PseudoHarmonicSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        scalatrix
        nlohmann_json::nlohmann_json
        ${ALSA_LIBRARIES}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)

# Copy web UI to build output (if built)
add_custom_command(TARGET PseudoHarmonicSynth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E true
    COMMENT "Build complete"
)
