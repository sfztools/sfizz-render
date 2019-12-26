cmake_minimum_required(VERSION 3.11)
project (sfizz-render VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
  sfizz
  GIT_REPOSITORY https://github.com/sfztools/sfizz.git
  GIT_TAG develop
)

FetchContent_GetProperties(sfizz)
if(NOT sfizz_POPULATED)
    FetchContent_Populate(sfizz)
    set (SFIZZ_JACK OFF CACHE BOOL "Disable sfizz's JACK client")
    set (SFIZZ_LV2 OFF CACHE BOOL "Disable sfizz's LV2 plugin")
    set (SFIZZ_SHARED OFF CACHE BOOL "Disable building sfizz as a shared lib")
    add_subdirectory(${sfizz_SOURCE_DIR} ${sfizz_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

FetchContent_Declare(
  midifile
  GIT_REPOSITORY https://github.com/craigsapp/midifile.git
)

FetchContent_GetProperties(midifile)
if(NOT midifile_POPULATED)
    FetchContent_Populate(midifile)
    set(midifile_SOURCES
        ${midifile_SOURCE_DIR}/src/Options.cpp
        ${midifile_SOURCE_DIR}/src/Binasc.cpp
        ${midifile_SOURCE_DIR}/src/MidiEvent.cpp
        ${midifile_SOURCE_DIR}/src/MidiEventList.cpp
        ${midifile_SOURCE_DIR}/src/MidiFile.cpp
        ${midifile_SOURCE_DIR}/src/MidiMessage.cpp
    )

    add_library(midifile STATIC ${midifile_SOURCES})
    target_include_directories(midifile PUBLIC ${midifile_SOURCE_DIR}/include)
endif()

FetchContent_Declare(
    ghc_filesystem
    GIT_REPOSITORY https://github.com/gulrak/filesystem.git
)

FetchContent_GetProperties(ghc_filesystem)
if(NOT ghc_filesystem_POPULATED)
    FetchContent_Populate(ghc_filesystem)
    add_subdirectory(${ghc_filesystem_SOURCE_DIR} ${ghc_filesystem_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_executable(sfizz-render main.cpp)
target_link_libraries(sfizz-render 
    sndfile
    sfizz::sfizz
    midifile
    absl::flags_usage
    absl::flags_parse
    ghc_filesystem
)

if (UNIX)
    include (GNUInstallDirs)
    install (TARGETS sfizz-render RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endif()