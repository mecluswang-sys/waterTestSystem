/**
 * @file read_pressure_from_plc.cpp
 * @brief 简单控制台工具：连接S7-1200并读取压力数据
 *
 * 用法：
 *   - 默认仅读 Pressure_kPa (REAL)
 *   - 支持数组/多通道：通过 --sensor-id 与以下配置计算偏移
 *     - db.pressure.number   (默认1)
 *     - db.pressure.item_size(默认8)
 *     - db.pressure.value_offset (默认2) → Pressure_kPa 的相对偏移
 *   - 若仅单通道，可将 item_size 设为 0 或忽略；将直接用 value_offset 作为绝对偏移
 */

#include "S7PLCClient.h"
#include "ConfigManager.h"
#include <iostream>
#include <string>

using namespace WaterTest;

struct CliOverrides
{
    int sensorId = -1;
    std::string ip;
    int rack = -1;
    int slot = -1;
    int dbNo = -1;
    int itemSize = INT_MIN; // use INT_MIN to detect not set
    int valueOffset = -1;
};

static CliOverrides parseArgs(int argc, char *argv[])
{
    CliOverrides o;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        auto next = [&](int &dst)
        { if (i + 1 < argc) { dst = std::stoi(argv[++i]); } };
        auto nextStr = [&](std::string &dst)
        { if (i + 1 < argc) { dst = argv[++i]; } };

        if (arg == "--sensor-id")
        {
            next(o.sensorId);
        }
        else if (arg == "--ip")
        {
            nextStr(o.ip);
        }
        else if (arg == "--rack")
        {
            next(o.rack);
        }
        else if (arg == "--slot")
        {
            next(o.slot);
        }
        else if (arg == "--db")
        {
            next(o.dbNo);
        }
        else if (arg == "--item-size")
        {
            next(o.itemSize);
        }
        else if (arg == "--value-offset")
        {
            next(o.valueOffset);
        }
    }
    return o;
}

int main(int argc, char *argv[])
{
    // 1) 读取配置
    auto &cfg = ConfigManager::getInstance();
    if (!cfg.loadConfig("config/system.conf"))
    {
        std::cerr << "无法加载配置文件: config/system.conf" << std::endl;
        return 1;
    }

    std::string ip = cfg.getString("plc.ip", "192.168.0.1");
    int rack = cfg.getInt("plc.rack", 0);
    int slot = cfg.getInt("plc.slot", 1);

    // Pressure_kPa 对应 DB/偏移配置（默认 DB1.DBD2）
    int dbNo = cfg.getInt("db.pressure.number", 1);
    int itemSize = cfg.getInt("db.pressure.item_size", 8);
    int valueOffset = cfg.getInt("db.pressure.value_offset", 2);

    // 2) 解析命令行覆盖项
    auto o = parseArgs(argc, argv);
    int sensorId = (o.sensorId > 0 ? o.sensorId : 1);
    if (!o.ip.empty())
        ip = o.ip;
    if (o.rack >= 0)
        rack = o.rack;
    if (o.slot >= 0)
        slot = o.slot;
    if (o.dbNo > 0)
        dbNo = o.dbNo;
    if (o.itemSize != INT_MIN)
        itemSize = o.itemSize;
    if (o.valueOffset >= 0)
        valueOffset = o.valueOffset;
    std::cout << "Sensor ID: " << sensorId << " | IP: " << ip
              << " | Rack/Slot: " << rack << "/" << slot
              << " | DB: " << dbNo << ", itemSize: " << itemSize
              << ", valueOffset: " << valueOffset << std::endl;

    // 3) 连接PLC
    S7PLCClient plc;
    S7PLCClient::ConnectionParams params;
    params.ipAddress = ip;
    params.rack = rack;
    params.slot = slot;

    if (!plc.connect(params))
    {
        std::cerr << "PLC connect failed: " << plc.getLastError() << std::endl;
        return 2;
    }
    std::cout << "Connected: " << plc.getConnectionInfo() << std::endl;

    // 4) 计算 Pressure_kPa 的偏移：单通道直接 valueOffset，多通道 (id-1)*itemSize + valueOffset
    int offset = valueOffset;
    if (itemSize > 0)
        offset = (sensorId - 1) * itemSize + valueOffset;

    float pressureReal = 0.0f;
    auto r = plc.readReal(dbNo, offset, pressureReal);
    if (r == S7PLCClient::Result::SUCCESS)
    {
        std::cout << "Pressure_kPa (REAL): " << pressureReal << std::endl;
        plc.disconnect();
        return 0;
    }

    std::cerr << "Read failed: " << plc.getLastError() << std::endl;
    plc.disconnect();
    return 3;
}
