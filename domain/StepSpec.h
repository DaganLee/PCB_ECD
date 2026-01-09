#ifndef STEPSPEC_H
#define STEPSPEC_H

#include <QString>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

/**
 * @brief 自动测试子动作规格
 * 
 * 每个测试步骤可包含多个子动作，按顺序执行
 */
struct SubAction {
    /**
     * @brief 子动作类型枚举
     */
    enum Type {
        // 电压控制类
        SetV1Voltage,           ///< 单独设置V1电压（0x02 + 通道 + 电压）
        SetV4Voltage,           ///< 单独设置V4电压（0x02 + 0x04 + 电压）
        OpenV1Channel,          ///< 打开V1通道（0x12 + 通道）
        OpenV4Channel,          ///< 打开V4通道（0x12 + 0x04）
        
        // 检测控制类
        StartDetection,         ///< 开启检测（启动采样）
        PauseDetection,         ///< 暂停检测（停止采样）
        CheckCurrent,           ///< 检查电流（自动判定是否符合阈值）
        
        // 按键模拟类
        PressKey,               ///< 模拟按键（开机键/右键/确认键等）
        
        // 流程控制类
        Delay,                  ///< 延时等待
        UserConfirm,            ///< 用户交互确认（弹窗）
        
        // 通道控制类
        OpenChannel             ///< 开启电压输出通道
    };
    
    /**
     * @brief 按键类型枚举
     */
    enum KeyType {
        KeyNone = 0,
        KeyPowerConfirm = 0x03, ///< 开机/确认键
        KeyRight = 0x02,        ///< 右键
        KeySw3 = 0x31,
        KeySw4 = 0x41,
        KeySw5 = 0x51,
        KeySw6 = 0x61
    };
    
    Type type;                  ///< 子动作类型
    
    // 电压参数（SetVoltage用）
    double v1Value;             ///< V1电压值
    double v2Value;             ///< V2电压值
    uint8_t v1Channel;          ///< V1输出通道 (0x01=V1, 0x02=V2, 0x03=V3)
    
    // 按键参数（PressKey用）
    KeyType key;                ///< 按键类型
    
    // 延时参数（Delay用）
    int delayMs;                ///< 延时毫秒数
    
    // 电流检测参数（CheckCurrent用）
    double currentThreshold;    ///< 电流阈值（单位：默认mA）
    bool isUpperLimit;          ///< true: <=阈值为Pass; false: >=阈值为Pass
    
    // 用户交互参数（UserConfirm用）
    QString confirmMessage;     ///< 弹窗提示信息
    
    // 通道开启参数（OpenChannel用）
    uint8_t openV1Channel;      ///< 要开启的V1通道
    uint8_t openV4Channel;      ///< 要开启的V4通道（0x04）
    
    /**
     * @brief 默认构造函数
     */
    SubAction()
        : type(Delay)
        , v1Value(0.0)
        , v2Value(0.0)
        , v1Channel(0x01)
        , key(KeyNone)
        , delayMs(0)
        , currentThreshold(0.0)
        , isUpperLimit(true)
        , openV1Channel(0x01)
        , openV4Channel(0x04)
    {}
    
    // ========== 静态工厂方法构造子动作 ==========
    /**
     * @brief 创建开启检测子动作
     */
    static SubAction createStartDetection() {
        SubAction action;
        action.type = StartDetection;
        return action;
    }
    
    /**
     * @brief 创建暂停检测子动作
     */
    static SubAction createPauseDetection() {
        SubAction action;
        action.type = PauseDetection;
        return action;
    }
    
    /**
     * @brief 创建电流检测子动作
     * @param threshold 电流阈值
     * @param upperLimit true表示 <=threshold 为Pass
     */
    static SubAction createCheckCurrent(double threshold, bool upperLimit = true) {
        SubAction action;
        action.type = CheckCurrent;
        action.currentThreshold = threshold;
        action.isUpperLimit = upperLimit;
        return action;
    }
    
    /**
     * @brief 创建按键模拟子动作
     */
    static SubAction createPressKey(KeyType k) {
        SubAction action;
        action.type = PressKey;
        action.key = k;
        return action;
    }
    
    /**
     * @brief 创建延时子动作
     */
    static SubAction createDelay(int ms) {
        SubAction action;
        action.type = Delay;
        action.delayMs = ms;
        return action;
    }
    
    /**
     * @brief 创建用户确认子动作
     */
    static SubAction createUserConfirm(const QString &message) {
        SubAction action;
        action.type = UserConfirm;
        action.confirmMessage = message;
        return action;
    }
    
    /**
     * @brief 创建打开V1通道子动作（0x12 + 通道ID）
     * @param v1Ch V1通道ID (0x01=V1, 0x02=V2, 0x03=V3)
     */
    static SubAction createOpenV1Channel(uint8_t v1Ch) {
        SubAction action;
        action.type = OpenV1Channel;
        action.v1Channel = v1Ch;
        return action;
    }
    
    /**
     * @brief 创建打开V2（v4）通道子动作（0x12 + 0x04）
     */
    static SubAction createOpenV4Channel() {
        SubAction action;
        action.type = OpenV4Channel;
        return action;
    }
    
    /**
     * @brief 创建设置V1电压子动作（0x02 + 通道ID + 电压BCD）
     * @param voltage 电压值
     * @param v1Ch V1通道ID (0x01=V1, 0x02=V2, 0x03=V3)，默认使用v1通道
     */
    static SubAction createSetV1Voltage(double voltage, uint8_t v1Ch = 0x01) {
        SubAction action;
        action.type = SetV1Voltage;
        action.v1Value = voltage;
        action.v1Channel = v1Ch;
        return action;
    }
    
    /**
     * @brief 创建设置V4电压子动作（0x02 + 0x04 + 电压码）
     * @param voltage 电压值
     */
    static SubAction createSetV4Voltage(double voltage) {
        SubAction action;
        action.type = SetV4Voltage;
        action.v2Value = voltage;  // 使用v2Value存储V4电压
        return action;
    }
    
    // ========== JSON 序列化/反序列化 ==========
    
    /**
     * @brief 将子动作序列化为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["type"] = static_cast<int>(type);
        obj["v1Value"] = v1Value;
        obj["v2Value"] = v2Value;
        obj["v1Channel"] = v1Channel;
        obj["key"] = static_cast<int>(key);
        obj["delayMs"] = delayMs;
        obj["currentThreshold"] = currentThreshold;
        obj["isUpperLimit"] = isUpperLimit;
        obj["confirmMessage"] = confirmMessage;
        obj["openV1Channel"] = openV1Channel;
        obj["openV4Channel"] = openV4Channel;
        return obj;
    }
    
    /**
     * @brief 从JSON对象反序列化子动作
     */
    static SubAction fromJson(const QJsonObject &obj) {
        SubAction action;
        action.type = static_cast<Type>(obj["type"].toInt());
        action.v1Value = obj["v1Value"].toDouble();
        action.v2Value = obj["v2Value"].toDouble();
        action.v1Channel = static_cast<uint8_t>(obj["v1Channel"].toInt());
        action.key = static_cast<KeyType>(obj["key"].toInt());
        action.delayMs = obj["delayMs"].toInt();
        action.currentThreshold = obj["currentThreshold"].toDouble();
        action.isUpperLimit = obj["isUpperLimit"].toBool(true);
        action.confirmMessage = obj["confirmMessage"].toString();
        action.openV1Channel = static_cast<uint8_t>(obj["openV1Channel"].toInt());
        action.openV4Channel = static_cast<uint8_t>(obj["openV4Channel"].toInt());
        return action;
    }
};

/**
 * @brief 自动测试步骤规格
 * 
 * 数据驱动的测试步骤配置，每个步骤包含多个子动作
 */
struct StepSpec {
    int id;                             ///< 步骤编号（1~N）
    QString name;                       ///< 步骤名称（用于UI显示）
    QString description;                ///< 步骤描述
    QVector<SubAction> actions;         ///< 子动作列表（按顺序执行）
    
    // 超时设置
    int stepTimeoutMs;                  ///< 整个步骤的超时时间（毫秒），默认60000
    
    /**
     * @brief 默认构造函数
     */
    StepSpec()
        : id(0)
        , stepTimeoutMs(60000)
    {}
    
    /**
     * @brief 构造函数
     * @param stepId 步骤ID
     * @param stepName 步骤名称
     * @param desc 步骤描述
     */
    StepSpec(int stepId, const QString &stepName, const QString &desc = QString())
        : id(stepId)
        , name(stepName)
        , description(desc)
        , stepTimeoutMs(60000)
    {}
    
    /**
     * @brief 添加子动作
     */
    StepSpec& addAction(const SubAction &action) {
        actions.append(action);
        return *this;
    }
    
    // ========== JSON 序列化/反序列化 ==========
    
    /**
     * @brief 将步骤序列化为JSON对象
     */
    QJsonObject toJson() const {
        QJsonObject obj;
        obj["id"] = id;
        obj["name"] = name;
        obj["description"] = description;
        obj["stepTimeoutMs"] = stepTimeoutMs;
        
        QJsonArray actionsArray;
        for (const SubAction &action : actions) {
            actionsArray.append(action.toJson());
        }
        obj["actions"] = actionsArray;
        
        return obj;
    }
    
    /**
     * @brief 从JSON对象反序列化步骤
     */
    static StepSpec fromJson(const QJsonObject &obj) {
        StepSpec step;
        step.id = obj["id"].toInt();
        step.name = obj["name"].toString();
        step.description = obj["description"].toString();
        step.stepTimeoutMs = obj["stepTimeoutMs"].toInt(60000);
        
        QJsonArray actionsArray = obj["actions"].toArray();
        for (const QJsonValue &val : actionsArray) {
            step.actions.append(SubAction::fromJson(val.toObject()));
        }
        
        return step;
    }
    
    /**
     * @brief 将步骤列表序列化为JSON文档
     */
    static QJsonDocument stepsToJson(const QVector<StepSpec> &steps) {
        QJsonArray stepsArray;
        for (const StepSpec &step : steps) {
            stepsArray.append(step.toJson());
        }
        
        QJsonObject root;
        root["version"] = "1.0";
        root["steps"] = stepsArray;
        
        return QJsonDocument(root);
    }
    
    /**
     * @brief 从JSON文档反序列化步骤列表
     */
    static QVector<StepSpec> stepsFromJson(const QJsonDocument &doc) {
        QVector<StepSpec> steps;
        
        QJsonObject root = doc.object();
        QJsonArray stepsArray = root["steps"].toArray();
        
        for (const QJsonValue &val : stepsArray) {
            steps.append(StepSpec::fromJson(val.toObject()));
        }
        
        return steps;
    }
};

#endif // STEPSPEC_H

