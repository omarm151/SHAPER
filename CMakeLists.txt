cmake_minimum_required(VERSION 3.22)

project(IlluminizeShaper VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if given (-DJUCE_PATH=...), otherwise download 9.0.4.
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Engine library (plain C++, shared by the plugin and the tests)
add_library(ShaperDSP STATIC
    source/dsp/Curve.cpp
    source/dsp/ShaperEngine.cpp)
target_include_directories(ShaperDSP PUBLIC source)
set_target_properties(ShaperDSP PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Plugin
set(SHAPER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND SHAPER_FORMATS AU)
endif()

juce_add_plugin(IlluminizeShaper
    COMPANY_NAME               "Illuminize"
    COMPANY_WEBSITE            ""
    BUNDLE_ID                  "com.illuminize.shaper"
    PLUGIN_MANUFACTURER_CODE   Illu
    PLUGIN_CODE                IlSh
    PRODUCT_NAME               "Illuminize Shaper"
    DESCRIPTION                "Editable-curve waveshaper"
    FORMATS                    ${SHAPER_FORMATS}
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    VST3_CATEGORIES            Fx Distortion
    AU_MAIN_TYPE               kAudioUnitType_Effect)

juce_add_binary_data(IlluminizeShaperData SOURCES
    assets/SpaceGrotesk-Regular.ttf
    assets/SpaceGrotesk-Medium.ttf
    assets/SpaceGrotesk-Bold.ttf)

target_sources(IlluminizeShaper PRIVATE
    source/PluginProcessor.cpp
    source/PluginEditor.cpp
    source/ui/Theme.cpp
    source/ui/CurveEditor.cpp
    source/ui/AudioForm.cpp
    source/ui/Widgets.cpp)

target_include_directories(IlluminizeShaper PRIVATE source)

target_compile_definitions(IlluminizeShaper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Linux-only: allow building in minimal environments (CI checks); macOS is unaffected.
if(UNIX AND NOT APPLE)
    target_compile_definitions(IlluminizeShaper PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0
        JUCE_ALSA=0 JUCE_JACK=0)
endif()

# Exact float comparisons (x == 0, identical point positions) are intentional in the curve code.
# (Source-level options come after JUCE's recommended warning flags, so this wins.)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set_source_files_properties(
        source/PluginProcessor.cpp source/PluginEditor.cpp source/ui/Theme.cpp
        source/ui/CurveEditor.cpp source/ui/AudioForm.cpp source/ui/Widgets.cpp
        TARGET_DIRECTORY IlluminizeShaper
        PROPERTIES COMPILE_OPTIONS -Wno-float-equal)
endif()

target_link_libraries(IlluminizeShaper
    PRIVATE
        ShaperDSP
        IlluminizeShaperData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Tests
enable_testing()
add_executable(EngineTests tests/EngineTests.cpp)
target_link_libraries(EngineTests PRIVATE ShaperDSP)
add_test(NAME EngineTests COMMAND EngineTests ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference)

# ---------------------------------------------------------------------------
# Developer tools (off by default): UI screenshots and a VST3 hosting check.
option(ILLUMINIZE_DEV_TOOLS "Build UI snapshot and VST3 host check tools" OFF)
if(ILLUMINIZE_DEV_TOOLS)
    juce_add_console_app(UiSnapshot PRODUCT_NAME "UiSnapshot")
    target_sources(UiSnapshot PRIVATE tools/UiSnapshot.cpp)
    # Re-uses the plugin's compiled code; only borrow JUCE's headers and settings.
    target_include_directories(UiSnapshot PRIVATE source
        $<TARGET_PROPERTY:juce::juce_audio_utils,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:IlluminizeShaper,INCLUDE_DIRECTORIES>)
    target_compile_definitions(UiSnapshot PRIVATE $<TARGET_PROPERTY:IlluminizeShaper,COMPILE_DEFINITIONS>)
    target_link_libraries(UiSnapshot PRIVATE IlluminizeShaper)

    juce_add_console_app(HostCheck PRODUCT_NAME "HostCheck" PLUGINHOST_AU TRUE)
    target_sources(HostCheck PRIVATE tools/HostCheck.cpp)
    target_compile_definitions(HostCheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    if(UNIX AND NOT APPLE)
        target_compile_definitions(HostCheck PRIVATE
            JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XINPUT=0 JUCE_ALSA=0 JUCE_JACK=0)
    endif()
    target_link_libraries(HostCheck PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
    if(APPLE)
        # Audio Unit hosting needs CoreAudioKit (AUGenericView).
        target_link_libraries(HostCheck PRIVATE "-framework CoreAudioKit" "-framework AudioUnit")
    endif()
endif()
