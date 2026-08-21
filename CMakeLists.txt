cmake_minimum_required(VERSION 3.20)

project(Installer LANGUAGES CXX)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

set(INSTALLER_SOURCES ${ALL_SOURCES})
list(FILTER INSTALLER_SOURCES EXCLUDE REGEX "[/\\\\]src[/\\\\]Setup[/\\\\]")

set(PHOENIX_SETUP_SOURCES
    src/Setup/main.cpp
    src/Setup/PhoenixSetup.cpp
    src/Packages/PackageManager.cpp
)

set(PHOENIX_SETUP_RESOURCE "${CMAKE_BINARY_DIR}/embedded/PhoenixSetup.exe")
set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")
set(ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(SCRIPTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
set(RESOURCE_HEADER "${RESOURCES_DIR}/resource.h")
file(TO_CMAKE_PATH "${RESOURCES_DIR}" RESOURCES_DIR)
file(TO_CMAKE_PATH "${ASSETS_DIR}" ASSETS_DIR)
file(TO_CMAKE_PATH "${SCRIPTS_DIR}" SCRIPTS_DIR)
file(TO_CMAKE_PATH "${RESOURCE_HEADER}" RESOURCE_HEADER)
file(TO_CMAKE_PATH "${PHOENIX_SETUP_RESOURCE}" PHOENIX_SETUP_RESOURCE)

set(EMBEDDED_RESOURCE_FILES
    "${ASSETS_DIR}/fonts/Inter-Bold.otf"
    "${ASSETS_DIR}/fonts/fa-solid-900.ttf"
    "${ASSETS_DIR}/fonts/fa-brands-400.otf"
    "${ASSETS_DIR}/images/logo.png"
    "${RESOURCES_DIR}/logo.ico"
    "${ASSETS_DIR}/packages.json"
    "${SCRIPTS_DIR}/Install.bat"
    "${SCRIPTS_DIR}/Common.bat"
    "${SCRIPTS_DIR}/Disk/CleanDisk.bat"
    "${SCRIPTS_DIR}/Disk/PrepareWindows.bat"
    "${SCRIPTS_DIR}/Disk/PrepareWindowsAndData.bat"
    "${SCRIPTS_DIR}/Windows/ApplyImage.bat"
    "${SCRIPTS_DIR}/Windows/SetupBoot.bat"
    "${SCRIPTS_DIR}/Windows/ConfigureWindows.bat"
    "${SCRIPTS_DIR}/Drivers/InstallDrivers.bat"
    "${SCRIPTS_DIR}/Programs/InstallPrograms.bat"
    "${SCRIPTS_DIR}/Tweaks/WindowsTweaks.bat"
    "${SCRIPTS_DIR}/Tweaks/Security.ps1"
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${EMBEDDED_RESOURCE_FILES})

set(EMBEDDED_RESOURCE_SIGNATURE "")
foreach(RESOURCE_FILE IN LISTS EMBEDDED_RESOURCE_FILES)
    if (EXISTS "${RESOURCE_FILE}")
        file(SHA256 "${RESOURCE_FILE}" RESOURCE_FILE_HASH)
    else()
        set(RESOURCE_FILE_HASH "missing")
    endif()

    string(APPEND EMBEDDED_RESOURCE_SIGNATURE "${RESOURCE_FILE}:${RESOURCE_FILE_HASH}\n")
endforeach()

string(SHA256 EMBEDDED_RESOURCE_HASH "${EMBEDDED_RESOURCE_SIGNATURE}")

configure_file(
    resources/resources.rc
    "${CMAKE_CURRENT_BINARY_DIR}/generated_resources.rc"
    @ONLY
)

add_executable(PhoenixSetup WIN32
    ${PHOENIX_SETUP_SOURCES}
)

target_compile_options(PhoenixSetup PRIVATE /utf-8)

target_include_directories(PhoenixSetup PRIVATE
    src
    third_party/json/include
)

target_compile_definitions(PhoenixSetup PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

target_link_libraries(PhoenixSetup PRIVATE
    shell32
)

add_custom_command(
    OUTPUT "${PHOENIX_SETUP_RESOURCE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/embedded"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:PhoenixSetup> "${PHOENIX_SETUP_RESOURCE}"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/generated_resources.rc"
    DEPENDS PhoenixSetup
    VERBATIM
)

add_custom_target(PhoenixSetupResource DEPENDS "${PHOENIX_SETUP_RESOURCE}")

add_executable(Installer WIN32
    ${INSTALLER_SOURCES}
    "${CMAKE_CURRENT_BINARY_DIR}/generated_resources.rc"
)

target_compile_options(Installer PRIVATE /utf-8)

add_dependencies(Installer PhoenixSetupResource)

target_include_directories(Installer PRIVATE
    src
    resources
    third_party/imgui
    third_party/imgui/backends
    third_party/json/include
    third_party/miniz
    third_party/stb
)

target_sources(Installer PRIVATE
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_win32.cpp
    third_party/imgui/backends/imgui_impl_dx11.cpp
)

target_compile_definitions(Installer PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

target_link_libraries(Installer PRIVATE
    d3d11
    dxgi
    dxguid
    d3dcompiler
    user32
    gdi32
    shell32
    ole32
    comdlg32
    advapi32
    shlwapi
)
