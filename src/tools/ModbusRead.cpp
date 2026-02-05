#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QCoreApplication>

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{

    // Modbus RTU CRC16 (poly 0xA001)
    uint16_t crc16(const QByteArray &data)
    {
        uint16_t crc = 0xFFFF;
        for (unsigned char byte : data)
        {
            crc ^= byte;
            for (int i = 0; i < 8; ++i)
            {
                bool lsb = crc & 0x0001;
                crc >>= 1;
                if (lsb)
                {
                    crc ^= 0xA001;
                }
            }
        }
        return crc;
    }

    bool readHoldingRegisters(QSerialPort &port, uint8_t unitId, uint16_t startAddr, uint16_t count, std::vector<uint16_t> &outValues)
    {
        QByteArray frame;
        frame.append(static_cast<char>(unitId));
        frame.append(static_cast<char>(0x03)); // function code: read holding registers
        frame.append(static_cast<char>((startAddr >> 8) & 0xFF));
        frame.append(static_cast<char>(startAddr & 0xFF));
        frame.append(static_cast<char>((count >> 8) & 0xFF));
        frame.append(static_cast<char>(count & 0xFF));

        uint16_t crc = crc16(frame);
        frame.append(static_cast<char>(crc & 0xFF));        // CRC low
        frame.append(static_cast<char>((crc >> 8) & 0xFF)); // CRC high

        if (port.write(frame) != frame.size())
        {
            std::cerr << "[ERROR] Failed to write to serial port\n";
            return false;
        }
        if (!port.waitForBytesWritten(500))
        {
            std::cerr << "[ERROR] Write timeout\n";
            return false;
        }

        // 响应长度：addr(1)+func(1)+byteCount(1)+data(2*count)+crc(2)
        const int expectedLength = 5 + count * 2;
        QByteArray response;

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
        while (response.size() < expectedLength && std::chrono::steady_clock::now() < deadline)
        {
            if (port.waitForReadyRead(100))
            {
                response += port.readAll();
            }
        }

        if (response.size() < expectedLength)
        {
            std::cerr << "[ERROR] Read timeout, received " << response.size() << " bytes\n";
            return false;
        }

        // 校验 CRC
        QByteArray dataForCrc = response.left(response.size() - 2);
        uint16_t crcResp = static_cast<uint8_t>(response[response.size() - 2]) |
                           (static_cast<uint8_t>(response[response.size() - 1]) << 8);
        uint16_t crcCalc = crc16(dataForCrc);
        if (crcResp != crcCalc)
        {
            std::cerr << "[ERROR] CRC check failed\n";
            return false;
        }

        if (static_cast<uint8_t>(response[1]) >= 0x80)
        {
            std::cerr << "[ERROR] Modbus exception, code: 0x" << std::hex << static_cast<int>(response[1]) << std::dec << "\n";
            return false;
        }

        if (response[0] != static_cast<char>(unitId))
        {
            std::cerr << "[ERROR] Unit ID mismatch, received: " << static_cast<int>(static_cast<uint8_t>(response[0])) << "\n";
            return false;
        }

        const int byteCount = static_cast<uint8_t>(response[2]);
        if (byteCount != count * 2)
        {
            std::cerr << "[ERROR] Byte count mismatch, expected " << count * 2 << " actual " << byteCount << "\n";
            return false;
        }

        outValues.clear();
        outValues.reserve(count);
        for (int i = 0; i < count; ++i)
        {
            uint16_t value = (static_cast<uint8_t>(response[3 + 2 * i]) << 8) |
                             static_cast<uint8_t>(response[4 + 2 * i]);
            outValues.push_back(value);
        }
        return true;
    }

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const std::vector<std::string> args(argv + 1, argv + argc);
    const std::string portName = args.size() > 0 ? args[0] : "COM5";
    const int baud = args.size() > 1 ? std::stoi(args[1]) : 38400;
    const int unit = args.size() > 2 ? std::stoi(args[2]) : 1;
    const int start = args.size() > 3 ? std::stoi(args[3]) : 2;
    const int count = args.size() > 4 ? std::stoi(args[4]) : 3;

    QSerialPort port;
    port.setPortName(QString::fromStdString(portName));
    port.setBaudRate(baud);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);

    if (!port.open(QIODevice::ReadWrite))
    {
        std::cerr << "[ERROR] Failed to open serial port: " << port.errorString().toStdString() << "\n";
        return 1;
    }

    std::vector<uint16_t> values;
    if (!readHoldingRegisters(port, static_cast<uint8_t>(unit), static_cast<uint16_t>(start), static_cast<uint16_t>(count), values))
    {
        return 2;
    }

    std::cout << "Port: " << portName << ", Baud: " << baud << ", Unit: " << unit
              << ", Start Reg: " << start << ", Count: " << count << "\n\n";

    for (size_t i = 0; i < values.size(); ++i)
    {
        std::cout << "R" << (start + static_cast<int>(i)) << " = " << values[i] << "\n";
    }

    // 如果读取了 R2, R3, R4，解析压力值（R3=小数位数，R4=原始值）
    if (count >= 3 && start <= 2 && (start + count) > 4)
    {
        const size_t idx2 = 2 - start; // R2 在数组中的索引
        const size_t idx3 = 3 - start; // R3
        const size_t idx4 = 4 - start; // R4

        if (idx3 < values.size() && idx4 < values.size())
        {
            uint16_t decimalPlaces = values[idx3];
            uint16_t rawValue = values[idx4];

            double actualValue = rawValue;
            for (uint16_t i = 0; i < decimalPlaces; ++i)
            {
                actualValue /= 10.0;
            }

            std::cout << "\n[Parsed] Pressure = " << std::fixed << std::setprecision(decimalPlaces)
                      << actualValue << " kPa (R4=" << rawValue << " / 10^" << decimalPlaces << ")\n";
        }
    }

    return 0;
}
