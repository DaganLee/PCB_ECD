#ifndef DEVICECONTROLLER_H
#define DEVICECONTROLLER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>
#include "domain/Command.h"
#include "domain/Measurement.h"

class SerialPortService;

/**
 * @brief 设备控制器类
 * 
 * 职责：
 * - 提供高级设备操作接口（开机、设置电压、测试通信等）
 * - 封装"地址+数据"的发送序列
 * - 统一超时和错误处理策略
 * - 通过信号向UI层报告操作结果和日志
 */
class DeviceController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param serialService 串口服务指针
     * @param parent 父对象指针
     */
    explicit DeviceController(SerialPortService *serialService, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~DeviceController() override;

    /**
     * @brief 连接到指定串口
     * @param portName 串口名称
     * @param baudRate 波特率，默认9600
     * @return 是否连接成功
     */
    bool connectToDevice(const QString &portName, int baudRate = 9600);

    /**
     * @brief 断开设备连接
     */
    void disconnectDevice();

    /**
     * @brief 检查是否已连接
     * @return true表示已连接
     */
    bool isConnected() const;

    /**
     * @brief 获取当前连接的串口名称
     * @return 串口名称
     */
    QString currentPortName() const;

    /**
     * @brief 发送开机命令
     * @return 是否发送成功
     */
    bool powerOn();

    /**
     * @brief 发送关机命令
     * @return 是否发送成功
     */
    bool powerOff();

    /**
     * @brief 发送电压控制命令帧（四字节：通道+V1+V2）
     * @param channelId 通道ID（0x01=V1, 0x02=V2, 0x03=V3）
     * @param v1Voltage V1电压值（1.2-5.0V），在组帧阶段会被编码为BCD
     * @param v2Voltage V2电压值（1.60-10.80V）
     * @return 是否发送成功
     */
    bool setVoltageControl(uint8_t channelId, double v1Voltage, double v2Voltage);

    /**
     * @brief 发送V123电压控制命令帧（三字节：0x02 + 通道ID + 电压BCD）
     * @param channelId 通道ID（0x01=V1, 0x02=V2, 0x03=V3）
     * @param voltage 电压值（1.2-5.0V）
     * @return 是否发送成功
     */
    bool setV123VoltageControl(uint8_t channelId, double voltage);

    /**
     * @brief 发送V4电压控制命令帧（三字节：0x02 + 0x04 + 电压BCD）
     * @param voltage 电压值（1.60-10.80V）
     * @return 是否发送成功
     */
    bool setV4VoltageControl(double voltage);

    /**
     * @brief 发送V123微调命令（StepAdjust，三字节：0x06 + 通道ID + 指令码）
     * @param v123ChannelId 0x01=V1, 0x02=V2, 0x03=V3
     * @param action 0x01=UP, 0x02=DOWN
     * @return 是否发送成功
     */
    bool v123StepAdjust(uint8_t v123ChannelId, uint8_t action);

    /**
     * @brief 发送V4微调命令（StepAdjust，三字节：0x06 + 0x04 + 指令码）
     * @param action 0x01=UP, 0x02=DOWN
     * @return 是否发送成功
     */
    bool v4StepAdjust(uint8_t action);

    bool openVoltageChannel(uint8_t v123ChannelId, uint8_t v4ChannelId);

    /**
     * @brief 单独开启V123通道（两字节：0x12 + 通道ID）
     * @param v123ChannelId 通道ID（0x01=V1, 0x02=V2, 0x03=V3）
     * @return 是否发送成功
     */
    bool openV123Channel(uint8_t v123ChannelId);

    /**
     * @brief 单独开启V4通道（两字节：0x12 + 0x04）
     * @return 是否发送成功
     */
    bool openV4Channel();

    /**
     * @brief 发送测试命令
     * @param testData 测试数据，默认为{0x12, 0x34}
     * @return 是否发送成功
     */
    bool sendTestCommand(const QByteArray &testData = QByteArray());

    /**
     * @brief 选择电流检测通道和档位（原一体化命令，保留兼容）
     * @param rangeCode 档位码（0x01=mA, 0x02=uA）
     * @param channelCode 通道码（0x21=CH2, 0x31=CH3, 0x41=CH4）
     * @return 是否发送成功
     */
    bool selectDetectionChannel(uint8_t rangeCode, uint8_t channelCode);
    
    /**
     * @brief 启动外部电流表连续检测模式
     * @return 是否发送成功
     */
    bool startExternalMeterDetection();
    
    /**
     * @brief 停止外部电流表连续检测模式
     * @return 是否发送成功
     */
    bool stopExternalMeterDetection();

    /**

     * @brief 取消当前正在等待的命令确认
     * 
     * 用于紧急暂停等场景，强制取消当前等待的命令确认状态
     */
    void cancelPendingCommand();

    /**
     * @brief 按下继电器开机/确认键（单步按键）
     * @return 是否发送成功
     */
    bool pressPowerConfirmKey();

    /**
     * @brief 按下继电器右键
     * @return 是否发送成功
     */
    bool pressRightKey();

    /**
     * @brief 按下继电器SW3键
     * @return 是否发送成功
     */
    bool pressSw3Key();

    /**
     * @brief 按下继电器SW4键
     * @return 是否发送成功
     */
    bool pressSw4Key();

    /**
     * @brief 按下继电器SW5键
     * @return 是否发送成功
     */
    bool pressSw5Key();

    /**
     * @brief 按下继电器SW6键
     * @return 是否发送成功
     */
    bool pressSw6Key();

    /**
     * @brief 发送IAP跳转命令（跳转到Bootloader）
     * @return 是否发送成功
     * @note 此命令发送后设备将复位进入Bootloader模式
     */
    bool sendIapJumpCommand();

signals:
    /**
     * @brief 日志信息
     * @param message 日志消息
     */
    void logMessage(const QString &message);

    /**
     * @brief 连接状态变化
     * @param isConnected true表示已连接，false表示已断开
     * @param portName 端口名称
     */
    void connectionStatusChanged(bool isConnected, const QString &portName);

    /**
     * @brief 设备数据接收
     * @param data 接收到的数据
     */
    void dataReceived(const QByteArray &data);

    /**
     * @brief 命令确认完成（类型安全版本）
     * @param command 命令类型
     * @param success 是否确认成功
     * @param sentData 发送的数据
     * @param responseData 接收到的回应数据
     */
    void commandConfirmed(Command command, bool success, const QByteArray &sentData, const QByteArray &responseData);
    
    /**
     * @brief 外部电流表测量值接收
     * @param valueMa 电流值（mA）
     * 
     * 数据来自外部RS485电流表（通过MCU转发的0x50帧）
     */
    void externalMeasurementReceived(float valueMa);

private slots:
    /**
     * @brief 处理串口服务的数据接收
     * @param data 接收到的数据
     */
    void onSerialDataReceived(const QByteArray &data);

    /**
     * @brief 处理串口服务的错误
     * @param errorString 错误描述
     */
    void onSerialError(const QString &errorString);

    /**
     * @brief 处理串口状态变化
     * @param isOpen 是否打开
     */
    void onPortStatusChanged(bool isOpen);

    /**
     * @brief 处理命令确认超时
     */
    void onCommandConfirmationTimeout();

private:

    /**
     * @brief 初始化信号连接
     */
    void setupConnections();

    /**
     * @brief 发送地址和数据的组合命令（底层）
     * @param slaveAddress 从机地址
     * @param data 数据内容
     * @return 是否发送成功
     */
    bool sendAddressAndData(uint8_t slaveAddress, const QByteArray &data);

public:
    /**
     * @brief 验证电压值范围
     * @param voltage 电压值
     * @return 是否在有效范围内
     */
    bool isValidVoltage(double voltage) const;

private:

    /**
     * @brief 开始命令确认等待
     * @param command 命令类型
     * @param sentData 发送的数据
     * @param expectedResponse 期望的回应
     */
    void startCommandConfirmation(Command command, 
                                  const QByteArray &sentData, 
                                  const QByteArray &expectedResponse);

    /**
     * @brief 取消当前的命令确认等待
     */
    void cancelCommandConfirmation();

    /**
     * @brief 检查接收到的数据是否匹配期望回应
     * @param receivedData 接收到的数据
     * @return 是否匹配
     */
    bool checkResponseMatch(const QByteArray &receivedData);

    /**
     * @brief 处理命令确认成功
     * @param responseData 回应数据
     */
    void handleCommandConfirmationSuccess(const QByteArray &responseData);

    /**
     * @brief 处理命令确认失败
     * @param reason 失败原因
     */
    void handleCommandConfirmationFailure(const QString &reason);

    // 成员变量
    SerialPortService *m_serialService;         ///< 串口服务指针
    bool m_isConnected;                         ///< 连接状态标志

    // 命令确认相关成员
    Command m_pendingCommand;                   ///< 当前待确认的命令类型
    QByteArray m_sentData;                      ///< 发送的命令数据
    QByteArray m_expectedResponse;              ///< 期望的回应数据
    QByteArray m_receivedBuffer;                ///< 接收数据缓冲区
    QTimer *m_confirmationTimer;                ///< 确认超时定时器

    // 测量数据相关成员
    QByteArray m_measureDataBuffer;             ///< 测量数据接收缓冲区
    Measurement::Range m_currentRange;          ///< 当前档位
    Measurement::Channel m_currentChannel;      ///< 当前通道

    // 配置常量
    static constexpr int kConfirmationTimeoutMs = 5000;  ///< 确认超时时间（毫秒）- 增加容错性，应对20ms循环
    static constexpr int kMaxRetries = 2;                ///< 最大重试次数
    int m_retryCount;                                   ///< 当前重试次数
};

#endif // DEVICECONTROLLER_H
