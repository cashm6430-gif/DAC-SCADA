cmake_minimum_required(VERSION 3.16)
project(DAC-SCADA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_PREFIX_PATH "E:/Qt/bin")

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Network Sql SerialPort SerialBus Charts
)

add_executable(${PROJECT_NAME}
    main.cpp
    communication/comm_interface.h
    communication/serialport_comm.h
    communication/serialport_comm.cpp
    communication/modbus_rtu.h
    communication/modbus_rtu.cpp
    ui/mainwindow.h
    ui/mainwindow.cpp
    core/device_manager.h
    core/device_manager.cpp
    core/data_cache.h
    core/data_cache.cpp
    core/alarm_engine.h
    core/alarm_engine.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::Sql
    Qt6::SerialPort Qt6::SerialBus Qt6::Charts
)
