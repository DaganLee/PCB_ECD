#include "OtaController.h"
#include <QDebug>
#include <QFileInfo>

OtaController::OtaController(QObject *parent)
    : QObject(parent)
    , m_serialPort(new QSerialPort(this))
    , m_currentPacket(0)
    , m_totalPackets(0)
    , m_state(Idle)
    , m_retryCount(0)
    , m_timeoutTimer(new QTimer(this))
{
    // 连接串口读取信号
    connect(m_serialPort, &QSerialPort::readyRead, 
            this, &OtaController::onSerialDataReady);
    
    // 连接超时定时器
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, 
            this, &OtaController::onTimeout);
}

OtaController::~OtaController()
{
    cancelUpgrade();
}

bool OtaController::startUpgrade(const QString &portName, const QString &firmwarePath)
{
    // 检查当前状态
    if (isUpgrading()) {
        emit logMessage(tr("升级正在进行中，请等待完成"));
        return false;
    }

    // 加载固件文件
    if (!loadFirmware(firmwarePath)) {
        emit logMessage(tr("加载固件文件失败: %1").arg(firmwarePath));
        return false;
    }

    // 打开串口
    if (!openSerialPort(portName)) {
        emit logMessage(tr("打开串口失败: %1").arg(portName));
        return false;
    }

    // 初始化状态
    m_currentPacket = 0;
    m_retryCount = 0;
    m_rxBuffer.clear();

    // 发送握手
    setState(Connecting);
    sendHandshake();

    return true;
}

void OtaController::cancelUpgrade()
{
    stopTimeoutTimer();
    closeSerialPort();
    
    if (m_state != Idle && m_state != Completed && m_state != Error) {
        setState(Idle);
        emit upgradeFinished(false, tr("升级已取消"));
    }
}

bool OtaController::openSerialPort(const QString &portName)
{
    // 如果已打开，先关闭
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(9600);          // Bootloader 使用 9600bps
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit logMessage(tr("串口打开失败: %1").arg(m_serialPort->errorString()));
        return false;
    }

    emit logMessage(tr("OTA 串口已打开: %1 @ 9600bps").arg(portName));
    return true;
}

void OtaController::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
        emit logMessage(tr("OTA 串口已关闭"));
    }
}

bool OtaController::loadFirmware(const QString &filePath)
{
    QFile file(filePath);
    
    if (!file.exists()) {
        emit logMessage(tr("固件文件不存在: %1").arg(filePath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(tr("无法打开固件文件: %1").arg(file.errorString()));
        return false;
    }

    m_firmwareData = file.readAll();
    file.close();

    if (m_firmwareData.isEmpty()) {
        emit logMessage(tr("固件文件为空"));
        return false;
    }

    // 检查固件大小（APP 区最大约 54KB）
    constexpr uint32_t APP_SIZE_MAX = 54 * 1024;
    if (static_cast<uint32_t>(m_firmwareData.size()) > APP_SIZE_MAX) {
        emit logMessage(tr("固件文件过大: %1 字节，最大允许 %2 字节")
                        .arg(m_firmwareData.size())
                        .arg(APP_SIZE_MAX));
        return false;
    }

    // 填充固件信息
    m_fwInfo.firmware_size = static_cast<uint32_t>(m_firmwareData.size());
    m_fwInfo.firmware_crc32 = OtaProtocol::calculateCRC32(
        reinterpret_cast<const uint8_t*>(m_firmwareData.constData()),
        m_fwInfo.firmware_size);
    m_fwInfo.packet_size = OtaProtocol::PACKET_DATA_SIZE;
    m_fwInfo.packet_count = (m_fwInfo.firmware_size + OtaProtocol::PACKET_DATA_SIZE - 1) 
                            / OtaProtocol::PACKET_DATA_SIZE;
    m_fwInfo.version_major = 1;
    m_fwInfo.version_minor = 0;
    m_fwInfo.version_patch = 0;
    m_fwInfo.reserved = 0;

    m_totalPackets = m_fwInfo.packet_count;

    emit logMessage(tr("固件加载成功: %1 字节, %2 个数据包, CRC32=0x%3")
                    .arg(m_fwInfo.firmware_size)
                    .arg(m_totalPackets)
                    .arg(m_fwInfo.firmware_crc32, 8, 16, QChar('0')));

    return true;
}

void OtaController::sendHandshake()
{
    QByteArray frame = OtaProtocol::buildHandshakeFrame();
    m_serialPort->write(frame);
    m_serialPort->flush();
    
    emit logMessage(tr("发送握手帧..."));
    startTimeoutTimer(3000);
}

void OtaController::sendStartUpgrade()
{
    QByteArray frame = OtaProtocol::buildStartFrame(m_fwInfo);
    m_serialPort->write(frame);
    m_serialPort->flush();
    
    emit logMessage(tr("发送开始升级帧..."));
    startTimeoutTimer(5000);  // 擦除 Flash 需要较长时间
}

void OtaController::sendNextDataPacket()
{
    if (m_currentPacket >= m_totalPackets) {
        // 所有数据包发送完成，发送结束帧
        setState(WaitingFinish);
        sendFinish();
        return;
    }

    // 计算当前包的数据范围
    uint32_t offset = m_currentPacket * OtaProtocol::PACKET_DATA_SIZE;
    uint16_t dataLen = qMin(static_cast<uint32_t>(OtaProtocol::PACKET_DATA_SIZE),
                            m_fwInfo.firmware_size - offset);

    // 构建并发送数据包
    QByteArray frame = OtaProtocol::buildDataFrame(
        m_currentPacket,
        reinterpret_cast<const uint8_t*>(m_firmwareData.constData() + offset),
        dataLen);
    
    m_serialPort->write(frame);
    m_serialPort->flush();

    // 更新进度
    int percent = static_cast<int>((m_currentPacket + 1) * 100 / m_totalPackets);
    emit progressChanged(percent);

    startTimeoutTimer(2000);
}

void OtaController::sendFinish()
{
    QByteArray frame = OtaProtocol::buildFinishFrame();
    m_serialPort->write(frame);
    m_serialPort->flush();
    
    emit logMessage(tr("发送完成帧..."));
    startTimeoutTimer(5000);  // 校验 CRC32 需要时间
}

void OtaController::onSerialDataReady()
{
    // 读取数据到缓冲区
    m_rxBuffer.append(m_serialPort->readAll());

    // 查找完整帧（以帧尾 0x5A 结束）
    while (true) {
        // 查找帧头
        int headerPos = -1;
        for (int i = 0; i < m_rxBuffer.size() - 1; i++) {
            if (static_cast<uint8_t>(m_rxBuffer[i]) == OtaProtocol::FRAME_HEADER1 &&
                static_cast<uint8_t>(m_rxBuffer[i + 1]) == OtaProtocol::FRAME_HEADER2) {
                headerPos = i;
                break;
            }
        }

        if (headerPos < 0) {
            // 没有找到帧头，清空缓冲区
            m_rxBuffer.clear();
            break;
        }

        // 丢弃帧头之前的数据
        if (headerPos > 0) {
            m_rxBuffer = m_rxBuffer.mid(headerPos);
        }

        // 检查是否有足够的数据读取长度字段
        if (m_rxBuffer.size() < 4) {
            break;
        }

        // 读取长度
        uint16_t payloadLen = (static_cast<uint8_t>(m_rxBuffer[2]) << 8) | 
                               static_cast<uint8_t>(m_rxBuffer[3]);
        uint16_t frameLen = 2 + 2 + payloadLen + 2 + 1;  // Header + Length + Payload + CRC + Tail

        // 检查是否接收到完整帧
        if (m_rxBuffer.size() < frameLen) {
            break;
        }

        // 检查帧尾
        if (static_cast<uint8_t>(m_rxBuffer[frameLen - 1]) != OtaProtocol::FRAME_TAIL) {
            // 帧尾错误，丢弃第一个字节重新查找
            m_rxBuffer = m_rxBuffer.mid(1);
            continue;
        }

        // 提取完整帧
        QByteArray frame = m_rxBuffer.left(frameLen);
        m_rxBuffer = m_rxBuffer.mid(frameLen);

        // 处理响应
        stopTimeoutTimer();
        processResponse(frame);
    }
}

void OtaController::processResponse(const QByteArray &response)
{
    uint8_t cmd = OtaProtocol::parseResponseCommand(response);

    // 处理错误响应
    if (cmd == OtaProtocol::CMD_ERROR) {
        uint8_t errCode = OtaProtocol::parseErrorCode(response);
        QString errMsg;
        switch (errCode) {
            case OtaProtocol::ERR_FRAME_FORMAT: errMsg = tr("帧格式错误"); break;
            case OtaProtocol::ERR_CRC: errMsg = tr("CRC 校验失败"); break;
            case OtaProtocol::ERR_SEQ: errMsg = tr("序号错误"); break;
            case OtaProtocol::ERR_FLASH_ERASE: errMsg = tr("Flash 擦除失败"); break;
            case OtaProtocol::ERR_FLASH_WRITE: errMsg = tr("Flash 写入失败"); break;
            case OtaProtocol::ERR_VERIFY: errMsg = tr("固件校验失败"); break;
            case OtaProtocol::ERR_SIZE: errMsg = tr("固件大小错误"); break;
            default: errMsg = tr("未知错误 (0x%1)").arg(errCode, 2, 16, QChar('0')); break;
        }
        finishUpgrade(false, tr("设备返回错误: %1").arg(errMsg));
        return;
    }

    // 根据当前状态处理响应
    switch (m_state) {
        case Connecting:
            if (cmd == OtaProtocol::CMD_HANDSHAKE_ACK) {
                emit logMessage(tr("握手成功"));
                m_retryCount = 0;
                setState(StartingUpgrade);
                sendStartUpgrade();
            }
            break;

        case StartingUpgrade:
            if (cmd == OtaProtocol::CMD_START_ACK) {
                emit logMessage(tr("设备准备就绪，开始传输固件..."));
                m_retryCount = 0;
                m_currentPacket = 0;
                setState(SendingData);
                sendNextDataPacket();
            }
            break;

        case SendingData:
            if (cmd == OtaProtocol::CMD_DATA_ACK) {
                m_retryCount = 0;
                m_currentPacket++;
                sendNextDataPacket();
            }
            break;

        case WaitingFinish:
            if (cmd == OtaProtocol::CMD_FINISH_ACK) {
                emit logMessage(tr("固件校验成功，升级完成！"));
                finishUpgrade(true, tr("固件升级成功！"));
            }
            break;

        default:
            break;
    }
}

void OtaController::onTimeout()
{
    m_retryCount++;

    if (m_retryCount > MAX_RETRY) {
        finishUpgrade(false, tr("通讯超时，重试次数已用完"));
        return;
    }

    emit logMessage(tr("超时，重试 %1/%2...").arg(m_retryCount).arg(MAX_RETRY));

    // 根据当前状态重发
    switch (m_state) {
        case Connecting:
            sendHandshake();
            break;
        case StartingUpgrade:
            sendStartUpgrade();
            break;
        case SendingData:
            sendNextDataPacket();
            break;
        case WaitingFinish:
            sendFinish();
            break;
        default:
            break;
    }
}

void OtaController::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        emit stateChanged(state);
    }
}

void OtaController::finishUpgrade(bool success, const QString &message)
{
    stopTimeoutTimer();
    closeSerialPort();
    
    setState(success ? Completed : Error);
    emit progressChanged(success ? 100 : 0);
    emit upgradeFinished(success, message);
}

void OtaController::startTimeoutTimer(int ms)
{
    m_timeoutTimer->start(ms);
}

void OtaController::stopTimeoutTimer()
{
    m_timeoutTimer->stop();
}
