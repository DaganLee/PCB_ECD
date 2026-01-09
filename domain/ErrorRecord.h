#ifndef ERRORRECORD_H
#define ERRORRECORD_H

#include <QString>
#include <QDateTime>

/**
 * @brief 测试错误记录结构
 * 
 * 用于记录自动化测试过程中发生的错误，包含精确到子动作级别的错误信息
 */
struct ErrorRecord {
    int stepIndex;              ///< 步骤索引（0-based）
    QString stepName;           ///< 步骤名称（如"第一步测试"）
    int actionIndex;            ///< 子动作索引（-1表示步骤级错误，如超时）
    QString actionDescription;  ///< 动作描述（如"检测工作电流"、"用户确认电量显示"）
    QString errorType;          ///< 错误类型（如"电流超限"、"用户取消"、"超时"）
    QString errorDetail;        ///< 详细错误信息
    QDateTime timestamp;        ///< 错误发生时间
    
    // 测量数据（用于电流检测失败，无效时为-1）
    double measuredValue;       ///< 实际测量值（mA）
    double thresholdValue;      ///< 阈值（mA）
    
    /**
     * @brief 默认构造函数
     */
    ErrorRecord()
        : stepIndex(-1)
        , actionIndex(-1)
        , timestamp(QDateTime::currentDateTime())
        , measuredValue(-1.0)
        , thresholdValue(-1.0)
    {}
    
    /**
     * @brief 构造函数（通用错误）
     */
    ErrorRecord(int step, const QString &stepNm, int action, 
                const QString &actionDesc, const QString &errType, 
                const QString &errDetail)
        : stepIndex(step)
        , stepName(stepNm)
        , actionIndex(action)
        , actionDescription(actionDesc)
        , errorType(errType)
        , errorDetail(errDetail)
        , timestamp(QDateTime::currentDateTime())
        , measuredValue(-1.0)
        , thresholdValue(-1.0)
    {}
    
    /**
     * @brief 构造函数（电流检测错误，包含测量数据）
     */
    ErrorRecord(int step, const QString &stepNm, int action,
                const QString &actionDesc, const QString &errType,
                const QString &errDetail, double measured, double threshold)
        : stepIndex(step)
        , stepName(stepNm)
        , actionIndex(action)
        , actionDescription(actionDesc)
        , errorType(errType)
        , errorDetail(errDetail)
        , timestamp(QDateTime::currentDateTime())
        , measuredValue(measured)
        , thresholdValue(threshold)
    {}
    
    /**
     * @brief 是否包含有效的测量数据
     */
    bool hasMeasurementData() const {
        return measuredValue >= 0.0 && thresholdValue >= 0.0;
    }
    
    /**
     * @brief 获取格式化的测量值字符串
     */
    QString formattedMeasuredValue() const {
        if (!hasMeasurementData()) {
            return QString("-");
        }
        return QString("%1 mA").arg(measuredValue, 0, 'f', 3);
    }
    
    /**
     * @brief 获取格式化的阈值字符串
     */
    QString formattedThreshold() const {
        if (!hasMeasurementData()) {
            return QString("-");
        }
        return QString("%1 mA").arg(thresholdValue, 0, 'f', 3);
    }
};

#endif // ERRORRECORD_H
