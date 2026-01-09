#ifndef TASKLISTWIDGET_H
#define TASKLISTWIDGET_H

#include <QWidget>
#include "app/TestSequenceRunner.h"
#include "domain/StepSpec.h"

class DeviceController;
class Widget;
class QTableWidget;
class QTextEdit;
class QPushButton;
class QLabel;

/**
 * @brief 任务列表窗口（自动化测试控制台）
 *
 * 职责：
 * - 显示自动测试步骤列表
 * - 提供测试启动/暂停/停止控制
 * - 显示测试日志和执行状态
 * - 处理用户交互确认弹窗
 */
class TaskListWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param controller 设备控制器指针（依赖注入）
     * @param mainWidget 主界面指针（用于跳转）
     * @param parent 父窗口指针
     */
    explicit TaskListWidget(DeviceController *controller, Widget *mainWidget = nullptr, QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TaskListWidget() override;

protected:
    /**
     * @brief 重写关闭事件，关闭时退出应用程序
     * @param event 关闭事件对象
     */
    void closeEvent(QCloseEvent *event) override;

signals:
    /**
     * @brief 测试完成信号
     * @param allPassed 是否全部通过
     * @param passedCount 通过的步骤数
     * @param totalCount 总步骤数
     */
    void testFinished(bool allPassed, int passedCount, int totalCount);

public slots:
    /**
     * @brief 导出当前测试配置到 JSON 文件
     * @details 可由主界面按钮调用，将当前测试步骤序列化为 JSON 并保存到用户选择的文件
     */
    void exportConfiguration();

    /**
     * @brief 从 JSON 文件导入测试配置
     * @details 可由主界面按钮调用，从用户选择的 JSON 文件加载测试步骤
     */
    void importConfiguration();

private slots:
    // 按钮槽函数
    void onStartClicked();
    void onPauseClicked();
    void onStopClicked();
    void onEngineerModeClicked();
    void onErrorRecordClicked();    ///< 查看错误记录按钮槽函数

    // TestSequenceRunner 信号槽
    void onRunnerStateChanged(TestSequenceRunner::State newState);
    void onStepStarted(int stepIndex, const StepSpec &step);
    void onStepFinished(int stepIndex, bool success, const QString &message);
    void onActionStarted(int stepIndex, int actionIndex, const SubAction &action);
    void onUserConfirmRequired(const QString &message);
    void onRunnerLogMessage(const QString &message);
    void onSequenceFinished(bool allPassed, int passedCount, int totalCount);
    void onCurrentCheckResult(int stepIndex, double value, double threshold, bool passed);

private:
    /**
     * @brief 初始化UI组件
     */
    void initUI();

    /**
     * @brief 初始化信号槽连接
     */
    void initConnections();

    /**
     * @brief 加载测试步骤到表格
     */
    void loadStepsToTable();

    /**
     * @brief 更新按钮状态
     */
    void updateButtonStates();

    /**
     * @brief 追加日志消息
     * @param message 日志内容
     * @param isError 是否为错误消息
     */
    void appendLog(const QString &message, bool isError = false);

    /**
     * @brief 设置表格行状态
     * @param row 行索引
     * @param status 状态文本
     * @param isSuccess 是否成功（用于颜色）
     */
    void setRowStatus(int row, const QString &status, bool isSuccess = true);

    /**
     * @brief 高亮当前执行行
     * @param row 行索引
     */
    void highlightRow(int row);

    /**
     * @brief 清除所有行高亮
     */
    void clearRowHighlights();

private:
    // 依赖
    DeviceController *m_deviceController;       ///< 设备控制器指针
    TestSequenceRunner *m_runner;               ///< 测试序列执行引擎
    Widget *m_mainWidget;                       ///< 主界面指针（用于跳转）

    // UI 控件
    QTableWidget *m_stepTable;                  ///< 测试步骤表格
    QTextEdit *m_logEdit;                       ///< 日志显示框
    QPushButton *m_startButton;                 ///< 开始按钮
    QPushButton *m_pauseButton;                 ///< 暂停/继续按钮
    QPushButton *m_stopButton;                  ///< 停止按钮
    QPushButton *m_closeButton;                 ///< 关闭按钮
    QPushButton *m_engineerButton;              ///< 工程界面按钮
    QPushButton *m_errorRecordButton;           ///< 错误记录按钮
    QLabel *m_statusLabel;                      ///< 状态标签

    // 状态
    bool m_isPaused;                            ///< 是否处于暂停状态
};

#endif // TASKLISTWIDGET_H
