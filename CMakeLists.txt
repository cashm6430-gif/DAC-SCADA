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
    PrintSupport
    Test
)

# ── QCustomPlot (vendored single-file charting library) ─────────────
# Third-party code — compiled as its own static lib with warnings off so the
# project-wide /W4 "zero warning" policy only applies to first-party sources.
add_library(qcustomplot STATIC
    third_party/qcustomplot/qcustomplot.cpp
)
target_include_directories(qcustomplot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qcustomplot
)
target_link_libraries(qcustomplot PUBLIC
    Qt6::Widgets Qt6::Gui Qt6::Core Qt6::PrintSupport
)
if(MSVC)
    target_compile_options(qcustomplot PRIVATE /W0)
endif()

# ── QXlsx (vendored xlsx writer, MIT) ──────────────────────────
# Third-party — /W0 keeps the first-party /W4 zero-warning policy. QXlsx's
# internal zip uses Qt's private QZipWriter/QZipReader (exported from Qt6Core);
# Qt 6.8 < 6.10 needs no GuiPrivate, only the QtCore private header path.
file(GLOB QXLSX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qxlsx/source/*.cpp
)
file(GLOB QXLSX_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qxlsx/header/*.h
)
# QXlsx's zip reader/writer include <private/qzip*.h> — locate the QtCore
# private header directory from Qt6::Core's include dirs (e.g. .../QtCore/6.8.4/QtCore).
get_target_property(_qtcore_incs Qt6::Core INTERFACE_INCLUDE_DIRECTORIES)
foreach(_inc IN LISTS _qtcore_incs)
    file(GLOB _priv_hdr "${_inc}/*/QtCore/private/qzipwriter_p.h")
    if(_priv_hdr)
        get_filename_component(_qtcore_private_inc "${_priv_hdr}" DIRECTORY)
        get_filename_component(_qtcore_private_inc "${_qtcore_private_inc}" DIRECTORY)
        break()
    endif()
endforeach()
if(NOT _qtcore_private_inc)
    message(FATAL_ERROR "QtCore private headers (qzipwriter_p.h) not found — QXlsx cannot build.")
endif()
add_library(qxlsx STATIC ${QXLSX_SOURCES} ${QXLSX_HEADERS})
target_include_directories(qxlsx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/qxlsx/header
)
target_include_directories(qxlsx PRIVATE "${_qtcore_private_inc}")
# Also add its parent so `<QtCore/private/qglobal_p.h>`-style includes resolve.
get_filename_component(_qtcore_private_parent "${_qtcore_private_inc}" DIRECTORY)
target_include_directories(qxlsx PRIVATE "${_qtcore_private_parent}")
target_compile_definitions(qxlsx PRIVATE
    QT_NO_KEYWORDS
    QT_NO_CAST_TO_ASCII
    QT_NO_CAST_FROM_ASCII
    QT_NO_URL_CAST_FROM_STRING
    QT_NO_CAST_FROM_BYTEARRAY
    QT_USE_QSTRINGBUILDER
    QT_DISABLE_DEPRECATED_BEFORE=0x060600
)
target_link_libraries(qxlsx PUBLIC Qt6::Core Qt6::Gui)
if(MSVC)
    target_compile_options(qxlsx PRIVATE /W0 "SHELL:/permissive")
endif()

# ── Source files ──────────────────────────────────
set(SOURCES
    main.cpp
    communication/modbus_client_interface.h
    communication/modbus_tcp_client.h
    communication/modbus_tcp_client.cpp
    communication/modbus_serial_client.h
    communication/modbus_serial_client.cpp
    ui/mainwindow.h
    ui/mainwindow.cpp
    ui/main_viewmodel.h
    ui/main_viewmodel.cpp
    ui/curve_panel.h
    ui/curve_panel.cpp
    ui/gauge_widget.h
    ui/gauge_widget.cpp
    ui/dashboard_panel.h
    ui/dashboard_panel.cpp
    ui/history_panel.h
    ui/history_panel.cpp
    ui/write_register_dialog.h
    ui/write_register_dialog.cpp
    core/data_cache.h
    core/data_cache.cpp
    core/alarm_engine.h
    core/alarm_engine.cpp
    core/data_collector.h
    core/data_collector.cpp
    core/history_store.h
    core/history_store.cpp
    core/app_logger.h
    core/app_logger.cpp
    core/sample_queue.h
    core/types.h
    simulator/simulated_device.h
    simulator/simulated_device.cpp
    simulator/simulated_modbus_server.h
    simulator/simulated_modbus_server.cpp
    simulator/simulated_serial_server.h
    simulator/simulated_serial_server.cpp
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
    qcustomplot
    qxlsx
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

# ── Copy config/ next to the executable (post-build) ─
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/config"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/config"
    COMMENT "Copying config/ to executable directory..."
    VERBATIM
)

# ── Unit tests (QTest) ─────────────────────────────
# Fast, deterministic, headless — no simulators or 8 s event-loop integration
# tests. One executable per test group (each QTEST_*_MAIN defines its own
# main). Run with: ctest --test-dir build_release  (or build_debug)
enable_testing()

function(add_dac_test target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target_name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Test Qt6::Sql)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /utf-8)
    endif()
    # The app is deployed by windeployqt, which does not know about Qt6Test
    # (the GUI executable does not link it) — copy the test DLL next to the
    # test executable so it can actually start.
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::Test>" "$<TARGET_FILE_DIR:${target_name}>"
        COMMENT "Copying Qt6Test DLL next to ${target_name}..."
        VERBATIM)
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES TIMEOUT 120)
endfunction()

add_dac_test(test_backoff tests/test_backoff.cpp)
add_dac_test(test_sample_queue tests/test_sample_queue.cpp)
add_dac_test(test_alarm_engine tests/test_alarm_engine.cpp core/alarm_engine.cpp)
add_dac_test(test_history_store tests/test_history_store.cpp core/history_store.cpp)
add_dac_test(test_gauge tests/test_gauge.cpp ui/gauge_widget.cpp)
# test_gauge compiles gauge_widget.cpp (a QWidget subclass) → needs Widgets.
target_link_libraries(test_gauge PRIVATE Qt6::Widgets)
add_dac_test(test_app_logger tests/test_app_logger.cpp core/app_logger.cpp)

