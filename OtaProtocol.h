#ifndef OTAPROTOCOL_H
#define OTAPROTOCOL_H

#include <cstdint>
#include <QByteArray>

/**
 * @brief OTA 协议定义
 * 
 * 与 Bootloader 的 ota_protocol.h 完全对齐
 * 帧格式：Header(2) + Length(2) + CMD(1) + SEQ(2) + Data(N) + CRC16(2) + Tail(1)
 */
namespace OtaProtocol {

/******************************************************************************
 * 协议常量定义
 ******************************************************************************/
constexpr uint8_t FRAME_HEADER1 = 0xAA;
constexpr uint8_t FRAME_HEADER2 = 0x55;
constexpr uint8_t FRAME_TAIL = 0x5A;

constexpr uint16_t DATA_MAX_LEN = 256;      // 单帧最大数据长度
constexpr uint16_t FRAME_MIN_LEN = 10;      // 帧最小长度（不含数据）

constexpr uint16_t PACKET_DATA_SIZE = 128;  // 每个数据包的固件数据大小

/******************************************************************************
 * 命令定义（与 Bootloader 对齐）
 ******************************************************************************/
enum Command : uint8_t {
    CMD_HANDSHAKE       = 0x01,     // 握手命令（PC → MCU）
    CMD_HANDSHAKE_ACK   = 0x81,     // 握手响应（MCU → PC）
    
    CMD_START_UPGRADE   = 0x02,     // 开始升级（PC → MCU，包含固件信息）
    CMD_START_ACK       = 0x82,     // 开始响应（MCU → PC）
    
    CMD_DATA_PACKET     = 0x03,     // 数据包（PC → MCU）
    CMD_DATA_ACK        = 0x83,     // 数据包响应（MCU → PC）
    
    CMD_FINISH          = 0x04,     // 完成升级（PC → MCU）
    CMD_FINISH_ACK      = 0x84,     // 完成响应（MCU → PC）
    
    CMD_ERROR           = 0xFF,     // 错误响应（MCU → PC）
};

/******************************************************************************
 * 错误码定义
 ******************************************************************************/
enum ErrorCode : uint8_t {
    ERR_NONE            = 0x00,     // 无错误
    ERR_FRAME_FORMAT    = 0x01,     // 帧格式错误
    ERR_CRC             = 0x02,     // CRC 校验失败
    ERR_SEQ             = 0x03,     // 序号错误
    ERR_FLASH_ERASE     = 0x04,     // Flash 擦除失败
    ERR_FLASH_WRITE     = 0x05,     // Flash 写入失败
    ERR_VERIFY          = 0x06,     // 固件校验失败
    ERR_SIZE            = 0x07,     // 固件大小错误
    ERR_TIMEOUT         = 0x08,     // 超时
    ERR_UNKNOWN         = 0xFF,     // 未知错误
};

/******************************************************************************
 * 固件信息结构体（与 Bootloader OTA_FirmwareInfo_t 对齐）
 ******************************************************************************/
#pragma pack(push, 1)
struct FirmwareInfo {
    uint32_t firmware_size;         // 固件总大小
    uint32_t firmware_crc32;        // 固件 CRC32 校验值
    uint16_t packet_count;          // 总包数
    uint16_t packet_size;           // 每包大小
    uint8_t  version_major;         // 固件版本号
    uint8_t  version_minor;
    uint8_t  version_patch;
    uint8_t  reserved;
};
#pragma pack(pop)

/******************************************************************************
 * CRC 计算函数
 ******************************************************************************/

/**
 * @brief 计算 CRC16（用于帧校验）
 * @param data 数据指针
 * @param length 数据长度
 * @return CRC16 值
 */
inline uint16_t calculateCRC16(const uint8_t *data, uint16_t length)
{
    // CRC16 查找表（多项式：0x8005）
    static const uint16_t crc16_table[256] = {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
    };

    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t index = static_cast<uint8_t>(crc ^ data[i]);
        crc = (crc >> 8) ^ crc16_table[index];
    }
    return crc;
}

/**
 * @brief 计算 CRC32（用于固件校验）
 * @param data 数据指针
 * @param length 数据长度
 * @return CRC32 值
 */
inline uint32_t calculateCRC32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
    }
    return ~crc;
}

/******************************************************************************
 * 帧构建函数
 ******************************************************************************/

/**
 * @brief 构建 OTA 协议帧
 * @param cmd 命令字
 * @param seq 序号
 * @param data 数据（可为空）
 * @param dataLen 数据长度
 * @return 完整的帧数据
 */
inline QByteArray buildFrame(uint8_t cmd, uint16_t seq, const uint8_t *data, uint16_t dataLen)
{
    QByteArray frame;
    
    // 帧头
    frame.append(static_cast<char>(FRAME_HEADER1));
    frame.append(static_cast<char>(FRAME_HEADER2));
    
    // 长度（CMD + SEQ + Data）
    uint16_t payloadLen = 1 + 2 + dataLen;
    frame.append(static_cast<char>((payloadLen >> 8) & 0xFF));
    frame.append(static_cast<char>(payloadLen & 0xFF));
    
    // 命令
    frame.append(static_cast<char>(cmd));
    
    // 序号
    frame.append(static_cast<char>((seq >> 8) & 0xFF));
    frame.append(static_cast<char>(seq & 0xFF));
    
    // 数据
    if (data != nullptr && dataLen > 0) {
        frame.append(reinterpret_cast<const char*>(data), dataLen);
    }
    
    // CRC16（从 Length 到 Data 结束）
    uint16_t crc = calculateCRC16(reinterpret_cast<const uint8_t*>(frame.constData() + 2), 
                                   frame.size() - 2);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    frame.append(static_cast<char>(crc & 0xFF));
    
    // 帧尾
    frame.append(static_cast<char>(FRAME_TAIL));
    
    return frame;
}

/**
 * @brief 构建握手帧
 * @return 握手帧数据
 */
inline QByteArray buildHandshakeFrame()
{
    return buildFrame(CMD_HANDSHAKE, 0, nullptr, 0);
}

/**
 * @brief 构建开始升级帧
 * @param info 固件信息
 * @return 开始升级帧数据
 */
inline QByteArray buildStartFrame(const FirmwareInfo &info)
{
    return buildFrame(CMD_START_UPGRADE, 0, 
                      reinterpret_cast<const uint8_t*>(&info), 
                      sizeof(FirmwareInfo));
}

/**
 * @brief 构建数据包帧
 * @param seq 序号
 * @param data 固件数据
 * @param dataLen 数据长度
 * @return 数据包帧
 */
inline QByteArray buildDataFrame(uint16_t seq, const uint8_t *data, uint16_t dataLen)
{
    return buildFrame(CMD_DATA_PACKET, seq, data, dataLen);
}

/**
 * @brief 构建完成升级帧
 * @return 完成升级帧数据
 */
inline QByteArray buildFinishFrame()
{
    return buildFrame(CMD_FINISH, 0, nullptr, 0);
}

/**
 * @brief 解析响应帧，提取命令字
 * @param frame 接收到的帧数据
 * @return 命令字，解析失败返回 0
 */
inline uint8_t parseResponseCommand(const QByteArray &frame)
{
    if (frame.size() < FRAME_MIN_LEN) {
        return 0;
    }
    
    // 检查帧头
    if (static_cast<uint8_t>(frame[0]) != FRAME_HEADER1 ||
        static_cast<uint8_t>(frame[1]) != FRAME_HEADER2) {
        return 0;
    }
    
    // 检查帧尾
    if (static_cast<uint8_t>(frame[frame.size() - 1]) != FRAME_TAIL) {
        return 0;
    }
    
    // 提取命令字
    return static_cast<uint8_t>(frame[4]);
}

/**
 * @brief 解析响应帧，提取错误码（仅当命令为 CMD_ERROR 时有效）
 * @param frame 接收到的帧数据
 * @return 错误码
 */
inline uint8_t parseErrorCode(const QByteArray &frame)
{
    if (frame.size() < FRAME_MIN_LEN + 1) {
        return ERR_UNKNOWN;
    }
    
    // 错误码在数据区的第一个字节（CMD + SEQ 之后）
    return static_cast<uint8_t>(frame[7]);
}

} // namespace OtaProtocol

#endif // OTAPROTOCOL_H
