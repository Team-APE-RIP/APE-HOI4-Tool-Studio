cmake_minimum_required(VERSION 3.16)

project(APEHOI4ToolStudio VERSION 1.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON) # For plugins to link to core

# 添加这一行，告诉编译器在 src 目录下查找头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 资源文件
set(TS_FILES
    resources/translations/zh_CN.ts
    resources/translations/zh_TW.ts
    resources/translations/en_US.ts
)

set(QRC_FILES
    resources/resources.qrc
)

# Core Library Sources (Shared between App and Tools)
set(CORE_SOURCES
    src/ConfigManager.cpp
    src/ConfigManager.h
    src/LocalizationManager.cpp
    src/LocalizationManager.h
    src/Logger.cpp
    src/Logger.h
    src/PathValidator.cpp
    src/PathValidator.h
    src/FileManager.cpp
    src/FileManager.h
    src/RecursiveFileSystemWatcher.cpp
    src/RecursiveFileSystemWatcher.h
    src/TagManager.cpp
    src/TagManager.h
    src/ToolInterface.h
    src/ToolIpcProtocol.h
    src/ToolProxyInterface.cpp
    src/ToolProxyInterface.h
    src/ToolManager.cpp
    src/ToolManager.h
)

# Create Core Library
add_library(APEHOI4Core SHARED ${CORE_SOURCES})
target_link_libraries(APEHOI4Core PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)
target_compile_definitions(APEHOI4Core PUBLIC APP_VERSION="${PROJECT_VERSION}")

# Main Application Sources (includes ToolHostMode for subprocess support)
set(APP_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SetupDialog.cpp
    src/SetupDialog.h
    src/CustomMessageBox.cpp
    src/CustomMessageBox.h
    src/SettingsPage.cpp
    src/SettingsPage.h
    src/ConfigPage.cpp
    src/ConfigPage.h
    src/ToolsPage.cpp
    src/ToolsPage.h
    src/ToolHostMode.cpp
    src/ToolHostMode.h
    src/LoadingOverlay.cpp
    src/LoadingOverlay.h
    ${QRC_FILES}
    ${TS_FILES}
)

if(WIN32)
    # 隐藏控制台窗口
    set(CMAKE_WIN32_EXECUTABLE ON)
    # 添加 Windows 图标资源
    list(APPEND APP_SOURCES resources/windows_icon.rc)
endif()

add_executable(APEHOI4ToolStudio ${APP_SOURCES})
target_link_libraries(APEHOI4ToolStudio PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network APEHOI4Core)

# --- FileManager Tool (Example Plugin) ---
set(FILEMANAGER_TOOL_SOURCES
    tools/FileManagerTool/FileManagerTool.cpp
    tools/FileManagerTool/FileManagerTool.h
    tools/FileManagerTool/FileTreeWidget.cpp
    tools/FileManagerTool/FileTreeWidget.h
)

add_library(FileManagerTool SHARED ${FILEMANAGER_TOOL_SOURCES})
target_link_libraries(FileManagerTool PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets APEHOI4Core)

# Output directory for tools
set_target_properties(FileManagerTool PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/FileManagerTool"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/FileManagerTool"
)

# Copy metadata.json to output directory
add_custom_command(TARGET FileManagerTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/metadata.json"
    "$<TARGET_FILE_DIR:FileManagerTool>/metadata.json"
)

# Copy localization directory
add_custom_command(TARGET FileManagerTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/localization"
    "$<TARGET_FILE_DIR:FileManagerTool>/localization"
)

# Copy cover image
add_custom_command(TARGET FileManagerTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/cover.png"
    "$<TARGET_FILE_DIR:FileManagerTool>/cover.png"
)

# --- FlagManager Tool ---
set(FLAGMANAGER_TOOL_SOURCES
    tools/FlagManagerTool/FlagManagerTool.cpp
    tools/FlagManagerTool/FlagManagerTool.h
)

add_library(FlagManagerTool SHARED ${FLAGMANAGER_TOOL_SOURCES})
target_link_libraries(FlagManagerTool PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets APEHOI4Core)

# Link Windows WIC libraries for DDS/WebP/JXR support
if(WIN32)
    target_link_libraries(FlagManagerTool PRIVATE windowscodecs ole32)
endif()

set_target_properties(FlagManagerTool PROPERTIES 
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/FlagManagerTool"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/FlagManagerTool"
)

add_custom_command(TARGET FlagManagerTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/metadata.json"
    "$<TARGET_FILE_DIR:FlagManagerTool>/metadata.json"
)

add_custom_command(TARGET FlagManagerTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/localization"
    "$<TARGET_FILE_DIR:FlagManagerTool>/localization"
)

# Copy cover image if exists (optional for now)
add_custom_command(TARGET FlagManagerTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/cover.png"
    "$<TARGET_FILE_DIR:FlagManagerTool>/cover.png"
)
