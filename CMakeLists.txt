cmake_minimum_required(VERSION 3.16)
project(DAC-SCADA VERSION 1.0.0 LANGUAGES CXX)

# ── C++ Standard ──────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Qt auto-processing ────────────────────────────
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Qt prefix auto-detection ──────────────────────
if(NOT CMAKE_PREFIX_PATH)
    foreach(_path "E:/Qt/release/bin" "E:/Qt/debug/bin")
        if(EXISTS "${_path}/lib/cmake/Qt6/Qt6Config.cmake")
            set(CMAKE_PREFIX_PATH "${_path}" CACHE PATH
                "Qt installation prefix (auto-detected)" FORCE)
            message(STATUS "Qt prefix auto-detected: ${_path}")
            break()
        endif()
    endforeach()

    if(NOT CMAKE_PREFIX_PATH)
        message(FATAL_ERROR
            "Qt 6.8 not found.\n"
            "Use -DCMAKE_PREFIX_PATH= to point to one of:\n"
            "  E:/Qt/release/bin\n"
            "  E:/Qt/debug/bin\n"
            "Or use CMake presets: cmake --preset debug | cmake --preset release")
    endif()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
    Sql
    SerialPort
    SerialBus
    Charts
)

# ── Source files ──────────────────────────────────
set(SOURCES
    main.cpp
    communication/comm_interface.h
    communication/serialport_comm.h
    communication/serialport_comm.cpp
    communication/modbus_rtu.h
    communication/modbus_rtu.cpp
    ui/mainwindow.h
    ui/mainwindow.cpp
    ui/main_viewmodel.h
    ui/main_viewmodel.cpp
    core/device_manager.h
    core/device_manager.cpp
    core/data_cache.h
    core/data_cache.cpp
    core/alarm_engine.h
    core/alarm_engine.cpp
    core/channel_manager.h
    core/channel_manager.cpp
    core/types.h
    simulator/simulated_device.h
    simulator/simulated_device.cpp
    simulator/simulated_modbus_server.h
    simulator/simulated_modbus_server.cpp
)

# ── Executable ────────────────────────────────────
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Link Qt libraries ─────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::SerialPort
    Qt6::SerialBus
    Qt6::Charts
)

# ── Compiler warnings (MSVC) ──────────────────────
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
endif()

# ── Target properties ─────────────────────────────
set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# ── Deploy Qt DLLs (windeployqt) ──────────────────
# Qt6_DIR = E:/Qt/debug/bin/lib/cmake/Qt6
#   → up 3 levels → prefix = E:/Qt/debug/bin
#   → windeployqt → E:/Qt/debug/bin/bin/windeployqt.exe
get_filename_component(_dir "${Qt6_DIR}" DIRECTORY)  # .../lib/cmake
get_filename_component(_dir "${_dir}" DIRECTORY)     # .../lib
get_filename_component(_qt6_prefix "${_dir}" DIRECTORY)  # prefix
set(_windeployqt "${_qt6_prefix}/bin/windeployqt.exe")

if(EXISTS "${_windeployqt}")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${_windeployqt}"
            "$<TARGET_FILE:${PROJECT_NAME}>"
            --no-translations
            --no-compiler-runtime
            --no-opengl-sw
        COMMENT "Deploying Qt runtime DLLs..."
        VERBATIM
    )
    message(STATUS "Post-build: ${_windeployqt}")
else()
    message(WARNING "windeployqt not found — DLLs will not be copied automatically")
endif()
