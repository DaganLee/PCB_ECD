#ifndef MEASUREMENTCHARTWIDGET_H
#define MEASUREMENTCHARTWIDGET_H

#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsLineItem>
#include <QPushButton>
#include <QVector>
#include "domain/Measurement.h"

QT_CHARTS_USE_NAMESPACE

class InteractiveChartView;

/**
 * @brief 测量数据图表组件
 * 
 * 独立的图表组件，用于显示电流测量数据曲线。
 * 
 * 功能：
 * - 实时显示测量数据曲线
 * - 支持鼠标悬停查看数据点
 * - 支持右键标定两个点并计算区间平均值
 * - 支持重置曲线数据
 * - 支持滚轮缩放和拖拽平移
 */
class MeasurementChartWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针
     */
    explicit MeasurementChartWidget(QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~MeasurementChartWidget();

    /**
     * @brief 添加测量数据点到曲线
     * @param measurement 测量数据
     */
    void appendMeasurement(const Measurement &measurement);

    /**
     * @brief 重置图表（清空数据和标定点）
     */
    void resetChart();

    /**
     * @brief 清空曲线数据（不重置标定点）
     */
    void clearData();

    /**
     * @brief 获取当前测量次数
     * @return 测量次数
     */
    qint64 measurementCount() const;

    /**
     * @brief 获取当前平均值
     * @return 平均值，无数据返回0.0
     */
    double averageValue() const;

    /**
     * @brief 是否有标定范围
     * @return 是否有两个标定点
     */
    bool hasMarkedRange() const;

    /**
     * @brief 获取标定范围
     * @return 标定点索引对（起始，结束），无标定返回(-1, -1)
     */
    QPair<int, int> markedRange() const;

signals:
    /**
     * @brief 日志消息信号（替代直接调用外部日志函数）
     * @param message 日志消息
     */
    void logMessage(const QString &message);

    /**
     * @brief 测量数据添加后发射
     * @param count 当前测量次数
     * @param value 测量值
     */
    void measurementAdded(qint64 count, double value);

    /**
     * @brief 图表重置后发射
     */
    void chartReset();

    /**
     * @brief 区间标定完成后发射
     * @param startIndex 起始索引
     * @param endIndex 结束索引
     * @param avgValue 区间平均值
     */
    void rangeMarked(int startIndex, int endIndex, double avgValue);

private slots:
    /**
     * @brief 处理曲线上的鼠标悬停事件
     * @param point 悬停点的坐标
     * @param state 悬停状态（true=进入，false=离开）
     */
    void onSeriesHovered(const QPointF &point, bool state);

    /**
     * @brief 处理重置按钮点击
     */
    void onResetButtonClicked();

    /**
     * @brief 处理图表上的右键点击事件
     * @param point 点击点的坐标（数值坐标）
     */
    void onChartRightClicked(const QPointF &point);

    /**
     * @brief 更新标定竖线的位置（响应视口变化）
     */
    void updateMarkerLines();

private:
    /**
     * @brief 初始化图表组件
     */
    void initChart();

    /**
     * @brief 更新平均值显示（根据是否有标定点）
     */
    void updateAverageDisplay();

    /**
     * @brief 重写尺寸改变事件，动态调整重置按钮位置
     */
    void resizeEvent(QResizeEvent *event) override;

private:
    // 图表核心组件
    QChart *m_chart = nullptr;                     ///< 图表对象
    InteractiveChartView *m_chartView = nullptr;   ///< 图表视图（支持交互）
    QLineSeries *m_series = nullptr;               ///< 数据系列
    QValueAxis *m_axisX = nullptr;                 ///< X轴（测量次数）
    QValueAxis *m_axisY = nullptr;                 ///< Y轴（电流值）

    // 图形项
    QGraphicsSimpleTextItem *m_avgTextItem = nullptr;   ///< 平均值文本显示
    QGraphicsSimpleTextItem *m_tooltipItem = nullptr;   ///< 悬停提示文本
    QGraphicsLineItem *m_crosshairLine = nullptr;       ///< 竖线指示器

    // 控件
    QPushButton *m_resetButton = nullptr;               ///< 重置按钮

    // 统计数据
    qint64 m_measurementCount = 0;                      ///< 测量次数计数器
    double m_totalCurrentSum = 0.0;                     ///< 电流值总和
    qint64 m_chartStartMs = 0;                          ///< 图表起始时间（ms）

    // 标定点相关
    QVector<int> m_markedIndices;                       ///< 标定点索引（最多2个）
    QVector<QGraphicsLineItem*> m_markerLines;          ///< 标定点竖线（最多2条）
};

#endif // MEASUREMENTCHARTWIDGET_H
