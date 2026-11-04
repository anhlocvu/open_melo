cmake_minimum_required(VERSION 3.22)
project(OpenMelo VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_set_ara_sdk_path("${CMAKE_CURRENT_SOURCE_DIR}/libs/ARA_SDK")

juce_add_plugin(OpenMelo
    COMPANY_NAME "Tech VST"
    BUNDLE_ID "com.techvst.openmelo"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Tvst
    PLUGIN_CODE Omlo
    FORMATS VST3
    PRODUCT_NAME "Open Melo"
    IS_ARA_EFFECT TRUE
)

target_sources(OpenMelo
    PRIVATE
        Source/NvdaBridge.h
        Source/NoteModel.h
        Source/PitchShiftEngine.h
        Source/PitchShiftEngine.cpp
        Source/AuditionVoice.h
        Source/NoteManager.h
        Source/NoteManager.cpp
        Source/OpenMeloARA.h
        Source/OpenMeloARA.cpp
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

target_include_directories(OpenMelo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/nvda
)

target_compile_definitions(OpenMelo
    PUBLIC
        NOMINMAX=1
        WIN32_LEAN_AND_MEAN=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(OpenMelo
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Post-build: Copy nvdaControllerClient64.dll to VST3 output folder
if(TARGET OpenMelo_VST3)
    add_custom_command(TARGET OpenMelo_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/libs/nvda/nvdaControllerClient64.dll"
        $<TARGET_FILE_DIR:OpenMelo_VST3>
    )
endif()
