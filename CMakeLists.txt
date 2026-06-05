cmake_minimum_required(VERSION 3.16)

project(APEHOI4ToolStudio VERSION 2.3.0 LANGUAGES CXX)

set(APP_VERSION_STRING "2.3.0")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Concurrent
    Qml
    Quick
    QuickControls2
    QuickWidgets
)

set(APE_CURL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/libs/curl")
set(APE_CURL_INCLUDE_DIR "${APE_CURL_ROOT}/include")
set(APE_CURL_IMPORT_LIB "${APE_CURL_ROOT}/lib/libcurl.dll.a")
set(APE_CURL_BIN_DIR "${APE_CURL_ROOT}/bin")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(TS_FILES
    resources/translations/zh_CN.ts
    resources/translations/zh_TW.ts
    resources/translations/en_US.ts
    resources/translations/ru_RU.ts
)

set(QRC_FILES
    resources/resources.qrc
)

set(APE_COMPANY_NAME "Team APE:RIP")
set(APE_PRODUCT_NAME "APE HOI4 Tool Studio")
set(APE_COPYRIGHT "Copyright © 2026 Team APE:RIP")

set(APE_VERSION_RC_DIR "${CMAKE_CURRENT_BINARY_DIR}/versioninfo")
file(MAKE_DIRECTORY "${APE_VERSION_RC_DIR}")

function(ape_extract_descriptor_value descriptor_path key out_var)
    if(NOT EXISTS "${descriptor_path}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    file(READ "${descriptor_path}" descriptor_content)
    string(REGEX MATCH "${key}[ \t]*=[ \t]*\"([^\"]+)\"" descriptor_match "${descriptor_content}")

    if(CMAKE_MATCH_1)
        set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

function(ape_extract_json_string json_path key out_var)
    if(NOT EXISTS "${json_path}")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    file(READ "${json_path}" json_content)
    string(REGEX MATCH "\"${key}\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" json_match "${json_content}")

    if(CMAKE_MATCH_1)
        set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

function(ape_version_string_to_components version_string out_numeric out_string)
    if("${version_string}" STREQUAL "")
        set(version_string "0.0.0")
    endif()

    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" version_match "${version_string}")
    if(CMAKE_MATCH_1)
        set(version_numeric "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},0")
        set(version_text "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}.0")
    else()
        set(version_numeric "0,0,0,0")
        set(version_text "${version_string}")
    endif()

    set(${out_numeric} "${version_numeric}" PARENT_SCOPE)
    set(${out_string} "${version_text}" PARENT_SCOPE)
endfunction()

function(ape_parse_four_part_version_string version_string out_numeric out_string)
    if("${version_string}" STREQUAL "")
        set(${out_numeric} "0,0,0,0" PARENT_SCOPE)
        set(${out_string} "0.0.0.0" PARENT_SCOPE)
        return()
    endif()

    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)$" version_match "${version_string}")
    if(CMAKE_MATCH_1)
        set(version_numeric "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},${CMAKE_MATCH_4}")
        set(version_text "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}.${CMAKE_MATCH_4}")
    else()
        set(version_numeric "0,0,0,0")
        set(version_text "0.0.0.0")
    endif()

    set(${out_numeric} "${version_numeric}" PARENT_SCOPE)
    set(${out_string} "${version_text}" PARENT_SCOPE)
endfunction()

function(ape_app_version_string_to_components version_string out_numeric out_string)
    if("${version_string}" STREQUAL "")
        set(${out_numeric} "0,0,0,0" PARENT_SCOPE)
        set(${out_string} "0.0.0.0" PARENT_SCOPE)
        return()
    endif()

    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" version_match "${version_string}")
    if(CMAKE_MATCH_1)
        set(version_major "${CMAKE_MATCH_1}")
        set(version_minor "${CMAKE_MATCH_2}")
        set(version_channel_number "${CMAKE_MATCH_3}")
        set(version_revision 0)
        set(version_numeric "${version_major},${version_minor},${version_channel_number},${version_revision}")
        set(version_text "${version_major}.${version_minor}.${version_channel_number}.${version_revision}")
    else()
        string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.(stable|beta|alpha)-([0-9]+)$" version_match "${version_string}")
        if(CMAKE_MATCH_1)
            set(version_major "${CMAKE_MATCH_1}")
            set(version_minor "${CMAKE_MATCH_2}")
            set(version_channel "${CMAKE_MATCH_3}")
            set(version_revision "${CMAKE_MATCH_4}")

            if(version_channel STREQUAL "stable")
                set(version_channel_number 1)
            elseif(version_channel STREQUAL "beta")
                set(version_channel_number 2)
            elseif(version_channel STREQUAL "alpha")
                set(version_channel_number 3)
            else()
                set(version_channel_number 0)
            endif()

            set(version_numeric "${version_major},${version_minor},${version_channel_number},${version_revision}")
            set(version_text "${version_major}.${version_minor}.${version_channel_number}.${version_revision}")
        else()
            set(version_numeric "0,0,0,0")
            set(version_text "0.0.0.0")
        endif()
    endif()

    set(${out_numeric} "${version_numeric}" PARENT_SCOPE)
    set(${out_string} "${version_text}" PARENT_SCOPE)
endfunction()

function(ape_compute_file_hash_revision file_path out_var)
    if(NOT EXISTS "${file_path}")
        set(${out_var} "0" PARENT_SCOPE)
        return()
    endif()

    file(SHA256 "${file_path}" file_hash)
    string(SUBSTRING "${file_hash}" 0 4 file_hash_prefix)
    math(EXPR file_hash_revision "0x${file_hash_prefix} % 65535")
    set(${out_var} "${file_hash_revision}" PARENT_SCOPE)
endfunction()

function(ape_compose_version_with_revision base_version revision out_numeric out_string)
    if("${base_version}" STREQUAL "")
        set(base_version "0.0.0")
    endif()

    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$" version_match "${base_version}")
    if(CMAKE_MATCH_1)
        set(version_numeric "${CMAKE_MATCH_1},${CMAKE_MATCH_2},${CMAKE_MATCH_3},${revision}")
        set(version_text "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}.${CMAKE_MATCH_3}.${revision}")
    else()
        set(version_numeric "0,0,0,${revision}")
        set(version_text "0.0.0.${revision}")
    endif()

    set(${out_numeric} "${version_numeric}" PARENT_SCOPE)
    set(${out_string} "${version_text}" PARENT_SCOPE)
endfunction()

function(ape_configure_version_resource_explicit
    target_name
    file_description
    original_filename
    internal_name
    file_type
    numeric_version
    file_version_string
    product_version_string)
    if(NOT WIN32)
        return()
    endif()

    set(APE_RC_COMPANY "${APE_COMPANY_NAME}")
    set(APE_RC_PRODUCT_NAME "${APE_PRODUCT_NAME}")
    set(APE_RC_COPYRIGHT "${APE_COPYRIGHT}")
    set(APE_RC_FILE_DESCRIPTION "${file_description}")
    set(APE_RC_ORIGINAL_FILENAME "${original_filename}")
    set(APE_RC_INTERNAL_NAME "${internal_name}")
    set(APE_RC_FILE_TYPE "${file_type}")
    set(APE_RC_FILE_VERSION "${numeric_version}")
    set(APE_RC_PRODUCT_VERSION "${numeric_version}")
    set(APE_RC_FILE_VERSION_STR "${file_version_string}")
    set(APE_RC_PRODUCT_VERSION_STR "${product_version_string}")

    set(version_rc_output "${APE_VERSION_RC_DIR}/${target_name}_version.rc")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/resources/versioninfo.rc.in"
        "${version_rc_output}"
        @ONLY
    )

    target_sources(${target_name} PRIVATE "${version_rc_output}")
endfunction()

function(ape_configure_version_resource target_name file_description original_filename internal_name file_type version_string)
    if(NOT WIN32)
        return()
    endif()

    ape_version_string_to_components("${version_string}" ape_version_numeric ape_version_text)
    ape_configure_version_resource_explicit(
        ${target_name}
        "${file_description}"
        "${original_filename}"
        "${internal_name}"
        "${file_type}"
        "${ape_version_numeric}"
        "${ape_version_text}"
        "${version_string}"
    )
endfunction()

function(ape_configure_app_version_resource target_name file_description original_filename internal_name file_type version_string)
    if(NOT WIN32)
        return()
    endif()

    ape_app_version_string_to_components("${version_string}" ape_version_numeric ape_version_text)
    ape_configure_version_resource_explicit(
        ${target_name}
        "${file_description}"
        "${original_filename}"
        "${internal_name}"
        "${file_type}"
        "${ape_version_numeric}"
        "${ape_version_text}"
        "${version_string}"
    )
endfunction()

set(APE_LIBRARY_VERSION_JSON "${CMAKE_CURRENT_SOURCE_DIR}/resources/library_versions.json")
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "core" ape_core_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "tool" ape_tool_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "plugin" ape_plugin_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "logger" ape_logger_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "config" ape_config_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "localization" ape_localization_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "messageBox" ape_message_box_version_base)
ape_extract_json_string("${APE_LIBRARY_VERSION_JSON}" "file" ape_file_version_base)

set(APE_CORE_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.core.dll")
set(APE_TOOL_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.tool.dll")
set(APE_PLUGIN_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.plugin.dll")
set(APE_LOGGER_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.logger.dll")
set(APE_CONFIG_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.config.dll")
set(APE_LOCALIZATION_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.localization.dll")
set(APE_MESSAGE_BOX_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.messageBox.dll")
set(APE_FILE_VERSION_REVISION "0" CACHE STRING "Revision number for APEHOI4ToolStudio.file.dll")

ape_compose_version_with_revision("${ape_core_version_base}" "${APE_CORE_VERSION_REVISION}" ape_core_version_numeric_text ape_core_version_file_string)
ape_compose_version_with_revision("${ape_tool_version_base}" "${APE_TOOL_VERSION_REVISION}" ape_tool_version_numeric_text ape_tool_version_file_string)
ape_compose_version_with_revision("${ape_plugin_version_base}" "${APE_PLUGIN_VERSION_REVISION}" ape_plugin_version_numeric_text ape_plugin_version_file_string)
ape_compose_version_with_revision("${ape_logger_version_base}" "${APE_LOGGER_VERSION_REVISION}" ape_logger_version_numeric_text ape_logger_version_file_string)
ape_compose_version_with_revision("${ape_config_version_base}" "${APE_CONFIG_VERSION_REVISION}" ape_config_version_numeric_text ape_config_version_file_string)
ape_compose_version_with_revision("${ape_localization_version_base}" "${APE_LOCALIZATION_VERSION_REVISION}" ape_localization_version_numeric_text ape_localization_version_file_string)
ape_compose_version_with_revision("${ape_message_box_version_base}" "${APE_MESSAGE_BOX_VERSION_REVISION}" ape_message_box_version_numeric_text ape_message_box_version_file_string)
ape_compose_version_with_revision("${ape_file_version_base}" "${APE_FILE_VERSION_REVISION}" ape_file_version_numeric_text ape_file_version_file_string)

ape_parse_four_part_version_string("${ape_core_version_file_string}" ape_core_version_numeric_seed ape_core_version_file_string)
ape_parse_four_part_version_string("${ape_tool_version_file_string}" ape_tool_version_numeric_seed ape_tool_version_file_string)
ape_parse_four_part_version_string("${ape_plugin_version_file_string}" ape_plugin_version_numeric_seed ape_plugin_version_file_string)
ape_parse_four_part_version_string("${ape_logger_version_file_string}" ape_logger_version_numeric_seed ape_logger_version_file_string)
ape_parse_four_part_version_string("${ape_config_version_file_string}" ape_config_version_numeric_seed ape_config_version_file_string)
ape_parse_four_part_version_string("${ape_localization_version_file_string}" ape_localization_version_numeric_seed ape_localization_version_file_string)
ape_parse_four_part_version_string("${ape_message_box_version_file_string}" ape_message_box_version_numeric_seed ape_message_box_version_file_string)
ape_parse_four_part_version_string("${ape_file_version_file_string}" ape_file_version_numeric_seed ape_file_version_file_string)

function(ape_apply_binary_name target_name output_name)
    set_target_properties(${target_name} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${output_name}"
    )
endfunction()

function(ape_copy_optional_file target_name source_path destination_name)
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${source_path}" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:${target_name}>" -DAPE_DEST_NAME="${destination_name}" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_file.cmake"
    )
endfunction()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_file.cmake" [=[
if(EXISTS "${APE_SOURCE}")
    file(COPY "${APE_SOURCE}" DESTINATION "${APE_DEST_DIR}")
    if(NOT "${APE_DEST_NAME}" STREQUAL "")
        get_filename_component(APE_SOURCE_NAME "${APE_SOURCE}" NAME)
        if(NOT "${APE_SOURCE_NAME}" STREQUAL "${APE_DEST_NAME}")
            file(RENAME "${APE_DEST_DIR}/${APE_SOURCE_NAME}" "${APE_DEST_DIR}/${APE_DEST_NAME}")
        endif()
    endif()
endif()
]=])

set(APE_LOGGER_SOURCES
    src/Logger.cpp
    src/Logger.h
)

set(APE_CONFIG_SOURCES
    src/ConfigManager.cpp
    src/ConfigManager.h
    src/GameLanguageCatalog.cpp
    src/GameLanguageCatalog.h
    src/PackageRegistry.cpp
    src/PackageRegistry.h
)

set(APE_LOCALIZATION_SOURCES
    src/LocalizationManager.cpp
    src/LocalizationManager.h
)

set(APE_MESSAGE_BOX_SOURCES
    src/CustomMessageBox.cpp
    src/CustomMessageBox.h
    src/OverlayAcrylicMaterial.cpp
    src/OverlayAcrylicMaterial.h
)

set(APE_FILE_SOURCES
    src/GamePathDiscovery.cpp
    src/GamePathDiscovery.h
    src/PathValidator.cpp
    src/PathValidator.h
    src/FileManager.cpp
    src/FileManager.h
    src/RecursiveFileSystemWatcher.cpp
    src/RecursiveFileSystemWatcher.h
)

set(APE_CORE_SOURCES
    src/ApiRequests.cpp
    src/ApiRequests.h
    src/AuthManager.cpp
    src/AuthManager.h
    src/HttpClient.cpp
    src/HttpClient.h
    src/SslConfig.cpp
    src/SslConfig.h
    resources/core_resources.qrc
)

set(APE_PLUGIN_SOURCES
    src/PluginAbi.h
    src/PluginAbiBroker.cpp
    src/PluginAbiBroker.h
    src/PluginDescriptorParser.cpp
    src/PluginDescriptorParser.h
    src/PluginManager.cpp
    src/PluginManager.h
    src/PluginRuntimeContext.cpp
    src/PluginRuntimeContext.h
)

set(APE_TOOL_SOURCES
    src/AcrylicCudaProcessor.cpp
    src/AcrylicCudaProcessor.h
    src/AcrylicScreenCapture.cpp
    src/AcrylicScreenCapture.h
    src/PluginAbi.h
    src/ToolInterface.h
    src/ToolIpcProtocol.h
    src/ToolDescriptorParser.cpp
    src/ToolDescriptorParser.h
    src/ToolGuiRuntime.h
    src/ToolQmlBridge.cpp
    src/ToolQmlBridge.h
    src/ToolQmlHostComponents.cpp
    src/ToolQmlHostComponents.h
    src/ToolQmlHostController.cpp
    src/ToolQmlHostController.h
    src/ToolQmlThemeProvider.cpp
    src/ToolQmlThemeProvider.h
    src/ToolScriptedHostController.cpp
    src/ToolScriptedHostController.h
    src/ToolProxyInterface.cpp
    src/ToolProxyInterface.h
    src/ToolRuntimeContext.cpp
    src/ToolRuntimeContext.h
    src/ToolManager.cpp
    src/ToolManager.h
    src/ToolGuiModelAdapter.cpp
    src/ToolGuiModelAdapter.h
    src/ToolUiContainer.cpp
    src/ToolUiContainer.h
)

add_library(APEHTSLogger SHARED ${APE_LOGGER_SOURCES})
target_link_libraries(APEHTSLogger PRIVATE Qt6::Core Qt6::Gui)
target_compile_definitions(APEHTSLogger PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSLogger "APEHOI4ToolStudio.logger")
ape_configure_version_resource_explicit(
    APEHTSLogger
    "APE HOI4 Tool Studio Logger Library"
    "APEHOI4ToolStudio.logger.dll"
    "APEHOI4ToolStudio.logger"
    "0x2L"
    "${ape_logger_version_numeric_seed}"
    "${ape_logger_version_file_string}"
    "${ape_logger_version_base}"
)

add_library(APEHTSConfig SHARED ${APE_CONFIG_SOURCES})
target_link_libraries(APEHTSConfig PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets APEHTSLogger)
target_compile_definitions(APEHTSConfig PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSConfig "APEHOI4ToolStudio.config")
ape_configure_version_resource_explicit(
    APEHTSConfig
    "APE HOI4 Tool Studio Config Library"
    "APEHOI4ToolStudio.config.dll"
    "APEHOI4ToolStudio.config"
    "0x2L"
    "${ape_config_version_numeric_seed}"
    "${ape_config_version_file_string}"
    "${ape_config_version_base}"
)

add_library(APEHTSLocalization SHARED ${APE_LOCALIZATION_SOURCES})
target_link_libraries(APEHTSLocalization PRIVATE Qt6::Core)
target_compile_definitions(APEHTSLocalization PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSLocalization "APEHOI4ToolStudio.localization")
ape_configure_version_resource_explicit(
    APEHTSLocalization
    "APE HOI4 Tool Studio Localization Library"
    "APEHOI4ToolStudio.localization.dll"
    "APEHOI4ToolStudio.localization"
    "0x2L"
    "${ape_localization_version_numeric_seed}"
    "${ape_localization_version_file_string}"
    "${ape_localization_version_base}"
)

add_library(APEHTSMessageBox SHARED ${APE_MESSAGE_BOX_SOURCES})
target_link_libraries(APEHTSMessageBox PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets APEHTSConfig APEHTSLocalization)
target_compile_definitions(APEHTSMessageBox PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSMessageBox "APEHOI4ToolStudio.messageBox")
ape_configure_version_resource_explicit(
    APEHTSMessageBox
    "APE HOI4 Tool Studio Message Box Library"
    "APEHOI4ToolStudio.messageBox.dll"
    "APEHOI4ToolStudio.messageBox"
    "0x2L"
    "${ape_message_box_version_numeric_seed}"
    "${ape_message_box_version_file_string}"
    "${ape_message_box_version_base}"
)

add_library(APEHTSFile SHARED ${APE_FILE_SOURCES})
target_link_libraries(APEHTSFile PRIVATE Qt6::Core Qt6::Concurrent APEHTSConfig APEHTSLogger)
target_compile_definitions(APEHTSFile PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSFile "APEHOI4ToolStudio.file")
ape_configure_version_resource_explicit(
    APEHTSFile
    "APE HOI4 Tool Studio File Library"
    "APEHOI4ToolStudio.file.dll"
    "APEHOI4ToolStudio.file"
    "0x2L"
    "${ape_file_version_numeric_seed}"
    "${ape_file_version_file_string}"
    "${ape_file_version_base}"
)

add_library(APEHTSCore SHARED ${APE_CORE_SOURCES})
target_link_libraries(APEHTSCore PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network "${APE_CURL_IMPORT_LIB}" APEHTSLogger APEHTSLocalization)
target_compile_definitions(APEHTSCore PUBLIC APP_VERSION="${APP_VERSION_STRING}" PRIVATE APE_USE_LIBCURL)
target_include_directories(APEHTSCore PUBLIC
    ${APE_CURL_INCLUDE_DIR}
)
ape_apply_binary_name(APEHTSCore "APEHOI4ToolStudio.core")
ape_configure_version_resource_explicit(
    APEHTSCore
    "APE HOI4 Tool Studio Core Library"
    "APEHOI4ToolStudio.core.dll"
    "APEHOI4ToolStudio.core"
    "0x2L"
    "${ape_core_version_numeric_seed}"
    "${ape_core_version_file_string}"
    "${ape_core_version_base}"
)

add_library(APEHTSPlugin SHARED ${APE_PLUGIN_SOURCES})
target_link_libraries(APEHTSPlugin PRIVATE Qt6::Core APEHTSLogger APEHTSConfig)
target_compile_definitions(APEHTSPlugin PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSPlugin "APEHOI4ToolStudio.plugin")
ape_configure_version_resource_explicit(
    APEHTSPlugin
    "APE HOI4 Tool Studio Plugin Library"
    "APEHOI4ToolStudio.plugin.dll"
    "APEHOI4ToolStudio.plugin"
    "0x2L"
    "${ape_plugin_version_numeric_seed}"
    "${ape_plugin_version_file_string}"
    "${ape_plugin_version_base}"
)

add_library(APEHTSTool SHARED ${APE_TOOL_SOURCES})
target_link_libraries(APEHTSTool PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickWidgets
    APEHTSLogger
    APEHTSConfig
    APEHTSLocalization
    APEHTSFile
    APEHTSPlugin
)
target_compile_definitions(APEHTSTool PUBLIC APP_VERSION="${APP_VERSION_STRING}")
ape_apply_binary_name(APEHTSTool "APEHOI4ToolStudio.tool")
ape_configure_version_resource_explicit(
    APEHTSTool
    "APE HOI4 Tool Studio Tool Library"
    "APEHOI4ToolStudio.tool.dll"
    "APEHOI4ToolStudio.tool"
    "0x2L"
    "${ape_tool_version_numeric_seed}"
    "${ape_tool_version_file_string}"
    "${ape_tool_version_base}"
)

if(WIN32)
    target_link_libraries(APEHTSCore PRIVATE windowscodecs ole32)
    target_link_libraries(APEHTSPlugin PRIVATE windowscodecs ole32)
    target_link_libraries(APEHTSTool PRIVATE windowscodecs ole32 d3d11 dxgi)
    target_compile_definitions(APEHTSLogger PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSConfig PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSLocalization PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSMessageBox PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSFile PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSCore PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSPlugin PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_definitions(APEHTSTool PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

set(UPDATER_SOURCES
    updater/main.cpp
)
if(WIN32)
    list(APPEND UPDATER_SOURCES resources/windows_icon_updater.rc)
endif()

add_executable(Updater WIN32 ${UPDATER_SOURCES})
target_link_libraries(Updater PRIVATE Qt6::Core)
set_target_properties(Updater PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main"
)
ape_configure_app_version_resource(Updater "APE HOI4 Tool Studio Updater" "Updater.exe" "Updater" "0x1L" "${APP_VERSION_STRING}")

set(SETUP_SOURCES
    setup/main.cpp
    setup/Setup.cpp
    setup/Setup.h
    resources/setup_resources.qrc
)

if(WIN32)
    list(APPEND SETUP_SOURCES resources/windows_icon.rc)
endif()

add_executable(SetupApp WIN32 ${SETUP_SOURCES})
target_link_libraries(SetupApp PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
set_target_properties(SetupApp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/setup"
)
ape_configure_app_version_resource(SetupApp "APE HOI4 Tool Studio Setup" "SetupApp.exe" "SetupApp" "0x1L" "${APP_VERSION_STRING}")

set(APP_SOURCES
    src/main.cpp
    src/RuntimeContextConfigurator.cpp
    src/RuntimeContextConfigurator.h
    src/AgreementEvidenceManager.cpp
    src/AgreementEvidenceManager.h
    src/ExternalPackageManager.cpp
    src/ExternalPackageManager.h
    src/FileAssociationManager.cpp
    src/FileAssociationManager.h
    src/SingleInstanceManager.cpp
    src/SingleInstanceManager.h
    src/StartupSplashScreen.cpp
    src/StartupSplashScreen.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/WindowAviRecorder.cpp
    src/WindowAviRecorder.h
    src/ToolUiContainer.cpp
    src/ToolUiContainer.h
    src/SetupDialog.cpp
    src/SetupDialog.h
    src/OverlayControlStyle.cpp
    src/OverlayControlStyle.h
    src/OverlayAcrylicMaterial.cpp
    src/OverlayAcrylicMaterial.h
    src/ScreenAcrylicBackdrop.cpp
    src/ScreenAcrylicBackdrop.h
    src/SettingsPage.cpp
    src/SettingsPage.h
    src/ConfigPage.cpp
    src/ConfigPage.h
    src/ToolsPage.cpp
    src/ToolsPage.h
    src/AccountPage.cpp
    src/AccountPage.h
    src/ToolHostMode.cpp
    src/ToolHostMode.h
    src/LoadingOverlay.cpp
    src/LoadingOverlay.h
    src/ConnectionWarningOverlay.cpp
    src/ConnectionWarningOverlay.h
    src/Update.cpp
    src/Update.h
    src/UserAgreementOverlay.cpp
    src/UserAgreementOverlay.h
    src/Advertisement.cpp
    src/Advertisement.h
    src/LoginDialog.cpp
    src/LoginDialog.h
    ${QRC_FILES}
    ${TS_FILES}
)

if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE ON)
    list(APPEND APP_SOURCES resources/windows_icon.rc)
endif()

add_executable(APEHOI4ToolStudio ${APP_SOURCES})
target_link_libraries(APEHOI4ToolStudio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickWidgets
    APEHTSLogger
    APEHTSConfig
    APEHTSLocalization
    APEHTSMessageBox
    APEHTSFile
    APEHTSCore
    APEHTSTool
    APEHTSPlugin
)
if(WIN32)
    target_link_libraries(APEHOI4ToolStudio PRIVATE shell32 winmm vfw32)
endif()
set_target_properties(APEHOI4ToolStudio PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main"
)
ape_configure_app_version_resource(APEHOI4ToolStudio "APE HOI4 Tool Studio" "APEHOI4ToolStudio.exe" "APEHOI4ToolStudio" "0x1L" "${APP_VERSION_STRING}")

add_custom_command(TARGET APEHOI4ToolStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${APE_CURL_BIN_DIR}"
    "$<TARGET_FILE_DIR:APEHOI4ToolStudio>"
    COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/resources/localisation" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:APEHOI4ToolStudio>" -DAPE_DEST_NAME="localisation" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake"
)

add_dependencies(APEHOI4ToolStudio Updater)

# Standalone tool host executable (non-GUI process for hosting tool workers)
set(TOOLHOST_SOURCES
    src/ToolHostMain.cpp
    src/ToolHostMode.cpp
    src/ToolHostMode.h
)

if(WIN32)
    list(APPEND TOOLHOST_SOURCES resources/windows_icon.rc)
endif()

add_executable(ToolHost ${TOOLHOST_SOURCES})
target_link_libraries(ToolHost PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    APEHTSLogger
    APEHTSConfig
    APEHTSLocalization
    APEHTSFile
    APEHTSTool
    APEHTSPlugin
)
set_target_properties(ToolHost PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main"
)
if(WIN32)
    target_compile_definitions(ToolHost PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
ape_configure_app_version_resource(ToolHost "APE HOI4 Tool Studio - Tool Host" "ToolHost.exe" "ToolHost" "0x1L" "${APP_VERSION_STRING}")

function(ape_configure_tool target_name source_dir)
    set(descriptor_path "${CMAKE_CURRENT_SOURCE_DIR}/tools/${source_dir}/descriptor.apehts")
    ape_extract_descriptor_value("${descriptor_path}" "id" tool_id)
    ape_extract_descriptor_value("${descriptor_path}" "version" tool_version)

    target_link_libraries(${target_name} PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        APEHTSLogger
        APEHTSConfig
        APEHTSLocalization
        APEHTSMessageBox
        APEHTSFile
        APEHTSCore
        APEHTSTool
        APEHTSPlugin
    )
    set_target_properties(${target_name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/${tool_id}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/${tool_id}"
    )
    ape_configure_version_resource(${target_name} "${target_name} Tool" "lib${target_name}.dll" "${target_name}" "0x2L" "${tool_version}")
    ape_copy_optional_file(${target_name} "${descriptor_path}" "descriptor.apehts")
    ape_copy_optional_file(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/tools/${source_dir}/LICENSE" "LICENSE")
    ape_copy_optional_file(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/tools/${source_dir}/cover.png" "cover.png")
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/tools/${source_dir}/localisation" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:${target_name}>" -DAPE_DEST_NAME="localisation" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake"
    )
endfunction()

file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake" [=[
if(EXISTS "${APE_SOURCE}")
    file(COPY "${APE_SOURCE}" DESTINATION "${APE_DEST_DIR}")
    if(NOT "${APE_DEST_NAME}" STREQUAL "")
        get_filename_component(APE_SOURCE_NAME "${APE_SOURCE}" NAME)
        if(NOT "${APE_SOURCE_NAME}" STREQUAL "${APE_DEST_NAME}")
            file(RENAME "${APE_DEST_DIR}/${APE_SOURCE_NAME}" "${APE_DEST_DIR}/${APE_DEST_NAME}")
        endif()
    endif()
endif()
]=])

set(FILEMANAGER_WORKER_SOURCES
    tools/FileManagerTool/FileManagerBridge.cpp
    tools/FileManagerTool/FileManagerBridge.h
    tools/FileManagerTool/main/FileManagerCore.cpp
    tools/FileManagerTool/main/FileManagerCore.h
)
add_library(FileManagerWorker SHARED ${FILEMANAGER_WORKER_SOURCES})
set_target_properties(FileManagerWorker PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
ape_apply_binary_name(FileManagerWorker "FileManagerTool")
target_include_directories(FileManagerWorker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool
)
target_link_libraries(FileManagerWorker PRIVATE
    Qt6::Core
    APEHTSTool
)
set_target_properties(FileManagerWorker PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000001"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000001"
)
if(WIN32)
    target_compile_definitions(FileManagerWorker PRIVATE TOOL_WORKER_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/descriptor.apehts" "version" filemanager_worker_version)
ape_configure_version_resource(
    FileManagerWorker
    "FileManagerTool Tool Worker"
    "FileManagerTool.dll"
    "FileManagerTool"
    "0x2L"
    "${filemanager_worker_version}"
)
ape_copy_optional_file(FileManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/descriptor.apehts" "descriptor.apehts")
ape_copy_optional_file(FileManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/LICENSE" "LICENSE")
ape_copy_optional_file(FileManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/cover.png" "cover.png")
ape_copy_optional_file(FileManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/FileManagerTool.qml" "FileManagerTool.qml")
add_custom_command(TARGET FileManagerWorker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/tools/FileManagerTool/localisation" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:FileManagerWorker>" -DAPE_DEST_NAME="localisation" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake"
)

set(LOGMANAGER_WORKER_SOURCES
    tools/LogManagerTool/LogManagerBridge.cpp
    tools/LogManagerTool/LogManagerBridge.h
    tools/LogManagerTool/main/LogManagerCore.cpp
    tools/LogManagerTool/main/LogManagerCore.h
    tools/LogManagerTool/main/LogEntry.h
    tools/LogManagerTool/main/LogFileSystem.h
    tools/LogManagerTool/main/LogScanner.cpp
    tools/LogManagerTool/main/LogScanner.h
    tools/LogManagerTool/main/LogParser.cpp
    tools/LogManagerTool/main/LogParser.h
)
add_library(LogManagerWorker SHARED ${LOGMANAGER_WORKER_SOURCES})
set_target_properties(LogManagerWorker PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
ape_apply_binary_name(LogManagerWorker "LogManagerTool")
target_include_directories(LogManagerWorker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool
)
target_link_libraries(LogManagerWorker PRIVATE
    Qt6::Core
    APEHTSTool
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        tools/LogManagerTool/LogManagerBridge.cpp
        PROPERTIES COMPILE_FLAGS "-O0"
    )
endif()
set_target_properties(LogManagerWorker PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000002"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000002"
)
if(WIN32)
    target_compile_definitions(LogManagerWorker PRIVATE TOOL_WORKER_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool/descriptor.apehts" "version" logmanager_worker_version)
ape_configure_version_resource(
    LogManagerWorker
    "LogManagerTool Tool Worker"
    "LogManagerTool.dll"
    "LogManagerTool"
    "0x2L"
    "${logmanager_worker_version}"
)
ape_copy_optional_file(LogManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool/descriptor.apehts" "descriptor.apehts")
ape_copy_optional_file(LogManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool/LICENSE" "LICENSE")
ape_copy_optional_file(LogManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool/cover.png" "cover.png")
ape_copy_optional_file(LogManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool/LogManagerTool.qml" "LogManagerTool.qml")
add_custom_command(TARGET LogManagerWorker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/tools/LogManagerTool/localisation" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:LogManagerWorker>" -DAPE_DEST_NAME="localisation" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake"
)

set(FLAGMANAGER_WORKER_SOURCES
    tools/FlagManagerTool/FlagManagerBridge.cpp
    tools/FlagManagerTool/FlagManagerBridge.h
    tools/FlagManagerTool/main/FlagManagerCore.cpp
    tools/FlagManagerTool/main/FlagManagerCore.h
    tools/FlagManagerTool/main/FlagTypes.h
    tools/FlagManagerTool/main/FlagImagePipeline.h
    tools/FlagManagerTool/main/FlagFileSystem.h
    tools/FlagManagerTool/main/FlagImage.cpp
    tools/FlagManagerTool/main/FlagImage.h
)
add_library(FlagManagerWorker SHARED ${FLAGMANAGER_WORKER_SOURCES})
set_target_properties(FlagManagerWorker PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
ape_apply_binary_name(FlagManagerWorker "FlagManagerTool")
target_include_directories(FlagManagerWorker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool
)
target_link_libraries(FlagManagerWorker PRIVATE
    Qt6::Core
    Qt6::Gui
    APEHTSTool
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        tools/FlagManagerTool/FlagManagerBridge.cpp
        PROPERTIES COMPILE_FLAGS "-O0"
    )
endif()
set_target_properties(FlagManagerWorker PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000003"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000003"
)
if(WIN32)
    target_compile_definitions(FlagManagerWorker PRIVATE TOOL_WORKER_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(FlagManagerWorker PRIVATE windowscodecs ole32)
endif()
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/descriptor.apehts" "version" flagmanager_worker_version)
ape_configure_version_resource(
    FlagManagerWorker
    "FlagManagerTool Tool Worker"
    "FlagManagerTool.dll"
    "FlagManagerTool"
    "0x2L"
    "${flagmanager_worker_version}"
)
ape_copy_optional_file(FlagManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/descriptor.apehts" "descriptor.apehts")
ape_copy_optional_file(FlagManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/LICENSE" "LICENSE")
ape_copy_optional_file(FlagManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/cover.png" "cover.png")
ape_copy_optional_file(FlagManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/FlagManagerTool.qml" "FlagManagerTool.qml")
add_custom_command(TARGET FlagManagerWorker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/tools/FlagManagerTool/localisation" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:FlagManagerWorker>" -DAPE_DEST_NAME="localisation" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake"
)

set(FONTMANAGER_WORKER_SOURCES
    tools/FontManagerTool/FontManagerBridge.cpp
    tools/FontManagerTool/FontManagerBridge.h
    tools/FontManagerTool/main/FontManagerCore.cpp
    tools/FontManagerTool/main/FontManagerCore.h
    tools/FontManagerTool/main/FontFileSystem.h
    tools/FontManagerTool/main/FontTypes.h
)
add_library(FontManagerWorker SHARED ${FONTMANAGER_WORKER_SOURCES})
set_target_properties(FontManagerWorker PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
ape_apply_binary_name(FontManagerWorker "FontManagerTool")
target_include_directories(FontManagerWorker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool
)
target_link_libraries(FontManagerWorker PRIVATE
    Qt6::Core
    Qt6::Gui
    APEHTSTool
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(
        tools/FontManagerTool/FontManagerBridge.cpp
        PROPERTIES COMPILE_FLAGS "-O0"
    )
endif()
set_target_properties(FontManagerWorker PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000004"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/tools/10000004"
)
if(WIN32)
    target_compile_definitions(FontManagerWorker PRIVATE TOOL_WORKER_EXPORTS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool/descriptor.apehts" "version" fontmanager_worker_version)
ape_configure_version_resource(
    FontManagerWorker
    "FontManagerTool Tool Worker"
    "FontManagerTool.dll"
    "FontManagerTool"
    "0x2L"
    "${fontmanager_worker_version}"
)
ape_copy_optional_file(FontManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool/descriptor.apehts" "descriptor.apehts")
ape_copy_optional_file(FontManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool/LICENSE" "LICENSE")
ape_copy_optional_file(FontManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool/cover.png" "cover.png")
ape_copy_optional_file(FontManagerWorker "${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool/FontManagerTool.qml" "FontManagerTool.qml")
add_custom_command(TARGET FontManagerWorker POST_BUILD
    COMMAND ${CMAKE_COMMAND} -DAPE_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/tools/FontManagerTool/localisation" -DAPE_DEST_DIR="$<TARGET_FILE_DIR:FontManagerWorker>" -DAPE_DEST_NAME="localisation" -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_optional_dir.cmake"
)

set(LUMORPHA_PLUGIN_SOURCES
    plugins/Lumorpha/LumorphaPluginExports.cpp
    plugins/Lumorpha/LumorphaBridgeTypes.h
    plugins/Lumorpha/LumorphaExports.h
    plugins/Lumorpha/main/Core/ImageBuffer.cpp
    plugins/Lumorpha/main/Core/ImageBuffer.h
    plugins/Lumorpha/main/Codecs/ImageCodec.cpp
    plugins/Lumorpha/main/Codecs/ImageCodec.h
    plugins/Lumorpha/main/Processing/ImageProcessor.cpp
    plugins/Lumorpha/main/Processing/ImageProcessor.h
    plugins/Lumorpha/main/Render/ImageRenderer.cpp
    plugins/Lumorpha/main/Render/ImageRenderer.h
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexImage.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexDDS.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexWIC.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexTGA.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexHDR.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexUtil.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexConvert.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexCompress.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexMisc.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexMipmaps.cpp
    plugins/Lumorpha/main/External/DirectXTex/DirectXTexResize.cpp
    plugins/Lumorpha/main/External/DirectXTex/BC.cpp
    plugins/Lumorpha/main/External/DirectXTex/BC4BC5.cpp
    plugins/Lumorpha/main/External/DirectXTex/BC6HBC7.cpp
)
add_library(Lumorpha SHARED ${LUMORPHA_PLUGIN_SOURCES})
target_include_directories(Lumorpha PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha/main
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha/main/External/DirectXTex
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha/main/External/DirectXMath
)
target_link_libraries(Lumorpha PRIVATE APEHTSPlugin)
set_target_properties(Lumorpha PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000006"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000006"
)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Lumorpha PRIVATE -Wno-ignored-attributes)
endif()
if(WIN32)
    target_compile_definitions(Lumorpha PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(Lumorpha PRIVATE windowscodecs ole32)
endif()
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha/descriptor.htsplugin" "version" lumorpha_plugin_version)
ape_configure_version_resource(Lumorpha "Lumorpha Plugin" "libLumorpha.dll" "Lumorpha" "0x2L" "${lumorpha_plugin_version}")
ape_copy_optional_file(Lumorpha "${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha/descriptor.htsplugin" "descriptor.htsplugin")
ape_copy_optional_file(Lumorpha "${CMAKE_CURRENT_SOURCE_DIR}/plugins/Lumorpha/LICENSE" "LICENSE")

set(DRYADATLAS_PLUGIN_SOURCES
    plugins/DryadAtlas/DryadAtlasPluginExports.cpp
)
add_library(DryadAtlas SHARED ${DRYADATLAS_PLUGIN_SOURCES})
target_link_libraries(DryadAtlas PRIVATE Qt6::Core Qt6::Gui)
set_target_properties(DryadAtlas PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000005"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000005"
)
if(WIN32)
    target_compile_definitions(DryadAtlas PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(DryadAtlas PRIVATE gdi32)
endif()
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/plugins/DryadAtlas/descriptor.htsplugin" "version" dryadatlas_plugin_version)
ape_configure_version_resource(DryadAtlas "DryadAtlas Plugin" "libDryadAtlas.dll" "DryadAtlas" "0x2L" "${dryadatlas_plugin_version}")
ape_copy_optional_file(DryadAtlas "${CMAKE_CURRENT_SOURCE_DIR}/plugins/DryadAtlas/descriptor.htsplugin" "descriptor.htsplugin")
ape_copy_optional_file(DryadAtlas "${CMAKE_CURRENT_SOURCE_DIR}/plugins/DryadAtlas/LICENSE" "LICENSE")

set(TAGLIST_PLUGIN_SOURCES
    plugins/TagList/TagListPluginExports.cpp
    plugins/TagList/main/TagManager.cpp
    plugins/TagList/main/TagManager.h
)
add_library(TagList SHARED ${TAGLIST_PLUGIN_SOURCES})
target_include_directories(TagList PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/TagList/main
)
target_link_libraries(TagList PRIVATE Qt6::Core APEHTSPlugin APEHTSCore APEHTSLogger APEHTSFile)
set_target_properties(TagList PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000004"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000004"
)
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/plugins/TagList/descriptor.htsplugin" "version" taglist_plugin_version)
ape_configure_version_resource(TagList "TagList Plugin" "libTagList.dll" "TagList" "0x2L" "${taglist_plugin_version}")
ape_copy_optional_file(TagList "${CMAKE_CURRENT_SOURCE_DIR}/plugins/TagList/descriptor.htsplugin" "descriptor.htsplugin")
ape_copy_optional_file(TagList "${CMAKE_CURRENT_SOURCE_DIR}/plugins/TagList/LICENSE" "LICENSE")

set(APEHOI4PARSER_PLUGIN_SOURCES
    plugins/APEHOI4Parser/APEHOI4ParserPluginExports.cpp
    plugins/APEHOI4Parser/APEHOI4ParserBridgeTypes.h
    plugins/APEHOI4Parser/APEHOI4ParserExports.h
    plugins/APEHOI4Parser/main/Core/ParserSession.cpp
    plugins/APEHOI4Parser/main/Core/ParserSession.h
    plugins/APEHOI4Parser/main/Core/SourceText.cpp
    plugins/APEHOI4Parser/main/Core/SourceText.h
    plugins/APEHOI4Parser/main/Diagnostics/Diagnostic.h
    plugins/APEHOI4Parser/main/Diagnostics/DiagnosticBag.cpp
    plugins/APEHOI4Parser/main/Diagnostics/DiagnosticBag.h
    plugins/APEHOI4Parser/main/Lexer/Token.h
    plugins/APEHOI4Parser/main/Lexer/Lexer.cpp
    plugins/APEHOI4Parser/main/Lexer/Lexer.h
    plugins/APEHOI4Parser/main/Ast/SyntaxKind.h
    plugins/APEHOI4Parser/main/Ast/SyntaxTree.cpp
    plugins/APEHOI4Parser/main/Ast/SyntaxTree.h
    plugins/APEHOI4Parser/main/Parser/Parser.cpp
    plugins/APEHOI4Parser/main/Parser/Parser.h
    plugins/APEHOI4Parser/main/Queries/ParseResultJson.cpp
    plugins/APEHOI4Parser/main/Queries/ParseResultJson.h
    plugins/APEHOI4Parser/main/Domain/Localization/LocalizationParser.cpp
    plugins/APEHOI4Parser/main/Domain/Localization/LocalizationParser.h
    plugins/APEHOI4Parser/main/Domain/Tags/TagFileParser.cpp
    plugins/APEHOI4Parser/main/Domain/Tags/TagFileParser.h
    plugins/APEHOI4Parser/main/Domain/Fonts/FontGfxParser.cpp
    plugins/APEHOI4Parser/main/Domain/Fonts/FontGfxParser.h
    plugins/APEHOI4Parser/main/Domain/Focus/FocusTreeParser.cpp
    plugins/APEHOI4Parser/main/Domain/Focus/FocusTreeParser.h
    plugins/APEHOI4Parser/main/Domain/Ideas/IdeasParser.cpp
    plugins/APEHOI4Parser/main/Domain/Ideas/IdeasParser.h
    plugins/APEHOI4Parser/main/Domain/ScriptedTriggers/ScriptedTriggerParser.cpp
    plugins/APEHOI4Parser/main/Domain/ScriptedTriggers/ScriptedTriggerParser.h
    plugins/APEHOI4Parser/main/Domain/ScriptedEffects/ScriptedEffectParser.cpp
    plugins/APEHOI4Parser/main/Domain/ScriptedEffects/ScriptedEffectParser.h
    plugins/APEHOI4Parser/main/Utils/Utf8.cpp
    plugins/APEHOI4Parser/main/Utils/Utf8.h
)
add_library(APEHOI4Parser SHARED ${APEHOI4PARSER_PLUGIN_SOURCES})
set_target_properties(APEHOI4Parser PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)
target_include_directories(APEHOI4Parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/APEHOI4Parser
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/APEHOI4Parser/main
)
target_link_libraries(APEHOI4Parser PRIVATE
    Qt6::Core
    APEHTSPlugin
)
set_target_properties(APEHOI4Parser PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000001"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/main/plugins/20000001"
)
ape_extract_descriptor_value("${CMAKE_CURRENT_SOURCE_DIR}/plugins/APEHOI4Parser/descriptor.htsplugin" "version" apehoi4parser_plugin_version)
ape_configure_version_resource(APEHOI4Parser "APEHOI4Parser Plugin" "libAPEHOI4Parser.dll" "APEHOI4Parser" "0x2L" "${apehoi4parser_plugin_version}")
ape_copy_optional_file(APEHOI4Parser "${CMAKE_CURRENT_SOURCE_DIR}/plugins/APEHOI4Parser/descriptor.htsplugin" "descriptor.htsplugin")
ape_copy_optional_file(APEHOI4Parser "${CMAKE_CURRENT_SOURCE_DIR}/plugins/APEHOI4Parser/LICENSE" "LICENSE")
