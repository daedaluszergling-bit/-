cmake_minimum_required(VERSION 3.22)
project(ZLabAnA VERSION 0.10.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(AnAData SOURCES
    Assets/FrederickatheGreat-Regular.ttf
    Assets/BarlowCondensed-Bold.ttf
    Assets/zlab_logo.png)

option(ANA_COPY_AFTER_BUILD "Copy plugin to system VST3 folder after build" ON)

juce_add_plugin(AnA
    COMPANY_NAME                "DIYAudio"
    PLUGIN_MANUFACTURER_CODE    Diya
    PLUGIN_CODE                 ZanA
    FORMATS                     VST3
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     ${ANA_COPY_AFTER_BUILD}
    VST3_CATEGORIES             "Fx" "Reverb"
    PRODUCT_NAME                "Z Lab AnA")

target_sources(AnA PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(AnA PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AnA
    PRIVATE AnAData juce::juce_audio_utils juce::juce_dsp juce::juce_opengl
    PUBLIC  juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
