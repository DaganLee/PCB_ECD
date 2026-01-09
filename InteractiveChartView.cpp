#include "InteractiveChartView.h"
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QMouseEvent>

InteractiveChartView::InteractiveChartView(QChart *chart, QWidget *parent)
    : QChartView(chart, parent)
{
}

InteractiveChartView::InteractiveChartView(QWidget *parent)
    : QChartView(parent)
{
}

void InteractiveChartView::wheelEvent(QWheelEvent *event)
{
    if (!chart())
        return;

    // 获取绘图区域
    QRectF plotArea = chart()->plotArea();
    
    // 获取鼠标位置（相对于ChartView）
    QPointF mousePos = event->position();
    
    // 检查鼠标是否在绘图区域内
    if (!plotArea.contains(mousePos)) {
        event->accept();
        return;
    }

    // 获取滚轮滚动方向
    qreal factor = 1.0;
    if (event->angleDelta().y() > 0) {
        // 向上滚动 - 放大
        factor = 1.0 + ZOOM_FACTOR;
    } else {
        // 向下滚动 - 缩小
        factor = 1.0 - ZOOM_FACTOR;
    }

    // 获取当前坐标轴
    QList<QAbstractAxis *> axesX = chart()->axes(Qt::Horizontal);
    QList<QAbstractAxis *> axesY = chart()->axes(Qt::Vertical);

    if (axesX.isEmpty() || axesY.isEmpty())
        return;

    QValueAxis *axisX = qobject_cast<QValueAxis*>(axesX.first());
    QValueAxis *axisY = qobject_cast<QValueAxis*>(axesY.first());

    if (!axisX || !axisY)
        return;

    // 获取当前坐标轴范围
    qreal xMin = axisX->min();
    qreal xMax = axisX->max();
    qreal yMin = axisY->min();
    qreal yMax = axisY->max();

    // 计算鼠标在绘图区域中的相对位置 (0.0 ~ 1.0)
    qreal xRatio = (mousePos.x() - plotArea.left()) / plotArea.width();
    qreal yRatio = 1.0 - (mousePos.y() - plotArea.top()) / plotArea.height(); // Y轴反向

    // 计算鼠标位置对应的数据值
    qreal mouseValueX = xMin + xRatio * (xMax - xMin);
    qreal mouseValueY = yMin + yRatio * (yMax - yMin);

    // 计算新的范围大小
    qreal newXRange = (xMax - xMin) / factor;
    qreal newYRange = (yMax - yMin) / factor;

    // 根据鼠标位置的相对比例，计算新的最小最大值
    // 保持鼠标指向的数据点在缩放后仍位于相同的屏幕位置
    qreal newXMin = mouseValueX - newXRange * xRatio;
    qreal newXMax = mouseValueX + newXRange * (1.0 - xRatio);
    qreal newYMin = mouseValueY - newYRange * yRatio;
    qreal newYMax = mouseValueY + newYRange * (1.0 - yRatio);

    // 防止过度缩小（设置最小范围）
    if (newXMax - newXMin < 1.0) {
        return;
    }
    if (newYMax - newYMin < 0.001) {
        return;
    }

    // 防止X轴最小值小于0
    if (newXMin < 0) {
        qreal shift = -newXMin;
        newXMin = 0;
        newXMax += shift;
    }

    // 防止Y轴最小值小于0
    if (newYMin < 0) {
        qreal shift = -newYMin;
        newYMin = 0;
        newYMax += shift;
    }

    // 应用新的范围
    axisX->setRange(newXMin, newXMax);
    axisY->setRange(newYMin, newYMax);

    // 发射视口变化信号
    emit viewportChanged();

    event->accept();
}

void InteractiveChartView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && chart())
    {
        m_isDragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor); // 改变光标为抓手形状
        event->accept();
    }
    else
    {
        QChartView::mousePressEvent(event);
    }
}

void InteractiveChartView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging && chart())
    {
        // 计算鼠标移动的偏移量
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();

        // 使用 chart()->scroll() 方法平移图表
        // 注意：scroll 的参数是像素偏移，负值表示向相反方向滚动
        chart()->scroll(-delta.x(), delta.y());
        
        // 发射视口变化信号
        emit viewportChanged();

        event->accept();
    }
    else
    {
        QChartView::mouseMoveEvent(event);
    }
}

void InteractiveChartView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor); // 恢复默认光标
        event->accept();
    }
    else if (event->button() == Qt::RightButton && chart())
    {
        // 处理右键点击：将屏幕坐标转换为数值坐标
        QPointF scenePos = mapToScene(event->pos());
        QPointF chartPos = chart()->mapFromScene(scenePos);
        QPointF valuePos = chart()->mapToValue(chartPos);
        
        emit rightClicked(valuePos);
        event->accept();
    }
    else
    {
        QChartView::mouseReleaseEvent(event);
    }
}
