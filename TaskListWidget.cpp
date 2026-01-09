#include "TaskListWidget.h"
#include "DeviceController.h"
#include "widget.h"
#include "app/TestStepFactory.h"
#include "ErrorRecordDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QTableWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QScrollBar>
#include <QFileDialog>
#include <QJsonDocument>
#include <QDir>
#include <QInputDialog>
#include <QCloseEvent>
#include <QApplication>

TaskListWidget::TaskListWidget(DeviceController *controller, Widget *mainWidget, QWidget *parent)
    : QWidget(parent)
    , m_deviceController(controller)
    , m_runner(nullptr)
    , m_mainWidget(mainWidget)
    , m_stepTable(nullptr)
    , m_logEdit(nullptr)
    , m_startButton(nullptr)
    , m_pauseButton(nullptr)
    , m_stopButton(nullptr)
    , m_closeButton(nullptr)
    , m_engineerButton(nullptr)
    , m_errorRecordButton(nullptr)
    , m_statusLabel(nullptr)
    , m_isPaused(false)
{
    initUI();
    
    // 创建测试序列执行引擎
    m_runner = new TestSequenceRunner(m_deviceController, this);
    
    // 加载测试步骤
    // 把工厂中创建好的测试步骤加载到执行引擎中👌
    m_runner->loadSteps(TestStepFactory::createPcbaTestSequence());
    
    // 初始化信号槽连接
    initConnections();
    
    // 加载步骤到表格
    loadStepsToTable();
    
    // 更新按钮状态
    updateButtonStates();
}

TaskListWidget::~TaskListWidget()
{
}

void TaskListWidget::closeEvent(QCloseEvent *event)
{
    // 接受关闭事件
    event->accept();
    
    // 退出应用程序
    QApplication::quit();
}

void TaskListWidget::initUI()
{
    // 设置窗口属性
    setWindowTitle(tr("自动化测试控制台"));
    setMinimumSize(800, 600);
    resize(900, 700);

    // 设置窗口标志：独立窗口
    setWindowFlags(Qt::Window);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ========== 标题区域 ==========
    QLabel *titleLabel = new QLabel(tr("PCBA 自动化测试"), this);
    titleLabel->setStyleSheet("font: bold 18pt; color: #2c3e50;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // ========== 状态标签 ==========
    m_statusLabel = new QLabel(tr("状态: 就绪"), this);
    m_statusLabel->setStyleSheet("font: 12pt; color: #27ae60; padding: 5px;");
    mainLayout->addWidget(m_statusLabel);

    // ========== 内容区域（分割器） ==========
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);

    // ----- 步骤表格 -----
    m_stepTable = new QTableWidget(this);
    m_stepTable->setColumnCount(4);
    m_stepTable->setHorizontalHeaderLabels({tr("步骤"), tr("名称"), tr("描述"), tr("状态")});
    m_stepTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_stepTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_stepTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_stepTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_stepTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_stepTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stepTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_stepTable->setAlternatingRowColors(true);
    m_stepTable->setStyleSheet(
        "QTableWidget { font-size: 11pt; }"
        "QTableWidget::item:selected { background-color: #3498db; color: white; }"
    );
    splitter->addWidget(m_stepTable);

    // ----- 日志区域 -----
    QWidget *logContainer = new QWidget(this);
    QVBoxLayout *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *logLabel = new QLabel(tr("执行日志:"), this);
    logLabel->setStyleSheet("font: bold 11pt; color: #34495e;");
    logLayout->addWidget(logLabel);
    
    m_logEdit = new QTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setStyleSheet(
        "QTextEdit { font-family: 'Consolas', 'Microsoft YaHei'; font-size: 10pt; "
        "background-color: #2c3e50; color: #ecf0f1; }"
    );
    logLayout->addWidget(m_logEdit);
    splitter->addWidget(logContainer);

    // 设置分割比例
    splitter->setSizes({400, 200});
    mainLayout->addWidget(splitter, 1);

    // ========== 按钮区域 ==========
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(15);

    m_startButton = new QPushButton(tr("▶ 开始测试"), this);
    m_startButton->setStyleSheet(
        "QPushButton { font: bold 12pt; padding: 10px 25px; background-color: #27ae60; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #2ecc71; }"
        "QPushButton:disabled { background-color: #95a5a6; }"
    );
    buttonLayout->addWidget(m_startButton);

    m_pauseButton = new QPushButton(tr("⏸ 暂停"), this);
    m_pauseButton->setStyleSheet(
        "QPushButton { font: bold 12pt; padding: 10px 25px; background-color: #f39c12; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #f1c40f; }"
        "QPushButton:disabled { background-color: #95a5a6; }"
    );
    buttonLayout->addWidget(m_pauseButton);

    m_stopButton = new QPushButton(tr("⏹ 停止"), this);
    m_stopButton->setStyleSheet(
        "QPushButton { font: bold 12pt; padding: 10px 25px; background-color: #e74c3c; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #c0392b; }"
        "QPushButton:disabled { background-color: #95a5a6; }"
    );
    buttonLayout->addWidget(m_stopButton);

    buttonLayout->addStretch();

    // 错误记录按钮
    m_errorRecordButton = new QPushButton(tr("📋 错误记录"), this);
    m_errorRecordButton->setStyleSheet(
        "QPushButton { font: 12pt; padding: 10px 25px; background-color: #9b59b6; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #8e44ad; }"
    );
    buttonLayout->addWidget(m_errorRecordButton);

    m_engineerButton = new QPushButton(tr("🔧 工程界面"), this);
    m_engineerButton->setStyleSheet(
        "QPushButton { font: 12pt; padding: 10px 25px; background-color: #e67e22; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #d35400; }"
    );
    buttonLayout->addWidget(m_engineerButton);

    m_closeButton = new QPushButton(tr("关闭"), this);
    m_closeButton->setStyleSheet(
        "QPushButton { font: 12pt; padding: 10px 25px; background-color: #7f8c8d; color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #95a5a6; }"
    );
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void TaskListWidget::initConnections()
{
    // 按钮信号
    connect(m_startButton, &QPushButton::clicked, this, &TaskListWidget::onStartClicked);
    connect(m_pauseButton, &QPushButton::clicked, this, &TaskListWidget::onPauseClicked);
    connect(m_stopButton, &QPushButton::clicked, this, &TaskListWidget::onStopClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);
    connect(m_engineerButton, &QPushButton::clicked, this, &TaskListWidget::onEngineerModeClicked);
    connect(m_errorRecordButton, &QPushButton::clicked, this, &TaskListWidget::onErrorRecordClicked);

    // TestSequenceRunner 信号
    connect(m_runner, &TestSequenceRunner::stateChanged, 
            this, &TaskListWidget::onRunnerStateChanged);
    connect(m_runner, &TestSequenceRunner::stepStarted, 
            this, &TaskListWidget::onStepStarted);
    connect(m_runner, &TestSequenceRunner::stepFinished, 
            this, &TaskListWidget::onStepFinished);
    connect(m_runner, &TestSequenceRunner::actionStarted, 
            this, &TaskListWidget::onActionStarted);
    connect(m_runner, &TestSequenceRunner::userConfirmRequired, 
            this, &TaskListWidget::onUserConfirmRequired);
    connect(m_runner, &TestSequenceRunner::logMessage, 
            this, &TaskListWidget::onRunnerLogMessage);
    connect(m_runner, &TestSequenceRunner::sequenceFinished, 
            this, &TaskListWidget::onSequenceFinished);
    connect(m_runner, &TestSequenceRunner::currentCheckResult, 
            this, &TaskListWidget::onCurrentCheckResult);
}

void TaskListWidget::loadStepsToTable()
{
    const QVector<StepSpec> &steps = m_runner->steps();
    m_stepTable->setRowCount(steps.size());

    for (int i = 0; i < steps.size(); ++i) {
        const StepSpec &step = steps[i];

        // 步骤编号
        QTableWidgetItem *idItem = new QTableWidgetItem(QString::number(step.id));
        idItem->setTextAlignment(Qt::AlignCenter);
        m_stepTable->setItem(i, 0, idItem);

        // 步骤名称
        QTableWidgetItem *nameItem = new QTableWidgetItem(step.name);
        m_stepTable->setItem(i, 1, nameItem);

        // 步骤描述
        QTableWidgetItem *descItem = new QTableWidgetItem(step.description);
        m_stepTable->setItem(i, 2, descItem);

        // 状态（初始为"待执行"）
        QTableWidgetItem *statusItem = new QTableWidgetItem(tr("待执行"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        statusItem->setForeground(QBrush(QColor("#7f8c8d")));
        m_stepTable->setItem(i, 3, statusItem);
    }
}

void TaskListWidget::updateButtonStates()
{
    TestSequenceRunner::State state = m_runner->state();

    switch (state) {
    case TestSequenceRunner::State::Idle:
    case TestSequenceRunner::State::Finished:
    case TestSequenceRunner::State::Aborted:
        m_startButton->setEnabled(true);
        m_pauseButton->setEnabled(false);
        m_stopButton->setEnabled(false);
        m_pauseButton->setText(tr("⏸ 暂停"));
        m_isPaused = false;
        break;

    case TestSequenceRunner::State::Running:
    case TestSequenceRunner::State::WaitingForMeasurement:
    case TestSequenceRunner::State::WaitingForAck:
    case TestSequenceRunner::State::WaitingForPauseAck:
        m_startButton->setEnabled(false);
        m_pauseButton->setEnabled(true);
        m_stopButton->setEnabled(true);
        m_pauseButton->setText(tr("⏸ 暂停"));
        m_isPaused = false;
        break;

    case TestSequenceRunner::State::Paused:
        m_startButton->setEnabled(false);
        m_pauseButton->setEnabled(true);
        m_stopButton->setEnabled(true);
        m_pauseButton->setText(tr("▶ 继续"));
        m_isPaused = true;
        break;

    case TestSequenceRunner::State::WaitingForUser:
        m_startButton->setEnabled(false);
        m_pauseButton->setEnabled(false);
        m_stopButton->setEnabled(true);
        break;
    }
}

void TaskListWidget::appendLog(const QString &message, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString color = isError ? "#e74c3c" : "#ecf0f1";
    QString formattedMsg = QString("<span style='color: #7f8c8d;'>[%1]</span> "
                                   "<span style='color: %2;'>%3</span>")
                           .arg(timestamp, color, message.toHtmlEscaped());
    m_logEdit->append(formattedMsg);
    
    // 自动滚动到底部
    QScrollBar *scrollBar = m_logEdit->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void TaskListWidget::setRowStatus(int row, const QString &status, bool isSuccess)
{
    if (row < 0 || row >= m_stepTable->rowCount()) {
        return;
    }

    QTableWidgetItem *statusItem = m_stepTable->item(row, 3);
    if (statusItem) {
        statusItem->setText(status);
        if (status == tr("执行中")) {
            statusItem->setForeground(QBrush(QColor("#3498db")));
        } else if (isSuccess) {
            statusItem->setForeground(QBrush(QColor("#27ae60")));
        } else {
            statusItem->setForeground(QBrush(QColor("#e74c3c")));
        }
    }
}

void TaskListWidget::highlightRow(int row)
{
    clearRowHighlights();
    
    if (row < 0 || row >= m_stepTable->rowCount()) {
        return;
    }

    for (int col = 0; col < m_stepTable->columnCount(); ++col) {
        QTableWidgetItem *item = m_stepTable->item(row, col);
        if (item) {
            item->setBackground(QBrush(QColor("#d5f4e6")));
        }
    }
    
    m_stepTable->selectRow(row);
}

void TaskListWidget::clearRowHighlights()
{
    for (int row = 0; row < m_stepTable->rowCount(); ++row) {
        for (int col = 0; col < m_stepTable->columnCount(); ++col) {
            QTableWidgetItem *item = m_stepTable->item(row, col);
            if (item) {
                item->setBackground(QBrush()); // 恢复默认背景
            }
        }
    }
}

// ========== 按钮槽函数 ==========

void TaskListWidget::onStartClicked()
{
    // 检查串口是否已连接
    if (!m_deviceController || !m_deviceController->isConnected())
    {
        QMessageBox::warning(this, tr("无法启动测试"),
                             tr("串口未连接！\n\n请先进入工程界面连接串口后再执行自动检测。"));
        return;
    }

    // 重置表格状态
    for (int i = 0; i < m_stepTable->rowCount(); ++i) {

        // 设置指定行的状态文本及背景色
        setRowStatus(i, tr("待执行"), true);
    }

    // 将每个单元格的背景恢复为默认状态，从而清除所有行的高亮显示。
    clearRowHighlights();
    
    // 清空日志
    m_logEdit->clear();
    
    appendLog(tr("========== 开始自动化测试 =========="));
    m_runner->start();
}

void TaskListWidget::onPauseClicked()
{
    if (m_isPaused) {
        appendLog(tr("恢复测试..."));
        m_runner->resume();
    } else {
        appendLog(tr("暂停测试..."));
        m_runner->pause();
    }
}

void TaskListWidget::onStopClicked()
{
    int ret = QMessageBox::question(this, tr("确认停止"),
                                    tr("确定要停止当前测试吗？"),
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        appendLog(tr("用户停止测试"), true);
        m_runner->stop();
    }
}

void TaskListWidget::onEngineerModeClicked()
{
    // 弹出密码输入框
    bool ok = false;
    QString password = QInputDialog::getText(this, 
                                             tr("权限验证"), 
                                             tr("请输入工程密码:"),
                                             QLineEdit::Password, 
                                             QString(), 
                                             &ok);
    
    if (ok) {
        // 验证密码
        if (password == "root") {
            // 密码正确，隐藏自己，显示主界面
            if (m_mainWidget) {
                this->hide();
                m_mainWidget->show();
            } else {
                QMessageBox::warning(this, tr("错误"), tr("主界面未初始化"));
            }
        } else {
            // 密码错误
            QMessageBox::warning(this, tr("验证失败"), tr("密码错误，无法进入工程界面"));
        }
    }
}

void TaskListWidget::onErrorRecordClicked()
{
    // 获取错误记录并显示对话框
    const QVector<ErrorRecord> &records = m_runner->getErrorRecords();
    ErrorRecordDialog dialog(records, this);
    dialog.exec();
}

// ========== TestSequenceRunner 信号槽 ==========

void TaskListWidget::onRunnerStateChanged(TestSequenceRunner::State newState)
{
    QString stateStr;
    QString color;
    
    switch (newState) {
    case TestSequenceRunner::State::Idle:
        stateStr = tr("就绪");
        color = "#27ae60";
        break;
    case TestSequenceRunner::State::Running:
        stateStr = tr("运行中");
        color = "#3498db";
        break;
    case TestSequenceRunner::State::Paused:
        stateStr = tr("已暂停");
        color = "#f39c12";
        break;
    case TestSequenceRunner::State::WaitingForUser:
        stateStr = tr("等待用户确认");
        color = "#9b59b6";
        break;
    case TestSequenceRunner::State::WaitingForMeasurement:
        stateStr = tr("等待测量数据");
        color = "#3498db";
        break;
    case TestSequenceRunner::State::WaitingForAck:
        stateStr = tr("等待指令确认");
        color = "#3498db";
        break;
    case TestSequenceRunner::State::WaitingForPauseAck:
        stateStr = tr("等待暂停确认");
        color = "#f39c12";
        break;
    case TestSequenceRunner::State::Finished:
        stateStr = tr("已完成");
        color = "#27ae60";
        break;
    case TestSequenceRunner::State::Aborted:
        stateStr = tr("已中止");
        color = "#e74c3c";
        break;
    }
    
    m_statusLabel->setText(tr("状态: %1").arg(stateStr));
    m_statusLabel->setStyleSheet(QString("font: 12pt; color: %1; padding: 5px;").arg(color));
    
    updateButtonStates();
}

void TaskListWidget::onStepStarted(int stepIndex, const StepSpec &step)
{
    Q_UNUSED(step)
    highlightRow(stepIndex);
    setRowStatus(stepIndex, tr("执行中"), true);
}

void TaskListWidget::onStepFinished(int stepIndex, bool success, const QString &message)
{
    QString statusText = success ? tr("✔ 通过") : tr("✘ 失败");
    setRowStatus(stepIndex, statusText, success);
    
    if (!success) {
        appendLog(tr("步骤 %1 失败: %2").arg(stepIndex + 1).arg(message), true);
    }
}

void TaskListWidget::onActionStarted(int stepIndex, int actionIndex, const SubAction &action)
{
    Q_UNUSED(stepIndex)
    Q_UNUSED(actionIndex)
    Q_UNUSED(action)
    // 可以在此添加更细粒度的UI更新
}

void TaskListWidget::onUserConfirmRequired(const QString &message)
{
    // 弹出确认对话框
    int ret = QMessageBox::question(this, tr("请确认"),
                                    message,
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::Yes);
    
    bool confirmed = (ret == QMessageBox::Yes);
    appendLog(tr("用户确认: %1").arg(confirmed ? tr("是") : tr("否")));
    m_runner->userConfirm(confirmed);
}

void TaskListWidget::onRunnerLogMessage(const QString &message)
{
    appendLog(message);
}

void TaskListWidget::onSequenceFinished(bool allPassed, int passedCount, int totalCount)
{
    clearRowHighlights();
    
    QString resultMsg;
    QMessageBox::Icon icon;
    
    if (allPassed) {
        resultMsg = tr("所有测试步骤全部通过！\n\n通过: %1/%2")
                    .arg(passedCount).arg(totalCount);
        icon = QMessageBox::Information;
        appendLog(tr("========== 测试完成: 全部通过 =========="));
    } else {
        resultMsg = tr("测试未完全通过。\n\n通过: %1/%2")
                    .arg(passedCount).arg(totalCount);
        icon = QMessageBox::Warning;
        appendLog(tr("========== 测试完成: 部分失败 =========="), true);
    }
    
    QMessageBox msgBox(icon, tr("测试结果"), resultMsg, QMessageBox::Ok, this);
    msgBox.exec();
    
    emit testFinished(allPassed, passedCount, totalCount);
}

void TaskListWidget::onCurrentCheckResult(int stepIndex, double value, double threshold, bool passed)
{
    QString resultStr = passed ? tr("PASS") : tr("FAIL");
    appendLog(tr("步骤 %1 电流检测: 测量值=%2, 阈值≤%3, 结果=%4")
              .arg(stepIndex + 1)
              .arg(value, 0, 'f', 3)
              .arg(threshold, 0, 'f', 3)
              .arg(resultStr),
              !passed);
}

// ========== 导入/导出配置 ==========

void TaskListWidget::exportConfiguration()
{
    // 检查是否正在运行
    if (m_runner->isRunning()) {
        QMessageBox::warning(this, tr("警告"), tr("测试正在运行中，请先停止测试再导出配置。"));
        return;
    }

    // 获取当前步骤列表
    const QVector<StepSpec> &steps = m_runner->steps();
    if (steps.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("当前没有可导出的测试步骤。"));
        return;
    }

    // 打开文件保存对话框
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("导出测试配置"),
        QDir::homePath() + "/pcba_test_config.json",
        tr("JSON 文件 (*.json)")
    );

    if (fileName.isEmpty()) {
        return;  // 用户取消
    }

    // 确保文件扩展名为 .json
    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
    }

    // 序列化为 JSON
    QJsonDocument doc = StepSpec::stepsToJson(steps);

    // 写入文件
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), 
            tr("无法打开文件进行写入:\n%1").arg(file.errorString()));
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    appendLog(tr("配置已导出到: %1").arg(fileName));
    QMessageBox::information(this, tr("导出成功"), 
        tr("测试配置已成功导出到:\n%1").arg(fileName));
}

void TaskListWidget::importConfiguration()
{
    // 检查是否正在运行
    if (m_runner->isRunning()) {
        QMessageBox::warning(this, tr("警告"), tr("测试正在运行中，请先停止测试再导入配置。"));
        return;
    }

    // 打开文件选择对话框
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("导入测试配置"),
        QDir::homePath(),
        tr("JSON 文件 (*.json)")
    );

    if (fileName.isEmpty()) {
        return;  // 用户取消
    }

    // 读取文件
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("错误"), 
            tr("无法打开文件进行读取:\n%1").arg(file.errorString()));
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    // 解析 JSON
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, tr("错误"), 
            tr("JSON 解析失败:\n%1").arg(parseError.errorString()));
        return;
    }

    // 反序列化步骤
    QVector<StepSpec> steps = StepSpec::stepsFromJson(doc);

    if (steps.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("导入的配置文件中没有有效的测试步骤。"));
        return;
    }

    // 确认是否覆盖当前配置
    QMessageBox::StandardButton ret = QMessageBox::question(
        this,
        tr("确认导入"),
        tr("将导入 %1 个测试步骤，这将替换当前的测试配置。\n\n是否继续？").arg(steps.size()),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret != QMessageBox::Yes) {
        return;
    }

    // 加载到执行引擎
    m_runner->loadSteps(steps);

    // 刷新表格显示
    loadStepsToTable();

    appendLog(tr("已从 %1 导入 %2 个测试步骤").arg(fileName).arg(steps.size()));
    QMessageBox::information(this, tr("导入成功"), 
        tr("已成功导入 %1 个测试步骤。").arg(steps.size()));
}
