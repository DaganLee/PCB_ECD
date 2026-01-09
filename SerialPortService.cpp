#include "SerialPortService.h"
#include "DeviceProtocol.h"

SerialPortService::SerialPortService(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_isOpen(false)
{
    setupConnections();
}

SerialPortService::~SerialPortService()
{
    closePort();
}

void SerialPortService::setupConnections()
{
    // 连接串口信号
    // 有新数据到来，触发读串口中数据的操作
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &SerialPortService::onReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialPortService::onErrorOccurred);
}

bool SerialPortService::openPort(const QString &portName, int baudRate)
{
    if (m_isOpen) {
        closePort();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    configurePort();

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        m_isOpen = true;
        emit portStatusChanged(true);
        return true;
    } else {
        emit errorOccurred(tr("无法打开串口 %1: %2").arg(portName, m_serialPort->errorString()));
        return false;
    }
}

void SerialPortService::closePort()
{
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }
    
    if (m_isOpen) {
        m_isOpen = false;
        emit portStatusChanged(false);
    }
}

bool SerialPortService::isOpen() const
{
    return m_isOpen && m_serialPort && m_serialPort->isOpen();
}

QString SerialPortService::portName() const
{
    return m_serialPort ? m_serialPort->portName() : QString();
}

void SerialPortService::configurePort()
{
    // 配置串口参数：8数据位，1停止位，无流控
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::SpaceParity);   // 默认为数据帧模式
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);
}

bool SerialPortService::writeAddressByte(uint8_t address, int timeoutMs)
{
    if (!isOpen()) {
        return false;
    }

    // 切换到MarkParity (9th bit = 1) 用于地址帧
    m_serialPort->setParity(QSerialPort::MarkParity);

    QByteArray addressData;
    addressData.append(static_cast<char>(address));

    qint64 bytesWritten = m_serialPort->write(addressData);
    if (bytesWritten != addressData.size()) {
        return false;
    }

    // 等待发送完成
    if (!m_serialPort->waitForBytesWritten(timeoutMs)) {
        return false;
    }

    // 立即切换回SpaceParity (9th bit = 0) 用于后续数据帧
    m_serialPort->setParity(QSerialPort::SpaceParity);

    return true;
}

bool SerialPortService::writeData(const QByteArray &data, int timeoutMs)
{
    if (!isOpen() || data.isEmpty()) {
        return false;
    }

    // 确保使用SpaceParity (9th bit = 0) 用于数据帧
    m_serialPort->setParity(QSerialPort::SpaceParity);

    qint64 bytesWritten = m_serialPort->write(data);
    if (bytesWritten != data.size()) {
        return false;
    }

    // 等待发送完成
    if (!m_serialPort->waitForBytesWritten(timeoutMs)) {
        return false;
    }

    return true;
}

void SerialPortService::onReadyRead()
{
    if (!m_serialPort) {
        return;
    }

    // 读取串口的中的数据
    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {

        // 释放信号，触发DeviceController::onSerialDataReceived处理接收到的数据——接收数据
        emit dataReceived(data);
    }
}

void SerialPortService::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    QString errorString = m_serialPort ? m_serialPort->errorString() : tr("未知串口错误");
    emit errorOccurred(errorString);
    
    // 检测致命错误类型，需要主动关闭串口
    bool isFatalError = false;
    switch (error) {
    case QSerialPort::ResourceError:       // 资源错误（设备被拔出）
    case QSerialPort::DeviceNotFoundError: // 设备未找到
    case QSerialPort::PermissionError:     // 权限错误（设备被占用或没有权限）
    case QSerialPort::UnknownError:        // 未知错误
        isFatalError = true;
        break;
    default:
        // 其他错误（如超时、写入错误等）可能是暂时性的，不主动关闭
        break;
    }
    
    // 遇到致命错误时主动关闭串口并通知上层
    if (isFatalError && m_isOpen) {
        closePort();  // 会自动发射 portStatusChanged(false)
    }
}
