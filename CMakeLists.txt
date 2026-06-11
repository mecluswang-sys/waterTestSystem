cmake_minimum_required(VERSION 3.15)
project(WaterTestSystem VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt配置
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找Qt6
find_package(Qt6 COMPONENTS Core Widgets Sql Network SerialPort REQUIRED)

# 查找SQLite3
find_package(unofficial-sqlite3 CONFIG REQUIRED)

# 查找Snap7库
find_package(snap7 CONFIG)
set(SNAP7_AVAILABLE FALSE)
if(snap7_FOUND)
    message(STATUS "Found Snap7 via vcpkg")
    set(SNAP7_LIBRARY snap7::snap7)
    set(SNAP7_AVAILABLE TRUE)
else()
    # 手动安装的Snap7
    message(STATUS "Snap7 not found via vcpkg, trying manual installation...")
    set(SNAP7_ROOT "C:/snap7" CACHE PATH "Snap7 installation directory")
    find_path(SNAP7_INCLUDE_DIR snap7.h HINTS ${SNAP7_ROOT}/include)
    find_library(SNAP7_LIBRARY NAMES snap7 HINTS ${SNAP7_ROOT}/lib)
    
    if(SNAP7_INCLUDE_DIR AND SNAP7_LIBRARY)
        message(STATUS "Found Snap7: ${SNAP7_LIBRARY}")
        set(SNAP7_AVAILABLE TRUE)
    else()
        message(WARNING "Snap7 not found. Please install via vcpkg or manually.")
    endif()
endif()

# 如果是手动安装的Snap7，添加include目录
if(SNAP7_INCLUDE_DIR)
    include_directories(${SNAP7_INCLUDE_DIR})
endif()

# 包含目录
include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/examples
)

# 源文件
set(SOURCES
    src/main.cpp
    src/DeviceManager.cpp
    src/DataLogger.cpp
    src/ConfigManager.cpp
    src/DatabaseManager.cpp
    src/DataPipelineManager.cpp
    src/PipelineTopology.cpp
    src/NetworkProtocol.cpp
    src/TerminalServer.cpp
    src/StationClient.cpp
    resources/styles.qrc
    src/gui/MainWindow.cpp
    src/gui/MonitorPanel.cpp
    src/gui/ConfigDialog.cpp
    src/gui/PreparationPanel.cpp
    src/gui/TestPanel.cpp
    src/gui/AutoTestPanel.cpp
    src/gui/Station1Panel.cpp
    src/gui/TickedSlider.cpp
    examples/PumpWidget.cpp
)

# Snap7可用时使用真实PLC实现，否则回退到stub实现
if(SNAP7_AVAILABLE)
    list(APPEND SOURCES src/S7PLCClient.cpp)
else()
    list(APPEND SOURCES src/S7PLCClient_stub.cpp)
endif()

# 头文件
set(HEADERS
    include/S7PLCClient.h
    include/DeviceManager.h
    include/DataLogger.h
    include/ConfigManager.h
    include/DatabaseManager.h
    include/DeviceTypes.h
    include/DataPipeline.h
    include/DataPipelineManager.h
    include/PipelineTopology.h
    include/NetworkProtocol.h
    include/TerminalServer.h
    include/StationClient.h
    include/gui/MainWindow.h
    include/gui/MonitorPanel.h
    include/gui/ConfigDialog.h
    include/gui/PreparationPanel.h
    include/gui/TestPanel.h
    include/gui/AutoTestPanel.h
    include/gui/Station1Panel.h
    include/gui/ElectricValveItem.h
    include/gui/RegulatingValveItem.h
    include/gui/TickedSlider.h
    examples/PumpWidget.h
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# 真实Snap7实现需要启用头文件分支
if(SNAP7_AVAILABLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE SNAP7_FOUND)
endif()

# 设置源文件编码为UTF-8
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()

# 链接Qt库
target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Core
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::SerialPort
    unofficial::sqlite3::sqlite3
)

# Modbus 调试小工具（串口）
#add_executable(modbus_reader src/tools/ModbusRead.cpp)
#target_link_libraries(modbus_reader PRIVATE Qt6::Core Qt6::SerialPort)

#if(WIN32)
#    # 控制台程序，便于输出读数
#    set_target_properties(modbus_reader PROPERTIES WIN32_EXECUTABLE FALSE)
#endif()

# 链接Snap7库
if(snap7_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE snap7::snap7)
elseif(SNAP7_LIBRARY)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SNAP7_LIBRARY})
else()
    message(WARNING "Snap7 library not found, linking may fail!")
endif()

# PLC压力读取示例（控制台）
# Temporarily disabled - snap7 not available
#add_executable(plc_pressure_reader
#    examples/read_pressure_from_plc.cpp
#    src/S7PLCClient.cpp
#    src/ConfigManager.cpp
#)
#
#if(snap7_FOUND)
#    target_link_libraries(plc_pressure_reader PRIVATE snap7::snap7)
#elseif(SNAP7_LIBRARY)
#    target_link_libraries(plc_pressure_reader PRIVATE ${SNAP7_LIBRARY})
#else()
#    message(WARNING "Snap7 library not found for plc_pressure_reader, linking may fail!")
#endif()
#
# 通用DB探针（控制台）
# Temporarily disabled - snap7 not available
#add_executable(s7peek_db
#    examples/s7peek_db.cpp
#    src/S7PLCClient.cpp
#    src/ConfigManager.cpp
#)
#
#if(snap7_FOUND)
#    target_link_libraries(s7peek_db PRIVATE snap7::snap7)
#elseif(SNAP7_LIBRARY)
#    target_link_libraries(s7peek_db PRIVATE ${SNAP7_LIBRARY})
#else()
#    message(WARNING "Snap7 library not found for s7peek_db, linking may fail!")
#endif()

# Windows特定设置
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 winmm)
    # GUI应用程序（不显示控制台）
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# 安装规则
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES ${CMAKE_SOURCE_DIR}/gpl.txt ${CMAKE_SOURCE_DIR}/lgpl.txt DESTINATION bin)
install(DIRECTORY config/ DESTINATION bin/config)

# 打印配置信息
message(STATUS "Project: ${PROJECT_NAME}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
