#ifndef PROTOCOLPARSER_H
#define PROTOCOLPARSER_H

#include <QByteArray>
#include <QVector>
#include <QDateTime>
#include "../domain/Measurement.h"

#include <QDebug>

/**
 * @brief 协议解析器
 * 
 * 职责：
 * - 从字节流中解析测量帧（0x13 + 4字节float）
 * - 提供纯函数接口，易于测试
 * - 不持有状态，由调用方管理缓冲区
 * 
 * 【协议改进】帧格式说明：
 * - 测量帧：[0x13] + [4字节float little-endian]
 * - 开始检测确认帧：[0x05]（单字节，与测量帧分离）
 * - 暂停检测确认帧：[0xAA] + [0x55]（双字节特征码）
 */
class ProtocolParser
{
public:
    /**
     * @brief 解析外部电流表测量帧（带帧头：0x50 + 4字节float）
     * @param buffer 输入/输出缓冲区，解析后会移除已处理的帧
     * @param[out] value 解析出的电流值（mA）
     * @return 是否成功解析出一帧数据
     * 
     * 帧格式：[0x50] + [4字节float little-endian]
     * 数据来自外部RS485电流表，由MCU转发
     */
    static bool parseExternalMeasurementWithHeader(QByteArray &buffer, float &outValue)
    {        
        // 检查是否有完整的外部测量帧（1字节帧头 + 4字节float）
        if (buffer.size() < 5) {
            return false;
        }
        
        // 检查帧头是否为0x50
        if (static_cast<uint8_t>(buffer[0]) != 0x50) {
            // 帧头不匹配，移除首字节继续查找
            buffer.remove(0, 1);
            return false;
        }
        
        // 提取4字节float（小端序）
        uint8_t bytes[4];
        bytes[0] = static_cast<uint8_t>(buffer[1]); // LSB
        bytes[1] = static_cast<uint8_t>(buffer[2]);
        bytes[2] = static_cast<uint8_t>(buffer[3]);
        bytes[3] = static_cast<uint8_t>(buffer[4]); // MSB
        
        // 转换为float
        float value;
        memcpy(&value, bytes, sizeof(float));        
        outValue = value;
        
        // 移除已处理的5字节（帧头+数据）
        buffer.remove(0, 5);        
        return true;
    }
    
    /**
     * @brief 解析外部电流表测量帧（无帧头，仅4字节float，兼容旧版本）
     * @deprecated 请使用 parseExternalMeasurementWithHeader
     */
    static bool parseExternalMeasurement(QByteArray &buffer, float &outValue)
    {        
        // 检查是否有完整的外部测量帧（4字节float）
        if (buffer.size() < 4) {
            return false;
        }
        
        // 提取4字节float（小端序）
        uint8_t bytes[4];
        bytes[0] = static_cast<uint8_t>(buffer[0]); // LSB
        bytes[1] = static_cast<uint8_t>(buffer[1]);
        bytes[2] = static_cast<uint8_t>(buffer[2]);
        bytes[3] = static_cast<uint8_t>(buffer[3]); // MSB
        
        // 转换为float
        float value;
        memcpy(&value, bytes, sizeof(float));        
        outValue = value;
        
        // 移除已处理的4字节
        buffer.remove(0, 4);        
        return true;
    }
    
    /**
     * @brief 验证确认帧是否匹配期望
     * @param receivedData 接收到的数据
     * @param expectedResponse 期望的回应
     * @return 是否匹配
     * 
     * 【协议改进】支持的确认帧类型：
     * - 开始检测：单字节 [0x05]
     * - 暂停检测：双字节 [0xAA, 0x55]
     * - 其他控制命令：多字节回显
     */
    static bool checkResponseMatch(const QByteArray &receivedData, 
                                    const QByteArray &expectedResponse)
    {
        if (expectedResponse.isEmpty() || receivedData.size() < expectedResponse.size()) {
            return false;
        }
        
        // 【协议改进】双字节暂停确认帧 [0xAA, 0x55] 的专用匹配
        // 这个双字节组合在float32中几乎不可能出现，可以直接匹配
        if (expectedResponse.size() == 2 && 
            static_cast<uint8_t>(expectedResponse[0]) == 0xAA &&
            static_cast<uint8_t>(expectedResponse[1]) == 0x55) {
            // 在接收数据中查找连续的 [0xAA, 0x55]
            for (int i = 0; i < receivedData.size() - 1; i++) {
                if (static_cast<uint8_t>(receivedData[i]) == 0xAA &&
                    static_cast<uint8_t>(receivedData[i + 1]) == 0x55) {
                    return true;
                }
            }
            return false;
        }
        
        // 防止误判电流测量帧的数据部分为控制帧
        // 对于单字节控制帧，需要验证它不是测量帧(0x13 + float)的一部分
        if (expectedResponse.size() == 1) {
            uint8_t expectedByte = static_cast<uint8_t>(expectedResponse[0]);

            // 使用逐字节 uint8_t 比较，避免 indexOf 的符号问题
            // QByteArray::indexOf 使用 char（有符号），0xAA = -86，可能无法正确匹配
            int matchPos = -1;
            for (int i = 0; i < receivedData.size(); i++) {
                if (static_cast<uint8_t>(receivedData[i]) == expectedByte) {
                    matchPos = i;
                    break;
                }
            }
            
            if (matchPos < 0) {
                return false;
            }
            
            // 【协议改进】0x05 (开始检测确认) 的简化匹配
            // 由于下位机现在只发送单独的 [0x05] 确认帧，不再混合测量帧
            // 可以更宽松地接受匹配
            if (expectedByte == 0x05) {
                return true;  // 直接接受
            }
            
            // ===== 0xAA (暂停指令) 的处理已移至双字节匹配 =====
            // 单字节 0xAA 匹配保留用于向后兼容（如果下位机未升级）
            if (expectedByte == 0xAA) {
                return true;  // 向后兼容：接受单字节0xAA
            }
            
            // ===== 其他控制指令的严格匹配逻辑 =====
            // 检查匹配位置是否紧跟在完整电流测量帧后面
            // 测量帧固定5字节：0x13 + 4字节float
            // 如果 matchPos >= 5 且 buffer[matchPos-5] == 0x13，说明前面是完整测量帧
            if (matchPos >= 5 && static_cast<uint8_t>(receivedData[matchPos - 5]) == 0x13) {
                // 前面是完整测量帧，当前位置是独立控制帧
                return true;
            }
            
            // 如果 matchPos == 0，说明控制帧在最开头
            if (matchPos == 0) {
                return true;
            }
            
            // 如果 matchPos 在1-4之间，需要检查是否在测量帧内部
            if (matchPos > 0 && matchPos < 5) {
                // 检查前面是否有0x13（如果有，说明在测量帧内部）
                for (int i = 0; i < matchPos; i++) {
                    if (static_cast<uint8_t>(receivedData[i]) == 0x13) {
                        return false; // 在测量帧内部，拒绝匹配
                    }
                }
                // 前面没有0x13，接受匹配
                return true;
            }
            
            // matchPos > 5 但前面不是完整测量帧的情况
            // 检查 matchPos-4 到 matchPos-1 之间是否有0x13
            for (int i = (matchPos >= 4 ? matchPos - 4 : 0); i < matchPos; i++) {
                if (static_cast<uint8_t>(receivedData[i]) == 0x13) {
                    return false; // 可能在测量帧内部
                }
            }
            
            return true; // 安全的独立控制帧
        }
        
        // 多字节控制帧，直接startsWith匹配
        // 判断receivedData是否以expectedResponse开头
        return receivedData.startsWith(expectedResponse);
    }
};

#endif // PROTOCOLPARSER_H
