cmake_minimum_required(VERSION 3.22)

project(Distortion VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to specify JUCE path or fetch it
set(JUCE_PATH "" CACHE PATH "Path to JUCE installation")

# Try to find JUCE locally first, otherwise fetch it
if(JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(STATUS "Using JUCE from: ${JUCE_PATH}")
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../JUCE/CMakeLists.txt")
    message(STATUS "Using JUCE from: ${CMAKE_SOURCE_DIR}/../JUCE")
    add_subdirectory(${CMAKE_SOURCE_DIR}/../JUCE ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "Fetching JUCE from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 7.0.12
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Plugin sources (shared between plugin and tests)
set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/CustomKnob.cpp
    Source/CustomKnob.h
)

# Test sources
set(TEST_SOURCES
    Source/Tests/TestRunner.cpp
    Source/Tests/DistortionTests.cpp
    Source/Tests/DistortionTests.h
    Source/Tests/TestUtilities.h
)

# ============================================================================
# Main Plugin Target (VST3 + Standalone)
# ============================================================================
juce_add_plugin(Distortion
    COMPANY_NAME "Elar Music Audio"
    BUNDLE_ID "com.ElarMusicAudio.Distortion"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Elar
    PLUGIN_CODE Dist
    FORMATS VST3 VST2 Standalone
    PRODUCT_NAME "Distortion"
)

target_sources(Distortion PRIVATE ${PLUGIN_SOURCES})

# Generate JuceHeader.h for Projucer-style compatibility
juce_generate_juce_header(Distortion)

# Add binary data (background image)
juce_add_binary_data(DistortionData SOURCES
    Resources/background.png
    "Resources/Logo Title.png"
    Resources/Orbitron-Bold.ttf
    Resources/Orbitron-Regular.ttf
)

target_compile_definitions(Distortion
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(Distortion
    PRIVATE
        DistortionData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================================================
# Post-build: Fix invalid JSON in VST3 moduleinfo.json
# JUCE generates JSON with trailing commas which breaks some DAWs
# ============================================================================
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
    add_custom_command(TARGET Distortion_VST3 POST_BUILD
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/fix_moduleinfo_json.py"
                "${CMAKE_BINARY_DIR}/Distortion_artefacts/$<CONFIG>/VST3" --all
        COMMENT "Fixing moduleinfo.json trailing commas..."
        VERBATIM
    )
endif()

# ============================================================================
# Test Runner Target (Console Application)
# ============================================================================
juce_add_console_app(DistortionTests
    COMPANY_NAME "Elar Music Audio"
    BUNDLE_ID "com.ElarMusicAudio.DistortionTests"
    PRODUCT_NAME "DistortionTests"
)

target_sources(DistortionTests PRIVATE
    ${PLUGIN_SOURCES}
    ${TEST_SOURCES}
)

# Generate JuceHeader.h for Projucer-style compatibility
juce_generate_juce_header(DistortionTests)

target_compile_definitions(DistortionTests
    PRIVATE
        JUCE_DEBUG=1
        JucePlugin_Name="Distortion"
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_STANDALONE_APPLICATION=1
)

target_link_libraries(DistortionTests
    PRIVATE
        DistortionData
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ============================================================================
# Custom target to run tests
# ============================================================================
add_custom_target(run_tests
    COMMAND DistortionTests
    DEPENDS DistortionTests
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running Distortion Tests..."
)

# ============================================================================
# Install targets
# ============================================================================
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Install VST3 to user's VST3 directory on Linux
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Distortion_artefacts/${CMAKE_BUILD_TYPE}/VST3/
            DESTINATION "$ENV{HOME}/.vst3"
            USE_SOURCE_PERMISSIONS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # Install VST3 to user's VST3 directory on macOS
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Distortion_artefacts/${CMAKE_BUILD_TYPE}/VST3/
            DESTINATION "$ENV{HOME}/Library/Audio/Plug-Ins/VST3"
            USE_SOURCE_PERMISSIONS)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Install VST3 to Common Files on Windows
    install(DIRECTORY ${CMAKE_BINARY_DIR}/Distortion_artefacts/${CMAKE_BUILD_TYPE}/VST3/
            DESTINATION "$ENV{COMMONPROGRAMFILES}/VST3"
            USE_SOURCE_PERMISSIONS)
endif()

# Custom target to build and install in one step
add_custom_target(build_install
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target Distortion_VST3 -j --clean-first
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "Installation complete! VST3 installed successfully."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Building and installing Distortion VST3..."
)
