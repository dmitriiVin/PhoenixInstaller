cmake_minimum_required(VERSION 3.20)

project(Installer LANGUAGES CXX)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE SOURCES
    src/*.cpp
)

add_executable(Installer WIN32
    ${SOURCES}
    resources/resources.rc
)

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