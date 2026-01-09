#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QScopedPointer>
#include <QVector>
#include <QSet>
#include <QPushButton>
#include "domain/Command.h"
#include "domain/Measurement.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class SerialPortManager;
class SerialPortService;
class DeviceController;
class OtaController;
class QButtonGroup;
class TaskListWidget;
class MeasurementChartWidget;

/**
 * @brief 主界面Widget
 *
 * 职责：
 * - 管理UI界面交互
 * - 处理用户输入和输出
 * - 与串口管理器协作完成串口通信功能
 */
class Widget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit Widget(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~Widget() override;

    /**
     * @brief 获取设备控制器指针
     * @return DeviceController 指针
     */
    DeviceController* deviceController() const;

    /**
     * @brief 显示任务列表窗口（自动测试界面）
     * @details 创建或显示 TaskListWidget，并隐藏主界面
     */
    void showTaskList();

protected:
    /**
     * @brief 事件过滤器，用于监听lineEdit的焦点获得事件
     * @param watched 被监听的对象
     * @param event 事件对象
     * @return 是否处理了该事件
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

    /**
     * @brief 重写关闭事件，隐藏主界面并返回自动测试界面
     * @param event 关闭事件对象
     */
    void closeEvent(QCloseEvent *event) override;

public slots:
    /**
     * @brief 设置选定的串口名称
     * @param portName 串口名称
     */
    void setSelectedPort(const QString &portName);

private slots:
    /**
     * @brief 串口列表发生变化时的处理
     * @param ports 新的串口列表
     */
    void onSerialPortsChanged(const QStringList &ports);

    /**
     * @brief 打开串口按钮点击处理
     */
    void onOpenSerialPortClicked();

    /**
     * @brief 电压输出按钮点击处理
     */
    void onV2_OutputClicked();

    /**
     * @brief V1(V123)电压输出按钮点击处理
     */
    void onV1_OutputClicked();

    /**
     * @brief V1(V123)电压关闭按钮点击处理
     */
    void onV1_OutputOffClicked();

    /**
     * @brief V2(V4)电压关闭按钮点击处理
     */
    void onV2_OutputOffClicked();

    void onVoltageChannelOpenClicked();

    /**
     * @brief V123电压通道开启按钮点击处理
     */
    void onV123ChannelOpenClicked();

    /**
     * @brief 暂停检测按钮点击处理
     */
    void onDetectionPauseClicked();

    /**
     * @brief 开机按钮点击处理
     */
    void onPowerButtonClicked();

    /**
     * @brief 确认键按钮点击处理
     */
    void onPowerConfirmClicked();

    /**
     * @brief 右键按钮点击处理
     */
    void onRightKeyClicked();

    /**
     * @brief SW3按键点击处理
     */
    void onSw3Clicked();

    /**
     * @brief SW4按键点击处理
     */
    void onSw4Clicked();

    /**
     * @brief SW5按键点击处理
     */
    void onSw5Clicked();

    /**
     * @brief SW6按键点击处理
     */
    void onSw6Clicked();

    /**
     * @brief OTA 升级按钮点击处理
     */
    void onUpdateClicked();

    /**
     * @brief OTA 升级进度更新处理
     * @param percent 进度百分比
     */
    void onOtaProgressChanged(int percent);

    /**
     * @brief OTA 升级完成处理
     * @param success 是否成功
     * @param message 结果消息
     */
    void onOtaUpgradeFinished(bool success, const QString &message);

    /**
     * @brief 测试电流表功能
     */
    void onDetectionClicked();

    /**
     * @brief 处理设备控制器的日志消息
     * @param message 日志消息
     */
    void onDeviceLogMessage(const QString &message);

    /**
     * @brief 处理设备连接状态变化
     * @param isConnected 是否已连接
     * @param portName 端口名称
     */
    void onDeviceConnectionChanged(bool isConnected, const QString &portName);

    /**
     * @brief 处理设备命令确认结果（类型安全版本）
     * @param command 命令类型
     * @param success 是否确认成功
     * @param sentData 发送的数据
     * @param responseData 接收到的回应数据
     */
    void onDeviceCommandConfirmed(Command command, bool success, const QByteArray &sentData, const QByteArray &responseData);
    
    /**
     * @brief 处理外部电流表测量值接收
     * @param valueMa 电流值（mA）
     */
    void onExternalMeasurementReceived(float valueMa);

private:
    /**
     * @brief 初始化UI组件控件。禁用串口下拉框，等待串口检测完成，并开启定时器，进行串口监控。
     */
    void initializeComponents();

    /**
     * @brief 启动 OTA 升级流程（内部辅助函数）
     * @param portName 串口名称
     * @param filePath 固件文件路径
     */
    void startOtaProcess(const QString &portName, const QString &filePath);

    /**
     * @brief 初始化信号槽连接
     */
    void initConnections();

    /**
     * @brief 初始化设备控制器和串口服务
     */
    void initDeviceController();

    /**
     * @brief 获取当前选中的串口名称
     * @return 串口名称，如果没有选中返回空字符串
     */
    QString selectedPortName() const;

    /**
     * @brief 设置串口下拉框的状态
     * @param enabled true表示启用
     */
    void setPortComboBoxEnabled(bool enabled);

    /**
     * @brief 更新串口下拉框内容
     * @param ports 串口列表
     * @param keepSelection 是否保持当前选择
     */
    void updatePortComboBox(const QStringList &ports, bool keepSelection = true);

    /**
     * @brief 智能追加文本到textEdit并自动滚动
     * @param text 要追加的文本
     * @details 只有当用户接近textEdit底部时才自动滚动，否则保持当前位置
     */
    void appendTextWithAutoScroll(const QString &text);

    /**
     * @brief 获取V1通道电压值
     * @return V1电压值，失败返回-1.0
     */
    double getV1Voltage() const;

    /**
     * @brief 获取V2通道电压值
     * @return V2电压值，失败返回-1.0
     */
    double getV2Voltage() const;

    /**
     * @brief 获取选中的V1/V2/V3通道ID
     * @return 通道ID (0x01=V1, 0x02=V2, 0x03=V3)，未选择返回0x00
     */
    uint8_t getSelectedChannelId() const;

    /**
     * @brief 获取V1档位电压的BCD编码
     * @return 电压BCD值 (0x22=2.2V, 0x24=2.4V)，未选择返回0x00
     */
    uint8_t getV1VoltageBcd() const;

    /**
     * @brief 初始化单选按钮组和互斥逻辑
     */
    void initializeButtonGroups();

private slots:
    /**
     * @brief lineEdit获得焦点时的处理
     * @details 当用户点击lineEdit准备输入时，清除V2选择并恢复初始提示
     */
    void onLineEditV2FocusIn();

    /**
     * @brief V1 电压输入框获得焦点时的处理
     */
    void onLineEditV1FocusIn();

    /**
     * @brief 自动测试按钮点击处理，弹出任务列表窗口
     */
    void onAutoTestClicked();

    /**
     * @brief 导出任务配置按钮点击处理
     * @details 调用 TaskListWidget 的 exportConfiguration() 方法
     */
    void onExportTaskClicked();

    /**
     * @brief 导入任务配置按钮点击处理
     * @details 调用 TaskListWidget 的 importConfiguration() 方法
     */
    void onImportTaskClicked();

private:
    /**
     * @brief 设置所有可操作控件的启用状态
     * @param enabled 是否启用
     */
    void setControlsEnabled(bool enabled);

private:
    // 成员变量
    QScopedPointer<Ui::Widget> ui;                          ///< UI指针，使用智能指针托管
    QScopedPointer<SerialPortManager> m_serialPortManager;  ///< 串口管理器
    QScopedPointer<SerialPortService> m_serialPortService;  ///< 串口服务
    QScopedPointer<DeviceController> m_deviceController;    ///< 设备控制器
    QScopedPointer<OtaController> m_otaController;          ///< OTA 升级控制器
    QScopedPointer<TaskListWidget> m_taskListWidget;        ///< 任务列表窗口
    bool m_isInitialized;                                   ///< 初始化完成标志
    
    // 单选按钮组
    QButtonGroup *m_v1ButtonGroup;                          ///< V1电压选择组(groupBox_v1: 2.2V/2.4V)
    QButtonGroup *m_v1ChannelGroup;                         ///< V1通道选择组(groupBox_v1: V1/V2/V3)
    QButtonGroup *m_v2ButtonGroup;                          ///< V2电压选择组(groupBox_v2)
    
    // 按钮原始文本缓存
    QString m_originalPowerButtonText;                      ///< 主电源按钮原始文本
        
    // 串口首次开机状态管理
    QStringList m_knownPorts;                               ///< 当前已知的串口列表，用于检测变化
    QSet<QString> m_firstPowerDonePorts;                    ///< 已完成首次开机的端口集合
    
    // 电流检测通道状态管理
    bool m_isChannelOpened;                                 ///< 通道是否已开启
    uint8_t m_currentRangeCode;                             ///< 当前开启的档位码
    uint8_t m_currentChannelCode;                           ///< 当前开启的通道码

    // 图表组件
    MeasurementChartWidget *m_chartWidget = nullptr;        ///< 测量数据图表组件
    
    // 日志清空按钮
    QPushButton *m_clearLogButton = nullptr;                ///< 清空日志按钮

private slots:
    /**
     * @brief 处理清空日志按钮点击
     * @details 清空日志显示框中的所有内容
     */
    void onClearLogClicked();
};

#endif // WIDGET_H
