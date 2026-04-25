cmake_minimum_required(VERSION 3.22)

project(Flow88Master VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

if(DEFINED JUCE_DIR)
    set(FLOW88_JUCE_DIR "${JUCE_DIR}")
elseif(DEFINED ENV{JUCE_DIR})
    set(FLOW88_JUCE_DIR "$ENV{JUCE_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    set(FLOW88_JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
else()
    message(FATAL_ERROR
        "JUCE not found.\n"
        "Provide JUCE using one of these options:\n"
        "  1. Populate the repo-local JUCE submodule\n"
        "  2. Configure with -DJUCE_DIR=<path-to-JUCE>\n"
        "  3. Set the JUCE_DIR environment variable"
    )
endif()

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory("${FLOW88_JUCE_DIR}" JUCE)

juce_add_binary_data(Flow88MasterAssets
    SOURCES
        Assets/Profiles/Subgenres/house.json
        Assets/Profiles/Subgenres/techno.json
        Assets/Profiles/Subgenres/melodic.json
        Assets/Profiles/Subgenres/trance.json
        Assets/Profiles/Subgenres/progressive.json
        Assets/Profiles/Styles/clean.json
        Assets/Profiles/Styles/club.json
        Assets/Profiles/Styles/warm.json
)

add_library(Flow88MasterCore STATIC
    Source/flow88/state/FlowParameterIDs.h
    Source/flow88/state/FlowParameterLayout.h
    Source/flow88/state/FlowParameterLayout.cpp
    Source/flow88/state/FlowProfiles.h
    Source/flow88/state/FlowProfiles.cpp
    Source/flow88/state/FlowSessionState.h
    Source/flow88/state/FlowSessionState.cpp
    Source/flow88/state/FlowStateSerializer.h
    Source/flow88/state/FlowStateSerializer.cpp
    Source/flow88/dsp/FlowMasteringChain.h
    Source/flow88/dsp/FlowMasteringChain.cpp
    Source/flow88/dsp/FlowMeterSource.h
    Source/flow88/dsp/FlowMeterSource.cpp
    Source/flow88/eval/FlowOfflineAnalysis.h
    Source/flow88/eval/FlowOfflineAnalysis.cpp
)

target_include_directories(Flow88MasterCore PUBLIC Source)

target_link_libraries(Flow88MasterCore
    PUBLIC
        Flow88MasterAssets
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_dsp
)

target_compile_definitions(Flow88MasterCore
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

add_library(Flow88MasterUI STATIC
    Source/flow88/ui/FlowTheme.h
    Source/flow88/ui/FlowLookAndFeel.h
    Source/flow88/ui/FlowLookAndFeel.cpp
    Source/flow88/ui/FlowWidgets.h
    Source/flow88/ui/FlowWidgets.cpp
)

target_include_directories(Flow88MasterUI PUBLIC Source)

target_link_libraries(Flow88MasterUI
    PUBLIC
        Flow88MasterCore
    PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_gui_extra
)

target_compile_definitions(Flow88MasterUI
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

juce_add_plugin(Flow88Master
    COMPANY_NAME "Flow88 Audio"
    PRODUCT_NAME "Flow88 Master"
    BUNDLE_ID "com.flow88audio.flow88master"
    PLUGIN_MANUFACTURER_CODE F88A
    PLUGIN_CODE F8MT
    IS_SYNTH FALSE
    IS_MIDI_EFFECT FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    AUDIO_INPUT_CHANNELS 2
    AUDIO_OUTPUT_CHANNELS 2
    FORMATS VST3
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Flow88Master)

target_sources(Flow88Master
    PRIVATE
        Source/PluginProcessor.h
        Source/PluginProcessor.cpp
        Source/PluginEditor.h
        Source/PluginEditor.cpp
)

target_link_libraries(Flow88Master
    PRIVATE
        Flow88MasterCore
        Flow88MasterUI
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(Flow88Master
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

add_library(Flow88MasterValidation STATIC
    Source/flow88/eval/FlowPresetEvaluation.h
    Source/flow88/eval/FlowPresetEvaluation.cpp
)

target_include_directories(Flow88MasterValidation PUBLIC Source)

target_link_libraries(Flow88MasterValidation
    PUBLIC
        Flow88Master
        Flow88MasterCore
        juce::juce_audio_formats
        juce::juce_core
)

target_compile_definitions(Flow88MasterValidation
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

juce_add_console_app(Flow88MasterEval PRODUCT_NAME "Flow88 Master Eval")

target_sources(Flow88MasterEval
    PRIVATE
        Source/Tools/Flow88MasterEval.cpp
)

target_link_libraries(Flow88MasterEval
    PRIVATE
        Flow88MasterValidation
        Flow88Master
        Flow88MasterCore
        juce::juce_audio_formats
        juce::juce_core
)

target_compile_definitions(Flow88MasterEval
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

if(BUILD_TESTING)
    juce_add_console_app(Flow88MasterTests PRODUCT_NAME "Flow88 Master Tests")

    target_sources(Flow88MasterTests
        PRIVATE
            Tests/Flow88MasterTests.cpp
    )

    target_link_libraries(Flow88MasterTests
        PRIVATE
            Flow88Master
            Flow88MasterValidation
            Flow88MasterCore
            juce::juce_audio_basics
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_core
    )

    target_compile_definitions(Flow88MasterTests
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    add_test(NAME Flow88MasterTests COMMAND Flow88MasterTests)
endif()
