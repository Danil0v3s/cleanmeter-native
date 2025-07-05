# Create core library
add_library(OverlayCore STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/core/object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/fs/file.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/fs/path.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/io/io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/tools/log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/tools/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/thread/lock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/thread/timer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib/trace/dbg.cpp
)

target_include_directories(OverlayCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/corelib
)

target_compile_definitions(OverlayCore PUBLIC
    UNICODE
    _UNICODE
)

# Detours library
add_library(detours STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Detours/src/detours.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Detours/src/disasm.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Detours/src/image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Detours/src/creatwth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Detours/src/modules.cpp
)

target_include_directories(detours PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Detours/include
)

target_compile_definitions(detours PUBLIC
    UNICODE
    _UNICODE
) 