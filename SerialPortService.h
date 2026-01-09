#ifndef SERIALPORTSERVICE_H
#define SERIALPORTSERVICE_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>
#include <QString>

/**
 * @brief 串口服务类
 *
 * 职责：
 * - 封装QSerialPort的底层操作
 * - 提供原子的地址帧和数据帧发送
 * - 管理9位地址/数据协议的校验位切换
 * - 通过信号提供非阻塞的数据接收和错误通知
 */
class SerialPortService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit SerialPortService(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~SerialPortService() override;

    /**
     * @brief 打开串口
     * @param portName 串口名称
     * @param baudRate 波特率，默认9600
     * @return 是否成功打开
     */
    bool openPort(const QString &portName, int baudRate = 9600);

    /**
     * @brief 关闭串口
     */
    void closePort();

    /**
     * @brief 检查串口是否已打开
     * @return true表示已打开
     */
    bool isOpen() const;

    /**
     * @brief 获取串口名称
     * @return 当前串口名称
     */
    QString portName() const;

    /**
     * @brief 发送地址字节（使用MarkParity，9th bit = 1）
     * @param address 地址字节
     * @param timeoutMs 写入超时时间，默认1000ms
     * @return 是否发送成功
     */
    bool writeAddressByte(uint8_t address, int timeoutMs = 1000);

    /**
     * @brief 发送数据字节（使用SpaceParity，9th bit = 0）
     * @param data 要发送的数据
     * @param timeoutMs 写入超时时间，默认1000ms
     * @return 是否发送成功
     */
    bool writeData(const QByteArray &data, int timeoutMs = 1000);

signals:
    /**
     * @brief 有数据可读时发射
     * @param data 接收到的数据
     */
    void dataReceived(const QByteArray &data);

    /**
     * @brief 串口错误发生时发射
     * @param errorString 错误描述
     */
    void errorOccurred(const QString &errorString);

    /**
     * @brief 串口状态变化时发射
     * @param isOpen true表示已打开，false表示已关闭
     */
    void portStatusChanged(bool isOpen);

private slots:
    /**
     * @brief 处理串口数据可读事件
     */
    void onReadyRead();

    /**
     * @brief 处理串口错误事件
     * @param error 错误码
     */
    void onErrorOccurred(QSerialPort::SerialPortError error);

private:
    /**
     * @brief 初始化信号连接
     */
    void setupConnections();

    /**
     * @brief 配置串口参数
     */
    void configurePort();

    // 成员变量
    QSerialPort *m_serialPort;     ///< 串口对象
    bool m_isOpen;                 ///< 串口状态标志
};

#endif // SERIALPORTSERVICE_H
