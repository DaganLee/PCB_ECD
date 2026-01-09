#ifndef SERIALPORTMANAGER_H
#define SERIALPORTMANAGER_H

#include <QObject>
#include <QSerialPortInfo>
#include <QTimer>
#include <QStringList>
#include <QScopedPointer>

/**
 * @brief 串口设备管理器
 * 
 * 职责：
 * - 检测USB转串口设备
 * - 管理可用串口列表
 * - 通过信号通知UI更新
 */
class SerialPortManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数，explicit防止隐式转换
     * @param parent 父对象指针
     */
    explicit SerialPortManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数，override重写基类的虚析构函数，确保基类确实有虚析构函数
     */
    ~SerialPortManager() override;

    /**
     * @brief 开始检测串口，并开启定时器，监测串口的状态，默认2000ms检测一次。
     * @param intervalMs 检测间隔（毫秒），占位参数，默认2000ms。
     */
    void startMonitoring(int intervalMs = 2000);

    /**
     * @brief 停止串口监控
     */
    void stopMonitoring();

    /**
     * @brief 获取当前可用的串口列表
     * @return 串口名称列表
     */
    QStringList availablePorts() const;

    /**
     * @brief 检查是否正在监控
     * @return true表示正在监控
     */
    bool isMonitoring() const;

public slots:
    /**
     * @brief 立即检测串口（手动触发）
     */
    void detectPorts();

signals:
    /**
     * @brief 串口列表发生变化时发射
     * @param ports 新的串口列表
     */
    void portsChanged(const QStringList &ports);

    /**
     * @brief 串口设备插入时发射
     * @param portName 新插入的串口名称
     */
    void portAdded(const QString &portName);

    /**
     * @brief 串口设备移除时发射
     * @param portName 被移除的串口名称
     */
    void portRemoved(const QString &portName);

private slots:
    /**
     * @brief 定时器触发的检测槽函数
     */
    void onDetectionTimerTimeout();

private:
    /**
     * @brief 初始化组件，配置检测定时器
     */
    void initializeComponents();

    /**
     * @brief 初始化信号槽连接，定时结束，调用检测串口函数，
     * 暂存可用的串口列表，发送当前识别到的串口以及新增或减少的串口的信号
     */
    void initConnections();

    /**
     * @brief 检查设备是否为USB转串口
     * @param portInfo 串口信息
     * @return true表示是USB转串口设备
     */
    bool isUsbSerialDevice(const QSerialPortInfo &portInfo) const;

    /**
     * @brief 比较串口列表并发射相应的串口
     * @param newPorts 新检测到的串口列表
     */
    void compareAndEmitSignals(const QStringList &newPorts);

    // 成员变量
    // 智能指针类模板，确保在当前作用域消失时所指向的对象会被删除。
    QScopedPointer<QTimer> m_detectionTimer;    ///< 检测定时器
    QStringList m_currentPorts;                 ///< 当前串口列表
    bool m_isMonitoring;                        ///< 监控状态标志
};

#endif // SERIALPORTMANAGER_H
