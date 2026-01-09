#include "SerialPortManager.h"
#include <QSerialPort>
#include <algorithm>

SerialPortManager::SerialPortManager(QObject *parent)
    : QObject(parent)

    // 创建定时器对象，将返回的指针赋值给m_dT
    ,
      m_detectionTimer(new QTimer(this)), m_isMonitoring(false)
{
    // 配置检测定时器
    initializeComponents();

    // 检测串口，暂存可用的串口列表，发送当前识别到的串口以及新增或减少的串口的信号
    initConnections();
}

SerialPortManager::~SerialPortManager()
{
    stopMonitoring();
}

void SerialPortManager::initializeComponents()
{
    // 配置检测定时器
    m_detectionTimer->setSingleShot(false); // 非单次定时器
    m_detectionTimer->setInterval(2000);    // 默认2秒间隔

    // 初始化状态
    m_isMonitoring = false;
}

void SerialPortManager::initConnections()
{
    // 连接定时器信号槽

    // 定时结束，调用检测串口函数，暂存可用的串口列表，发送当前识别到的串口以及新增或减少的串口的信号
    connect(m_detectionTimer.data(), &QTimer::timeout,
            this, &SerialPortManager::onDetectionTimerTimeout);
}

void SerialPortManager::startMonitoring(int intervalMs)
{
    if (m_isMonitoring)
    {
        return; // 已经在监控中
    }

    // 设置监测器的检测间隔时间
    m_detectionTimer->setInterval(intervalMs);

    // 监测函数
    // 立即进行一次检测
    detectPorts();

    // 启动定时器，检测串口的状态
    // 连接SerialPortManager::onDetectionTimerTimeout槽函数，当定时器超时QTimer::timeout时触发。
    // 槽函数中调用detectPorts监测函数。
    m_detectionTimer->start();
    m_isMonitoring = true;
}

void SerialPortManager::stopMonitoring()
{
    if (!m_isMonitoring)
    {
        return; // 已经停止
    }

    m_detectionTimer->stop();
    m_isMonitoring = false;
}

QStringList SerialPortManager::availablePorts() const
{
    return m_currentPorts;
}

bool SerialPortManager::isMonitoring() const
{
    return m_isMonitoring;
}

void SerialPortManager::detectPorts()
{
    // 识别到的可用的串口列表
    QStringList newPorts;

    // 获取所有可用的串口设备
    const auto serialPortInfos = QSerialPortInfo::availablePorts();

    // for (const QSerialPortInfo &portInfo : serialPortInfos) {
    //     // 过滤USB转串口设备
    //     if (isUsbSerialDevice(portInfo)) {

    //         // 将识别到的可用的串口添加到newPorts中
    //         newPorts << portInfo.portName();
    //     }
    // }

    // 不过滤USB转串口设备。测试用
    for (const QSerialPortInfo &portInfo : serialPortInfos)
    {
        newPorts << portInfo.portName();
    }

    // 排序串口名称以保持一致性
    std::sort(newPorts.begin(), newPorts.end());

    // 比较并发射相应串口名称，并在UI控件上显示当前识别到的串口以及新增或减少的串口
    compareAndEmitSignals(newPorts);
}

void SerialPortManager::onDetectionTimerTimeout()
{
    detectPorts();
}

bool SerialPortManager::isUsbSerialDevice(const QSerialPortInfo &portInfo) const
{
    // 获取设备描述和厂商信息（转为小写便于比较）
    QString description = portInfo.description().toLower();
    QString manufacturer = portInfo.manufacturer().toLower();

    // 常见的USB转串口芯片厂商和描述关键词
    bool hasUsbKeywords = description.contains("usb") ||
            description.contains("serial") ||
            description.contains("converter") ||
            description.contains("bridge");

    bool hasKnownManufacturer = manufacturer.contains("ftdi") ||
            manufacturer.contains("prolific") ||
            manufacturer.contains("silicon") ||
            manufacturer.contains("ch340") ||
            manufacturer.contains("ch341") ||
            manufacturer.contains("cp210");

    // USB设备通常有厂商ID
    bool hasVendorId = portInfo.hasVendorIdentifier();

    return hasUsbKeywords || hasKnownManufacturer || hasVendorId;
}

void SerialPortManager::compareAndEmitSignals(const QStringList &newPorts)
{
    if (newPorts == m_currentPorts)
    {
        return; // 列表没有变化，无需处理
    }

    // 找出新增的串口
    QStringList addedPorts;
    for (const QString &port : newPorts)
    {
        // 如果新串口不在当前列表中
        if (!m_currentPorts.contains(port))
        {
            addedPorts << port;
        }
    }

    // 找出移除的串口
    QStringList removedPorts;
    for (const QString &port : m_currentPorts)
    {
        // 如果旧串口不在新列表中
        if (!newPorts.contains(port))
        {
            removedPorts << port;
        }
    }

    // 更新当前列表
    m_currentPorts = newPorts;

    // 发射变化信号
    // Widget::onSerialPortsChanged槽，当串口列表更新后触发。
    emit portsChanged(m_currentPorts);
}
