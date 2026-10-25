cmake_minimum_required(VERSION 3.22)
project(GumboPot VERSION 0.3.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum supported macOS")
endif()
set(GUMBO_JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "JUCE 8.0.4 checkout")
if(NOT EXISTS "${GUMBO_JUCE_PATH}/CMakeLists.txt")
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 51d11a2be6d5c97ccf12b4e5e827006e19f0555a GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(JUCE)
else()
    add_subdirectory("${GUMBO_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
endif()
option(GUMBO_BUILD_TESTS "Build the native audio/state/export checks and screenshot renderer" ON)
option(GUMBO_BUILD_STANDALONE "Optional standalone diagnostic app" OFF)
set(GUMBO_FORMATS VST3)
if(APPLE)
    list(APPEND GUMBO_FORMATS AU)
endif()
if(GUMBO_BUILD_STANDALONE)
    list(APPEND GUMBO_FORMATS Standalone)
endif()
juce_add_plugin(GumboPot
    COMPANY_NAME "Gumbo Pot Audio"
    BUNDLE_ID "audio.gumbopot.instrument"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    PLUGIN_MANUFACTURER_CODE Gmbo
    PLUGIN_CODE Gpot
    FORMATS ${GUMBO_FORMATS}
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "Gumbo Pot")
target_sources(GumboPot PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/LayerPlayer.cpp)
target_compile_definitions(GumboPot PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(GumboPot PRIVATE juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(GUMBO_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(GumboNativeChecks PRODUCT_NAME "Gumbo Native Checks")
    target_sources(GumboNativeChecks PRIVATE tests/NativeChecks.cpp)
    target_compile_definitions(GumboNativeChecks PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(GumboNativeChecks PRIVATE GumboPot juce::juce_audio_utils juce::juce_dsp)
    target_include_directories(GumboNativeChecks PRIVATE Source)
    add_test(NAME native_audio_state_export COMMAND GumboNativeChecks)
endif()
