cmake_minimum_required(VERSION 3.21)
project(PexelManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6 modules
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Multimedia
    MultimediaWidgets
)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/settings.cpp
    src/pexelsapi.cpp
    src/medialistwidget.cpp
    src/videoplayerwidget.cpp
    src/projectmanager.cpp
    src/downloadmanager.cpp
    src/uploadmanager.cpp
)

set(HEADERS
    src/mainwindow.h
    src/settings.h
    src/pexelsapi.h
    src/medialistwidget.h
    src/videoplayerwidget.h
    src/projectmanager.h
    src/downloadmanager.h
    src/uploadmanager.h
    src/mediametadata.h
)

# Main executable
qt_add_executable(PexelManager
    ${SOURCES}
    ${HEADERS}
)

# Link libraries
target_link_libraries(PexelManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# Include directories
target_include_directories(PexelManager PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows-specific: copy Qt DLLs for running
if(WIN32)
    # Set the output directory
    set_target_properties(PexelManager PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
