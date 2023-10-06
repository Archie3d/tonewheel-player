set(TARGET "tonewheel_player")

juce_add_plugin(${TARGET}
    PLUGIN_MANUFACTURER_CODE Arbe
    PLUGIN_CODE TwPl
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Tonewheel Player"
    COMPANY_NAME "Arthur Benilov"
    BUNDLE_ID "com.ab.tonewheel_player"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(${TARGET})

set(SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineProxy.h
    ${CMAKE_CURRENT_SOURCE_DIR}/EngineProxy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginConsole.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginConsole.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp
)

target_sources(${TARGET} PUBLIC ${SRC})

target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE res "${CMAKE_CURRENT_SOURCE_DIR}/resources/*.*")
juce_add_binary_data(${TARGET}_res SOURCES ${res})
target_link_libraries(${TARGET} PUBLIC ${TARGET}_res)

target_compile_definitions(${TARGET}
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_MODAL_LOOPS_PERMITTED=1
)

if(MSVC)
    target_compile_options(${TARGET} PUBLIC "/wd4100") # unused formal parameter
    target_compile_options(${TARGET} PUBLIC "/wd4459") # local variable eclipses class member
endif()

target_link_libraries(${TARGET}
    PRIVATE
        juce::juce_core
        juce::juce_data_structures
        juce::juce_audio_basics
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_opengl

        juce::vitro
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_link_libraries(${TARGET}
    PRIVATE
        tonewheel
        ScriptX
)

juce_generate_juce_header(${TARGET})
