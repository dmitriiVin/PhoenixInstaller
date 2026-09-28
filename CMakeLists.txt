cmake_minimum_required(VERSION 3.20)

project(Installer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =========================
# SOURCES
# =========================

file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

list(FILTER ALL_SOURCES EXCLUDE REGEX
    "[/\\\\]src[/\\\\]Setup[/\\\\]"
)

list(FILTER ALL_SOURCES EXCLUDE REGEX
    "[/\\\\]src[/\\\\]Installer[/\\\\]InstallationLauncher\\.cpp$"
)

add_executable(Installer
    ${ALL_SOURCES}
)

# =========================
# THIRD PARTY
# =========================

target_include_directories(Installer PRIVATE
    src

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

    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)

# =========================
# COMPILE OPTIONS
# =========================

target_compile_options(Installer PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_compile_definitions(Installer PRIVATE
    NDEBUG
)

# =========================
# DEPENDENCIES
# =========================

find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLFW REQUIRED glfw3)

target_include_directories(Installer PRIVATE
    ${GLFW_INCLUDE_DIRS}
)

target_link_directories(Installer PRIVATE
    ${GLFW_LIBRARY_DIRS}
)

target_link_libraries(Installer PRIVATE
    ${GLFW_LIBRARIES}
    OpenGL::GL
    dl
    pthread
)

# =========================
# RUNTIME RESOURCES
# =========================

add_custom_command(TARGET Installer POST_BUILD

    # packages.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/packages.json"
        "$<TARGET_FILE_DIR:Installer>/packages.json"

    # assets/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Installer>/assets"

    # bcd-sys
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/bcd-sys-2.4-x86_64.AppImage"
        "$<TARGET_FILE_DIR:Installer>/bcd-sys-2.4-x86_64.AppImage"

    # ms-sys
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ms-sys"
        "$<TARGET_FILE_DIR:Installer>/ms-sys"

    COMMENT "Copying runtime resources..."
)