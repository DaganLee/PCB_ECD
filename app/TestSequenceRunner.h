#ifndef TESTSEQUENCERUNNER_H
#define TESTSEQUENCERUNNER_H

#include <QObject>
#include <QVector>
#include <QTimer>
#include "domain/StepSpec.h"
#include "domain/Measurement.h"
#include "domain/Command.h"
#include "domain/ErrorRecord.h"

class DeviceController;

/**
 * @brief 测试序列执行引擎
 * 
 * 职责：
 * - 按顺序执行测试步骤列表（StepSpec）
 * - 维护执行状态机（Idle/Running/Paused/WaitingForUser）
 * - 异步调度：发送指令后等待响应，通过信号槽驱动下一步
 * - 自动判定电流测量结果（Pass/Fail）
 * - 通过信号向UI层报告执行进度和结果
 */
class TestSequenceRunner : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 执行状态枚举
     */
    enum class State {
        Idle,               ///< 空闲，未运行
        Running,            ///< 正在执行
        Paused,             ///< 已暂停
        WaitingForUser,     ///< 等待用户交互确认
        WaitingForMeasurement, ///< 等待测量数据
        WaitingForAck,      ///< 等待指令确认（ACK）（指令执行完后的ACK）
        WaitingForPauseAck, ///< 等待暂停指令确认（用户点击暂停按钮后）
        Finished,           ///< 执行完成
        Aborted             ///< 已中止
    };
    Q_ENUM(State)

    /**
     * @brief 子动作执行结果
     */
    enum class ActionResult {
        Success,            ///< 成功
        Failed,             ///< 失败
        Timeout,            ///< 超时
        UserRejected        ///< 用户拒绝确认
    };
    Q_ENUM(ActionResult)

    /**
     * @brief 构造函数
     * @param deviceController 设备控制器指针
     * @param parent 父对象
     */
    explicit TestSequenceRunner(DeviceController *deviceController, QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~TestSequenceRunner() override;

    /**
     * @brief 加载测试步骤序列
     * @param steps 测试步骤列表
     */
    void loadSteps(const QVector<StepSpec> &steps);

    /**
     * @brief 获取当前加载的步骤列表
     */
    const QVector<StepSpec>& steps() const { return m_steps; }

    /**
     * @brief 获取当前执行状态
     */
    State state() const { return m_state; }

    /**
     * @brief 获取当前步骤索引
     */
    int currentStepIndex() const { return m_currentStepIndex; }

    /**
     * @brief 获取当前子动作索引
     */
    int currentActionIndex() const { return m_currentActionIndex; }

    /**
     * @brief 获取错误记录列表
     * @return 错误记录引用
     */
    const QVector<ErrorRecord>& getErrorRecords() const { return m_errorRecords; }

    /**
     * @brief 清空错误记录
     */
    void clearErrorRecords() { m_errorRecords.clear(); }

    /**
     * @brief 检查是否正在运行
     */
    bool isRunning() const { return m_state == State::Running || 
                                   m_state == State::WaitingForUser ||
                                   m_state == State::WaitingForMeasurement ||
                                   m_state == State::WaitingForAck ||
                                   m_state == State::WaitingForPauseAck; }

public slots:
    /**
     * @brief 开始执行测试序列
     */
    void start();

    /**
     * @brief 暂停执行
     */
    void pause();

    /**
     * @brief 恢复执行
     */
    void resume();

    /**
     * @brief 停止并中止执行
     */
    void stop();

    /**
     * @brief 用户确认交互（继续执行）
     * @param confirmed true表示用户确认通过，false表示用户标记失败
     */
    void userConfirm(bool confirmed);

    /**
     * @brief 接收测量数据（由外部连接DeviceController的信号）
     * @param measurement 测量数据
     */
    void onMeasurementReceived(const Measurement &measurement);

    /**
     * @brief 接收命令确认信号（由DeviceController发射）
     * @param command 命令类型
     * @param success 是否确认成功
     * @param sentData 发送的数据
     * @param responseData 接收到的回应数据
     */
    void onCommandConfirmed(Command command, bool success, const QByteArray &sentData, const QByteArray &responseData);

signals:
    /**
     * @brief 执行状态变化
     * @param newState 新状态
     */
    void stateChanged(TestSequenceRunner::State newState);

    /**
     * @brief 步骤开始执行
     * @param stepIndex 步骤索引
     * @param step 步骤规格
     */
    void stepStarted(int stepIndex, const StepSpec &step);

    /**
     * @brief 步骤执行完成
     * @param stepIndex 步骤索引
     * @param success 是否成功
     * @param message 结果消息
     */
    void stepFinished(int stepIndex, bool success, const QString &message);

    /**
     * @brief 子动作开始执行
     * @param stepIndex 步骤索引
     * @param actionIndex 子动作索引
     * @param action 子动作规格
     */
    void actionStarted(int stepIndex, int actionIndex, const SubAction &action);

    /**
     * @brief 子动作执行完成
     * @param stepIndex 步骤索引
     * @param actionIndex 子动作索引
     * @param result 执行结果
     * @param message 结果消息
     */
    void actionFinished(int stepIndex, int actionIndex, ActionResult result, const QString &message);

    /**
     * @brief 需要用户交互确认
     * @param message 确认消息
     */
    void userConfirmRequired(const QString &message);

    /**
     * @brief 日志消息
     * @param message 日志内容
     */
    void logMessage(const QString &message);

    /**
     * @brief 整个测试序列执行完成
     * @param allPassed 是否全部通过
     * @param passedCount 通过的步骤数
     * @param totalCount 总步骤数
     */
    void sequenceFinished(bool allPassed, int passedCount, int totalCount);

    /**
     * @brief 电流测量结果
     * @param stepIndex 步骤索引
     * @param value 测量值
     * @param threshold 阈值
     * @param passed 是否通过
     */
    void currentCheckResult(int stepIndex, double value, double threshold, bool passed);

private slots:
    /**
     * @brief 执行下一个子动作
     */
    void executeNextAction();

    /**
     * @brief 延时完成回调
     */
    void onDelayFinished();

    /**
     * @brief 测量超时回调
     */
    void onMeasurementTimeout();

    /**
     * @brief 步骤超时回调
     */
    void onStepTimeout();

    /**
     * @brief ACK超时回调
     */
    void onAckTimeout();

private:
    /**
     * @brief 设置执行状态
     */
    void setState(State newState);

    /**
     * @brief 执行单个子动作
     * @param action 子动作
     * @return 是否立即完成（false表示需要等待异步响应）
     * 👌
     */
    bool executeAction(const SubAction &action);

    /**
     * @brief 执行设置V1电压动作（0x02 + 通道 + 电压）
     */
    bool executeSetV1Voltage(const SubAction &action);

    /**
     * @brief 执行设置V4电压动作（0x02 + 0x04 + 电压）
     */
    bool executeSetV4Voltage(const SubAction &action);

    /**
     * @brief 执行打开V1通道动作（0x12 + 通道）
     */
    bool executeOpenV1Channel(const SubAction &action);

    /**
     * @brief 执行打开V4通道动作（0x12 + 0x04）
     */
    bool executeOpenV4Channel();

    /**
     * @brief 执行开启检测动作
     */
    bool executeStartDetection();

    /**
     * @brief 执行暂停检测动作
     */
    bool executePauseDetection();

    /**
     * @brief 执行按键模拟动作
     */
    bool executePressKey(const SubAction &action);

    /**
     * @brief 执行延时动作
     */
    bool executeDelay(const SubAction &action);

    /**
     * @brief 执行用户确认动作
     */
    bool executeUserConfirm(const SubAction &action);

    /**
     * @brief 执行电流检测动作
     */
    bool executeCheckCurrent(const SubAction &action);

    /**
     * @brief 执行通道开启动作
     */
    bool executeOpenChannel(const SubAction &action);

    /**
     * @brief 前进到下一个步骤
     */
    void advanceToNextStep();

    /**
     * @brief 完成当前步骤
     */
    void finishCurrentStep(bool success, const QString &message);

    /**
     * @brief 完成整个序列
     */
    void finishSequence();

    /**
     * @brief 记录日志
     */
    void log(const QString &message);

    /**
     * @brief 记录错误
     * @param actionDesc 动作描述
     * @param errorType 错误类型
     * @param errorDetail 错误详情
     * @param measuredValue 测量值（可选，默认-1表示无测量数据）
     * @param thresholdValue 阈值（可选，默认-1表示无测量数据）
     */
    void recordError(const QString &actionDesc, const QString &errorType, 
                     const QString &errorDetail, 
                     double measuredValue = -1.0, double thresholdValue = -1.0);

private:
    // 依赖
    DeviceController *m_deviceController;

    // 测试数据
    QVector<StepSpec> m_steps;              ///< 测试步骤列表
    QVector<bool> m_stepResults;            ///< 每个步骤的执行结果
    QVector<ErrorRecord> m_errorRecords;    ///< 错误记录列表

    // 执行状态
    State m_state;                          ///< 当前状态
    int m_currentStepIndex;                 ///< 当前步骤索引
    int m_currentActionIndex;               ///< 当前子动作索引

    // 定时器
    QTimer *m_delayTimer;                   ///< 延时定时器
    QTimer *m_measurementTimer;             ///< 测量超时定时器
    QTimer *m_stepTimer;                    ///< 步骤超时定时器
    QTimer *m_ackTimer;                     ///< ACK超时定时器（子动作超时定时器）

    // 电流检测相关
    double m_pendingCurrentThreshold;       ///< 待检测的电流阈值
    bool m_pendingIsUpperLimit;             ///< 待检测的阈值类型
    bool m_waitingForMeasurement;           ///< 是否正在等待测量数据
    bool m_isDetectionActive;               ///< 下位机检测是否已激活（用于判断暂停时是否需要发送暂停指令）

    // 暂停恢复相关
    int m_remainingStepTime;                ///< 暂停时保存步骤超时剩余时间
    int m_remainingDelayTime;               ///< 暂停时保存延时剩余时间
    int m_remainingMeasurementTime;         ///< 暂停时保存测量超时剩余时间
    int m_remainingAckTime;                 ///< 暂停时保存ACK超时剩余时间
    State m_prePauseState;                  ///< 暂停前的状态

    // 配置常量
    static constexpr int kDefaultMeasurementTimeoutMs = 5000;   ///< 默认测量超时
    static constexpr int kDefaultStepTimeoutMs = 60000;         ///< 默认步骤超时
    static constexpr int kDefaultAckTimeoutMs = 5000;           ///< 默认ACK超时
    static constexpr int kActionDelayMs = 100;                  ///< 动作间延时（毫秒）
};

#endif // TESTSEQUENCERUNNER_H
