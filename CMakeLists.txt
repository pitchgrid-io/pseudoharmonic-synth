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
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "PseudoHarmonic"
    COMPANY_NAME "PitchGrid"
    BUNDLE_ID "io.pitchgrid.pseudoharmonic"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    PLUGIN_MANUFACTURER_CODE PtGd
    PLUGIN_CODE PhSy
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
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
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
)

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED alsa)
endif()

# Webview asset packaging (only for release builds)
if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Release build - setting up webview build...")

    set(WEBVIEW_DIR "${CMAKE_SOURCE_DIR}/ui")
    set(WEBVIEW_DIST_DIR "${WEBVIEW_DIR}/dist")
    set(WEBVIEW_ZIP_OUTPUT "${CMAKE_SOURCE_DIR}/Assets/webview-gui.zip")

    find_program(NODE_EXECUTABLE node)
    find_program(NPM_EXECUTABLE npm)

    if(NODE_EXECUTABLE AND NPM_EXECUTABLE)
        execute_process(
            COMMAND ${NPM_EXECUTABLE} --version
            RESULT_VARIABLE NPM_TEST_RESULT
            OUTPUT_VARIABLE NPM_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(NOT NPM_TEST_RESULT EQUAL 0)
            set(NPM_EXECUTABLE "NPM_EXECUTABLE-NOTFOUND")
        else()
            message(STATUS "NPM version: ${NPM_VERSION}")
        endif()
    endif()

    if(NODE_EXECUTABLE AND NPM_EXECUTABLE)
        set(BUILD_WEBVIEW_SCRIPT "${CMAKE_BINARY_DIR}/build_webview.cmake")
        file(WRITE ${BUILD_WEBVIEW_SCRIPT} "
            message(STATUS \"Building webview assets...\")

            if(NOT EXISTS \"${WEBVIEW_DIR}/node_modules\")
                execute_process(
                    COMMAND ${NPM_EXECUTABLE} install
                    WORKING_DIRECTORY ${WEBVIEW_DIR}
                    RESULT_VARIABLE NPM_INSTALL_RESULT
                    OUTPUT_VARIABLE NPM_INSTALL_OUTPUT
                    ERROR_VARIABLE NPM_INSTALL_ERROR
                )
                if(NOT NPM_INSTALL_RESULT EQUAL 0)
                    message(FATAL_ERROR \"Failed to install webview dependencies: \${NPM_INSTALL_ERROR}\")
                endif()
            endif()

            execute_process(
                COMMAND ${NPM_EXECUTABLE} run build
                WORKING_DIRECTORY ${WEBVIEW_DIR}
                RESULT_VARIABLE NPM_BUILD_RESULT
                OUTPUT_VARIABLE NPM_BUILD_OUTPUT
                ERROR_VARIABLE NPM_BUILD_ERROR
            )
            if(NOT NPM_BUILD_RESULT EQUAL 0)
                message(FATAL_ERROR \"Failed to build webview: \${NPM_BUILD_ERROR}\")
            endif()

            file(GLOB_RECURSE WEBVIEW_FILES RELATIVE ${WEBVIEW_DIST_DIR} \"${WEBVIEW_DIST_DIR}/*\")
            if(WEBVIEW_FILES)
                file(MAKE_DIRECTORY \"${CMAKE_SOURCE_DIR}/Assets\")
                execute_process(
                    COMMAND \${CMAKE_COMMAND} -E tar \"cf\" \"${WEBVIEW_ZIP_OUTPUT}\" --format=zip .
                    WORKING_DIRECTORY ${WEBVIEW_DIST_DIR}
                    RESULT_VARIABLE ZIP_RESULT
                    ERROR_VARIABLE ZIP_ERROR
                )
                if(NOT ZIP_RESULT EQUAL 0)
                    message(FATAL_ERROR \"Failed to create webview zip: \${ZIP_ERROR}\")
                endif()
                message(STATUS \"Created webview zip: ${WEBVIEW_ZIP_OUTPUT}\")
            else()
                message(FATAL_ERROR \"No files found in webview build directory\")
            endif()
        ")

        add_custom_target(BuildWebview ALL
            COMMAND ${CMAKE_COMMAND} -P ${BUILD_WEBVIEW_SCRIPT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Building webview assets..."
            VERBATIM
        )

        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/Assets")

        # Create dummy zip for initial configure if needed
        if(NOT EXISTS ${WEBVIEW_ZIP_OUTPUT})
            file(WRITE "${CMAKE_SOURCE_DIR}/Assets/dummy.txt" "dummy")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar "cf" ${WEBVIEW_ZIP_OUTPUT} --format=zip dummy.txt
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/Assets"
            )
            file(REMOVE "${CMAKE_SOURCE_DIR}/Assets/dummy.txt")
        endif()

        juce_add_binary_data(WebviewData SOURCES
            ${WEBVIEW_ZIP_OUTPUT}
        )

        add_dependencies(WebviewData BuildWebview)
        add_dependencies(PseudoHarmonicSynth WebviewData)

        message(STATUS "Webview will be built at compile time")
    else()
        message(FATAL_ERROR "Node.js/npm not found. Required for release builds.\n"
            "Please install Node.js from https://nodejs.org/")
    endif()
else()
    message(STATUS "Debug build - webview will use local development server")
endif()

target_link_libraries(PseudoHarmonicSynth
    PRIVATE
        juce::juce_audio_utils
        juce::juce_gui_basics
        juce::juce_gui_extra
        scalatrix
        nlohmann_json::nlohmann_json
        ${ALSA_LIBRARIES}
        $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:WebviewData>
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
)
