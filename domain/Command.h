#ifndef COMMAND_H
#define COMMAND_H

#include <QString>
#include <QObject>
/**
 * @brief 设备命令枚举（类型安全）
 * 
 * 用于替代"字符串匹配"的命令分发，避免i18n风险
 */
enum class Command {
    None,               ///< 无命令
    TestCommand,        ///< 测试通信
    PowerOn,            ///< 普通开机
    FirstPowerOn,       ///< 首次开机
    PowerOff,           ///< 关机
    VoltageControl,
    V123VoltageControl, ///< V123电压控制（3字节：0x02 + 通道ID + 电压）
    V4VoltageControl,   ///< V4电压控制（3字节：0x02 + 0x04 + 电压）
    StepAdjust,
    VoltageChannelOpen,
    V123ChannelOpen,    ///< V123通道开启（2字节：0x12 + 通道ID）
    V4ChannelOpen,      ///< V4通道开启（2字节：0x12 + 0x04）
    DetectionSelect,    ///< 电流检测通道选择（原一体化命令，保留兼容）
    ChannelConfig,      ///< 通道配置（仅配置档位和通道，不启动采样）
    StartDetection,     ///< 开始检测（使用外部电流表测量）
    PauseDetection,     ///< 暂停检测（停止采样和数据发送）
    RelayPowerConfirm,  ///< 继电器-开机/确认键（单步按键）
    RelayRight,         ///< 继电器-右键
    RelaySw3,           ///< 继电器-SW3按键
    RelaySw4,           ///< 继电器-SW4按键
    RelaySw5,           ///< 继电器-SW5按键
    RelaySw6,           ///< 继电器-SW6按键
    StopExternalMeter   ///< 停止外部电流表连续检测
};

/**
 * @brief 将命令枚举转换为可读字符串（用于日志/UI显示）
 * @param cmd 命令枚举
 * @return 本地化的命令名称
 */
inline QString commandToString(Command cmd) {
    switch (cmd) {
    case Command::TestCommand:      return QObject::tr("测试通信");
    case Command::PowerOn:          return QObject::tr("开机");
    case Command::FirstPowerOn:     return QObject::tr("首次开机");
    case Command::PowerOff:         return QObject::tr("关机");
    case Command::VoltageControl:   return QObject::tr("输出电压控制");
    case Command::V123VoltageControl: return QObject::tr("V123电压控制");
    case Command::V4VoltageControl: return QObject::tr("V4电压控制");
    case Command::StepAdjust:       return QObject::tr("电压微调");
    case Command::VoltageChannelOpen: return QObject::tr("电压输出通道开启");
    case Command::V123ChannelOpen: return QObject::tr("V123通道开启");
    case Command::V4ChannelOpen: return QObject::tr("V4通道开启");
    case Command::DetectionSelect:  return QObject::tr("电流检测通道选择");
    case Command::ChannelConfig:    return QObject::tr("通道配置");
    case Command::StartDetection:   return QObject::tr("开始检测（电流表）");
    case Command::PauseDetection:   return QObject::tr("暂停检测");
    case Command::RelayPowerConfirm: return QObject::tr("继电器-确认键");
    case Command::RelayRight:       return QObject::tr("继电器-右键");
    case Command::StopExternalMeter: return QObject::tr("停止外部电流表检测");
    default:                        return QObject::tr("未知操作");
    }
}

#endif // COMMAND_H

