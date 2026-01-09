#ifndef DEVICEPROTOCOL_H
#define DEVICEPROTOCOL_H

#include <QtGlobal>
#include <QByteArray>
#include <QString>

namespace DeviceProtocol {

// 基本通信参数
constexpr uint8_t kSlaveAddress = 0xC0;
constexpr int kBaud = 9600;
constexpr int kWriteTimeoutMs = 1000;
constexpr int kReadTimeoutMs = 1000;

// 命令字
enum class CommandId : uint8_t {
    Power = 0x01,
    Voltage = 0x02,             // 输出电压控制
    Detection = 0x03,           // 原一体化检测命令（保留兼容）
    ChannelConfig = 0x04,       // 通道配置命令（仅配置档位和通道）
    StartDetection = 0x50,      // 开始检测命令（启动外部电流表连续检测）
    StepAdjust = 0x06,          // 微调命令（上/下步进）
    PauseDetection = 0xAA,      // 暂停检测命令（0xAA在float32中几乎不会出现，避免与测量帧混淆）
    VoltageChannelOpen = 0x12,  // v1234电压输出通道开启命令
    IapJump = 0x99              // IAP跳转命令（跳转到Bootloader）
};

// IAP跳转指令第二字节确认码
constexpr uint8_t kIapJumpAck2 = 0xAA;

// 【协议改进】暂停检测确认帧第2字节
// 与0xAA组成双字节确认[0xAA, 0x55]，避免与float数据误判
constexpr uint8_t kPauseDetectionAck2 = 0x55;

// 电流检测档位枚举
enum class RangeCode : uint8_t {
    MilliAmp = 0x01,    ///< mA档
    MicroAmp = 0x02     ///< uA档
};

// 电流检测通道枚举
enum class ChannelCode : uint8_t {
    CH1 = 0x11,
    CH2 = 0x21,
    CH3 = 0x31,
    CH4 = 0x41
};

// 继电器按键码枚举
enum class RelayKeyCode : uint8_t {
    Right = 0x02,           ///< 右键
    PowerConfirm = 0x03,    ///< 开机/确认键（单步按键）
    Sw3 = 0x31,             ///< SW3按键
    Sw4 = 0x41,             ///< SW4按键
    Sw5 = 0x51,             ///< SW5按键
    Sw6 = 0x61              ///< SW6按键
};

// 枚举转字符串工具函数
inline QString rangeCodeToString(RangeCode range) {
    return (range == RangeCode::MilliAmp) ? "mA" : "uA";
}

inline QString channelCodeToString(ChannelCode channel) {
    switch (channel) {
    case ChannelCode::CH1: return "CH1";
    case ChannelCode::CH2: return "CH2";
    case ChannelCode::CH3: return "CH3";
    case ChannelCode::CH4: return "CH4";
    default: return "Unknown";
    }
}

// 电压数值映射：0.0V ~ 9.9V → BCD单字节编码（0x00 ~ 0x99）
// 映射规则：整数位→高4位，小数第一位→低4位
// 示例：9.9→0x99，0.1→0x01，5.9→0x59
// 超过一位小数时四舍五入到一位，边界强制压回 [0.0, 9.9]
inline uint8_t encodeVoltage(double voltage) {
    // 四舍五入到一位小数
    voltage = qRound(voltage * 10.0) / 10.0;
    
    // 边界处理：强制压回 [0.0, 9.9]
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 9.9) voltage = 9.9;
    
    // 分离整数位和小数位
    int integerPart = static_cast<int>(voltage);           // 整数位：0-9
    int decimalPart = static_cast<int>(qRound((voltage - integerPart) * 10.0)); // 小数第一位：0-9
    
    // BCD编码：高4位=整数位，低4位=小数位
    return static_cast<uint8_t>((integerPart << 4) | decimalPart);
}

// V4电压映射：将电压值映射为下位机特定的指令码
// 对于特殊电压值使用预定义的指令码，其他使用BCD编码
inline uint8_t encodeV4Voltage(double voltage) {
    // 四舍五入到两位小数
    voltage = qRound(voltage * 100.0) / 100.0;
    
    // 特殊电压值映射（匹配下位机CMD_V4_VOLTAGE_xxx宏定义）
    if (qAbs(voltage - 2.90) < 0.01) return 0x29;  // CMD_V4_VOLTAGE_2P9
    if (qAbs(voltage - 3.20) < 0.01) return 0x32;  // CMD_V4_VOLTAGE_3P2
    if (qAbs(voltage - 3.45) < 0.01) return 0xD9;  // CMD_V4_VOLTAGE_3P45 (特殊码)
    if (qAbs(voltage - 3.65) < 0.01) return 0xDB;  // CMD_V4_VOLTAGE_3P65 (特殊码)
    if (qAbs(voltage - 3.85) < 0.01) return 0xDD;  // CMD_V4_VOLTAGE_3P85 (特殊码)
    if (qAbs(voltage - 3.90) < 0.01) return 0x39;  // CMD_V4_VOLTAGE_3P9
    if (qAbs(voltage - 4.05) < 0.01) return 0xE5;  // CMD_V4_VOLTAGE_4P05 (特殊码)
    if (qAbs(voltage - 4.70) < 0.01) return 0x47;  // CMD_V4_VOLTAGE_4P7
    if (qAbs(voltage - 5.50) < 0.01) return 0x55;  // CMD_V4_VOLTAGE_5P5
    if (qAbs(voltage - 0.00) < 0.01) return 0x00;  // CMD_V4_VOLTAGE_OFF
    
    // 其他电压值使用BCD编码（自定义电压）
    return encodeVoltage(voltage);
}

// 控制开机帧构造
inline QByteArray buildPowerOn() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Power)));
    data.append(static_cast<char>(0x01));
    return data;
}

// 控制关机帧构造
inline QByteArray buildPowerOff() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Power)));
    data.append(static_cast<char>(0x00));
    return data;
}

// 控制输出电压帧构造（四字节）：命令字 + 通道ID + V1电压BCD + V2电压码
inline QByteArray buildVoltageControl(uint8_t channelId, double v1Voltage, double v2Voltage) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Voltage)));  // 第1字节：命令字(0x02)
    data.append(static_cast<char>(channelId));                                 // 第2字节：通道ID(0x01/0x02/0x03)
    // 在组帧阶段将数值电压编码为单字节BCD
    data.append(static_cast<char>(encodeVoltage(v1Voltage)));                 // 第3字节：V1电压BCD
    data.append(static_cast<char>(encodeVoltage(v2Voltage)));                 // 第4字节：V2电压码
    return data;
}

// V123电压控制帧构造（三字节）：命令字(0x02) + 通道ID(0x01/0x02/0x03) + 电压BCD
inline QByteArray buildV123VoltageControl(uint8_t channelId, double voltage) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Voltage)));  // 第1字节：命令字(0x02)
    data.append(static_cast<char>(channelId));                                 // 第2字节：通道ID(0x01/0x02/0x03)
    data.append(static_cast<char>(encodeVoltage(voltage)));                   // 第3字节：电压BCD
    return data;
}

// V4电压控制帧构造（三字节）：命令字(0x02) + 通道ID(0x04) + 特定指令码
inline QByteArray buildV4VoltageControl(double voltage) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Voltage)));  // 第1字节：命令字(0x02)
    data.append(static_cast<char>(0x04));                                      // 第2字节：V4通道ID固定为0x04
    data.append(static_cast<char>(encodeV4Voltage(voltage)));                 // 第3字节：V4特定指令码
    return data;
}

inline QByteArray buildVoltageChannelOpen(uint8_t v123ChannelId, uint8_t v4ChannelId) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::VoltageChannelOpen)));
    data.append(static_cast<char>(v123ChannelId));
    data.append(static_cast<char>(v4ChannelId));
    return data;
}

// V123通道开启帧构造（两字节）：命令字(0x12) + 通道ID(0x01/0x02/0x03)
inline QByteArray buildV123ChannelOpen(uint8_t v123ChannelId) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::VoltageChannelOpen)));
    data.append(static_cast<char>(v123ChannelId));
    return data;
}

// V4通道开启帧构造（两字节）：命令字(0x12) + 通道ID(0x04)
inline QByteArray buildV4ChannelOpen() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::VoltageChannelOpen)));
    data.append(static_cast<char>(0x04)); // V4通道ID固定为0x04
    return data;
}

// 电流检测通道选择帧构造（三字节）：命令字 + 档位码 + 通道码（原一体化命令，保留兼容）
inline QByteArray buildDetection(uint8_t rangeCode, uint8_t channelCode) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Detection))); // 第1字节：命令字
    data.append(static_cast<char>(rangeCode));                                  // 第2字节：档位码
    data.append(static_cast<char>(channelCode));                                // 第3字节：通道码
    return data;
}

inline QByteArray buildStartDetection() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::StartDetection))); // 第1字节：命令字
    return data;
}

// 暂停检测帧构造（一字节）：仅命令字
inline QByteArray buildPauseDetection() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::PauseDetection))); // 第1字节：命令字 0xAA
    return data;
}

// 【协议改进】暂停检测期望回复帧构造（双字节）：[0xAA, 0x55]
// 用于上位机验证暂停确认
inline QByteArray buildPauseDetectionExpectedResponse() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::PauseDetection))); // 0xAA
    data.append(static_cast<char>(kPauseDetectionAck2));                              // 0x55
    return data;
}

// V123微调帧构造（三字节）：命令字(0x06) + 通道ID(0x01/0x02/0x03) + 指令码(0x01=UP,0x02=DOWN)
inline QByteArray buildV123StepAdjust(uint8_t v123ChannelId, uint8_t action) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::StepAdjust)));
    data.append(static_cast<char>(v123ChannelId));
    data.append(static_cast<char>(action));
    return data;
}

// V4微调帧构造（三字节）：命令字(0x06) + 通道ID(0x04) + 指令码(0x01=UP,0x02=DOWN)
inline QByteArray buildV4StepAdjust(uint8_t action) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::StepAdjust)));
    data.append(static_cast<char>(0x04)); // V4通道ID
    data.append(static_cast<char>(action));
    return data;
}

// 继电器按键模拟帧构造（两字节）：命令字 + 按键码（复用Power命令字）
inline QByteArray buildRelayKey(RelayKeyCode keyCode) {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::Power)));  // 第1字节：命令字（Power）
    data.append(static_cast<char>(static_cast<uint8_t>(keyCode)));           // 第2字节：按键码
    return data;
}

// IAP跳转指令帧构造（两字节）：0x99 + 0xAA
// 用于通知APP跳转到Bootloader进行固件升级
inline QByteArray buildIapJump() {
    QByteArray data;
    data.append(static_cast<char>(static_cast<uint8_t>(CommandId::IapJump))); // 第1字节：命令字 0x99
    data.append(static_cast<char>(kIapJumpAck2));                              // 第2字节：确认码 0xAA
    return data;
}

// 十六进制字符串
inline QString toHex(const QByteArray &bytes) {
    return bytes.toHex(' ').toUpper();
}

} // namespace DeviceProtocol

#endif // DEVICEPROTOCOL_H


