cmake_minimum_required(VERSION 3.22)

project(AutoWah VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Never bake the local dependency sysroot into distributed binaries.
set(CMAKE_SKIP_RPATH TRUE)

option(AUTOWAH_BUILD_TESTS "Build the offline DSP test executable" ON)
option(AUTOWAH_BUILD_SCREENSHOT "Build the headless editor renderer" ON)
option(AUTOWAH_BUILD_HOST_TEST "Build the VST3 host smoke test" ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/JUCE/CMakeLists.txt")
    add_subdirectory(external/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.8
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(AutoWahAssets
    SOURCES
        Resources/LatoRegular.ttf
        Resources/LatoSemibold.ttf
        Resources/LatoBlack.ttf
        Resources/Lato-OFL.txt)

set(AUTOWAH_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Parameters.h
    Source/Presets.h
    Source/dsp/AutoWahEngine.h
    Source/ui/Theme.cpp
    Source/ui/Theme.h
    Source/ui/StudioLookAndFeel.cpp
    Source/ui/StudioLookAndFeel.h
    Source/ui/Controls.cpp
    Source/ui/Controls.h
    Source/ui/FilterDisplay.cpp
    Source/ui/FilterDisplay.h)

set(AUTOWAH_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

juce_add_plugin(AutoWah
    COMPANY_NAME "BLS Audio"
    BUNDLE_ID "com.blsaudio.autowah"
    PLUGIN_MANUFACTURER_CODE Blsa
    PLUGIN_CODE Awah
    FORMATS VST3 Standalone
    PRODUCT_NAME "Auto-Wah"
    DESCRIPTION "Dual-source envelope/LFO auto-wah dynamic filter"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx")

target_sources(AutoWah PRIVATE ${AUTOWAH_SOURCES})
target_include_directories(AutoWah PRIVATE Source)
target_compile_definitions(AutoWah PUBLIC ${AUTOWAH_DEFINITIONS})
target_link_libraries(AutoWah
    PRIVATE
        AutoWahAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(AUTOWAH_BUILD_TESTS)
    enable_testing()

    juce_add_console_app(AutoWahTests PRODUCT_NAME "AutoWahTests")
    target_sources(AutoWahTests PRIVATE Tests/DspTests.cpp ${AUTOWAH_SOURCES})
    target_include_directories(AutoWahTests PRIVATE Source)
    target_compile_definitions(AutoWahTests PRIVATE ${AUTOWAH_DEFINITIONS})
    target_link_libraries(AutoWahTests
        PRIVATE
            AutoWahAssets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_test(NAME autowah_dsp COMMAND $<TARGET_FILE:AutoWahTests>)
endif()

if(AUTOWAH_BUILD_HOST_TEST)
    juce_add_console_app(AutoWahHostSmokeTest PRODUCT_NAME "AutoWahHostSmokeTest")

    target_sources(AutoWahHostSmokeTest PRIVATE Tools/HostSmokeTest.cpp)
    target_compile_definitions(AutoWahHostSmokeTest PRIVATE
        ${AUTOWAH_DEFINITIONS}
        JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(AutoWahHostSmokeTest
        PRIVATE
            juce::juce_audio_utils
            juce::juce_gui_extra
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)

    add_dependencies(AutoWahHostSmokeTest AutoWah_VST3)
endif()

if(AUTOWAH_BUILD_SCREENSHOT)
    juce_add_gui_app(AutoWahScreenshot
        PRODUCT_NAME "Auto-Wah Screenshot"
        COMPANY_NAME "BLS Audio"
        BUNDLE_ID "com.blsaudio.autowah.screenshot")

    target_sources(AutoWahScreenshot PRIVATE ${AUTOWAH_SOURCES} Tools/RenderEditor.cpp)
    target_include_directories(AutoWahScreenshot PRIVATE Source)
    target_compile_definitions(AutoWahScreenshot PRIVATE ${AUTOWAH_DEFINITIONS})
    target_link_libraries(AutoWahScreenshot
        PRIVATE
            AutoWahAssets
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
