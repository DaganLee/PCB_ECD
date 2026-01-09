#include "widget.h"
#include "ui_widget.h"
#include "SerialPortManager.h"
#include "SerialPortService.h"
#include "DeviceController.h"
#include "DeviceProtocol.h"
#include "MeasurementChartWidget.h"
#include "TaskListWidget.h"
#include "OtaController.h"
#include <QDoubleValidator>
#include <QScrollBar>
#include <QTime>
#include <QButtonGroup>
#include <QRadioButton>
#include <QAbstractButton>
#include <QEvent>
#include <QFocusEvent>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QDateTime>
#include <QPushButton>
#include <QApplication>

#include <QDebug>

Widget::Widget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::Widget),
      m_serialPortManager(new SerialPortManager(this)),
      m_serialPortService(new SerialPortService(this)),
      m_deviceController(new DeviceController(m_serialPortService.data(), this)),
      m_otaController(new OtaController(this)),
      m_isInitialized(false),
      m_v1ButtonGroup(new QButtonGroup(this)),
      m_v1ChannelGroup(new QButtonGroup(this)),
      m_v2ButtonGroup(new QButtonGroup(this)),
      m_knownPorts(),
      m_firstPowerDonePorts(),
      m_isChannelOpened(false),
      m_currentRangeCode(0x00),
      m_currentChannelCode(0x00)
{
    ui->setupUi(this);
    initializeComponents();   // 👌
    initializeButtonGroups(); // 👌
    initDeviceController();   // 👌 最终测量显示值的更新
    initConnections();
    m_isInitialized = true;

    // 初始化测量数据图表组件
    m_chartWidget = new MeasurementChartWidget(ui->widget_chart);
    QVBoxLayout *chartLayout = new QVBoxLayout(ui->widget_chart);
    chartLayout->setContentsMargins(0, 0, 0, 0);
    chartLayout->addWidget(m_chartWidget);
    
    // 连接图表组件的日志信号（可选）
    connect(m_chartWidget, &MeasurementChartWidget::logMessage,
            this, &Widget::appendTextWithAutoScroll);
}

Widget::~Widget()
{
    // QScopedPointer 会自动管理资源释放
    // DeviceController 会自动断开连接
}

DeviceController* Widget::deviceController() const
{
    return m_deviceController.data();
}

/// @brief
/// @param watched
/// @param event
/// @return
bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    // 监听lineEdit的焦点获得事件
    if (event->type() == QEvent::FocusIn)
    {
        if (watched == ui->lineEdit_voltage_V2)
        {
            // 调用V2焦点获得处理函数
            onLineEditV2FocusIn();
        }
        else if (watched == ui->lineEdit_voltage_V1)
        {
            // 调用V1焦点获得处理函数
            onLineEditV1FocusIn();
        }
    }

    // 继续传递事件给基类处理
    return QWidget::eventFilter(watched, event);
}

void Widget::initializeComponents()
{
    // 初始化UI组件状态

    // 禁用串口选择框，等待串口检测完成
    setPortComboBoxEnabled(false);

    // 设置textEdit为只读模式，允许选择和复制但禁止编辑
    ui->textEdit_receive->setReadOnly(true);

    // 初始化清空日志按钮（位于日志显示框右上角）
    m_clearLogButton = new QPushButton(tr("清空"), ui->groupBox_receive);
    m_clearLogButton->setFixedSize(60, 30);
    m_clearLogButton->setStyleSheet("font: 14pt \"Agency FB\"; color: rgb(0, 0, 0);");
    // 将按钮定位到groupBox_receive的右上角（相对于groupBox）
    m_clearLogButton->move(ui->groupBox_receive->width() - m_clearLogButton->width() - 10, 15);
    m_clearLogButton->show();
    connect(m_clearLogButton, &QPushButton::clicked, this, &Widget::onClearLogClicked);

    // 设置检测结果显示框为只读模式，允许选择和复制但禁止编辑
    ui->lineEdit_detection->setReadOnly(true);

    // 设置输入电压文本框的提示文本
    ui->lineEdit_voltage_V2->setPlaceholderText(tr("设置V2电压："));
    // 电压文本框输入验证，限制范围与小数位，高级限定法
    // 电路设计使得v2电压范围为1.60V-10.80V，保留1位小数精度（与新需求一致）
    QDoubleValidator *validator = new QDoubleValidator(1.60, 10.80, 1, this);
    validator->setNotation(QDoubleValidator::StandardNotation); // 使用标准表示法表示输入的电压值
    ui->lineEdit_voltage_V2->setValidator(validator);              // 将限定条件器设置到文本框中

    // V1 文本框初始提示与验证（范围 1.2V ~ 5.0V，1 位小数）
    ui->lineEdit_voltage_V1->setPlaceholderText(tr("设置V1电压："));
    QDoubleValidator *validator_v1 = new QDoubleValidator(1.20, 5.00, 1, this);
    validator_v1->setNotation(QDoubleValidator::StandardNotation);
    ui->lineEdit_voltage_V1->setValidator(validator_v1);

    ui->pushButton_detection_pause->setEnabled(true);

    // 注意：串口管理器监控将在 initConnections() 之后启动，
    // 以确保信号连接完成后才进行首次端口检测
}

void Widget::initConnections()
{
    // 连接串口管理器信号，信号参数为当前检测到的新的串口列表
    connect(m_serialPortManager.data(), &SerialPortManager::portsChanged,
            this, &Widget::onSerialPortsChanged);

    // 打开串口按钮
    connect(ui->pushButton_openSerial, &QPushButton::clicked,
            this, &Widget::onOpenSerialPortClicked);

    // V1(V123)电压输出
    connect(ui->pushButton_output_V1, &QPushButton::clicked,
            this, &Widget::onV1_OutputClicked);

    // V1(V123)电压关闭
    connect(ui->pushButton_output_V1_off, &QPushButton::clicked,
            this, &Widget::onV1_OutputOffClicked);

    // V2(V4)电压输出
    connect(ui->pushButton_output_V2, &QPushButton::clicked,
            this, &Widget::onV2_OutputClicked);

    // V2(V4)电压关闭
    connect(ui->pushButton_output_V2_off, &QPushButton::clicked,
            this, &Widget::onV2_OutputOffClicked);

    // V123电压通道开启（pushButton_output_succeed_v1）
    connect(ui->pushButton_output_succeed_v1, &QPushButton::clicked,
            this, &Widget::onV123ChannelOpenClicked);

    // V4电压通道开启（pushButton_output_succeed_v2）
    connect(ui->pushButton_output_succeed_v2, &QPushButton::clicked,
            this, &Widget::onVoltageChannelOpenClicked);

    // 电流表测试按钮
    connect(ui->pushButton_detection, &QPushButton::clicked,
            this, &Widget::onDetectionClicked);

    // 暂停检测按钮
    connect(ui->pushButton_detection_pause, &QPushButton::clicked,
            this, &Widget::onDetectionPauseClicked);

    // 继电器开机/确认键按钮
    connect(ui->pushButton_power_confirm, &QPushButton::clicked,
            this, &Widget::onPowerConfirmClicked);

    // 继电器右键按钮
    connect(ui->pushButton_right, &QPushButton::clicked,
            this, &Widget::onRightKeyClicked);

    // 继电器SW3按键
    connect(ui->pushButton_sw3, &QPushButton::clicked,
            this, &Widget::onSw3Clicked);

    // 继电器SW4按键
    connect(ui->pushButton_sw4, &QPushButton::clicked,
            this, &Widget::onSw4Clicked);

    // 继电器SW5按键
    connect(ui->pushButton_sw5, &QPushButton::clicked,
            this, &Widget::onSw5Clicked);

    // 继电器SW6按键
    connect(ui->pushButton_sw6, &QPushButton::clicked,
            this, &Widget::onSw6Clicked);

    // 自动测试按钮 - 弹出任务列表窗口
    connect(ui->pushButton_autoTest, &QPushButton::clicked,
            this, &Widget::onAutoTestClicked);

    // 导出任务配置按钮
    connect(ui->pushButton_exportTask, &QPushButton::clicked,
            this, &Widget::onExportTaskClicked);

    // 导入任务配置按钮
    connect(ui->pushButton_importTask, &QPushButton::clicked,
            this, &Widget::onImportTaskClicked);

    // 安装事件过滤器以监听lineEdit的焦点获得事件
    ui->lineEdit_voltage_V2->installEventFilter(this);
    ui->lineEdit_voltage_V1->installEventFilter(this);

    // V1电压下拉框选择事件：当用户选择下拉框时，清空手动输入框
    connect(ui->comboBox_voltage_V1, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index)
            {
        if (index >= 0) {
            // 用户选择了下拉框，清空手动输入框
            ui->lineEdit_voltage_V1->clear();
            ui->lineEdit_voltage_V1->setPlaceholderText(tr("由固定电压决定"));
        } });

    // V2电压下拉框选择事件：当用户选择下拉框时，清空手动输入框
    connect(ui->comboBox_voltage_V2, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index)
            {
        if (index >= 0) {
            // 用户选择了下拉框，清空手动输入框
            ui->lineEdit_voltage_V2->clear();
            ui->lineEdit_voltage_V2->setPlaceholderText(tr("由固定电压决定"));
        } });

    // V1 微调按钮 -> 发送 V123 StepAdjust 命令（0x06 + channel + action）
    // 根据当前选中的 V1/V2/V3 通道发送对应的通道ID
    connect(ui->pushButton_v1_up, &QPushButton::clicked, this, [this]()
            {
                if (!m_deviceController->isConnected())
                {
                    appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
                    return;
                }

                // 获取当前选中的通道ID
                uint8_t channelId = getSelectedChannelId();
                if (channelId == 0x00)
                {
                    appendTextWithAutoScroll(tr("错误：请先选择V1/V2/V3通道"));
                    return;
                }

                m_deviceController->v123StepAdjust(channelId, 0x01); // 选中通道, UP
            });
    connect(ui->pushButton_v1_down, &QPushButton::clicked, this, [this]()
            {
                if (!m_deviceController->isConnected())
                {
                    appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
                    return;
                }

                // 获取当前选中的通道ID
                uint8_t channelId = getSelectedChannelId();
                if (channelId == 0x00)
                {
                    appendTextWithAutoScroll(tr("错误：请先选择V1/V2/V3通道"));
                    return;
                }

                m_deviceController->v123StepAdjust(channelId, 0x02); // 选中通道, DOWN
            });
    // V4 微调按钮 -> 发送 V4 StepAdjust 命令（0x06 + 0x04 + action，统一3字节格式）
    connect(ui->pushButton_v2_up, &QPushButton::clicked, this, [this]()
            {
                if (!m_deviceController->isConnected())
                {
                    appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
                    return;
                }
                m_deviceController->v4StepAdjust(0x01); // V4(通道码0x04), UP
            });
    connect(ui->pushButton_v2_down, &QPushButton::clicked, this, [this]()
            {
                if (!m_deviceController->isConnected())
                {
                    appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
                    return;
                }
                m_deviceController->v4StepAdjust(0x02); // V4(通道码0x04), DOWN
            });

    // IAP 升级按钮
    connect(ui->pushButton_update, &QPushButton::clicked,
            this, &Widget::onUpdateClicked);

    // OTA 控制器信号连接
    connect(m_otaController.data(), &OtaController::progressChanged,
            this, &Widget::onOtaProgressChanged);
    connect(m_otaController.data(), &OtaController::upgradeFinished,
            this, &Widget::onOtaUpgradeFinished);
    connect(m_otaController.data(), &OtaController::logMessage,
            this, &Widget::appendTextWithAutoScroll);

    // 在所有信号连接完成后，启动串口管理器监控
    // 这样可以确保首次端口检测的结果能被 onSerialPortsChanged 槽函数接收到
    // 解决了"下位机先连接、上位机后启动时无法识别串口"的问题
    // 默认检测间隔为2000ms，占位参数
    m_serialPortManager->startMonitoring();
}

void Widget::initDeviceController()
{
    // 连接设备控制器信号

    // 显示日志信息，信号参数为日志信息
    connect(m_deviceController.data(), &DeviceController::logMessage,
            this, &Widget::onDeviceLogMessage);

    // 串口连接状态改变信号，信号参数为连接状态和串口名称
    // 当串口连接状态改变时——连上串口还是断开串口
    // 更新UI控件状态
    connect(m_deviceController.data(), &DeviceController::connectionStatusChanged,
            this, &Widget::onDeviceConnectionChanged);

    // ‘控制命令’确认信号，信号参数为命令、成功与否、发送的数据和响应的数据
    connect(m_deviceController.data(), &DeviceController::commandConfirmed,
            this, &Widget::onDeviceCommandConfirmed);

    // 外部电流表测量数据接收信号（来自RS485电流表，经MCU转发）
    connect(m_deviceController.data(), &DeviceController::externalMeasurementReceived,
            this, &Widget::onExternalMeasurementReceived);
}


// 设置当前选中的串口
void Widget::setSelectedPort(const QString &portName)
{
    if (!m_isInitialized)
    {
        return;
    }

    int index = ui->comboBox_serialList->findText(portName);
    if (index >= 0)
    {
        ui->comboBox_serialList->setCurrentIndex(index);
    }
}

void Widget::onSerialPortsChanged(const QStringList &ports)
{
    // 检测端口列表变化，找出被移除的端口
    QStringList removedPorts;
    for (const QString &port : m_knownPorts)
    {
        // 判断当前已知端口列表中是否包含当前检测到的新的串口列表
        if (!ports.contains(port))
        {
            removedPorts << port;
        }
    }

    // 清除已移除端口的首次开机记录（端口消失后重新出现需要重新首次开机）
    for (const QString &port : removedPorts)
    {
        m_firstPowerDonePorts.remove(port);
    }

    // 兜底检查：如果当前连接的端口消失了，主动断开连接
    if (m_deviceController->isConnected())
    {
        QString currentPort = m_deviceController->currentPortName();
        if (!currentPort.isEmpty() && removedPorts.contains(currentPort))
        {
            appendTextWithAutoScroll(tr("检测到当前连接的串口 %1 已被移除，自动断开连接").arg(currentPort));
            m_deviceController->disconnectDevice();
            // disconnectDevice 会触发 connectionStatusChanged(false)，进而更新 UI 按钮为"打开串口"
        }
    }

    // 更新已知端口列表
    m_knownPorts = ports;

    // 更新UI控件，串口列表
    updatePortComboBox(ports, true);
}

void Widget::onOpenSerialPortClicked()
{
    // 获取当前选中的串口名称
    QString portName = selectedPortName();
    if (portName.isEmpty() || portName == tr("无可用串口"))
    {
        appendTextWithAutoScroll(tr("错误：请先选择一个有效的串口"));
        return;
    }

    // 如果设备已经连接，则断开它——关闭串口功能
    if (m_deviceController->isConnected())
    {
        m_deviceController->disconnectDevice();
        ui->pushButton_openSerial->setText(tr("打开串口"));
        return;
    }

    // 尝试连接到设备
    if (m_deviceController->connectToDevice(portName, DeviceProtocol::kBaud))
    {
        ui->pushButton_openSerial->setText(tr("关闭串口"));

        // 发送测试命令：0x12 0x34
        QByteArray testData;
        testData.append(static_cast<char>(0x34)); // 测试数据1
        testData.append(static_cast<char>(0x34)); // 测试数据2
        m_deviceController->sendTestCommand(testData);
    }
}


// 
void Widget::onV1_OutputClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 检查是否选择了V1/V2/V3通道
    uint8_t channelId = getSelectedChannelId();
    if (channelId == 0x00)
    {
        appendTextWithAutoScroll(tr("错误：请选择V1/V2/V3输出通道"));
        return;
    }

    // 获取V1输出通道的电压数值（来自groupBox_v1的单选按钮或lineEdit）
    double v1Voltage = getV1Voltage();
    if (v1Voltage < 0)
    {
        appendTextWithAutoScroll(tr("错误：请选择或输入V1电压值（1.2~5.0V）"));
        return;
    }
    // 本地校验V1范围（1.2~5.0V）
    if (v1Voltage < 1.2 || v1Voltage > 5.0)
    {
        appendTextWithAutoScroll(tr("错误：V1电压值超出范围（1.2~5.0V）"));
        return;
    }

    // 通道名称映射
    QString channelName;
    switch (channelId)
    {
    case 0x01:
        channelName = "V1";
        break;
    case 0x02:
        channelName = "V2";
        break;
    case 0x03:
        channelName = "V3";
        break;
    }

    // 使用设备控制器发送V123电压控制命令（3字节：0x02 + 通道ID + 电压BCD）
    appendTextWithAutoScroll(tr("发送V123电压控制：通道=%1, 电压=%2V")
                                 .arg(channelName)
                                 .arg(v1Voltage, 0, 'f', 1));

    m_deviceController->setV123VoltageControl(channelId, v1Voltage);
}

void Widget::onV1_OutputOffClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 检查是否选择了V1/V2/V3通道
    uint8_t channelId = getSelectedChannelId();
    if (channelId == 0x00)
    {
        appendTextWithAutoScroll(tr("错误：请选择V1/V2/V3输出通道"));
        return;
    }

    // 通道名称映射
    QString channelName;
    switch (channelId)
    {
    case 0x01:
        channelName = "V1";
        break;
    case 0x02:
        channelName = "V2";
        break;
    case 0x03:
        channelName = "V3";
        break;
    }

    // 发送关闭命令：电压值设为0.0
    appendTextWithAutoScroll(tr("发送V123电压关闭：通道=%1")
                                 .arg(channelName));

    m_deviceController->setV123VoltageControl(channelId, 0.0);
}

void Widget::onV2_OutputClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 获取V2(V4)输出通道的电压值（来自groupBox_v2的单选按钮或lineEdit_voltage）
    double v4Voltage = getV2Voltage();
    if (v4Voltage < 0)
    {
        appendTextWithAutoScroll(tr("错误：请选择V4电压或在输入框中输入有效电压值"));
        return;
    }

    // 验证V4电压值范围
    if (!m_deviceController->isValidVoltage(v4Voltage))
    {
        appendTextWithAutoScroll(tr("错误：V4电压值超出范围（1.60~10.80V）"));
        return;
    }

    // 使用设备控制器发送V4电压控制命令（3字节：0x02 + 0x04 + 电压BCD）
    appendTextWithAutoScroll(tr("发送V4电压控制：电压=%1V")
                                 .arg(v4Voltage, 0, 'f', 2));

    m_deviceController->setV4VoltageControl(v4Voltage);
}

void Widget::onV2_OutputOffClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 发送V4通道关闭命令：电压值设为0.0
    appendTextWithAutoScroll(tr("发送V4电压关闭"));

    m_deviceController->setV4VoltageControl(0.0);
}

void Widget::onVoltageChannelOpenClicked()
{
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    ui->pushButton_output_succeed_v2->setEnabled(false);
    QString originalText = ui->pushButton_output_succeed_v2->text();
    ui->pushButton_output_succeed_v2->setText(tr("开启中..."));

    // 发送V4通道开启命令（2字节：0x12 + 0x04）
    bool success = m_deviceController->openV4Channel();
    if (!success)
    {
        ui->pushButton_output_succeed_v2->setText(originalText);
        ui->pushButton_output_succeed_v2->setEnabled(true);
        appendTextWithAutoScroll(tr("V4通道开启命令发送失败"));
    }
}

void Widget::onV123ChannelOpenClicked()
{
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 检查是否选择了v123通道
    uint8_t channelId = getSelectedChannelId();
    if (channelId == 0x00)
    {
        appendTextWithAutoScroll(tr("错误：请选择V1/V2/V3输出通道"));
        return;
    }

    ui->pushButton_output_succeed_v1->setEnabled(false);
    QString originalText = ui->pushButton_output_succeed_v1->text();
    ui->pushButton_output_succeed_v1->setText(tr("开启中..."));

    // 发送V123通道开启命令（2字节：0x12 + 通道ID）
    bool success = m_deviceController->openV123Channel(channelId);
    if (!success)
    {
        ui->pushButton_output_succeed_v1->setText(originalText);
        ui->pushButton_output_succeed_v1->setEnabled(true);
        appendTextWithAutoScroll(tr("V123通道开启命令发送失败"));
    }
}

void Widget::onPowerButtonClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }
}

void Widget::onPowerConfirmClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用按钮防重复点击
    ui->pushButton_power_confirm->setEnabled(false);
    ui->pushButton_right->setEnabled(false);

    // 发送继电器确认键命令给下位机
    bool success = m_deviceController->pressPowerConfirmKey();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);
        appendTextWithAutoScroll(tr("继电器-确认键命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}

void Widget::onRightKeyClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用按钮防重复点击
    ui->pushButton_power_confirm->setEnabled(false);
    ui->pushButton_right->setEnabled(false);

    // 显示"执行中…"状态
    QString originalText = ui->pushButton_right->text();
    ui->pushButton_right->setText(tr("执行中..."));

    // 发送继电器右键命令给下位机
    bool success = m_deviceController->pressRightKey();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        ui->pushButton_right->setText(originalText);
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);
        appendTextWithAutoScroll(tr("继电器-右键命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}

void Widget::onSw3Clicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用所有继电器按钮防重复点击
    ui->pushButton_power_confirm->setEnabled(false);
    ui->pushButton_right->setEnabled(false);
    ui->pushButton_sw3->setEnabled(false);
    ui->pushButton_sw4->setEnabled(false);
    ui->pushButton_sw5->setEnabled(false);
    ui->pushButton_sw6->setEnabled(false);

    // 显示"执行中…"状态
    QString originalText = ui->pushButton_sw3->text();
    ui->pushButton_sw3->setText(tr("执行中..."));

    // 发送继电器SW3键命令给下位机
    bool success = m_deviceController->pressSw3Key();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        ui->pushButton_sw3->setText(originalText);
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);
        ui->pushButton_sw3->setEnabled(true);
        ui->pushButton_sw4->setEnabled(true);
        ui->pushButton_sw5->setEnabled(true);
        ui->pushButton_sw6->setEnabled(true);
        appendTextWithAutoScroll(tr("继电器-SW3键命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}

void Widget::onSw4Clicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用所有继电器按钮防重复点击
    ui->pushButton_power_confirm->setEnabled(false);
    ui->pushButton_right->setEnabled(false);
    ui->pushButton_sw3->setEnabled(false);
    ui->pushButton_sw4->setEnabled(false);
    ui->pushButton_sw5->setEnabled(false);
    ui->pushButton_sw6->setEnabled(false);

    // 显示"执行中…"状态
    QString originalText = ui->pushButton_sw4->text();
    ui->pushButton_sw4->setText(tr("执行中..."));

    // 发送继电器SW4键命令给下位机
    bool success = m_deviceController->pressSw4Key();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        ui->pushButton_sw4->setText(originalText);
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);
        ui->pushButton_sw3->setEnabled(true);
        ui->pushButton_sw4->setEnabled(true);
        ui->pushButton_sw5->setEnabled(true);
        ui->pushButton_sw6->setEnabled(true);
        appendTextWithAutoScroll(tr("继电器-SW4键命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}

void Widget::onSw5Clicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用所有继电器按钮防重复点击
    ui->pushButton_power_confirm->setEnabled(false);
    ui->pushButton_right->setEnabled(false);
    ui->pushButton_sw3->setEnabled(false);
    ui->pushButton_sw4->setEnabled(false);
    ui->pushButton_sw5->setEnabled(false);
    ui->pushButton_sw6->setEnabled(false);

    // 显示"执行中…"状态
    QString originalText = ui->pushButton_sw5->text();
    ui->pushButton_sw5->setText(tr("执行中..."));

    // 发送继电器SW5键命令给下位机
    bool success = m_deviceController->pressSw5Key();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        ui->pushButton_sw5->setText(originalText);
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);
        ui->pushButton_sw3->setEnabled(true);
        ui->pushButton_sw4->setEnabled(true);
        ui->pushButton_sw5->setEnabled(true);
        ui->pushButton_sw6->setEnabled(true);
        appendTextWithAutoScroll(tr("继电器-SW5键命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}

void Widget::onSw6Clicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用所有继电器按钮防重复点击
    ui->pushButton_power_confirm->setEnabled(false);
    ui->pushButton_right->setEnabled(false);
    ui->pushButton_sw3->setEnabled(false);
    ui->pushButton_sw4->setEnabled(false);
    ui->pushButton_sw5->setEnabled(false);
    ui->pushButton_sw6->setEnabled(false);

    // 显示"执行中…"状态
    QString originalText = ui->pushButton_sw6->text();
    ui->pushButton_sw6->setText(tr("执行中..."));

    // 发送继电器SW6键命令给下位机
    bool success = m_deviceController->pressSw6Key();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        ui->pushButton_sw6->setText(originalText);
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);
        ui->pushButton_sw3->setEnabled(true);
        ui->pushButton_sw4->setEnabled(true);
        ui->pushButton_sw5->setEnabled(true);
        ui->pushButton_sw6->setEnabled(true);
        appendTextWithAutoScroll(tr("继电器-SW6键命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}




/******************************************************************************
 * OTA 升级相关槽函数
 ******************************************************************************/

void Widget::onUpdateClicked()
{
    // 检查是否正在升级
    if (m_otaController->isUpgrading())
    {
        QMessageBox::warning(this, tr("提示"), tr("升级正在进行中，请等待完成"));
        return;
    }

    // 弹出文件选择对话框，选择固件文件
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("选择固件文件"),
        QString(),
        tr("二进制文件 (*.bin);;所有文件 (*.*)"));

    if (filePath.isEmpty())
    {
        // 用户取消选择
        return;
    }

    // 获取当前选中的串口
    QString portName = selectedPortName();
    if (portName.isEmpty())
    {
        QMessageBox::warning(this, tr("错误"), tr("请先选择串口"));
        return;
    }

    // 检查当前是否连接了 APP (9600bps)
    if (m_serialPortService->isOpen())
    {
        // 步骤 A: 发送跳转指令给 APP，让它重启到 Bootloader
        appendTextWithAutoScroll(tr("正在发送跳转指令给 APP (0xC0 + 0x99 0xAA)..."));

        // 【关键修复】使用 DeviceController 发送 IAP 跳转指令
        // 会先发送 0xC0 地址帧（唤醒下位机），再发送 0x99 0xAA 数据帧
        bool success = m_deviceController->sendIapJumpCommand();
        
        if (!success) {
            appendTextWithAutoScroll(tr("错误：发送跳转指令失败"));
            return;
        }
        
        appendTextWithAutoScroll(tr("跳转指令已发送: 0xC0(地址帧) + 0x99 0xAA(数据帧)"));

        // 延迟 200ms 确保数据物理发送完成
        QTimer::singleShot(200, this, [this, portName, filePath]()
        {
            // 先断开 DeviceController 的连接
            m_deviceController->disconnectDevice();
            
            // 确保 SerialPortService 的串口也关闭
            if (m_serialPortService->isOpen()) {
                m_serialPortService->closePort();
            }
            
            ui->pushButton_openSerial->setText(tr("打开串口"));
            appendTextWithAutoScroll(tr("已断开 APP 连接，等待设备重启进入 Bootloader..."));

            // 步骤 C: 延迟 2 秒，等待单片机复位并进入 Bootloader 模式
            // （单片机执行软复位 + Bootloader 初始化串口需要时间）
            QTimer::singleShot(2000, this, [this, portName, filePath]() {
                startOtaProcess(portName, filePath);
            });
        });
        
        return;  // 【重要】立即返回，防止继续执行
    }
    else
    {
        // 如果当前没有连接 APP（假设设备已经在 Bootloader 模式）
        // 直接尝试连接 Bootloader
        appendTextWithAutoScroll(tr("检测到串口未打开，假设设备已在 Bootloader 模式"));
        startOtaProcess(portName, filePath);
    }
}

void Widget::startOtaProcess(const QString &portName, const QString &filePath)
{
    // 重置进度条
    ui->progressBar->setValue(0);

    // 禁用升级按钮，防止重复点击
    ui->pushButton_update->setEnabled(false);
    ui->pushButton_update->setText(tr("升级中..."));

    appendTextWithAutoScroll(tr("尝试连接 Bootloader (9600bps)..."));
    appendTextWithAutoScroll(tr("固件文件: %1").arg(filePath));

    // 启动 OTA 控制器（它会以 9600bps 打开串口）
    if (!m_otaController->startUpgrade(portName, filePath))
    {
        // 启动失败
        ui->pushButton_update->setEnabled(true);
        ui->pushButton_update->setText(tr("升级"));
        QMessageBox::critical(this, tr("错误"),
                              tr("无法连接 Bootloader，请确认：\n"
                                 "1. 设备已正确重启\n"
                                 "2. 串口未被占用\n"
                                 "3. 设备处于 Bootloader 模式"));
    }
}

void Widget::onOtaProgressChanged(int percent)
{
    ui->progressBar->setValue(percent);
}

void Widget::onOtaUpgradeFinished(bool success, const QString &message)
{
    // 恢复升级按钮状态
    ui->pushButton_update->setEnabled(true);
    ui->pushButton_update->setText(tr("升级"));

    // 记录日志
    appendTextWithAutoScroll(message);

    // 弹窗提示结果
    if (success)
    {
        ui->progressBar->setValue(100);
        QMessageBox::information(this, tr("升级成功"), tr("固件升级成功！\n设备将自动重启。"));
    }
    else
    {
        ui->progressBar->setValue(0);
        QMessageBox::critical(this, tr("升级失败"), message);
    }
}

QString Widget::selectedPortName() const
{
    return ui->comboBox_serialList->currentText();
}

void Widget::setPortComboBoxEnabled(bool enabled)
{
    ui->comboBox_serialList->setEnabled(enabled);
}

void Widget::updatePortComboBox(const QStringList &ports, bool keepSelection)
{
    QString currentSelection;

    // 保持当前选中的串口
    if (keepSelection)
    {
        // 获取当前选中的串口名称
        currentSelection = ui->comboBox_serialList->currentText();
    }

    // 清空并重新填充ComboBox
    ui->comboBox_serialList->clear();

    if (ports.isEmpty())
    {
        ui->comboBox_serialList->addItem(tr("无可用串口"));
        setPortComboBoxEnabled(false);
    }
    else
    {
        // 将新串口列表添加到ComboBox中
        ui->comboBox_serialList->addItems(ports);
        setPortComboBoxEnabled(true); // 启用串口选择框

        // 尝试恢复之前的选择的串口
        // 若之前未选择串口或没有串口，则依旧无串口
        if (keepSelection && !currentSelection.isEmpty())
        {
            // 取之前选中的串口名称在当前ComboBox中的索引
            int index = ui->comboBox_serialList->findText(currentSelection);

            // 找到匹配项
            if (index >= 0)
            {
                // 通过索引设置之前选中的串口为当前的串口，使得串口选择保持不变
                ui->comboBox_serialList->setCurrentIndex(index);
            }
        }
    }
}

void Widget::onDeviceLogMessage(const QString &message)
{
    appendTextWithAutoScroll(message);
}

void Widget::onDeviceConnectionChanged(bool isConnected, const QString &portName)
{
    Q_UNUSED(portName);
    
    if (isConnected)
    {
        ui->pushButton_openSerial->setText(tr("关闭串口"));

        // 启用继电器按钮
        ui->pushButton_power_confirm->setEnabled(true);
        ui->pushButton_right->setEnabled(true);

        // 重置通道状态
        m_isChannelOpened = false;
        m_currentRangeCode = 0x00;
        m_currentChannelCode = 0x00;
    }
    else
    {
        ui->pushButton_openSerial->setText(tr("打开串口"));

        // 断开连接后，重置电源状态和按钮
        // 注意：不改变 m_firstPowerDonePorts，保留该端口的首次开机记录

        // 禁用继电器按钮
        ui->pushButton_power_confirm->setEnabled(false);
        ui->pushButton_right->setEnabled(false);

        // 重置通道状态
        m_isChannelOpened = false;
        m_currentRangeCode = 0x00;
        m_currentChannelCode = 0x00;
    }
}

void Widget::onExternalMeasurementReceived(float valueMa)
{
    // 更新lineEdit_detection显示外部电流表的测量值
    QString displayText = QString("%1 mA").arg(valueMa, 0, 'f', 5);
    ui->lineEdit_detection->setText(displayText);

    // 将测量数据添加到图表
    if (m_chartWidget)
    {
        Measurement measurement;
        measurement.rawValue = valueMa;
        measurement.range = Measurement::Range::MilliAmp;  // 默认mA档
        measurement.channel = Measurement::Channel::Unknown;
        measurement.timestamp = QDateTime::currentDateTime();
        
        m_chartWidget->appendMeasurement(measurement);
    }

    // 同时输出到日志（可选，DeviceController已经记录）
    // appendTextWithAutoScroll(tr("收到外部电流表测量值: %1").arg(displayText));
}

void Widget::appendTextWithAutoScroll(const QString &text)
{
    // 获取滚动条
    QScrollBar *scrollBar = ui->textEdit_receive->verticalScrollBar();

    // 检查是否接近底部（允许一定的容差，比如10像素）
    bool wasAtBottom = (scrollBar->value() >= scrollBar->maximum() - 10);

    // 获取当前时间并格式化为 (HH:mm) 格式
    QString currentTime = QTime::currentTime().toString("(hh:mm)");

    // 为文本添加时间戳前缀
    QString textWithTimestamp = currentTime + " " + text;

    // 追加带时间戳的文本
    ui->textEdit_receive->append(textWithTimestamp);

    // 如果之前接近底部，自动滚动到新的底部
    if (wasAtBottom)
    {
        scrollBar->setValue(scrollBar->maximum());
    }
}

void Widget::onDeviceCommandConfirmed(Command command, bool success, const QByteArray &sentData, const QByteArray &responseData)
{
    Q_UNUSED(sentData);
    Q_UNUSED(responseData);

    QString operationName = commandToString(command);

    if (success)
    {
        appendTextWithAutoScroll(tr("✓ %1 操作成功确认").arg(operationName));

        // 根据命令类型进行特定的UI更新（使用类型安全的switch）
        switch (command)
        {
        case Command::TestCommand:
            // 测试成功，串口连接确认有效
            appendTextWithAutoScroll(tr("串口通信测试成功，连接状态良好"));
            break;

        case Command::PowerOn:
            appendTextWithAutoScroll(tr("设备开机成功"));
            break;

        case Command::FirstPowerOn:
            appendTextWithAutoScroll(tr("设备首次开机成功"));
            // 首次开机成功后更新电源状态和按钮文本，恢复按钮可点击

            // 记录当前端口已完成首次开机，后续同端口连接将走普通开机流程
            {
                QString currentPort = m_deviceController->currentPortName();
                if (!currentPort.isEmpty())
                {
                    m_firstPowerDonePorts.insert(currentPort);
                    appendTextWithAutoScroll(tr("端口 %1 首次开机流程已完成").arg(currentPort));
                }
            }
            break;

        case Command::PowerOff:
            appendTextWithAutoScroll(tr("设备关机成功"));
            break;

        case Command::VoltageControl:
            appendTextWithAutoScroll(tr("输出电压控制指令已确认生效"));
            break;

        case Command::V123VoltageControl:
            appendTextWithAutoScroll(tr("V123电压控制指令已确认生效"));
            break;

        case Command::V4VoltageControl:
            appendTextWithAutoScroll(tr("V4电压控制指令已确认生效"));
            break;

        case Command::VoltageChannelOpen:
            appendTextWithAutoScroll(tr("电压输出通道开启指令已确认生效"));
            ui->pushButton_output_succeed_v2->setText(tr("开启通道"));
            ui->pushButton_output_succeed_v2->setEnabled(true);
            break;

        case Command::V123ChannelOpen:
            appendTextWithAutoScroll(tr("V123通道开启指令已确认生效"));
            ui->pushButton_output_succeed_v1->setText(tr("开启通道"));
            ui->pushButton_output_succeed_v1->setEnabled(true);
            break;

        case Command::V4ChannelOpen:
            appendTextWithAutoScroll(tr("V4通道开启指令已确认生效"));
            ui->pushButton_output_succeed_v2->setText(tr("开启通道"));
            ui->pushButton_output_succeed_v2->setEnabled(true);
            break;

        case Command::DetectionSelect:
            appendTextWithAutoScroll(tr("电流检测通道选择指令已确认生效"));
            break;

        case Command::ChannelConfig:
            appendTextWithAutoScroll(tr("通道配置指令已确认生效"));
            // 恢复通道开启按钮状态
            break;

        case Command::StartDetection:
            appendTextWithAutoScroll(tr("开始检测指令已确认生效"));
            // 恢复检测按钮状态
            ui->pushButton_detection->setText(tr("电流检测"));
            ui->pushButton_detection->setEnabled(true);
            // 启用停止检测按钮
            ui->pushButton_detection_pause->setEnabled(true);
            ui->pushButton_detection_pause->setText(tr("停止检测"));
            ui->pushButton_detection_pause->setToolTip(tr("点击停止外部电流表连续检测"));
            break;

        case Command::StopExternalMeter:
            appendTextWithAutoScroll(tr("外部电流表连续检测已停止"));
            // 禁用停止按钮，恢复检测按钮可用
            ui->pushButton_detection_pause->setEnabled(false);
            ui->pushButton_detection_pause->setText(tr("停止检测"));
            ui->pushButton_detection_pause->setToolTip(tr("请先开始电流检测"));
            ui->pushButton_detection->setEnabled(true);
            break;

        case Command::RelayPowerConfirm:
            appendTextWithAutoScroll(tr("继电器-确认键执行成功"));
            // 恢复继电器按钮状态（不修改按钮文字）
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            break;

        case Command::RelayRight:
            appendTextWithAutoScroll(tr("继电器-右键执行成功"));
            // 恢复继电器按钮状态
            ui->pushButton_right->setText(tr("右键"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            break;

        case Command::RelaySw3:
            appendTextWithAutoScroll(tr("继电器-SW3键执行成功"));
            // 恢复继电器按钮状态
            ui->pushButton_sw3->setText(tr("SW3"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::RelaySw4:
            appendTextWithAutoScroll(tr("继电器-SW4键执行成功"));
            // 恢复继电器按钮状态
            ui->pushButton_sw4->setText(tr("SW4"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::RelaySw5:
            appendTextWithAutoScroll(tr("继电器-SW5键执行成功"));
            // 恢复继电器按钮状态
            ui->pushButton_sw5->setText(tr("SW5"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::RelaySw6:
            appendTextWithAutoScroll(tr("继电器-SW6键执行成功"));
            // 恢复继电器按钮状态
            ui->pushButton_sw6->setText(tr("SW6"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        default:
            break;
        }
    }
    else
    {
        appendTextWithAutoScroll(tr("✗ %1 操作失败").arg(operationName));

        // 根据命令类型进行特定的错误处理
        switch (command)
        {
        case Command::TestCommand:
            appendTextWithAutoScroll(tr("警告：串口通信测试失败，请检查连接或重试"));
            break;

        case Command::PowerOn:
            appendTextWithAutoScroll(tr("设备开机失败，请检查设备状态"));
            break;

        case Command::FirstPowerOn:
            appendTextWithAutoScroll(tr("设备首次开机失败，请检查设备状态"));
            break;

        case Command::PowerOff:
            appendTextWithAutoScroll(tr("设备关机失败，请检查设备状态"));
            break;

        case Command::VoltageControl:
            appendTextWithAutoScroll(tr("输出电压控制指令失败，请重新尝试"));
            break;

        case Command::V123VoltageControl:
            appendTextWithAutoScroll(tr("V123电压控制指令失败，请重新尝试"));
            break;

        case Command::V4VoltageControl:
            appendTextWithAutoScroll(tr("V4电压控制指令失败，请重新尝试"));
            break;

        case Command::VoltageChannelOpen:
            appendTextWithAutoScroll(tr("电压输出通道开启指令失败，请重新尝试"));
            ui->pushButton_output_succeed_v2->setText(tr("开启通道"));
            ui->pushButton_output_succeed_v2->setEnabled(true);
            break;

        case Command::V123ChannelOpen:
            appendTextWithAutoScroll(tr("V123通道开启指令失败，请重新尝试"));
            ui->pushButton_output_succeed_v1->setText(tr("开启通道"));
            ui->pushButton_output_succeed_v1->setEnabled(true);
            break;

        case Command::V4ChannelOpen:
            appendTextWithAutoScroll(tr("V4通道开启指令失败，请重新尝试"));
            ui->pushButton_output_succeed_v2->setText(tr("开启通道"));
            ui->pushButton_output_succeed_v2->setEnabled(true);
            break;

        case Command::DetectionSelect:
            appendTextWithAutoScroll(tr("电流检测通道选择指令失败，请重新尝试"));
            break;

        case Command::ChannelConfig:
            appendTextWithAutoScroll(tr("通道配置指令失败，请重新尝试"));
            // 恢复通道开启按钮状态
            break;

        case Command::RelayPowerConfirm:
            appendTextWithAutoScroll(tr("继电器-确认键执行失败，请重新尝试"));
            // 恢复继电器按钮状态（不修改按钮文字）
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            break;

        case Command::RelayRight:
            appendTextWithAutoScroll(tr("继电器-右键执行失败，请重新尝试"));
            // 恢复继电器按钮状态
            ui->pushButton_right->setText(tr("右键"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            break;

        case Command::RelaySw3:
            appendTextWithAutoScroll(tr("继电器-SW3键执行失败，请重新尝试"));
            // 恢复继电器按钮状态
            ui->pushButton_sw3->setText(tr("SW3"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::RelaySw4:
            appendTextWithAutoScroll(tr("继电器-SW4键执行失败，请重新尝试"));
            // 恢复继电器按钮状态
            ui->pushButton_sw4->setText(tr("SW4"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::RelaySw5:
            appendTextWithAutoScroll(tr("继电器-SW5键执行失败，请重新尝试"));
            // 恢复继电器按钮状态
            ui->pushButton_sw5->setText(tr("SW5"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::RelaySw6:
            appendTextWithAutoScroll(tr("继电器-SW6键执行失败，请重新尝试"));
            // 恢复继电器按钮状态
            ui->pushButton_sw6->setText(tr("SW6"));
            ui->pushButton_power_confirm->setEnabled(true);
            ui->pushButton_right->setEnabled(true);
            ui->pushButton_sw3->setEnabled(true);
            ui->pushButton_sw4->setEnabled(true);
            ui->pushButton_sw5->setEnabled(true);
            ui->pushButton_sw6->setEnabled(true);
            break;

        case Command::StartDetection:
            appendTextWithAutoScroll(tr("开始检测指令失败，请重新尝试"));
            // 恢复检测按钮状态
            ui->pushButton_detection->setText(tr("电流检测"));
            ui->pushButton_detection->setEnabled(true);
            break;

        case Command::StopExternalMeter:
            appendTextWithAutoScroll(tr("停止外部电流表检测失败，请重新尝试"));
            // 恢复停止按钮状态
            ui->pushButton_detection_pause->setText(tr("停止检测"));
            ui->pushButton_detection_pause->setEnabled(true);
            break;

        default:
            break;
        }
    }
}

void Widget::initializeButtonGroups()
{

    // 设置V1通道按钮组 (groupBox_v1: radioButton_V1, radioButton_V2, radioButton_V3)
    m_v1ChannelGroup->addButton(ui->radioButton_V1); // V1通道
    m_v1ChannelGroup->addButton(ui->radioButton_V2); // V2通道
    m_v1ChannelGroup->addButton(ui->radioButton_V3); // V3通道

}

void Widget::onLineEditV1FocusIn()
{
    // 将comboBox_voltage_V1重置为无选择状态
    ui->comboBox_voltage_V1->setCurrentIndex(-1);

    // 恢复初始的placeholder文本（使用初始化时设置的文本）
    ui->lineEdit_voltage_V1->setPlaceholderText(tr("自定义V1电压："));
}

void Widget::onAutoTestClicked()
{
    showTaskList();
}

void Widget::showTaskList()
{
    // 如果任务列表窗口尚未创建，则创建它（传入设备控制器和主界面指针）
    if (!m_taskListWidget)
    {
        m_taskListWidget.reset(new TaskListWidget(m_deviceController.data(), this));
    }

    // 显示任务列表窗口
    m_taskListWidget->show();
    m_taskListWidget->raise();          // 将窗口置于顶层
    m_taskListWidget->activateWindow(); // 激活窗口
    
    // 隐藏主界面
    this->hide();
}

void Widget::closeEvent(QCloseEvent *event)
{
    // 拦截关闭事件，不退出程序
    event->ignore();
    
    // 隐藏主界面并返回自动测试界面
    showTaskList();
}

void Widget::onExportTaskClicked()
{
    // 确保 TaskListWidget 已创建（但不显示窗口）
    if (!m_taskListWidget)
    {
        m_taskListWidget.reset(new TaskListWidget(m_deviceController.data(), this));
    }

    // 调用 TaskListWidget 的导出方法
    m_taskListWidget->exportConfiguration();
}

void Widget::onImportTaskClicked()
{
    // 确保 TaskListWidget 已创建（但不显示窗口）
    if (!m_taskListWidget)
    {
        m_taskListWidget.reset(new TaskListWidget(m_deviceController.data(), this));
    }

    // 调用 TaskListWidget 的导入方法
    m_taskListWidget->importConfiguration();
}

double Widget::getV1Voltage() const
{
    // 优先检查lineEdit_voltage_v1手动输入
    QString voltageText = ui->lineEdit_voltage_V1->text().trimmed();
    if (!voltageText.isEmpty())
    {
        bool conversionOk = false;
        double voltage = voltageText.toDouble(&conversionOk);
        if (conversionOk)
        {
            return voltage;
        }
    }

    // 如果手动输入为空，则从comboBox_voltage_V1下拉框获取
    int currentIndex = ui->comboBox_voltage_V1->currentIndex();
    if (currentIndex >= 0)
    {
        QString comboText = ui->comboBox_voltage_V1->currentText();
        // 提取电压数值，例如 "1.80V" -> 1.80
        QString numericPart = comboText.left(comboText.length() - 1); // 移除'V'
        bool conversionOk = false;
        double voltage = numericPart.toDouble(&conversionOk);
        if (conversionOk)
        {
            return voltage;
        }
    }

    return -1.0; // 未选择或无效
}

double Widget::getV2Voltage() const
{
    // 优先检查lineEdit_voltage手动输入
    QString voltageText = ui->lineEdit_voltage_V2->text().trimmed();
    if (!voltageText.isEmpty())
    {
        bool conversionOk = false;
        double voltage = voltageText.toDouble(&conversionOk);
        if (conversionOk)
        {
            return voltage;
        }
    }

    // 如果手动输入为空，则从comboBox_voltage_V2下拉框获取
    int currentIndex = ui->comboBox_voltage_V2->currentIndex();
    if (currentIndex >= 0)
    {
        QString comboText = ui->comboBox_voltage_V2->currentText();
        // 提取电压数值，例如 "2.90V" -> 2.90
        QString numericPart = comboText.left(comboText.length() - 1); // 移除'V'
        bool conversionOk = false;
        double voltage = numericPart.toDouble(&conversionOk);
        if (conversionOk)
        {
            return voltage;
        }
    }

    return -1.0; // 未选择或无效
}

uint8_t Widget::getSelectedChannelId() const
{
    if (ui->radioButton_V1->isChecked())
    {
        return 0x01; // V1通道
    }
    else if (ui->radioButton_V2->isChecked())
    {
        return 0x02; // V2通道
    }
    else if (ui->radioButton_V3->isChecked())
    {
        return 0x03; // V3通道
    }
    return 0x00; // 未选择
}

uint8_t Widget::getV1VoltageBcd() const
{
    double v1Voltage = getV1Voltage();
    if (v1Voltage < 0)
    {
        return 0x00; // 未选择
    }
    // 使用协议层的BCD编码函数
    return DeviceProtocol::encodeVoltage(v1Voltage);
}

void Widget::onLineEditV2FocusIn()
{
    // 将comboBox_voltage_V2重置为无选择状态
    ui->comboBox_voltage_V2->setCurrentIndex(-1);

    // 恢复初始的placeholder文本（使用初始化时设置的文本）
    ui->lineEdit_voltage_V2->setPlaceholderText(tr("自定义V2电压："));
}

void Widget::onDetectionClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：请先打开串口连接"));
        return;
    }

    // 禁用检测按钮防重复点击
    ui->pushButton_detection->setEnabled(false);
    ui->pushButton_detection->setText(tr("执行中..."));

    // 发送开始检测命令（启动外部电流表），触发新一轮测量
    bool success = m_deviceController->startExternalMeterDetection();

    // 如果发送失败，立即恢复按钮状态
    if (!success)
    {
        appendTextWithAutoScroll(tr("电流检测命令发送失败"));
    }
    // 成功发送后，等待应答处理：
    // - 成功应答会在 onDeviceCommandConfirmed() 中处理：恢复按钮状态，提示成功
    // - 失败/超时会在 onDeviceCommandConfirmed() 中处理：恢复按钮状态，提示失败信息
}

void Widget::onDetectionPauseClicked()
{
    // 检查设备是否已连接
    if (!m_deviceController->isConnected())
    {
        appendTextWithAutoScroll(tr("错误：设备未连接"));
        return;
    }

    // 禁用按钮防止重复点击
    ui->pushButton_detection_pause->setEnabled(false);
    ui->pushButton_detection_pause->setText(tr("停止中..."));

    // 发送停止外部电流表连续检测命令
    bool success = m_deviceController->stopExternalMeterDetection();

    if (!success)
    {
        appendTextWithAutoScroll(tr("停止检测命令发送失败"));
        // 恢复按钮状态
        ui->pushButton_detection_pause->setEnabled(true);
        ui->pushButton_detection_pause->setText(tr("停止检测"));
    }
    // 成功发送后，等待应答处理：
    // - 成功/失败应答会在 onDeviceCommandConfirmed() 中处理
}

void Widget::setControlsEnabled(bool enabled)
{
    // 串口控件
    ui->comboBox_serialList->setEnabled(enabled);
    ui->pushButton_openSerial->setEnabled(enabled);

    // 继电器按键控件
    ui->pushButton_power_confirm->setEnabled(enabled);
    ui->pushButton_right->setEnabled(enabled);

    // 电压设定控件
    ui->radioButton_V1->setEnabled(enabled); // V1通道
    ui->radioButton_V2->setEnabled(enabled); // V2通道
    ui->radioButton_V3->setEnabled(enabled); // V3通道

    ui->lineEdit_voltage_V2->setEnabled(enabled);
    ui->pushButton_output_V2->setEnabled(enabled);

}

// 处理清空日志按钮点击
void Widget::onClearLogClicked()
{
    // 清空日志显示框中的所有内容
    ui->textEdit_receive->clear();
}
