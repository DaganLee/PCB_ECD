#include "TestSequenceRunner.h"
#include "DeviceController.h"
#include <QDebug>

TestSequenceRunner::TestSequenceRunner(DeviceController *deviceController, QObject *parent)
    : QObject(parent), m_deviceController(deviceController), m_state(State::Idle), m_currentStepIndex(-1), m_currentActionIndex(-1), m_delayTimer(new QTimer(this)), m_measurementTimer(new QTimer(this)), m_stepTimer(new QTimer(this)), m_ackTimer(new QTimer(this)), m_pendingCurrentThreshold(0.0), m_pendingIsUpperLimit(true), m_waitingForMeasurement(false), m_isDetectionActive(false), m_remainingStepTime(0), m_remainingDelayTime(0), m_remainingMeasurementTime(0), m_remainingAckTime(0), m_prePauseState(State::Idle)
{
    // 定时器配置为单次触发
    m_delayTimer->setSingleShot(true);
    m_measurementTimer->setSingleShot(true);
    m_stepTimer->setSingleShot(true);
    m_ackTimer->setSingleShot(true);

    // 连接定时器信号

    // 当延时操作完成且当前状态为 Running 时，再调度执行下一个动作。
    connect(m_delayTimer, &QTimer::timeout, this, &TestSequenceRunner::onDelayFinished);

    connect(m_measurementTimer, &QTimer::timeout, this, &TestSequenceRunner::onMeasurementTimeout);
    connect(m_stepTimer, &QTimer::timeout, this, &TestSequenceRunner::onStepTimeout);
    connect(m_ackTimer, &QTimer::timeout, this, &TestSequenceRunner::onAckTimeout);

    // 连接DeviceController的信号
    if (m_deviceController)
    {
        // 连接命令确认信号
        connect(m_deviceController, &DeviceController::commandConfirmed,
                this, &TestSequenceRunner::onCommandConfirmed);
        
        // 连接外部电流表测量数据信号，用于 CheckCurrent 动作
        connect(m_deviceController, &DeviceController::externalMeasurementReceived,
                this, [this](float valueMa) {
            // 外部电流表直接返回 mA 值，无需转换
            if (!m_waitingForMeasurement || m_state != State::WaitingForMeasurement) {
                return;
            }

            m_measurementTimer->stop();
            m_waitingForMeasurement = false;

            double value = valueMa;  // 直接使用 mA 值
            QString unit = "mA";

            bool passed = m_pendingIsUpperLimit ? (value <= m_pendingCurrentThreshold)
                                                : (value >= m_pendingCurrentThreshold);

            QString resultStr = passed ? tr("PASS") : tr("FAIL");
            QString compareOp = m_pendingIsUpperLimit ? "<=" : ">=";

            log(tr("电流测量: %1 %2, 阈值: %3 %4 %5 - %6")
                    .arg(value, 0, 'f', 3)
                    .arg(unit)
                    .arg(compareOp)
                    .arg(m_pendingCurrentThreshold, 0, 'f', 3)
                    .arg(unit)
                    .arg(resultStr));

            emit currentCheckResult(m_currentStepIndex, value, m_pendingCurrentThreshold, passed);

            ActionResult result = passed ? ActionResult::Success : ActionResult::Failed;
            emit actionFinished(m_currentStepIndex, m_currentActionIndex, result,
                                tr("测量值: %1 %2").arg(value, 0, 'f', 3).arg(unit));

            if (passed) {
                setState(State::Running);
                QTimer::singleShot(kActionDelayMs, this, &TestSequenceRunner::executeNextAction);
            } else {
                QString compareOp2 = m_pendingIsUpperLimit ? ">" : "<";
                QString detail = tr("测量值 %1 mA %2 阈值 %3 mA")
                    .arg(value, 0, 'f', 3)
                    .arg(compareOp2)
                    .arg(m_pendingCurrentThreshold, 0, 'f', 3);
                recordError(tr("电流检测"), tr("电流超限"), detail, value, m_pendingCurrentThreshold);
                finishCurrentStep(false, tr("电流检测未通过"));
            }
        });
    }
}

TestSequenceRunner::~TestSequenceRunner()
{
    stop();
}

void TestSequenceRunner::loadSteps(const QVector<StepSpec> &steps)
{
    if (isRunning())
    {
        log(tr("无法在运行时加载步骤"));
        return;
    }
    m_steps = steps;
    m_stepResults.clear();
    m_stepResults.resize(steps.size());
    log(tr("已加载 %1 个测试步骤").arg(steps.size()));
}

void TestSequenceRunner::start()
{
    if (m_steps.isEmpty())
    {
        log(tr("没有可执行的测试步骤"));
        return;
    }

    if (isRunning())
    {
        log(tr("测试已在运行中"));
        return;
    }

    log(tr("========== 开始执行测试序列 =========="));
    m_currentStepIndex = 0;
    m_currentActionIndex = -1;
    m_stepResults.fill(false);
    m_errorRecords.clear();       // 清空错误记录
    m_isDetectionActive = false;  // 重置检测激活标志

    setState(State::Running);

    // 启动第一个步骤
    if (m_currentStepIndex < m_steps.size())
    {
        const StepSpec &step = m_steps[m_currentStepIndex];

        // 触发TaskListWidget::onStepStarted，在文本上显示执行中
        emit stepStarted(m_currentStepIndex, step);
        log(tr("步骤 %1: %2").arg(step.id).arg(step.name));

        // 启动步骤超时定时器
        m_stepTimer->start(step.stepTimeoutMs > 0 ? step.stepTimeoutMs : kDefaultStepTimeoutMs);

        // 开始执行第一个子动作
        QTimer::singleShot(0, this, &TestSequenceRunner::executeNextAction);
    }
}

void TestSequenceRunner::pause()
{
    // 允许在运行、等待测量或等待ACK时暂停
    if (m_state != State::Running && m_state != State::WaitingForMeasurement && m_state != State::WaitingForAck)
    {
        return;
    }

    log(tr("请求暂停测试..."));

    // 1. 保存当前状态（用于恢复）
    m_prePauseState = m_state;

    // 2. 保存各定时器剩余时间并停止定时器
    if (m_stepTimer->isActive())
    {
        m_remainingStepTime = m_stepTimer->remainingTime();
        m_stepTimer->stop();
    }
    else
    {
        m_remainingStepTime = 0;
    }

    if (m_delayTimer->isActive())
    {
        m_remainingDelayTime = m_delayTimer->remainingTime();
        m_delayTimer->stop();
    }
    else
    {
        m_remainingDelayTime = 0;
    }

    if (m_measurementTimer->isActive())
    {
        m_remainingMeasurementTime = m_measurementTimer->remainingTime();
        m_measurementTimer->stop();
    }
    else
    {
        m_remainingMeasurementTime = 0;
    }

    if (m_ackTimer->isActive())
    {
        m_remainingAckTime = m_ackTimer->remainingTime();
        m_ackTimer->stop();
    }
    else
    {
        m_remainingAckTime = 0;
    }

    // 3. 根据检测激活状态决定是否发送停止检测指令
    //    只要检测处于激活状态（无论当前是 WaitingForMeasurement 还是 WaitingForAck），都必须发送停止指令
    if (m_isDetectionActive)
    {
        // 清理测量等待标志
        m_waitingForMeasurement = false;

        // 发送停止检测指令并等待ACK（优雅暂停）
        log(tr("检测处于激活状态，发送停止检测指令，等待下位机确认..."));
        if (m_deviceController)
        {
            bool success = m_deviceController->stopExternalMeterDetection();
            if (success)
            {
                // 进入等待暂停ACK状态
                setState(State::WaitingForPauseAck);
                m_ackTimer->start(kDefaultAckTimeoutMs);
            }
            else
            {
                // 发送失败，直接进入暂停状态
                log(tr("停止检测指令发送失败，直接进入暂停状态"));
                setState(State::Paused);
                log(tr("测试已暂停"));
            }
        }
        else
        {
            // 无设备控制器，直接进入暂停状态
            setState(State::Paused);
            log(tr("测试已暂停"));
        }
    }
    else
    {
        // 检测未激活，无需发送暂停指令，直接进入暂停状态
        log(tr("当前检测未激活，直接暂停"));
        setState(State::Paused);
        log(tr("测试已暂停"));
    }
}

void TestSequenceRunner::resume()
{
    if (m_state != State::Paused)
    {
        return;
    }

    log(tr("测试恢复中..."));

    // 1. 如果暂停前检测是激活状态，需要重新开启检测
    //    因为暂停时发送了停止检测指令，下位机已停止采样
    if (m_isDetectionActive && m_deviceController)
    {
        log(tr("恢复前重新开启外部电流表检测"));
        m_deviceController->startExternalMeterDetection();
        // 如果暂停前是在等待测量数据，恢复标志位
        if (m_prePauseState == State::WaitingForMeasurement) {
            m_waitingForMeasurement = true;
        }
    }

    // 2. 恢复之前的状态
    setState(m_prePauseState);
    log(tr("测试已恢复"));

    bool specificTimerResumed = false;

    // 3. 恢复步骤超时定时器
    if (m_remainingStepTime > 0)
    {
        m_stepTimer->start(m_remainingStepTime);
        m_remainingStepTime = 0;
    }

    // 4. 恢复延时定时器
    if (m_remainingDelayTime > 0)
    {
        m_delayTimer->start(m_remainingDelayTime);
        m_remainingDelayTime = 0;
        specificTimerResumed = true;
    }

    // 5. 恢复测量超时定时器
    if (m_remainingMeasurementTime > 0)
    {
        m_measurementTimer->start(m_remainingMeasurementTime);
        m_remainingMeasurementTime = 0;
        specificTimerResumed = true;
    }

    // 6. 恢复ACK超时定时器
    if (m_remainingAckTime > 0)
    {
        m_ackTimer->start(m_remainingAckTime);
        m_remainingAckTime = 0;
        specificTimerResumed = true;
    }

    // 如果没有恢复特定的子动作定时器（说明暂停发生在动作间隙），且之前是运行状态，则继续执行
    if (!specificTimerResumed && m_prePauseState == State::Running)
    {
        QTimer::singleShot(0, this, &TestSequenceRunner::executeNextAction);
    }
}

void TestSequenceRunner::stop()
{
    if (m_state == State::Idle || m_state == State::Finished || m_state == State::Aborted)
    {
        return;
    }

    log(tr("测试中止中..."));

    // 停止所有定时器
    m_delayTimer->stop();
    m_measurementTimer->stop();
    m_stepTimer->stop();
    m_ackTimer->stop();

    // 如果检测处于激活状态，发送停止检测指令到硬件
    if (m_isDetectionActive && m_deviceController)
    {
        log(tr("检测处于激活状态，发送停止检测指令到硬件..."));
        m_deviceController->stopExternalMeterDetection();
    }

    m_waitingForMeasurement = false;
    m_isDetectionActive = false;  // 重置检测激活标志
    setState(State::Aborted);
    log(tr("测试已中止"));
}

void TestSequenceRunner::userConfirm(bool confirmed)
{
    if (m_state != State::WaitingForUser)
    {
        return;
    }

    if (confirmed)
    {
        log(tr("用户确认通过"));
        setState(State::Running);
        // 继续执行下一个动作
        QTimer::singleShot(kActionDelayMs, this, &TestSequenceRunner::executeNextAction);
    }
    else
    {
        log(tr("用户标记失败"));
        // 获取当前用户确认动作的描述
        QString confirmMsg;
        if (m_currentStepIndex >= 0 && m_currentStepIndex < m_steps.size() &&
            m_currentActionIndex >= 0 && m_currentActionIndex < m_steps[m_currentStepIndex].actions.size()) {
            confirmMsg = m_steps[m_currentStepIndex].actions[m_currentActionIndex].confirmMessage;
        }
        recordError(tr("用户确认: %1").arg(confirmMsg), tr("用户取消"), tr("用户在确认弹窗中点击了否"));
        emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::UserRejected, tr("用户标记失败"));
        finishCurrentStep(false, tr("用户标记测试失败"));
    }
}

void TestSequenceRunner::onMeasurementReceived(const Measurement &measurement)
{
    // 此方法已废弃，电流测量逻辑已移至 lambda 中直接处理
    Q_UNUSED(measurement)
}

void TestSequenceRunner::executeNextAction()
{
    if (m_state != State::Running)
    {
        return;
    }

    // 前进到下一个子动作
    m_currentActionIndex++;

    // 检查是否还有子动作
    if (m_currentStepIndex >= m_steps.size())
    {
        finishSequence();
        return;
    }

    const StepSpec &currentStep = m_steps[m_currentStepIndex];

    if (m_currentActionIndex >= currentStep.actions.size())
    {
        // 当前步骤的所有子动作已完成
        finishCurrentStep(true, tr("步骤完成"));
        return;
    }

    // 执行当前子动作
    const SubAction &action = currentStep.actions[m_currentActionIndex];

    // 触发TaskListWidget::onActionStarted槽函数
    emit actionStarted(m_currentStepIndex, m_currentActionIndex, action);

    bool immediateComplete = executeAction(action);

    if (immediateComplete)
    {
        // 动作立即完成，继续下一个
        emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::Success, QString());
        QTimer::singleShot(kActionDelayMs, this, &TestSequenceRunner::executeNextAction);
    }
    // 否则等待异步回调（延时、用户确认、测量数据等）
}

void TestSequenceRunner::onDelayFinished()
{
    if (m_state != State::Running)
    {
        return;
    }

    log(tr("延时完成"));
    emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::Success, tr("延时完成"));

    // singleShot静态函数会在指定的时间间隔后调用一个槽函数，0表示当队列中的事件处理完后立即调用后续函数
    QTimer::singleShot(0, this, &TestSequenceRunner::executeNextAction);
}

void TestSequenceRunner::onMeasurementTimeout()
{
    if (!m_waitingForMeasurement)
    {
        return;
    }

    m_waitingForMeasurement = false;
    log(tr("测量超时"));
    recordError(tr("电流检测"), tr("测量超时"), tr("等待电流测量数据超时"));
    emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::Timeout, tr("测量数据超时"));
    finishCurrentStep(false, tr("电流测量超时"));
}

void TestSequenceRunner::onStepTimeout()
{
    if (m_state == State::Idle || m_state == State::Finished || m_state == State::Aborted)
    {
        return;
    }

    log(tr("步骤超时"));
    recordError(tr("步骤执行"), tr("超时"), tr("步骤执行超时"));
    finishCurrentStep(false, tr("步骤执行超时"));
}

void TestSequenceRunner::onAckTimeout()
{
    // 处理暂停ACK超时
    if (m_state == State::WaitingForPauseAck)
    {
        log(tr("暂停指令确认超时，强制进入暂停状态"));
        setState(State::Paused);
        log(tr("测试已暂停"));
        return;
    }

    // 处理普通指令ACK超时
    if (m_state != State::WaitingForAck)
    {
        return;
    }

    log(tr("指令确认超时（ACK超时）"));
    recordError(tr("指令确认"), tr("超时"), tr("等待下位机ACK响应超时"));
    emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::Timeout, tr("指令确认超时"));
    finishCurrentStep(false, tr("指令确认超时"));
}

void TestSequenceRunner::onCommandConfirmed(Command command, bool success, const QByteArray &sentData, const QByteArray &responseData)
{
    Q_UNUSED(sentData)
    Q_UNUSED(responseData)

    // 处理停止检测指令的ACK确认（用户点击暂停按钮触发）
    if (m_state == State::WaitingForPauseAck)
    {
        m_ackTimer->stop();
        if (success && command == Command::StopExternalMeter)
        {
            log(tr("停止检测指令已确认，进入暂停状态"));
            // 注意：这里不设置 m_isDetectionActive = false
            // 因为 resume() 时需要根据这个标志来决定是否重新开启检测
            // m_isDetectionActive 保持 true，表示"暂停前检测是激活的"
        }
        else
        {
            log(tr("停止检测指令确认失败，但仍进入暂停状态"));
        }
        setState(State::Paused);
        log(tr("测试已暂停"));
        return;
    }

    // 只在等待ACK状态下处理
    if (m_state != State::WaitingForAck)
    {
        return;
    }

    m_ackTimer->stop();

    if (success)
    {
        log(tr("指令确认成功"));
        emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::Success, tr("指令已确认"));
        setState(State::Running);
        QTimer::singleShot(kActionDelayMs, this, &TestSequenceRunner::executeNextAction);
    }
    else
    {
        // ACK失败时，如果是StartDetection，需要回滚m_isDetectionActive
        if (command == Command::StartDetection)
        {
            m_isDetectionActive = false;
            log(tr("开启检测ACK失败，检测状态回滚为：停止"));
        }
        log(tr("指令确认失败"));
        recordError(tr("指令确认"), tr("确认失败"), tr("下位机返回NACK或响应异常"));
        emit actionFinished(m_currentStepIndex, m_currentActionIndex, ActionResult::Failed, tr("指令确认失败"));
        finishCurrentStep(false, tr("指令确认失败"));
    }
}

void TestSequenceRunner::setState(State newState)
{
    if (m_state != newState)
    {
        m_state = newState;
        emit stateChanged(newState);
    }
}

bool TestSequenceRunner::executeAction(const SubAction &action)
{
    switch (action.type)
    {
    case SubAction::SetV1Voltage:
        return executeSetV1Voltage(action);
    case SubAction::SetV4Voltage:
        return executeSetV4Voltage(action);
    case SubAction::OpenV1Channel:
        return executeOpenV1Channel(action);
    case SubAction::OpenV4Channel:
        return executeOpenV4Channel();
    case SubAction::StartDetection:
        return executeStartDetection();
    case SubAction::PauseDetection:
        return executePauseDetection();
    case SubAction::CheckCurrent:
        return executeCheckCurrent(action);
    case SubAction::PressKey:
        return executePressKey(action);
    case SubAction::Delay:
        return executeDelay(action);
    case SubAction::UserConfirm:
        return executeUserConfirm(action);
    case SubAction::OpenChannel:
        return executeOpenChannel(action);
    default:
        log(tr("未知的子动作类型: %1").arg(static_cast<int>(action.type)));
        return true;
    }
}

bool TestSequenceRunner::executeSetV1Voltage(const SubAction &action)
{
    log(tr("设置V1电压: %1V (通道0x%2)")
            .arg(action.v1Value, 0, 'f', 2)
            .arg(action.v1Channel, 2, 16, QChar('0')));

    // 发送V1电压控制命令：0x02 + 通道ID + 电压BCD
    bool success = m_deviceController->setV123VoltageControl(action.v1Channel, action.v1Value);

    if (!success)
    {
        log(tr("设置V1电压失败"));
        return true; // 发送失败，立即完成（会被标记为失败）
    }

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false; // 需要等待ACK
}

bool TestSequenceRunner::executeSetV4Voltage(const SubAction &action)
{
    log(tr("设置V4电压: %1V")
            .arg(action.v2Value, 0, 'f', 2));

    // 发送V4电压控制命令：0x02 + 0x04 + 电压码
    bool success = m_deviceController->setV4VoltageControl(action.v2Value);

    if (!success)
    {
        log(tr("设置V4电压失败"));
        return true;
    }

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

bool TestSequenceRunner::executeOpenV1Channel(const SubAction &action)
{
    log(tr("打开V1通道: 0x%1")
            .arg(action.v1Channel, 2, 16, QChar('0')));

    // 发送V1通道开启命令：0x12 + 通道ID
    bool success = m_deviceController->openV123Channel(action.v1Channel);

    if (!success)
    {
        log(tr("打开V1通道失败"));
        return true;
    }

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

bool TestSequenceRunner::executeOpenV4Channel()
{
    log(tr("打开V4通道: 0x04"));

    // 发送V4通道开启命令：0x12 + 0x04
    bool success = m_deviceController->openV4Channel();

    if (!success)
    {
        log(tr("打开V4通道失败"));
        return true;
    }

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

bool TestSequenceRunner::executeStartDetection()
{
    log(tr("开启外部电流表连续检测"));
    
    bool success = m_deviceController->startExternalMeterDetection();
    if (!success)
    {
        log(tr("开启外部电流表检测失败"));
        return true;  // 发送失败，立即完成
    }

    // 预先标记检测已激活
    m_isDetectionActive = true;
    log(tr("检测状态已标记为：激活"));

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

bool TestSequenceRunner::executePauseDetection()
{
    log(tr("停止外部电流表连续检测"));
    bool success = m_deviceController->stopExternalMeterDetection();
    if (!success)
    {
        log(tr("停止外部电流表检测失败"));
        return true;
    }

    // 标记检测已停止（预先设置）
    m_isDetectionActive = false;
    log(tr("检测状态已标记为：停止"));

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

bool TestSequenceRunner::executePressKey(const SubAction &action)
{
    QString keyName;
    bool success = false;

    switch (action.key)
    {
    case SubAction::KeyPowerConfirm:
        keyName = tr("开机/确认键");
        success = m_deviceController->pressPowerConfirmKey();
        break;
    case SubAction::KeyRight:
        keyName = tr("右键");
        success = m_deviceController->pressRightKey();
        break;
    case SubAction::KeySw3:
        keyName = tr("SW3");
        success = m_deviceController->pressSw3Key();
        break;
    case SubAction::KeySw4:
        keyName = tr("SW4");
        success = m_deviceController->pressSw4Key();
        break;
    case SubAction::KeySw5:
        keyName = tr("SW5");
        success = m_deviceController->pressSw5Key();
        break;
    case SubAction::KeySw6:
        keyName = tr("SW6");
        success = m_deviceController->pressSw6Key();
        break;
    default:
        keyName = tr("未知按键");
        break;
    }

    log(tr("模拟按键: %1").arg(keyName));
    if (!success)
    {
        log(tr("按键模拟失败"));
        return true;
    }

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

bool TestSequenceRunner::executeDelay(const SubAction &action)
{
    log(tr("延时等待: %1 ms").arg(action.delayMs));
    m_delayTimer->start(action.delayMs);
    return false; // 需要等待定时器回调
}

bool TestSequenceRunner::executeUserConfirm(const SubAction &action)
{
    log(tr("等待用户确认: %1").arg(action.confirmMessage));
    setState(State::WaitingForUser);
    emit userConfirmRequired(action.confirmMessage);
    return false; // 需要等待用户响应
}

bool TestSequenceRunner::executeCheckCurrent(const SubAction &action)
{
    QString compareOp = action.isUpperLimit ? "<=" : ">=";
    log(tr("开始电流检测, 阈值: %1 %2").arg(compareOp).arg(action.currentThreshold, 0, 'f', 3));

    m_pendingCurrentThreshold = action.currentThreshold;
    m_pendingIsUpperLimit = action.isUpperLimit;
    m_waitingForMeasurement = true;

    setState(State::WaitingForMeasurement);
    m_measurementTimer->start(kDefaultMeasurementTimeoutMs);

    return false; // 需要等待测量数据
}

bool TestSequenceRunner::executeOpenChannel(const SubAction &action)
{
    log(tr("开启通道: V1通道=0x%1, V4通道=0x%2")
            .arg(action.openV1Channel, 2, 16, QChar('0'))
            .arg(action.openV4Channel, 2, 16, QChar('0')));

    bool success = m_deviceController->openVoltageChannel(action.openV1Channel, action.openV4Channel);
    if (!success)
    {
        log(tr("开启通道失败"));
        return true;
    }

    // 进入等待ACK状态
    setState(State::WaitingForAck);
    m_ackTimer->start(kDefaultAckTimeoutMs);
    return false;
}

void TestSequenceRunner::advanceToNextStep()
{
    m_stepTimer->stop();
    m_currentStepIndex++;
    m_currentActionIndex = -1;

    if (m_currentStepIndex >= m_steps.size())
    {
        finishSequence();
        return;
    }

    // 恢复运行状态（确保从失败/等待状态切换后能继续执行）
    setState(State::Running);

    const StepSpec &step = m_steps[m_currentStepIndex];
    emit stepStarted(m_currentStepIndex, step);
    log(tr("步骤 %1: %2").arg(step.id).arg(step.name));

    // 启动新步骤的超时定时器
    m_stepTimer->start(step.stepTimeoutMs > 0 ? step.stepTimeoutMs : kDefaultStepTimeoutMs);

    // 开始执行第一个子动作
    QTimer::singleShot(kActionDelayMs, this, &TestSequenceRunner::executeNextAction);
}

void TestSequenceRunner::finishCurrentStep(bool success, const QString &message)
{
    m_stepTimer->stop();

    if (m_currentStepIndex >= 0 && m_currentStepIndex < m_stepResults.size())
    {
        m_stepResults[m_currentStepIndex] = success;
    }

    QString resultStr = success ? tr("通过") : tr("失败");
    log(tr("步骤 %1 %2: %3").arg(m_currentStepIndex + 1).arg(resultStr).arg(message));

    emit stepFinished(m_currentStepIndex, success, message);

    // 前进到下一个步骤
    advanceToNextStep();
}

void TestSequenceRunner::finishSequence()
{
    m_delayTimer->stop();
    m_measurementTimer->stop();
    m_stepTimer->stop();
    m_ackTimer->stop();

    int passedCount = m_stepResults.count(true);
    int totalCount = m_steps.size();
    bool allPassed = (passedCount == totalCount);

    log(tr("========== 测试序列完成 =========="));
    log(tr("结果: %1/%2 步骤通过").arg(passedCount).arg(totalCount));

    setState(State::Finished);
    emit sequenceFinished(allPassed, passedCount, totalCount);
}

void TestSequenceRunner::log(const QString &message)
{
    qDebug() << "[TestSequenceRunner]" << message;
    emit logMessage(message);
}

void TestSequenceRunner::recordError(const QString &actionDesc, const QString &errorType,
                                      const QString &errorDetail,
                                      double measuredValue, double thresholdValue)
{
    QString stepName;
    if (m_currentStepIndex >= 0 && m_currentStepIndex < m_steps.size()) {
        stepName = m_steps[m_currentStepIndex].name;
    } else {
        stepName = tr("未知步骤");
    }

    ErrorRecord record(m_currentStepIndex, stepName, m_currentActionIndex,
                       actionDesc, errorType, errorDetail,
                       measuredValue, thresholdValue);
    m_errorRecords.append(record);

    log(tr("[错误记录] 步骤%1 - %2: %3").arg(m_currentStepIndex + 1).arg(errorType).arg(errorDetail));
}
