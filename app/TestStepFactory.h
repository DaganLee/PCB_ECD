#ifndef TESTSTEPFACTORY_H
#define TESTSTEPFACTORY_H

#include <QObject>
#include "domain/StepSpec.h"
#include <QVector>

/**
 * @brief 测试步骤工厂类
 *
 * 职责：
 * - 创建预定义的测试步骤序列
 * - 提供标准PCBA测试流程的步骤配置
 */
class TestStepFactory
{
public:
    /**
     * @brief 创建完整的PCBA测试序列（4个步骤）
     *
     * @return 测试步骤列表
     */
    static QVector<StepSpec> createPcbaTestSequence();

    /**
     * @brief 创建步骤一：静态电流与开机测试
     */
    static StepSpec createStep1_StaticCurrentAndPowerOn();

    /**
     * @brief 创建步骤二：过压保护测试
     */
    static StepSpec createStep2_OvervoltageProtection();

    /**
     * @brief 创建步骤三：正常电压测试
     */
    static StepSpec createStep3_NormalVoltageTest();

    /**
     * @brief 创建步骤四：低电量关机测试
     */
    static StepSpec createStep4_LowBatteryShutdown();
};

// ========== 实现 ==========

inline QVector<StepSpec> TestStepFactory::createPcbaTestSequence()
{
    QVector<StepSpec> steps;
    steps.append(createStep1_StaticCurrentAndPowerOn());
    steps.append(createStep2_OvervoltageProtection());
    steps.append(createStep3_NormalVoltageTest());
    steps.append(createStep4_LowBatteryShutdown());
    return steps;
}

inline StepSpec TestStepFactory::createStep1_StaticCurrentAndPowerOn()
{
    // 构造函数
    StepSpec step(1, QObject::tr("第一步测试"),
                  QObject::tr("V1=2.2V, V2=2.9V, 检测关机电流≤5uA，开机后蜂鸣器响时检测工作电流≤160mA"));

    // 1. 打开V1（v1默认）通道：0x12 0x01
    step.addAction(SubAction::createOpenV1Channel(0x01));

    // 2. 设置V1电压2.2V：0x02 0x01 0x22
    step.addAction(SubAction::createSetV1Voltage(2.2, 0x01));

    // 3. 打开V2（v4）通道：0x12 0x04
    step.addAction(SubAction::createOpenV4Channel());

    // 4. 设置V4电压2.9V：0x02 0x04 0x29
    step.addAction(SubAction::createSetV4Voltage(2.9));

    // 5. 开启检测
    step.addAction(SubAction::createStartDetection());

    // 6. 延时等待测量稳定
    step.addAction(SubAction::createDelay(8000));

    // 7. 检测关机电流 ≤ 5uA (0.005mA)
    step.addAction(SubAction::createCheckCurrent(0.005, true)); // <=0.005mA

    // 8. 暂停检测（停止外部电流表）
    step.addAction(SubAction::createPauseDetection());

    // 9. 短按【开机/确认键】开机
    step.addAction(SubAction::createPressKey(SubAction::KeyPowerConfirm));

    // 10. 延时等待开机（业务延时，等待设备启动）
    step.addAction(SubAction::createDelay(5000));

    // 11. 短按【右键】进入语言选择界面
    step.addAction(SubAction::createPressKey(SubAction::KeyRight));

    // 12. 延时等待界面切换（业务延时）
    step.addAction(SubAction::createDelay(1000));

    // 13. 用户确认：是否显示4格电量
    step.addAction(SubAction::createUserConfirm(QObject::tr("请确认电池是否显示4格电量？")));

    // 14. 重新开启检测（使用外部电流表）
    step.addAction(SubAction::createStartDetection());

    // 15. 延时等待测量稳定（业务延时）
    step.addAction(SubAction::createDelay(8000));

    // 16. 检测工作电流 ≤ 160mA（蜂鸣器响时）
    step.addAction(SubAction::createCheckCurrent(160.0, true)); // <=160mA

    // 17. 暂停检测（停止外部电流表）
    step.addAction(SubAction::createPauseDetection());

    step.stepTimeoutMs = 120000; // 2分钟超时
    return step;
}

inline StepSpec TestStepFactory::createStep2_OvervoltageProtection()
{
    StepSpec step(2, QObject::tr("第二步测试"),
                  QObject::tr("V1=2.4V, V2=5.5V, 按【开机/确认键】无法开机"));

    // 1. 设置V1电压2.4V：0x02 0x01 0x24
    step.addAction(SubAction::createSetV1Voltage(2.4, 0x01));

    // 2. 设置V4电压5.5V：0x02 0x04 0x55
    step.addAction(SubAction::createSetV4Voltage(5.5));

    // 3. 延时等待电压稳定（业务延时）
    step.addAction(SubAction::createDelay(500));

    // 4. 短按【开机/确认键】尝试开机
    step.addAction(SubAction::createPressKey(SubAction::KeyPowerConfirm));

    // 5. 延时等待（业务延时，等待设备响应）
    step.addAction(SubAction::createDelay(3000));

    // 6. 用户确认：设备是否无法开机（人工策略）
    step.addAction(SubAction::createUserConfirm(QObject::tr("请确认设备是否未开机？")));

    step.stepTimeoutMs = 120000; // 2分钟超时
    return step;
}

inline StepSpec TestStepFactory::createStep3_NormalVoltageTest()
{
    StepSpec step(3, QObject::tr("第三步测试"),
                  QObject::tr("V1=2.4V, V2=3.9V, 开机后显示3格电量，检测工作电流≤120mA"));

    // 1. 设置V4电压3.9V：0x02 0x04 0x39
    step.addAction(SubAction::createSetV4Voltage(3.9));

    // 2. 延时等待电压稳定（业务延时）
    step.addAction(SubAction::createDelay(500));

    // 3. 短按【开机/确认键】开机
    step.addAction(SubAction::createPressKey(SubAction::KeyPowerConfirm));

    // 4. 延时等待开机（业务延时，等待设备启动）
    step.addAction(SubAction::createDelay(5000));

    // 5. 短按【右键】进入测量界面
    step.addAction(SubAction::createPressKey(SubAction::KeyRight));

    // 6. 延时等待界面切换（业务延时）
    step.addAction(SubAction::createDelay(1000));

    // 7. 用户确认：是否显示3格电量
    step.addAction(SubAction::createUserConfirm(QObject::tr("请确认是否显示3格电量？")));

    // 8. 开启检测
    step.addAction(SubAction::createStartDetection());

    // 9. 延时等待测量稳定（业务延时，必须保留）
    step.addAction(SubAction::createDelay(8000));

    // 10. 检测工作电流 ≤ 120mA
    step.addAction(SubAction::createCheckCurrent(120.0, true)); // <=120mA

    // 11. 暂停检测
    step.addAction(SubAction::createPauseDetection());

    step.stepTimeoutMs = 120000; // 2分钟超时
    return step;
}

inline StepSpec TestStepFactory::createStep4_LowBatteryShutdown()
{
    StepSpec step(4, QObject::tr("第四步测试"),
                  QObject::tr("V1=2.4V, V2=2.9V, 等待自动关机，检测关机电流≤5uA"));

    // 1. 设置V4电压2.9V：0x02 0x04 0x29
    step.addAction(SubAction::createSetV4Voltage(2.9));

    // 2. 等待15秒（业务延时，设备显示电池没电提示并自动关机）
    step.addAction(SubAction::createDelay(15000));

    // 3. 开启检测（使用外部电流表）
    step.addAction(SubAction::createStartDetection());

    // 4. 延时等待测量稳定（业务延时，必须保留）
    step.addAction(SubAction::createDelay(8000));

    // 5. 检测关机电流 ≤ 5uA (0.005mA)
    step.addAction(SubAction::createCheckCurrent(0.005, true)); // <=0.005mA

    // 6. 暂停检测（停止外部电流表）
    step.addAction(SubAction::createPauseDetection());

    step.stepTimeoutMs = 120000; // 2分钟超时
    return step;
}

#endif // TESTSTEPFACTORY_H
