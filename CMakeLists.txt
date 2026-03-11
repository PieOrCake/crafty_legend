cmake_minimum_required(VERSION 3.20)
project(CraftyLegend)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release build for optimized, stripped output
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-Os -DNDEBUG")

# Cross-compilation setup for Windows on Linux
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

# Static linking to avoid Windows DLL dependencies
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_SHARED_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -DWIN32_LEAN_AND_MEAN")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories(${CMAKE_SOURCE_DIR}/lib/imgui)
include_directories(${CMAKE_SOURCE_DIR}/lib/imgui/backends)
include_directories(${CMAKE_SOURCE_DIR}/lib/nlohmann)

# ImGui source files
set(IMGUI_SOURCES
    lib/imgui/imgui.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/imgui_tables.cpp
    lib/imgui/imgui_widgets.cpp
)

# Source files
set(SOURCES
    src/dllmain_hotkey.cpp
    src/DataManager.cpp
    src/GW2API.cpp
    src/IconManager.cpp
    ${IMGUI_SOURCES}
)

# Header files
set(HEADERS
    include/nexus/Nexus.h
)

# Create shared library (DLL)
add_library(CraftyLegend SHARED ${SOURCES} ${HEADERS})

# Copy JSON files and icons to CraftyLegend subdirectory alongside DLL
add_custom_command(TARGET CraftyLegend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/CraftyLegend/legendaries.json $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/CraftyLegend/items.json $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/CraftyLegend/recipes.json $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/CraftyLegend/currencies.json $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/CraftyLegend/icon_anvil.png $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend/
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/data/CraftyLegend/icon_anvil_hover.png $<TARGET_FILE_DIR:CraftyLegend>/CraftyLegend/
)

# Force Windows DLL extension
set_target_properties(CraftyLegend PROPERTIES
    SUFFIX ".dll"
    PREFIX ""
)

# Link Windows libraries for DirectX 9 and Win32
target_link_libraries(CraftyLegend 
    d3d9
    gdi32
    wininet
)

# Module definition file for clean DLL export
target_link_options(CraftyLegend PRIVATE -Wl,--out-implib,CraftyLegend.lib -Wl,--strip-all)
target_sources(CraftyLegend PRIVATE ${CMAKE_SOURCE_DIR}/CraftyLegend.def)

# Output directory
set_target_properties(CraftyLegend PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
