#ifndef OTACONTROLLER_H
#define OTACONTROLLER_H

#include <QObject>
#include <QSerialPort>
#include <QFile>
#include <QTimer>
#include <QByteArray>
#include "OtaProtocol.h"

/**
 * @brief OTA 升级控制器
 * 
 * 职责：
 * - 管理独立的 9600bps 串口连接（与现有业务串口分离）
 * - 读取 .bin 固件文件并分包发送
 * - 实现 OTA 协议状态机
 * - 处理超时重传
 * - 发送进度更新信号
 */
class OtaController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief OTA 升级状态
     */
    enum State {
        Idle,               ///< 空闲状态
        Connecting,         ///< 正在连接（握手）
        StartingUpgrade,    ///< 发送开始升级命令
        SendingData,        ///< 发送数据包中
        WaitingFinish,      ///< 等待完成确认
        Completed,          ///< 升级完成
        Error               ///< 错误状态
    };
    Q_ENUM(State)

    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit OtaController(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~OtaController() override;

    /**
     * @brief 开始 OTA 升级
     * @param portName 串口名称
     * @param firmwarePath 固件文件路径 (.bin)
     * @return true 启动成功，false 启动失败
     */
    bool startUpgrade(const QString &portName, const QString &firmwarePath);

    /**
     * @brief 取消升级
     */
    void cancelUpgrade();

    /**
     * @brief 获取当前状态
     * @return 当前状态
     */
    State currentState() const { return m_state; }

    /**
     * @brief 检查是否正在升级
     * @return true 正在升级
     */
    bool isUpgrading() const { return m_state != Idle && m_state != Completed && m_state != Error; }

signals:
    /**
     * @brief 升级进度更新信号
     * @param percent 进度百分比 (0-100)
     */
    void progressChanged(int percent);

    /**
     * @brief 升级完成信号
     * @param success true 成功，false 失败
     * @param message 结果消息
     */
    void upgradeFinished(bool success, const QString &message);

    /**
     * @brief 状态变化信号
     * @param state 新状态
     */
    void stateChanged(State state);

    /**
     * @brief 日志消息信号
     * @param message 日志内容
     */
    void logMessage(const QString &message);

private slots:
    /**
     * @brief 串口数据接收处理
     */
    void onSerialDataReady();

    /**
     * @brief 超时处理
     */
    void onTimeout();

private:
    /**
     * @brief 打开串口（9600bps, 8N1）
     * @param portName 串口名称
     * @return true 成功
     */
    bool openSerialPort(const QString &portName);

    /**
     * @brief 关闭串口
     */
    void closeSerialPort();

    /**
     * @brief 加载固件文件
     * @param filePath 文件路径
     * @return true 成功
     */
    bool loadFirmware(const QString &filePath);

    /**
     * @brief 发送握手帧
     */
    void sendHandshake();

    /**
     * @brief 发送开始升级帧
     */
    void sendStartUpgrade();

    /**
     * @brief 发送下一个数据包
     */
    void sendNextDataPacket();

    /**
     * @brief 发送完成帧
     */
    void sendFinish();

    /**
     * @brief 处理接收到的响应
     * @param response 响应数据
     */
    void processResponse(const QByteArray &response);

    /**
     * @brief 设置状态
     * @param state 新状态
     */
    void setState(State state);

    /**
     * @brief 完成升级（成功或失败）
     * @param success 是否成功
     * @param message 消息
     */
    void finishUpgrade(bool success, const QString &message);

    /**
     * @brief 启动超时定时器
     * @param ms 超时时间（毫秒）
     */
    void startTimeoutTimer(int ms = 3000);

    /**
     * @brief 停止超时定时器
     */
    void stopTimeoutTimer();

private:
    // 串口相关
    QSerialPort *m_serialPort;          ///< 独立的 OTA 串口
    QByteArray m_rxBuffer;              ///< 接收缓冲区

    // 固件相关
    QByteArray m_firmwareData;          ///< 固件数据
    OtaProtocol::FirmwareInfo m_fwInfo; ///< 固件信息
    uint16_t m_currentPacket;           ///< 当前包序号
    uint16_t m_totalPackets;            ///< 总包数

    // 状态机
    State m_state;                      ///< 当前状态
    int m_retryCount;                   ///< 重试计数
    static constexpr int MAX_RETRY = 3; ///< 最大重试次数

    // 定时器
    QTimer *m_timeoutTimer;             ///< 超时定时器
};

#endif // OTACONTROLLER_H
