#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <QString>
#include <QDateTime>

/**
 * @brief 电流测量值数据结构
 * 
 * 封装从下位机接收到的测量数据，包含通道、档位、数值等完整信息
 */
struct Measurement {
    enum class Range {
        MilliAmp,   ///< mA档
        MicroAmp    ///< uA档
    };
    
    enum class Channel {
        CH1 = 0x11,
        CH2 = 0x21,
        CH3 = 0x31,
        CH4 = 0x41,
        Unknown = 0x00
    };
    
    float rawValue;         ///< 原始测量值（mA）
    Range range;            ///< 当前档位
    Channel channel;        ///< 测量通道
    QDateTime timestamp;    ///< 测量时间戳
    
    /**
     * @brief 获取显示值（根据档位自动转换单位）
     * @return 带单位的显示字符串
     */

     // 在selectDetectionChannel中更新内部的range值
     // ？？？？？？？？？？有没有必要去乘以1000？？？？？？？？？？
    QString displayValue() const {
        if (range == Range::MicroAmp) {
            // uA档：将mA转换为uA显示
            float valueUA = rawValue * 1000.0f;
            return QString("%1 uA").arg(valueUA, 0, 'f', 2);
        } else {
            // mA档：直接显示mA
            return QString("%1 mA").arg(rawValue, 0, 'f', 3);
        }
    }
    
    /**
     * @brief 获取数值（自动转换单位）
     * @return 显示数值
     */
    double displayNumber() const {
        if (range == Range::MicroAmp) {
            return rawValue * 1000.0;  // mA -> uA
        } else {
            return rawValue;
        }
    }
    
    /**
     * @brief 获取单位字符串
     * @return 单位
     */
    QString unit() const {
        return (range == Range::MicroAmp) ? "uA" : "mA";
    }
    
    /**
     * @brief 通道转字符串
     */
    static QString channelToString(Channel ch) {
        switch (ch) {
        case Channel::CH1: return "CH1";
        case Channel::CH2: return "CH2";
        case Channel::CH3: return "CH3";
        case Channel::CH4: return "CH4";
        default: return "Unknown";
        }
    }
};

#endif // MEASUREMENT_H

