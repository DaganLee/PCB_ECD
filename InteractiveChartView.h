#ifndef INTERACTIVECHARTVIEW_H
#define INTERACTIVECHARTVIEW_H

#include <QtCharts/QChartView>
#include <QWheelEvent>

QT_CHARTS_USE_NAMESPACE

/**
 * @brief 支持交互式缩放的ChartView
 * 
 * 功能：
 * - 鼠标滚轮以当前位置为中心进行缩放
 */
class InteractiveChartView : public QChartView
{
    Q_OBJECT

public:
    explicit InteractiveChartView(QChart *chart, QWidget *parent = nullptr);
    explicit InteractiveChartView(QWidget *parent = nullptr);

signals:
    /**
     * @brief 右键点击图表上的点时发射
     * @param point 点击点的数值坐标
     */
    void rightClicked(const QPointF &point);
    
    /**
     * @brief 图表视口变化时发射（缩放或平移）
     */
    void viewportChanged();

protected:
    /**
     * @brief 重写鼠标滚轮事件，实现以鼠标位置为中心的缩放
     */
    void wheelEvent(QWheelEvent *event) override;
    
    /**
     * @brief 重写鼠标按下事件，开始拖拽
     */
    void mousePressEvent(QMouseEvent *event) override;
    
    /**
     * @brief 重写鼠标移动事件，执行拖拽平移
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    
    /**
     * @brief 重写鼠标释放事件，结束拖拽
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    static constexpr double ZOOM_FACTOR = 0.1; // 缩放因子
    
    bool m_isDragging = false;      // 是否正在拖拽
    QPoint m_lastMousePos;          // 上一次鼠标位置
};

#endif // INTERACTIVECHARTVIEW_H
