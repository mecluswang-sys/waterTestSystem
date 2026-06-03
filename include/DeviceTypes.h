/**
 * @file DeviceTypes.h
 * @brief Device data type definitions
 * @description Define various device data structures in the water medium test system
 */

#ifndef DEVICE_TYPES_H
#define DEVICE_TYPES_H

#include <string>
#include <chrono>
#include <cstdint>

namespace WaterTest
{

    // Device status enumeration
    enum class DeviceStatus
    {
        OFFLINE = 0,
        ONLINE = 1,
        FAULT = 2,
        MAINTENANCE = 3
    };

    // Valve status
    enum class ValveStatus
    {
        CLOSED = 0,
        OPEN = 1,
        OPENING = 2,
        CLOSING = 3,
        FAULT = 4
    };

    // Pressure sensor data
    struct PressureSensor
    {
        uint16_t id;         // Sensor ID (1-11)
        float pressure;      // Pressure value (kPa)
        int displayDecimals; // Suggested display precision from PLC metadata
        float maxPressure;   // Maximum pressure (kPa)
        float minPressure;   // Minimum pressure (kPa)
        DeviceStatus status; // Device status
        std::chrono::system_clock::time_point timestamp;

        PressureSensor() : id(0), pressure(0.0f), displayDecimals(-1), maxPressure(1000.0f),
                           minPressure(0.0f), status(DeviceStatus::OFFLINE) {}
    };

    // Flow meter data
    struct FlowMeter
    {
        uint16_t id;         // Flow meter ID (1-4)
        std::string name;    // Name (Flow meter 1, 2, 3, 4)
        float flowRate;      // Instantaneous flow rate (m³/h)
        float totalFlow;     // Total flow (m³)
        float temperature;   // Medium temperature (℃)
        uint16_t unitCode;   // Unit code from register 105
        std::string unitLabel; // Decoded engineering unit label
        uint16_t emptyPipeAlarm; // Empty pipe alarm from register 106
        uint16_t excitationAlarm; // Excitation alarm from register 107
        DeviceStatus status; // Device status
        std::chrono::system_clock::time_point timestamp;

        FlowMeter() : id(0), flowRate(0.0f), totalFlow(0.0f),
                      temperature(0.0f), unitCode(0), unitLabel("L/min"),
                      emptyPipeAlarm(0), excitationAlarm(0),
                      status(DeviceStatus::OFFLINE) {}
    };

    // Electric valve data
    struct ElectricValve
    {
        uint16_t id;               // Valve ID (1-11)
        std::string name;          // Valve name
        ValveStatus status;        // Valve status
        uint8_t openingDegree;     // Opening degree (0-100%)
        float operationTime;       // Operation time (s)
        uint32_t operationCount;   // Operation count
        DeviceStatus deviceStatus; // Device status
        std::chrono::system_clock::time_point timestamp;

        ElectricValve() : id(0), status(ValveStatus::CLOSED), openingDegree(0),
                          operationTime(0.0f), operationCount(0),
                          deviceStatus(DeviceStatus::OFFLINE) {}
    };

    // Frequency pump data
    struct FrequencyPump
    {
        uint16_t id;         // Pump ID (1-2)
        std::string name;    // Pump name
        bool isRunning;      // Is running
        float frequency;     // Frequency (Hz)
        float current;       // Current (A)
        float power;         // Power (kW)
        float speed;         // Speed (rpm)
        DeviceStatus status; // Device status
        std::chrono::system_clock::time_point timestamp;

        FrequencyPump() : id(0), isRunning(false), frequency(0.0f),
                          current(0.0f), power(0.0f), speed(0.0f),
                          status(DeviceStatus::OFFLINE) {}
    };

    // Temperature sensor data
    struct TemperatureSensor
    {
        uint16_t id;          // Sensor ID
        float temperature;    // Temperature (℃)
        float maxTemperature; // Maximum temperature
        float minTemperature; // Minimum temperature
        DeviceStatus status;  // Device status
        std::chrono::system_clock::time_point timestamp;

        TemperatureSensor() : id(0), temperature(0.0f), maxTemperature(100.0f),
                              minTemperature(-20.0f), status(DeviceStatus::OFFLINE) {}
    };

    // Regulating valve data
    struct RegulatingValve
    {
        uint16_t id;               // Regulating valve ID
        std::string name;          // Name
        float openingSetpoint;     // Commanded opening degree (0-100%)
        float openingPercent;      // Actual opening feedback (0-100%)
        ValveStatus status;        // Valve status
        DeviceStatus deviceStatus; // Device status
        std::chrono::system_clock::time_point timestamp;

        RegulatingValve() : id(0), openingSetpoint(0.0f), openingPercent(0.0f),
                            status(ValveStatus::CLOSED),
                            deviceStatus(DeviceStatus::OFFLINE) {}
    };

    // System operation mode
    enum class SystemMode
    {
        MANUAL = 0,
        AUTO = 1,
        TEST = 2,
        EMERGENCY = 3
    };

    // Test pipeline system
    enum class TestLine
    {
        DN30 = 1,
        DN50 = 2,
        DN100 = 3,
        DN150 = 4
    };

    // System status
    struct SystemStatus
    {
        SystemMode mode;       // Operation mode
        bool isRunning;        // Is running
        TestLine activeLine;   // Current active test line
        uint32_t totalTests;   // Total test count
        uint32_t successTests; // Successful test count
        uint32_t failedTests;  // Failed test count
        std::chrono::system_clock::time_point startTime;

        SystemStatus() : mode(SystemMode::MANUAL), isRunning(false),
                         activeLine(TestLine::DN30), totalTests(0),
                         successTests(0), failedTests(0) {}
    };

    // Alarm level
    enum class AlarmLevel
    {
        INFO = 0,
        WARNING = 1,
        FAULT = 2,
        CRITICAL = 3
    };

    // Alarm information
    struct AlarmInfo
    {
        uint32_t id;         // Alarm ID
        AlarmLevel level;    // Alarm level
        std::string message; // Alarm message
        std::string source;  // Alarm source
        bool isActive;       // Is active
        std::chrono::system_clock::time_point timestamp;

        AlarmInfo() : id(0), level(AlarmLevel::INFO), isActive(false) {}
    };

} // namespace WaterTest

#endif // DEVICE_TYPES_H
