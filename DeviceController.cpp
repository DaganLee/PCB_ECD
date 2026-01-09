#include "DeviceController.h"
#include "SerialPortService.h"
#include "DeviceProtocol.h"
#include "protocol/ProtocolParser.h"

#include <QThread>
#include <QDateTime>
#include <cmath>

#include <QDebug>

DeviceController::DeviceController(SerialPortService *serialService, QObject *parent)
    : QObject(parent)
    , m_serialService(serialService)
    , m_isConnected(false)
    , m_pendingCommand(Command::None)
    , m_confirmationTimer(new QTimer(this))
    , m_currentRange(Measurement::Range::MilliAmp)
    , m_currentChannel(Measurement::Channel::CH1)
    , m_retryCount(0)
{
    Q_ASSERT(m_serialService != nullptr);
    setupConnections();
    
    // 配置确认定时器
    m_confirmationTimer->setSingleShot(true);
    connect(m_confirmationTimer, &QTimer::timeout, this, &DeviceController::onCommandConfirmationTimeout);
}

DeviceController::~DeviceController()
{
    cancelCommandConfirmation();
    disconnectDevice();
}

void DeviceController::setupConnections()
{
    // 连接串口服务的信号
    connect(m_serialService, &SerialPortService::dataReceived,
            this, &DeviceController::onSerialDataReceived);
    connect(m_serialService, &SerialPortService::errorOccurred,
            this, &DeviceController::onSerialError);
    connect(m_serialService, &SerialPortService::portStatusChanged,
            this, &DeviceController::onPortStatusChanged);
}

bool DeviceController::connectToDevice(const QString &portName, int baudRate)
{
    // 避免重复打开或串口占用，实现“先断后连”的安全流程
    if (m_isConnected) {
        disconnectDevice();
    }

    emit logMessage(tr("正在连接到串口: %1").arg(portName));

    if (m_serialService->openPort(portName, baudRate)) {
        m_isConnected = true;
        emit logMessage(tr("成功连接到串口: %1 (%2,8,N,1)").arg(portName).arg(baudRate));
        emit connectionStatusChanged(true, portName);
        return true;
    } else {
        emit logMessage(tr("连接失败: %1").arg(portName));
        return false;
    }
}

void DeviceController::disconnectDevice()
{
    if (m_isConnected) {
        // 获取当前连接的串口名称
        QString portName = m_serialService->portName();
        m_serialService->closePort();
        m_isConnected = false;
        emit logMessage(tr("已断开连接: %1").arg(portName));
        emit connectionStatusChanged(false, portName);
    }
}

bool DeviceController::isConnected() const
{
    return m_isConnected && m_serialService && m_serialService->isOpen();
}

QString DeviceController::currentPortName() const
{
    return m_serialService ? m_serialService->portName() : QString();
}

bool DeviceController::powerOn()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("开机指令：开始向从机发送开机命令..."));

    // 开机帧：0xC0 0x01 0x01
    QByteArray powerOnData = DeviceProtocol::buildPowerOn();
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, powerOnData);

    if (success) {
        emit logMessage(tr("开机指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(powerOnData)));
        
        // 开始等待确认，期望回应 0x01 0x01
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));
        expectedResponse.append(static_cast<char>(0x01));
        startCommandConfirmation(Command::PowerOn, powerOnData, expectedResponse);
    } else {
        emit logMessage(tr("错误：开机命令发送失败"));
    }

    return success;
}

bool DeviceController::powerOff()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("关机指令：开始向从机发送关机命令..."));

    // 关机帧：0xC0 0x01 0x00
    QByteArray powerOffData = DeviceProtocol::buildPowerOff();
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, powerOffData);

    if (success) {
        emit logMessage(tr("关机指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(powerOffData)));
        
        // 开始等待确认，期望回应 0x01 0x00
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));
        expectedResponse.append(static_cast<char>(0x00));
        startCommandConfirmation(Command::PowerOff, powerOffData, expectedResponse);
    } else {
        emit logMessage(tr("错误：关机命令发送失败"));
    }

    return success;
}

bool DeviceController::setVoltageControl(uint8_t channelId, double v1Voltage, double v2Voltage)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 验证通道ID
    if (channelId != 0x01 && channelId != 0x02 && channelId != 0x03) {
        emit logMessage(tr("错误：无效的通道ID 0x%1").arg(channelId, 2, 16, QChar('0')));
        return false;
    }

    // 再次电压范围验证有点多余
    // 验证V1电压数值范围（1.2~5.0V）——不在此处转换为BCD，组帧时再编码
    if (std::isnan(v1Voltage) || v1Voltage < 1.2 || v1Voltage > 5.0) {
        emit logMessage(tr("错误：V1电压值无效或超出范围（1.2~5.0V） 当前=%1V").arg(v1Voltage, 0, 'f', 1));
        return false;
    }

    // 验证V2电压值范围
    if (!isValidVoltage(v2Voltage)) {
        emit logMessage(tr("错误：V2电压值超出范围，请输入1.60~10.80之间的值"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    // 通道名称映射
    QString channelName;
    switch (channelId) {
    case 0x01: channelName = "V1"; break;
    case 0x02: channelName = "V2"; break;
    case 0x03: channelName = "V3"; break;
    }

    // V1电压显示值（数值形式）
    emit logMessage(tr("电压控制指令：开始向从机发送 通道=%1 V1=%2V V2=%3V 控制命令...")
                    .arg(channelName)
                    .arg(v1Voltage, 0, 'f', 1)
                    .arg(v2Voltage, 0, 'f', 1));

    // 在组帧阶段将 V1/V2 编码为单字节（BCD）
    QByteArray voltageControlData = DeviceProtocol::buildVoltageControl(channelId, v1Voltage, v2Voltage);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, voltageControlData);

    if (success) {
        emit logMessage(tr("电压控制指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(voltageControlData)));
        
        // 开始等待确认，期望回应完整4字节：0x02 + 通道ID + V1电压BCD + V2电压码
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x02));                              // 命令字
        expectedResponse.append(static_cast<char>(channelId));                         // 通道ID
        expectedResponse.append(static_cast<char>(DeviceProtocol::encodeVoltage(v1Voltage))); // V1电压BCD
        expectedResponse.append(static_cast<char>(DeviceProtocol::encodeVoltage(v2Voltage)));  // V2电压码
        startCommandConfirmation(Command::VoltageControl, voltageControlData, expectedResponse);
    } else {
        emit logMessage(tr("错误：电压控制命令发送失败"));
    }

    return success;
}

bool DeviceController::setV123VoltageControl(uint8_t channelId, double voltage)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 验证通道ID
    if (channelId != 0x01 && channelId != 0x02 && channelId != 0x03) {
        emit logMessage(tr("错误：无效的通道ID 0x%1").arg(channelId, 2, 16, QChar('0')));
        return false;
    }

    // 验证V123电压数值范围（0.0=关闭, 1.2~5.0V=输出）
    if (std::isnan(voltage) || (voltage != 0.0 && (voltage < 1.2 || voltage > 5.0))) {
        emit logMessage(tr("错误：V123电压值无效或超出范围（0.0=关闭, 1.2~5.0V） 当前=%1V").arg(voltage, 0, 'f', 1));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    // 通道名称映射
    QString channelName;
    switch (channelId) {
    case 0x01: channelName = "V1"; break;
    case 0x02: channelName = "V2"; break;
    case 0x03: channelName = "V3"; break;
    }

    emit logMessage(tr("V123电压控制：开始向从机发送 通道=%1 电压=%2V 控制命令...")
                    .arg(channelName)
                    .arg(voltage, 0, 'f', 1));

    QByteArray data = DeviceProtocol::buildV123VoltageControl(channelId, voltage);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("V123电压控制指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));
        
        // 开始等待确认，期望回应3字节：0x02 + 通道ID + 电压BCD
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x02));
        expectedResponse.append(static_cast<char>(channelId));
        expectedResponse.append(static_cast<char>(DeviceProtocol::encodeVoltage(voltage)));
        startCommandConfirmation(Command::V123VoltageControl, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：V123电压控制命令发送失败"));
    }

    return success;
}

bool DeviceController::setV4VoltageControl(double voltage)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 验证V4电压值范围（0.0=关闭, 1.60~10.80V=输出）
    if (voltage != 0.0 && !isValidVoltage(voltage)) {
        emit logMessage(tr("错误：V4电压值超出范围（0.0=关闭, 1.60~10.80V）"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("V4电压控制：开始向从机发送 电压=%1V 控制命令...")
                    .arg(voltage, 0, 'f', 2));

    QByteArray data = DeviceProtocol::buildV4VoltageControl(voltage);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("V4电压控制指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));
        
        // 开始等待确认，期望回应3字节：0x02 + 0x04 + V4特定指令码
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x02));
        expectedResponse.append(static_cast<char>(0x04));
        expectedResponse.append(static_cast<char>(DeviceProtocol::encodeV4Voltage(voltage)));
        startCommandConfirmation(Command::V4VoltageControl, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：V4电压控制命令发送失败"));
    }

    return success;
}

bool DeviceController::v123StepAdjust(uint8_t v123ChannelId, uint8_t action)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 验证通道ID
    if (v123ChannelId != 0x01 && v123ChannelId != 0x02 && v123ChannelId != 0x03) {
        emit logMessage(tr("错误：无效的通道ID 0x%1").arg(v123ChannelId, 2, 16, QChar('0')));
        return false;
    }

    // 验证动作码（0x01=UP, 0x02=DOWN）
    if (action != 0x01 && action != 0x02) {
        emit logMessage(tr("错误：无效的动作码 0x%1").arg(action, 2, 16, QChar('0')));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    QString channelName;
    switch (v123ChannelId) {
    case 0x01: channelName = "V1"; break;
    case 0x02: channelName = "V2"; break;
    case 0x03: channelName = "V3"; break;
    }

    QString actionName = (action == 0x01) ? tr("UP") : tr("DOWN");
    emit logMessage(tr("V123微调：开始向从机发送 通道=%1 动作=%2 命令...").arg(channelName).arg(actionName));

    QByteArray data = DeviceProtocol::buildV123StepAdjust(v123ChannelId, action);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("V123微调指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));

        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x06));
        expectedResponse.append(static_cast<char>(v123ChannelId));
        expectedResponse.append(static_cast<char>(action));
        startCommandConfirmation(Command::StepAdjust, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：V123微调命令发送失败"));
    }

    return success;
}

bool DeviceController::v4StepAdjust(uint8_t action)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 验证动作码（0x01=UP, 0x02=DOWN）
    if (action != 0x01 && action != 0x02) {
        emit logMessage(tr("错误：无效的动作码 0x%1").arg(action, 2, 16, QChar('0')));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    QString actionName = (action == 0x01) ? tr("UP") : tr("DOWN");
    emit logMessage(tr("V4微调：开始向从机发送 通道=V4, 动作=%1 命令...").arg(actionName));

    QByteArray data = DeviceProtocol::buildV4StepAdjust(action);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("V4微调指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));

        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x06));
        expectedResponse.append(static_cast<char>(0x04)); // V4通道ID
        expectedResponse.append(static_cast<char>(action));
        startCommandConfirmation(Command::StepAdjust, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：V4微调命令发送失败"));
    }

    return success;
}

bool DeviceController::openVoltageChannel(uint8_t v123ChannelId, uint8_t v4ChannelId)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    // 验证v123通道
    if (v123ChannelId != 0x01 && v123ChannelId != 0x02 && v123ChannelId != 0x03) {
        emit logMessage(tr("错误：无效的通道ID 0x%1").arg(v123ChannelId, 2, 16, QChar('0')));
        return false;
    }

    // 验证v4通道
    if (v4ChannelId != 0x04) {
        emit logMessage(tr("错误：无效的V4通道ID 0x%1").arg(v4ChannelId, 2, 16, QChar('0')));
        return false;
    }

    QString channelName;
    switch (v123ChannelId) {
    case 0x01: channelName = "V1"; break;
    case 0x02: channelName = "V2"; break;
    case 0x03: channelName = "V3"; break;
    }

    emit logMessage(tr("电压输出通道开启：开始向从机发送 通道=%1 开启命令...").arg(channelName));

    QByteArray data = DeviceProtocol::buildVoltageChannelOpen(v123ChannelId, v4ChannelId);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("电压输出通道开启指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x12));
        expectedResponse.append(static_cast<char>(v123ChannelId));
        expectedResponse.append(static_cast<char>(v4ChannelId));
        startCommandConfirmation(Command::VoltageChannelOpen, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：电压输出通道开启命令发送失败"));
    }

    return success;
}

bool DeviceController::openV123Channel(uint8_t v123ChannelId)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    // 验证v123通道
    if (v123ChannelId != 0x01 && v123ChannelId != 0x02 && v123ChannelId != 0x03) {
        emit logMessage(tr("错误：无效的通道ID 0x%1").arg(v123ChannelId, 2, 16, QChar('0')));
        return false;
    }

    QString channelName;
    switch (v123ChannelId) {
    case 0x01: channelName = "V1"; break;
    case 0x02: channelName = "V2"; break;
    case 0x03: channelName = "V3"; break;
    }

    emit logMessage(tr("V123通道开启：开始向从机发送 通道=%1 开启命令...").arg(channelName));

    QByteArray data = DeviceProtocol::buildV123ChannelOpen(v123ChannelId);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("V123通道开启指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x12));
        expectedResponse.append(static_cast<char>(v123ChannelId));
        startCommandConfirmation(Command::V123ChannelOpen, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：V123通道开启命令发送失败"));
    }

    return success;
}

bool DeviceController::openV4Channel()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("V4通道开启：开始向从机发送 V4 开启命令..."));

    QByteArray data = DeviceProtocol::buildV4ChannelOpen();
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, data);

    if (success) {
        emit logMessage(tr("V4通道开启指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(data)));
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x12));
        expectedResponse.append(static_cast<char>(0x04));
        startCommandConfirmation(Command::V4ChannelOpen, data, expectedResponse);
    } else {
        emit logMessage(tr("错误：V4通道开启命令发送失败"));
    }

    return success;
}

bool DeviceController::sendTestCommand(const QByteArray &testData)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    QByteArray actualTestData = testData;
    if (actualTestData.isEmpty()) {
        // 默认测试数据
        actualTestData.append(static_cast<char>(0x34));
        actualTestData.append(static_cast<char>(0x34));
    }

    emit logMessage(tr("开始与从机通信测试..."));

    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, actualTestData);

    if (success) {
        emit logMessage(tr("测试命令发送成功，DATA: %1").arg(DeviceProtocol::toHex(actualTestData)));
        
        // 开始等待确认，期望回应与发送数据相同（移除阻塞等待）
        QByteArray expectedResponse = actualTestData;
        startCommandConfirmation(Command::TestCommand, actualTestData, expectedResponse);
    } else {
        emit logMessage(tr("错误：测试命令发送失败"));
    }

    return success;
}

bool DeviceController::selectDetectionChannel(uint8_t rangeCode, uint8_t channelCode)
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    // 验证档位码
    if (rangeCode != 0x01 && rangeCode != 0x02) {
        emit logMessage(tr("错误：无效的档位码 0x%1").arg(rangeCode, 2, 16, QChar('0')));
        return false;
    }

    // 验证通道码
    if (channelCode != 0x11 && channelCode != 0x21 && channelCode != 0x31 && channelCode != 0x41) {
        emit logMessage(tr("错误：无效的通道码 0x%1").arg(channelCode, 2, 16, QChar('0')));
        return false;
    }

    // 记录当前档位和通道（用于后续测量值解析）
    // ？？？？？？？？？？？两者方式还不一样？？？？？？？
    m_currentRange = (rangeCode == 0x01) ? Measurement::Range::MilliAmp : Measurement::Range::MicroAmp;
    m_currentChannel = static_cast<Measurement::Channel>(channelCode);

    QString rangeName = (rangeCode == 0x01) ? "mA" : "uA";
    QString channelName;
    switch (channelCode) {
    case 0x11: channelName = "CH1"; break;
    case 0x21: channelName = "CH2"; break;
    case 0x31: channelName = "CH3"; break;
    case 0x41: channelName = "CH4"; break;
    }

    emit logMessage(tr("电流检测指令：开始向从机发送%1通道/%2档位选择命令...").arg(channelName).arg(rangeName));

    // 清空测量缓冲区
    m_measureDataBuffer.clear();

    // 电流检测帧构造：0xC0 0x03 rangeCode channelCode
    QByteArray detectionData = DeviceProtocol::buildDetection(rangeCode, channelCode);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, detectionData);

    if (success) {
        emit logMessage(tr("电流检测指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(detectionData)));
        
        // 开始等待确认，期望回应完整3字节：0x03 + rangeCode + channelCode
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x03));         // 命令字
        expectedResponse.append(static_cast<char>(rangeCode));    // 档位码
        expectedResponse.append(static_cast<char>(channelCode));  // 通道码
        startCommandConfirmation(Command::DetectionSelect, detectionData, expectedResponse);
    } else {
        emit logMessage(tr("错误：电流检测命令发送失败"));
    }

    return success;
}

bool DeviceController::startExternalMeterDetection()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("启动外部电流表连续检测：开始向从机发送启动命令..."));

    // 清空测量缓冲区，准备接收新的测量数据
    m_measureDataBuffer.clear();

    // 启动外部电流表连续检测帧：0xC0 0x50
    QByteArray commandData;
    commandData.append(static_cast<char>(0x50));
    
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, commandData);

    if (success) {
        emit logMessage(tr("启动外部电流表连续检测指令发送完成，DATA: %1")
                       .arg(DeviceProtocol::toHex(commandData)));

        // 期望回应2字节：0x50 0xAA
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x50));
        expectedResponse.append(static_cast<char>(0xAA));
        startCommandConfirmation(Command::StartDetection, commandData, expectedResponse);
    } else {
        emit logMessage(tr("错误：启动外部电流表连续检测命令发送失败"));
    }

    return success;
}

bool DeviceController::stopExternalMeterDetection()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("停止外部电流表连续检测：开始向从机发送停止命令..."));

    // 停止外部电流表连续检测帧：0xC0 0x51
    QByteArray commandData;
    commandData.append(static_cast<char>(0x51));
    
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, commandData);

    if (success) {
        emit logMessage(tr("停止外部电流表连续检测指令发送完成，DATA: %1")
                       .arg(DeviceProtocol::toHex(commandData)));

        // 期望回应2字节：0x51 0x55
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x51));
        expectedResponse.append(static_cast<char>(0x55));
        startCommandConfirmation(Command::StopExternalMeter, commandData, expectedResponse);
    } else {
        emit logMessage(tr("错误：停止外部电流表连续检测命令发送失败"));
    }

    return success;
}

void DeviceController::cancelPendingCommand()
{
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("【取消确认】取消等待中的命令确认: %1").arg(static_cast<int>(m_pendingCommand)));
        cancelCommandConfirmation();
    }
}

bool DeviceController::pressPowerConfirmKey()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("继电器指令：开始向从机发送开机/确认键命令..."));

    // 继电器按键帧构造：0xC0 0x01 0x03（复用Power命令字）
    QByteArray relayData = DeviceProtocol::buildRelayKey(DeviceProtocol::RelayKeyCode::PowerConfirm);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, relayData);

    if (success) {
        emit logMessage(tr("继电器-确认键指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(relayData)));
        
        // 开始等待确认，期望回应完整2字节：0x01 + 0x03
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));  // 命令字（Power）
        expectedResponse.append(static_cast<char>(0x03));  // 按键码
        startCommandConfirmation(Command::RelayPowerConfirm, relayData, expectedResponse);
    } else {
        emit logMessage(tr("错误：继电器-确认键命令发送失败"));
    }

    return success;
}

bool DeviceController::pressRightKey()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // 检查是否有其他命令正在等待确认
    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("继电器指令：开始向从机发送右键命令..."));

    // 继电器按键帧构造：0xC0 0x01 0x02（复用Power命令字）
    QByteArray relayData = DeviceProtocol::buildRelayKey(DeviceProtocol::RelayKeyCode::Right);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, relayData);

    if (success) {
        emit logMessage(tr("继电器-右键指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(relayData)));
        
        // 开始等待确认，期望回应完整2字节：0x01 + 0x02
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));  // 命令字（Power）
        expectedResponse.append(static_cast<char>(0x02));  // 按键码
        startCommandConfirmation(Command::RelayRight, relayData, expectedResponse);
    } else {
        emit logMessage(tr("错误：继电器-右键命令发送失败"));
    }

    return success;
}

bool DeviceController::pressSw3Key()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("继电器指令：开始向从机发送SW3键命令..."));

    QByteArray relayData = DeviceProtocol::buildRelayKey(DeviceProtocol::RelayKeyCode::Sw3);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, relayData);

    if (success) {
        emit logMessage(tr("继电器-SW3键指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(relayData)));
        
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));  // 命令字
        expectedResponse.append(static_cast<char>(0x31));  // SW3按键码
        startCommandConfirmation(Command::RelaySw3, relayData, expectedResponse);
    } else {
        emit logMessage(tr("错误：继电器-SW3键命令发送失败"));
    }

    return success;
}

bool DeviceController::pressSw4Key()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("继电器指令：开始向从机发送SW4键命令..."));

    QByteArray relayData = DeviceProtocol::buildRelayKey(DeviceProtocol::RelayKeyCode::Sw4);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, relayData);

    if (success) {
        emit logMessage(tr("继电器-SW4键指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(relayData)));
        
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));  // 命令字
        expectedResponse.append(static_cast<char>(0x41));  // SW4按键码
        startCommandConfirmation(Command::RelaySw4, relayData, expectedResponse);
    } else {
        emit logMessage(tr("错误：继电器-SW4键命令发送失败"));
    }

    return success;
}

bool DeviceController::pressSw5Key()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("继电器指令：开始向从机发送SW5键命令..."));

    QByteArray relayData = DeviceProtocol::buildRelayKey(DeviceProtocol::RelayKeyCode::Sw5);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, relayData);

    if (success) {
        emit logMessage(tr("继电器-SW5键指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(relayData)));
        
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));  // 命令字
        expectedResponse.append(static_cast<char>(0x51));  // SW5按键码
        startCommandConfirmation(Command::RelaySw5, relayData, expectedResponse);
    } else {
        emit logMessage(tr("错误：继电器-SW5键命令发送失败"));
    }

    return success;
}

bool DeviceController::pressSw6Key()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    if (m_pendingCommand != Command::None) {
        emit logMessage(tr("错误：有其他命令正在等待确认，请稍后重试"));
        return false;
    }

    emit logMessage(tr("继电器指令：开始向从机发送SW6键命令..."));

    QByteArray relayData = DeviceProtocol::buildRelayKey(DeviceProtocol::RelayKeyCode::Sw6);
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, relayData);

    if (success) {
        emit logMessage(tr("继电器-SW6键指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(relayData)));
        
        QByteArray expectedResponse;
        expectedResponse.append(static_cast<char>(0x01));  // 命令字
        expectedResponse.append(static_cast<char>(0x61));  // SW6按键码
        startCommandConfirmation(Command::RelaySw6, relayData, expectedResponse);
    } else {
        emit logMessage(tr("错误：继电器-SW6键命令发送失败"));
    }

    return success;
}

bool DeviceController::sendIapJumpCommand()
{
    if (!isConnected()) {
        emit logMessage(tr("错误：设备未连接"));
        return false;
    }

    // IAP跳转指令不需要等待确认（设备会复位）
    // 因此不检查 m_pendingCommand 状态

    emit logMessage(tr("IAP升级指令：开始向从机发送跳转到Bootloader命令..."));

    // 构造IAP跳转指令：0x99 0xAA
    QByteArray iapData = DeviceProtocol::buildIapJump();
    bool success = sendAddressAndData(DeviceProtocol::kSlaveAddress, iapData);

    if (success) {
        emit logMessage(tr("IAP升级指令发送完成，DATA: %1").arg(DeviceProtocol::toHex(iapData)));
        // 不需要等待确认，设备收到后会复位进入Bootloader
    } else {
        emit logMessage(tr("错误：IAP升级指令发送失败"));
    }

    return success;
}

bool DeviceController::sendAddressAndData(uint8_t slaveAddress, const QByteArray &data)
{
    if (!m_serialService || !m_serialService->isOpen()) {
        return false;
    }

    // 发送地址帧
    if (!m_serialService->writeAddressByte(slaveAddress, DeviceProtocol::kWriteTimeoutMs)) {
        return false;
    }

    // 发送数据帧
    if (!m_serialService->writeData(data, DeviceProtocol::kWriteTimeoutMs)) {
        return false;
    }

    return true;
}

bool DeviceController::isValidVoltage(double voltage) const
{
    // 允许0.0作为关闭指令，或在正常范围内
    return voltage == 0.0 || (voltage >= 1.60 && voltage <= 10.80);
}

void DeviceController::onSerialDataReceived(const QByteArray &data)
{
    // ===== 正常处理流程 =====
#ifdef QT_DEBUG
    // 仅在调试模式下输出原始串口数据，避免高频日志导致UI卡顿
    emit logMessage(tr("收到从机回传: %1").arg(DeviceProtocol::toHex(data)));
#endif

    // 如果正在等待控制帧确认，有控制指令还未处理完，优先处理控制帧
    // m_pendingCommand在DeviceController::startCommandConfirmation中被设置
    if (m_pendingCommand != Command::None) {
        m_receivedBuffer.append(data);
        
        // ===== 通用确认帧处理（其他命令） =====
        // 立即检查是否有匹配的控制帧回应m_expectedResponse，缓冲区中可能混有测量的电流帧
        if (ProtocolParser::checkResponseMatch(m_receivedBuffer, m_expectedResponse)) {

            // 使用逐字节 uint8_t 比较查找匹配位置 
            // QByteArray::indexOf 对 0xAA 等高字节值存在符号问题
            int matchPos = -1;

            // 单字节控制帧：逐字节查找
            if (m_expectedResponse.size() == 1) {
                uint8_t expectedByte = static_cast<uint8_t>(m_expectedResponse[0]);
                for (int i = 0; i < m_receivedBuffer.size(); i++) {
                    if (static_cast<uint8_t>(m_receivedBuffer[i]) == expectedByte) {
                        matchPos = i;
                        break;
                    }
                }

            // 多字节控制帧：逐字节搜索
            } else {
                for (int i = 0; i <= m_receivedBuffer.size() - m_expectedResponse.size(); i++) {
                    bool match = true;
                    for (int j = 0; j < m_expectedResponse.size(); j++) {
                        if (static_cast<uint8_t>(m_receivedBuffer[i + j]) != 
                            static_cast<uint8_t>(m_expectedResponse[j])) {
                            match = false;
                            break;
                        }
                    }
                    if (match) {
                        matchPos = i;
                        break;
                    }
                }
            }
            
            // 提取匹配的控制帧
            if (matchPos >= 0) {

                // 提取子串（匹配的控制帧）
                QByteArray matchedResponse = m_receivedBuffer.mid(matchPos, m_expectedResponse.size());
                
                // 保留控制帧之后的剩余数据，包含外部电流表测量帧
                int afterMatchPos = matchPos + m_expectedResponse.size();
                QByteArray remainingData;

                // 检查是否有剩余数据（未超过缓冲区末尾）
                if (afterMatchPos < m_receivedBuffer.size()) {

                    // 存在剩余数据（电流测量帧）
                    remainingData = m_receivedBuffer.mid(afterMatchPos);
                }
                
                // 清空接收缓冲区（控制帧已处理）
                m_receivedBuffer.clear();
                
                // 处理控制帧确认
                handleCommandConfirmationSuccess(matchedResponse);
                
                // 将剩余数据加入测量缓冲区，继续解析外部电流表数据
                if (!remainingData.isEmpty()) {
                    m_measureDataBuffer.append(remainingData);
                    
                    // 解析外部电流表测量帧（带0x50帧头 + 4字节float）
                    float externalValue = 0.0f;
                    while (ProtocolParser::parseExternalMeasurementWithHeader(m_measureDataBuffer, externalValue)) {
#ifdef QT_DEBUG
                        emit logMessage(tr("收到外部电流表测量值: %1 mA").arg(externalValue, 0, 'f', 4));
#endif
                        emit externalMeasurementReceived(externalValue);
                    }
                }
                
                return; // 提前返回
            }
        }
        
        // 缓冲区清理逻辑优化
        if (m_receivedBuffer.size() > 100) {
            int lastPossibleStart = -1;
            for (int i = m_receivedBuffer.size() - 1; i >= 0; i--) {
                uint8_t byte = static_cast<uint8_t>(m_receivedBuffer[i]);
                if (byte == 0x01 || byte == 0x02 || byte == 0x03 || 
                    byte == 0x04 || byte == 0x12 || byte == 0x50) {
                    lastPossibleStart = i;
                    break;
                }
            }
            
            if (lastPossibleStart > 0) {
                m_receivedBuffer = m_receivedBuffer.mid(lastPossibleStart);
            } else {
                m_receivedBuffer.clear();
            }
        }

        return;
    }   

    // ===== 非等待确认帧状态，解析外部电流表测量帧 （连续发送测量帧，带0x50帧头）=====
    m_measureDataBuffer.append(data);
    
    // 解析外部电流表测量帧（带0x50帧头 + 4字节float）
    float externalValue = 0.0f;
    while (ProtocolParser::parseExternalMeasurementWithHeader(m_measureDataBuffer, externalValue)) {
#ifdef QT_DEBUG
        emit logMessage(tr("收到外部电流表测量值: %1 mA").arg(externalValue, 0, 'f', 4));
#endif
        emit externalMeasurementReceived(externalValue);
    }
}

void DeviceController::onSerialError(const QString &errorString)
{
    emit logMessage(tr("串口错误: %1").arg(errorString));
}

void DeviceController::onPortStatusChanged(bool isOpen)
{
    if (!isOpen && m_isConnected) {
        // 串口意外关闭，取消待确认命令
        cancelCommandConfirmation();
        m_isConnected = false;
        emit logMessage(tr("串口连接意外断开"));
        emit connectionStatusChanged(false, currentPortName());
    }
}

void DeviceController::onCommandConfirmationTimeout()
{
    QString operationName = commandToString(m_pendingCommand);
    emit logMessage(tr("命令确认超时 - %1").arg(operationName));
    
    // 检查是否需要重试
    if (m_retryCount < kMaxRetries) {
        m_retryCount++;
        emit logMessage(tr("正在重试命令 (%1/%2) - %3").arg(m_retryCount).arg(kMaxRetries).arg(operationName));
        
        // 重新发送命令
        if (sendAddressAndData(DeviceProtocol::kSlaveAddress, m_sentData)) {
            // 重置定时器继续等待
            m_confirmationTimer->start(kConfirmationTimeoutMs);
            emit logMessage(tr("重试命令已发送，等待确认..."));
        } else {
            handleCommandConfirmationFailure(tr("重试发送失败"));
        }
    } else {
        // 超过最大重试次数
        handleCommandConfirmationFailure(tr("超时未收到确认回应，已超过最大重试次数"));
    }
}

void DeviceController::startCommandConfirmation(Command command, 
                                                const QByteArray &sentData,
                                                const QByteArray &expectedResponse)
{
    // 取消之前的确认等待
    cancelCommandConfirmation();
    
    m_pendingCommand = command;
    m_sentData = sentData;
    m_expectedResponse = expectedResponse;
    m_receivedBuffer.clear();
    m_retryCount = 0;
    
    QString operationName = commandToString(command);
    
    // 启动超时定时器。超时时间到时释放timeout触发DeviceController::onCommandConfirmationTimeout，执行重传。
    m_confirmationTimer->start(kConfirmationTimeoutMs);

    // 释放信号显示在文本框中
    emit logMessage(tr("开始等待命令确认 - %1，期望回应: %2")
                    .arg(operationName)
                    .arg(DeviceProtocol::toHex(m_expectedResponse)));
}

void DeviceController::cancelCommandConfirmation()
{
    if (m_pendingCommand != Command::None) {
        m_confirmationTimer->stop();
        m_pendingCommand = Command::None;
        m_sentData.clear();
        m_expectedResponse.clear();
        m_receivedBuffer.clear();
        m_retryCount = 0;
    }
}

void DeviceController::handleCommandConfirmationSuccess(const QByteArray &responseData)
{
    QString operationName = commandToString(m_pendingCommand);
    emit logMessage(tr("命令确认成功 - %1，收到回应: %2")
                    .arg(operationName)
                    .arg(DeviceProtocol::toHex(responseData)));
    
    Command command = m_pendingCommand;
    QByteArray sentData = m_sentData;
    
    // 取消确认状态
    cancelCommandConfirmation();
    
    // 发射确认成功信号（使用类型安全的Command枚举）
    emit commandConfirmed(command, true, sentData, responseData);
}

void DeviceController::handleCommandConfirmationFailure(const QString &reason)
{
    QString operationName = commandToString(m_pendingCommand);
    emit logMessage(tr("命令确认失败 - %1: %2").arg(operationName).arg(reason));
    
    Command command = m_pendingCommand;
    QByteArray sentData = m_sentData;
    
    // 取消确认状态
    cancelCommandConfirmation();
    
    // 发射确认失败信号（使用类型安全的Command枚举）
    emit commandConfirmed(command, false, sentData, QByteArray());
}
