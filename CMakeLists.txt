cmake_minimum_required(VERSION 3.15)

project(PseudoHarmonicSynth VERSION 0.3.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(UNIX AND NOT APPLE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# Handle WebView2 for Windows
if(WIN32)
    set(WEBVIEW2_VERSION "1.0.3485.44")
    set(WEBVIEW2_CACHE_DIR "${CMAKE_BINARY_DIR}/_deps")
    file(GLOB EXISTING_WEBVIEW2 "${WEBVIEW2_CACHE_DIR}/*Microsoft.Web.WebView2*")

    if(NOT EXISTING_WEBVIEW2)
        message(STATUS "WebView2 package not found. Downloading...")
        set(WEBVIEW2_URL "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/${WEBVIEW2_VERSION}")
        set(WEBVIEW2_ZIP "${WEBVIEW2_CACHE_DIR}/webview2_temp.zip")

        file(DOWNLOAD "${WEBVIEW2_URL}" "${WEBVIEW2_ZIP}"
             STATUS DOWNLOAD_STATUS
             SHOW_PROGRESS)

        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
        list(GET DOWNLOAD_STATUS 1 DOWNLOAD_ERROR)

        if(DOWNLOAD_STATUS_CODE EQUAL 0)
            set(EXTRACT_DIR "${WEBVIEW2_CACHE_DIR}/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
            file(MAKE_DIRECTORY "${EXTRACT_DIR}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xf "${WEBVIEW2_ZIP}"
                WORKING_DIRECTORY "${EXTRACT_DIR}"
                RESULT_VARIABLE EXTRACT_RESULT
            )
            if(EXTRACT_RESULT EQUAL 0)
                file(REMOVE "${WEBVIEW2_ZIP}")
                message(STATUS "WebView2 ${WEBVIEW2_VERSION} downloaded and extracted")
            else()
                message(FATAL_ERROR "Failed to extract WebView2 package")
            endif()
        else()
            message(FATAL_ERROR "Failed to download WebView2 package: ${DOWNLOAD_ERROR}")
        endif()
    else()
        message(STATUS "Found existing WebView2 package: ${EXISTING_WEBVIEW2}")
    endif()

    set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${WEBVIEW2_CACHE_DIR}" CACHE PATH "WebView2 package location" FORCE)
endif()

# JUCE (git submodule)
add_subdirectory(JUCE)

# Scalatrix (git submodule)
add_subdirectory(scalatrix)

# nlohmann/json (header-only)
include(FetchContent)
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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_512.png"
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    NEEDS_WEBVIEW2 TRUE
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
    Source/Network/OSCReceiver.cpp
    Source/Network/OSCReceiver.h
    Source/Visualization/ConsonanceCurve.cpp
    Source/Visualization/ConsonanceCurve.h
    Source/MTS-ESP/libMTSClient.cpp
    Source/MTS-ESP/libMTSClient.h
)

target_include_directories(PseudoHarmonicSynth PRIVATE Source/MTS-ESP)

if(APPLE OR (UNIX AND NOT APPLE))
    # MTS-ESP client uses dlopen/dlsym at runtime
    target_link_libraries(PseudoHarmonicSynth PRIVATE ${CMAKE_DL_LIBS})
endif()

# Generate build timestamp at build time (not configure time)
set(TIMESTAMP_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildTimestamp.h")
add_custom_target(GenerateBuildTimestamp ALL
    COMMAND ${CMAKE_COMMAND}
        -DOUTPUT_FILE=${TIMESTAMP_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateTimestamp.cmake
    COMMENT "Generating build timestamp"
)
add_dependencies(PseudoHarmonicSynth GenerateBuildTimestamp)
target_include_directories(PseudoHarmonicSynth PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_compile_definitions(PseudoHarmonicSynth PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=1
    $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>
    $<$<CXX_COMPILER_ID:MSVC>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2=1>
    $<$<PLATFORM_ID:Windows>:JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1>
    $<$<PLATFORM_ID:Windows>:JUCE_ASIO=1>
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

    find_program(NODE_EXECUTABLE NAMES node
        HINTS "$ENV{NVM_HOME}" "$ENV{NVM_SYMLINK}" "C:/nvm4w/nodejs" "C:/Program Files/nodejs")
    find_program(NPM_EXECUTABLE NAMES npm npm.cmd
        HINTS "$ENV{NVM_HOME}" "$ENV{NVM_SYMLINK}" "C:/nvm4w/nodejs" "C:/Program Files/nodejs")

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

# Replace JUCE-generated Icon.icns with full multi-resolution version if available
set(FULL_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/assets/Icon.icns")
if(APPLE AND EXISTS "${FULL_ICNS}")
    set(ARTEFACTS_DIR "${CMAKE_BINARY_DIR}/PseudoHarmonicSynth_artefacts/$<CONFIG>")
    foreach(BUNDLE
        "${ARTEFACTS_DIR}/Standalone/PseudoHarmonic.app"
        "${ARTEFACTS_DIR}/AU/PseudoHarmonic.component"
        "${ARTEFACTS_DIR}/VST3/PseudoHarmonic.vst3")
        add_custom_command(TARGET PseudoHarmonicSynth POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${FULL_ICNS}"
                "${BUNDLE}/Contents/Resources/Icon.icns"
            COMMENT "Installing full-resolution icon into ${BUNDLE}"
        )
    endforeach()
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
