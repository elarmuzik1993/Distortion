cmake_minimum_required(VERSION 3.22)

# ============================================================================
# macOS: universal binary + minimum deployment target.
# These MUST be set before project()/the first target or they won't apply to
# the universal (arm64 + x86_64) build. Guarded so Linux/Windows are untouched.
# Overridable from the command line (e.g. a single-arch local dev build).
# ============================================================================
if(APPLE)
    if(NOT DEFINED CMAKE_OSX_ARCHITECTURES OR CMAKE_OSX_ARCHITECTURES STREQUAL "")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING
            "macOS build architectures (universal by default)" FORCE)
    endif()
    if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET OR CMAKE_OSX_DEPLOYMENT_TARGET STREQUAL "")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING
            "Minimum macOS version" FORCE)
    endif()
endif()

project(Distortion VERSION 2.3.0 LANGUAGES CXX)

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

# ============================================================================
# Auto-generate version header from git at build time
# ============================================================================
find_package(Git QUIET)
if(GIT_FOUND)
    add_custom_target(GitVersion ALL
        COMMAND ${CMAKE_COMMAND}
            -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_FILE=${CMAKE_SOURCE_DIR}/Source/GitVersion.h
            -P ${CMAKE_SOURCE_DIR}/cmake/GitVersion.cmake
        BYPRODUCTS ${CMAKE_SOURCE_DIR}/Source/GitVersion.h
        COMMENT "Generating GitVersion.h from git..."
    )
else()
    file(WRITE ${CMAKE_SOURCE_DIR}/Source/GitVersion.h
        "#pragma once\n#define GIT_VERSION_STRING \"v2.3\"\n")
endif()

function(add_git_version_dependency target)
    if(TARGET GitVersion)
        add_dependencies(${target} GitVersion)
    endif()
endfunction()

# Plugin sources (shared between plugin and tests)
set(PLUGIN_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/CustomKnob.cpp
    Source/CustomKnob.h
    Source/CyclingComboBox.h
    Source/LinearRamp.h
    Source/FactoryPresets.h
    Source/Diagnostics/AppPaths.h
    Source/Diagnostics/Report.h
    Source/Diagnostics/ITransport.h
    Source/Diagnostics/ReportStore.h
    Source/Diagnostics/ReportStore.cpp
    Source/Diagnostics/DiagnosticsSink.h
    Source/Diagnostics/ReportComposer.h
    Source/Diagnostics/ReportComposer.cpp
    Source/Diagnostics/ReportSender.h
    Source/Diagnostics/ReportSender.cpp
    Source/Diagnostics/ReportEndpoint.h
    Source/Diagnostics/CurlTransport.h
    Source/Diagnostics/CurlTransport.cpp
)

# Test sources
set(TEST_SOURCES
    Source/Tests/TestRunner.cpp
    Source/Tests/DistortionTests.cpp
    Source/Tests/DistortionTests.h
    Source/Tests/TestUtilities.h
    Source/RTAllocationGuard.h
    Source/RTAllocationGuard.cpp
)

# ============================================================================
# Main Plugin Target (VST3 + Standalone; + AU on macOS for Logic/GarageBand)
# ============================================================================
set(DISTORTION_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DISTORTION_FORMATS AU)
endif()

juce_add_plugin(Distortion
    VERSION 2.3.0
    COMPANY_NAME "Monolit Beatz"
    BUNDLE_ID "com.monolitbeatz.MonolitDistortion"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Mbds
    PLUGIN_CODE Dist
    AU_MAIN_TYPE kAudioUnitType_Effect
    FORMATS ${DISTORTION_FORMATS}
    PRODUCT_NAME "Sledge Distortion"
)

target_sources(Distortion PRIVATE ${PLUGIN_SOURCES})
add_git_version_dependency(Distortion)

# Generate JuceHeader.h for Projucer-style compatibility
juce_generate_juce_header(Distortion)

# Add binary data (background image)
juce_add_binary_data(DistortionData SOURCES
    Resources/ui_texture.png
    Resources/ui_texture_extreme.png
    "Resources/Logo Title.png"
    Resources/Orbitron-Bold.ttf
    Resources/Orbitron-Regular.ttf
)

target_compile_definitions(Distortion
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        # dlopen libcurl at runtime instead of hard-linking it: no version-pinned .so
        # dependency for distribution, and a missing libcurl degrades the bug reporter
        # gracefully (network call returns null) rather than failing plugin load.
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
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
# Post-build: embed LICENSE + THIRD-PARTY-NOTICES.md inside each bundle
# ============================================================================
# The GPL, the Orbitron OFL and the Steinberg VST3 BSD notice all bind *binary*
# distributions, so the terms must survive installation rather than only sitting
# beside the bundle inside an archive. Contents/Resources is where JUCE already
# writes moduleinfo.json, and putting them there means every channel inherits
# them for free: the Windows installer copies the bundle recursively, the macOS
# .pkg stages built bundles as its payload, and all three archives contain the
# bundle. Adding a distribution channel therefore needs no extra licence wiring.
set(DISTORTION_LICENCE_FILES
    "${CMAKE_SOURCE_DIR}/LICENSE"
    "${CMAKE_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
)

function(distortion_embed_licences target bundle_dir)
    if(NOT TARGET ${target})
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${bundle_dir}/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${DISTORTION_LICENCE_FILES}
                "${bundle_dir}/Contents/Resources"
        COMMENT "Embedding LICENSE + THIRD-PARTY-NOTICES.md into ${target}"
        VERBATIM
    )
endfunction()

set(DISTORTION_ARTEFACTS "${CMAKE_BINARY_DIR}/Distortion_artefacts/$<CONFIG>")
distortion_embed_licences(Distortion_VST3
    "${DISTORTION_ARTEFACTS}/VST3/Sledge Distortion.vst3")
if(APPLE)
    # AU and Standalone are real bundles only on Apple; on Windows and Linux the
    # standalone is a bare executable and is not shipped to users anyway.
    distortion_embed_licences(Distortion_AU
        "${DISTORTION_ARTEFACTS}/AU/Sledge Distortion.component")
    distortion_embed_licences(Distortion_Standalone
        "${DISTORTION_ARTEFACTS}/Standalone/Sledge Distortion.app")
endif()

# ============================================================================
# Test Runner Target (Console Application)
# ============================================================================
juce_add_console_app(DistortionTests
    COMPANY_NAME "Monolit Beatz"
    BUNDLE_ID "com.monolitbeatz.MonolitDistortionTests"
    PRODUCT_NAME "DistortionTests"
)

target_sources(DistortionTests PRIVATE
    ${PLUGIN_SOURCES}
    ${TEST_SOURCES}
)
add_git_version_dependency(DistortionTests)

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
        DISTORTION_RT_GUARD=1
        DISTORTION_UNIT_TEST=1
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
# Offline Render Harness (Console Application)
# Headless tool to render audio through the real PluginProcessor and to verify
# the Sub Guard crossover is magnitude-flat. See Source/Tools/RenderHarness.cpp.
# ============================================================================
juce_add_console_app(DistortionRender
    COMPANY_NAME "Monolit Beatz"
    BUNDLE_ID "com.monolitbeatz.MonolitDistortionRender"
    PRODUCT_NAME "DistortionRender"
)

target_sources(DistortionRender PRIVATE
    ${PLUGIN_SOURCES}
    Source/Tools/RenderHarness.cpp
)
add_git_version_dependency(DistortionRender)

juce_generate_juce_header(DistortionRender)

target_compile_definitions(DistortionRender
    PRIVATE
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

target_link_libraries(DistortionRender
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
# DistortionUiSnapshot
# Headless tool that paints the real PluginEditor into PNGs at every window
# scale, with no host window to shift or clip it. See Source/Tools/UiSnapshot.cpp.
# ============================================================================
juce_add_console_app(DistortionUiSnapshot
    COMPANY_NAME "Monolit Beatz"
    BUNDLE_ID "com.monolitbeatz.MonolitDistortionUiSnapshot"
    PRODUCT_NAME "DistortionUiSnapshot"
)

target_sources(DistortionUiSnapshot PRIVATE
    ${PLUGIN_SOURCES}
    Source/Tools/UiSnapshot.cpp
)
add_git_version_dependency(DistortionUiSnapshot)

juce_generate_juce_header(DistortionUiSnapshot)

target_compile_definitions(DistortionUiSnapshot
    PRIVATE
        # Tool-only: lets the harness pump the message loop so timer-driven
        # animations (the EXTREME crack fade) settle before a frame is captured.
        JUCE_MODAL_LOOPS_PERMITTED=1
        # Tool-only: exposes PluginEditor::setDataRootOverride so the harness
        # renders from settings it owns instead of the machine's saved state.
        DISTORTION_UI_SNAPSHOT=1
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

target_link_libraries(DistortionUiSnapshot
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
# Soak / Multi-Instance Harness (Console Application)
# Headless stress tool for the DoD 24h-stress and 10+-instance shipping gates.
# See Source/Tools/SoakHarness.cpp.
# ============================================================================
juce_add_console_app(DistortionSoak
    COMPANY_NAME "Monolit Beatz"
    BUNDLE_ID "com.monolitbeatz.MonolitDistortionSoak"
    PRODUCT_NAME "DistortionSoak"
)

target_sources(DistortionSoak PRIVATE
    ${PLUGIN_SOURCES}
    Source/Tools/SoakHarness.cpp
)
add_git_version_dependency(DistortionSoak)

juce_generate_juce_header(DistortionSoak)

target_compile_definitions(DistortionSoak
    PRIVATE
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

target_link_libraries(DistortionSoak
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
