cmake_minimum_required(VERSION 3.22)

# Version is <base>.<build number>. CI passes its run number as the build, so every release is newer than the last.
set(MUM_VERSION_BASE "0.1")
set(MUM_BUILD_NUMBER "0" CACHE STRING "Build number (CI run number); 0 for local builds")
set(MUM_UPDATE_BASE_URL "https://github.com/YbwSolo/MusicallyUnorthodox-MusicMaker/releases/latest/download" CACHE STRING "Where the plugin looks for latest.json")
project(MUMusicMaker VERSION ${MUM_VERSION_BASE}.${MUM_BUILD_NUMBER} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

option(MUM_BUILD_TESTS "Build the test runner and the screenshot tool" ON)
option(MUM_COPY_AFTER_BUILD "Copy the built plugin into the system plugin folders" OFF)

if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(MUMAssets SOURCES
    Assets/mu_logo.png
    Assets/styles.txt
    Assets/fonts/Anton-Regular.ttf
    Assets/fonts/Inter-Regular.ttf
    Assets/fonts/Inter-SemiBold.ttf
    engine/install_engine_mac.sh
    engine/start_engine_mac.sh
    engine/stop_engine_mac.sh
    engine/install_engine_windows.bat
    engine/start_engine_windows.bat
    engine/stop_engine_windows.bat)
set_target_properties(MUMAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(UNIX AND NOT APPLE)
    set(MUM_NEEDS_CURL TRUE)     # Linux is only used for testing; there JUCE talks to the internet through libcurl
else()
    set(MUM_NEEDS_CURL FALSE)
endif()

juce_add_plugin(MUMusicMaker
    COMPANY_NAME "Musically Unorthodox"
    COMPANY_WEBSITE "https://musicallyunorthodox.com"
    BUNDLE_ID "com.musicallyunorthodox.musicmaker"
    PLUGIN_MANUFACTURER_CODE Muno
    PLUGIN_CODE Mumm
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "MU Music Maker"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    NEEDS_CURL ${MUM_NEEDS_CURL}
    COPY_PLUGIN_AFTER_BUILD ${MUM_COPY_AFTER_BUILD}
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Generator Tools
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "MU Music Maker can record audio to use as the starting point for a song."
    HARDENED_RUNTIME_ENABLED TRUE)

set(MUM_CORE_SOURCES
    Source/core/Settings.cpp
    Source/core/Http.cpp
    Source/core/Engine.cpp
    Source/core/Online.cpp
    Source/core/EngineRunner.cpp
    Source/core/Library.cpp
    Source/core/Generator.cpp
    Source/core/Player.cpp
    Source/core/Recorder.cpp
    Source/core/Updater.cpp
    Source/ui/Theme.cpp)

target_sources(MUMusicMaker PRIVATE
    ${MUM_CORE_SOURCES}
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/SongList.cpp
    Source/ui/PlayerBar.cpp
    Source/ui/CreatePage.cpp
    Source/ui/LibraryPage.cpp
    Source/ui/SettingsPage.cpp
    Source/ui/HelpPage.cpp)

target_include_directories(MUMusicMaker PRIVATE Source)

target_compile_definitions(MUMusicMaker PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=$<IF:$<BOOL:${MUM_NEEDS_CURL}>,1,0>
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_MP3AUDIOFORMAT=1
    MUM_BUILD_NUMBER=${MUM_BUILD_NUMBER}
    MUM_VERSION_STRING="${PROJECT_VERSION}"
    MUM_UPDATE_BASE_URL="${MUM_UPDATE_BASE_URL}")

target_link_libraries(MUMusicMaker PRIVATE
    MUMAssets
    juce::juce_audio_utils
    juce::juce_cryptography
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

if(MUM_BUILD_TESTS)
    # Checks everything that does not need a DAW: talking to the engine (against a stand-in), the song library,
    # the queue, catching audio from the track, the player and the updater.
    juce_add_console_app(MUMTests PRODUCT_NAME "MUMTests")
    target_sources(MUMTests PRIVATE Source/tests/CoreTests.cpp)
    target_include_directories(MUMTests PRIVATE Source)
    target_link_libraries(MUMTests PRIVATE MUMusicMaker MUMAssets juce::juce_audio_utils juce::juce_cryptography)
    target_compile_definitions(MUMTests PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
        MUM_TESTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Source/tests")

    # Draws each page of the plugin to a PNG, so the look can be checked without opening a DAW.
    juce_add_gui_app(MUMPreview PRODUCT_NAME "MUMPreview")
    target_sources(MUMPreview PRIVATE Source/tests/Preview.cpp)
    target_include_directories(MUMPreview PRIVATE Source)
    target_link_libraries(MUMPreview PRIVATE MUMusicMaker MUMAssets juce::juce_audio_utils juce::juce_cryptography)
    target_compile_definitions(MUMPreview PRIVATE JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
endif()
