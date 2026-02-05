/**
 * @file s7peek_db.cpp
 * @brief 通用DB探针：读取任意DB起始/长度并以HEX输出，可选解析REAL/INT16
 */

#include "S7PLCClient.h"
#include "ConfigManager.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

using namespace WaterTest;

static void dump_hex(const uint8_t *data, int size)
{
    for (int i = 0; i < size; ++i)
    {
        if (i % 16 == 0)
            std::cout << "\n";
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%02X ", data[i]);
        std::cout << buf;
    }
    std::cout << "\n";
}

int main(int argc, char *argv[])
{
    int db = -1, start = 0, size = 4;
    std::string parseType; // "real" or "int16"
    std::string ip;
    int rack = -1, slot = -1;

    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto nextInt = [&](int &dst)
        { if (i + 1 < argc) dst = std::stoi(argv[++i]); };
        auto nextStr = [&](std::string &dst)
        { if (i + 1 < argc) dst = argv[++i]; };
        if (a == "--db")
            nextInt(db);
        else if (a == "--start")
            nextInt(start);
        else if (a == "--size")
            nextInt(size);
        else if (a == "--type")
            nextStr(parseType);
        else if (a == "--ip")
            nextStr(ip);
        else if (a == "--rack")
            nextInt(rack);
        else if (a == "--slot")
            nextInt(slot);
    }

    if (db < 0)
    {
        std::cerr << "用法: s7peek_db --db <num> [--start <n>] [--size <n>] [--type real|int16] [--ip <addr>] [--rack <n>] [--slot <n>]" << std::endl;
        return 1;
    }

    auto &cfg = ConfigManager::getInstance();
    cfg.loadConfig("config/system.conf");

    S7PLCClient::ConnectionParams p;
    if (!ip.empty())
        p.ipAddress = ip;
    else
        p.ipAddress = cfg.getString("plc.ip", "192.168.0.1");
    p.rack = (rack >= 0 ? rack : cfg.getInt("plc.rack", 0));
    p.slot = (slot >= 0 ? slot : cfg.getInt("plc.slot", 1));

    S7PLCClient cli;
    if (!cli.connect(p))
    {
        std::cerr << "连接失败: " << cli.getLastError() << std::endl;
        return 2;
    }

    std::vector<uint8_t> buf(size);
    auto r = cli.readDB(db, start, size, buf.data());
    if (r != S7PLCClient::Result::SUCCESS)
    {
        std::cerr << "读取失败: " << cli.getLastError() << " (db=" << db << ", start=" << start << ", size=" << size << ")" << std::endl;
        cli.disconnect();
        return 3;
    }

    std::cout << "读取成功: DB" << db << ", 起始=" << start << ", 大小=" << size << "; 数据:";
    dump_hex(buf.data(), size);

    if (parseType == "real" && size >= 4)
    {
        float v = 0.0f;
        // S7是大端，复用客户端的交换函数
        uint32_t raw;
        std::memcpy(&raw, buf.data(), 4);
        // 手动执行大小端转换
        uint32_t swapped = ((raw & 0xFF000000) >> 24) | ((raw & 0x00FF0000) >> 8) | ((raw & 0x0000FF00) << 8) | ((raw & 0x000000FF) << 24);
        std::memcpy(&v, &swapped, 4);
        std::cout << "REAL解析: " << v << "\n";
    }
    else if (parseType == "int16" && size >= 2)
    {
        uint16_t raw = (buf[0] << 8) | buf[1];
        int16_t v = static_cast<int16_t>(raw);
        std::cout << "INT16解析: " << v << "\n";
    }

    cli.disconnect();
    return 0;
}
