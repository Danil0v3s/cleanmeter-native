cmake_minimum_required(VERSION 3.16)
project(OverlayHook)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/Detours/include
)

# Library directories
link_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/lib/release
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/Detours/lib
)

# Add subdirectories
add_subdirectory(deps)
add_subdirectory(src)

# Main executable
add_executable(OverlayInjector
    src/main.cpp
)

target_link_libraries(OverlayInjector
    OverlayCore
    user32
    kernel32
    advapi32
    shell32
)

target_compile_definitions(OverlayInjector PRIVATE
    UNICODE
    _UNICODE
)

# Copy OverlayHook.dll next to OverlayInjector.exe after building
add_custom_command(TARGET OverlayInjector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:OverlayHook>
    $<TARGET_FILE_DIR:OverlayInjector>
    DEPENDS OverlayHook
    COMMENT "Copying OverlayHook.dll to OverlayInjector directory"
)

# Copy FW1FontWrapper.dll next to OverlayInjector.exe after building
add_custom_command(TARGET OverlayInjector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/deps/dll/FW1FontWrapper.x64.dll"
    $<TARGET_FILE_DIR:OverlayInjector>
    COMMENT "Copying FW1FontWrapper.dll to OverlayInjector directory"
)

# Overlay DLL
add_library(OverlayHook SHARED
    src/overlay/dllmain.cpp
    src/overlay/hook_manager.cpp
    src/graphics/graphics_factory.cpp
    src/graphics/dx9/dx9_graphics.cpp
    src/graphics/dx10/dx10_graphics.cpp
    src/graphics/dx11/dx11_graphics.cpp
    src/graphics/dx12/dx12_graphics.cpp
)

target_link_libraries(OverlayHook
    OverlayCore
    d3d9
    d3d10
    d3d11
    d3d12
    dxgi
    FW1FontWrapper.x64
    detours
    user32
    kernel32
    delayimp  # Required for delay loading
)

target_compile_definitions(OverlayHook PRIVATE
    UNICODE
    _UNICODE
)

# Add delay-loading for DirectX and FW1FontWrapper dependencies
target_link_options(OverlayHook PRIVATE
    /DELAYLOAD:d3d9.dll
    /DELAYLOAD:d3d10.dll
    /DELAYLOAD:d3d11.dll
    /DELAYLOAD:d3d12.dll
    /DELAYLOAD:dxgi.dll
    /DELAYLOAD:FW1FontWrapper.x64.dll
)

set_target_properties(OverlayHook PROPERTIES
    OUTPUT_NAME "OverlayHook"
    SUFFIX ".dll"
) 