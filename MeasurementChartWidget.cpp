#include "MeasurementChartWidget.h"
#include "InteractiveChartView.h"
#include <QVBoxLayout>
#include <QDateTime>
#include <QFont>
#include <QPen>
#include <QBrush>

MeasurementChartWidget::MeasurementChartWidget(QWidget *parent)
    : QWidget(parent)
{
    initChart();
    m_chartStartMs = QDateTime::currentMSecsSinceEpoch();
}

MeasurementChartWidget::~MeasurementChartWidget()
{
    // 清理标定线
    for (QGraphicsLineItem *line : m_markerLines)
    {
        if (line)
        {
            delete line;
        }
    }
    m_markerLines.clear();
}

void MeasurementChartWidget::initChart()
{
    // 创建数据系列
    m_series = new QLineSeries();

    // 创建图表
    m_chart = new QChart();
    m_chart->addSeries(m_series);

    // 创建坐标轴
    m_axisX = new QValueAxis();
    m_axisY = new QValueAxis();

    // 设置字体
    QFont axisLabelFont = QFont(QStringLiteral("Arial"), 14);
    QFont axisTitleFont = QFont(QStringLiteral("Arial"), 16, QFont::Bold);
    m_axisX->setLabelsFont(axisLabelFont);
    m_axisY->setLabelsFont(axisLabelFont);
    m_axisX->setTitleText("Measurement Count");
    m_axisY->setTitleText("Current");
    m_axisX->setTitleFont(axisTitleFont);
    m_axisY->setTitleFont(axisTitleFont);
    m_axisX->setRange(0.0, 100.0);
    m_axisY->setRange(0.0, 1.0);

    // 添加坐标轴到图表
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisX);
    m_series->attachAxis(m_axisY);

    // 创建交互式图表视图
    m_chartView = new InteractiveChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);

    // 设置布局
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_chartView);

    // 初始化测量计数器
    m_measurementCount = 0;
    m_totalCurrentSum = 0.0;

    // 初始化平均值文本项
    m_avgTextItem = new QGraphicsSimpleTextItem(m_chart);
    QFont avgFont(QStringLiteral("Arial"), 14, QFont::Bold);
    m_avgTextItem->setFont(avgFont);
    m_avgTextItem->setBrush(QBrush(Qt::darkBlue));
    m_avgTextItem->setText("Avg: 0.000");
    m_avgTextItem->setZValue(11);

    // 初始化悬停提示文本项
    m_tooltipItem = new QGraphicsSimpleTextItem(m_chart);
    QFont tooltipFont(QStringLiteral("Arial"), 12);
    m_tooltipItem->setFont(tooltipFont);
    m_tooltipItem->setBrush(QBrush(Qt::black));
    m_tooltipItem->setVisible(false);
    m_tooltipItem->setZValue(12);

    // 初始化竖线指示器
    m_crosshairLine = new QGraphicsLineItem(m_chart);
    QPen crosshairPen(Qt::red, 1, Qt::DashLine);
    m_crosshairLine->setPen(crosshairPen);
    m_crosshairLine->setVisible(false);
    m_crosshairLine->setZValue(10);

    // 创建重置按钮
    m_resetButton = new QPushButton(tr("重置"), this);
    m_resetButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #4CAF50;"
        "    color: white;"
        "    border: none;"
        "    padding: 5px 15px;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #45a049;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #3d8b40;"
        "}");
    m_resetButton->setCursor(Qt::PointingHandCursor);
    m_resetButton->setFixedSize(60, 30);
    // 初始位置会在resizeEvent中调整
    m_resetButton->raise();

    // 连接信号槽
    connect(m_resetButton, &QPushButton::clicked,
            this, &MeasurementChartWidget::onResetButtonClicked);

    connect(m_series, &QLineSeries::hovered,
            this, &MeasurementChartWidget::onSeriesHovered);

    connect(m_chartView, &InteractiveChartView::rightClicked,
            this, &MeasurementChartWidget::onChartRightClicked);

    connect(m_chartView, &InteractiveChartView::viewportChanged,
            this, &MeasurementChartWidget::updateMarkerLines);
}

void MeasurementChartWidget::appendMeasurement(const Measurement &measurement)
{
    if (!m_series || !m_axisX || !m_axisY || !m_avgTextItem)
        return;

    // 1. 更新测量次数和累加电流值
    m_measurementCount++;
    double yValue = measurement.displayNumber();
    m_totalCurrentSum += yValue;

    // 2. 计算平均值
    double avgValue = m_totalCurrentSum / m_measurementCount;

    // 3. 添加数据点
    m_series->append(static_cast<double>(m_measurementCount), yValue);

    // 更新单位显示
    m_axisY->setTitleText(QString("Current (%1)").arg(measurement.unit()));

    // 4. 动态调整X轴范围
    if (m_measurementCount > m_axisX->max())
    {
        m_axisX->setRange(0.0, m_measurementCount + 20);
    }
    else
    {
        m_axisX->setRange(0.0, qMax(100.0, static_cast<double>(m_measurementCount)));
    }

    // 5. 动态调整Y轴范围
    double axisYMax = yValue * 1.2;
    if (axisYMax < 1.0)
        axisYMax = 1.0;
    m_axisY->setRange(0.0, axisYMax);

    // 6. 更新平均值显示
    QString avgText = QString("Avg: %1 %2")
                          .arg(avgValue, 0, 'f', 3)
                          .arg(measurement.unit());
    m_avgTextItem->setText(avgText);

    // 7. 调整平均值文本位置
    QRectF plotArea = m_chart->plotArea();
    qreal textWidth = m_avgTextItem->boundingRect().width();
    m_avgTextItem->setPos(plotArea.right() - textWidth - 10,
                          plotArea.top() + 10);

    // 8. 控制点数
    const int maxPoints = 2000;
    if (m_series->count() > maxPoints)
    {
        m_series->removePoints(0, m_series->count() - maxPoints);
    }

    // 9. 更新标定竖线位置
    updateMarkerLines();

    // 10. 发射信号
    emit measurementAdded(m_measurementCount, yValue);
}

void MeasurementChartWidget::resetChart()
{
    if (!m_series || !m_axisX || !m_axisY)
        return;

    // 1. 清空曲线数据
    m_series->clear();

    // 2. 重置计数器
    m_measurementCount = 0;
    m_totalCurrentSum = 0.0;

    // 3. 重置坐标轴
    m_axisX->setRange(0.0, 100.0);
    m_axisY->setRange(0.0, 1.0);

    // 4. 更新平均值显示
    if (m_avgTextItem)
    {
        m_avgTextItem->setText("Avg: 0.000");
    }

    // 5. 隐藏提示和竖线
    if (m_tooltipItem)
    {
        m_tooltipItem->setVisible(false);
    }
    if (m_crosshairLine)
    {
        m_crosshairLine->setVisible(false);
    }

    // 6. 清理标定点
    m_markedIndices.clear();
    for (QGraphicsLineItem *line : m_markerLines)
    {
        if (line)
        {
            delete line;
        }
    }
    m_markerLines.clear();

    // 7. 重置起始时间
    m_chartStartMs = QDateTime::currentDateTime().toMSecsSinceEpoch();

    // 8. 发射信号
    emit chartReset();
}

void MeasurementChartWidget::clearData()
{
    if (!m_series)
        return;

    m_series->clear();
    m_measurementCount = 0;
    m_totalCurrentSum = 0.0;

    if (m_avgTextItem)
    {
        m_avgTextItem->setText("Avg: 0.000");
    }
}

qint64 MeasurementChartWidget::measurementCount() const
{
    return m_measurementCount;
}

double MeasurementChartWidget::averageValue() const
{
    if (m_measurementCount > 0)
    {
        return m_totalCurrentSum / m_measurementCount;
    }
    return 0.0;
}

bool MeasurementChartWidget::hasMarkedRange() const
{
    return m_markedIndices.size() == 2;
}

QPair<int, int> MeasurementChartWidget::markedRange() const
{
    if (m_markedIndices.size() == 2)
    {
        int start = qMin(m_markedIndices[0], m_markedIndices[1]);
        int end = qMax(m_markedIndices[0], m_markedIndices[1]);
        return qMakePair(start, end);
    }
    return qMakePair(-1, -1);
}

void MeasurementChartWidget::onSeriesHovered(const QPointF &point, bool state)
{
    if (!m_tooltipItem || !m_crosshairLine)
        return;

    if (state)
    {
        // 计算最近的整数索引
        int index = qRound(point.x()) - 1;

        // 边界检查
        if (index < 0 || index >= m_series->count())
        {
            return;
        }

        // 获取真实数据点
        QPointF actualPoint = m_series->at(index);

        // 更新提示文本
        QString tooltipText = QString("Count: %1\nCurrent: %2")
                                  .arg((int)actualPoint.x())
                                  .arg(actualPoint.y(), 0, 'f', 3);
        m_tooltipItem->setText(tooltipText);
        m_tooltipItem->setVisible(true);

        // 转换为图表坐标
        QPointF chartPoint = m_chart->mapToPosition(actualPoint, m_series);

        // 获取绘图区域
        QRectF plotArea = m_chart->plotArea();

        // 显示竖线
        m_crosshairLine->setLine(chartPoint.x(), plotArea.top(),
                                 chartPoint.x(), plotArea.bottom());
        m_crosshairLine->setVisible(true);

        // 计算提示框位置
        qreal tooltipWidth = m_tooltipItem->boundingRect().width();
        qreal tooltipHeight = m_tooltipItem->boundingRect().height();

        qreal xPos = chartPoint.x() + 10;
        qreal yPos = chartPoint.y() - tooltipHeight - 10;

        // 边界检查
        if (xPos + tooltipWidth > plotArea.right())
        {
            xPos = chartPoint.x() - tooltipWidth - 10;
        }
        if (yPos < plotArea.top())
        {
            yPos = chartPoint.y() + 10;
        }

        m_tooltipItem->setPos(xPos, yPos);
    }
    else
    {
        m_tooltipItem->setVisible(false);
        m_crosshairLine->setVisible(false);
    }
}

void MeasurementChartWidget::onResetButtonClicked()
{
    resetChart();
}

void MeasurementChartWidget::onChartRightClicked(const QPointF &point)
{
    if (!m_series || !m_chart)
        return;

    // 1. 计算最近的数据点索引
    int index = qRound(point.x()) - 1;

    // 2. 边界检查
    if (index < 0 || index >= m_series->count())
    {
        return;
    }

    // 3. 检查是否已经标定过这个点
    int existingPos = m_markedIndices.indexOf(index);

    if (existingPos != -1)
    {
        // 取消标定
        m_markedIndices.removeAt(existingPos);

        if (existingPos < m_markerLines.size() && m_markerLines[existingPos])
        {
            delete m_markerLines[existingPos];
            m_markerLines.removeAt(existingPos);
        }
    }
    else
    {
        // 添加标定
        if (m_markedIndices.size() >= 2)
        {
            // 替换最早的标定点
            m_markedIndices.removeFirst();

            if (!m_markerLines.isEmpty() && m_markerLines.first())
            {
                delete m_markerLines.first();
                m_markerLines.removeFirst();
            }
        }

        m_markedIndices.append(index);

        // 创建绿色竖线
        QGraphicsLineItem *markerLine = new QGraphicsLineItem(m_chart);
        QPen markerPen(Qt::green, 2, Qt::SolidLine);
        markerLine->setPen(markerPen);
        markerLine->setZValue(9);
        m_markerLines.append(markerLine);
    }

    // 4. 更新标定竖线位置
    updateMarkerLines();

    // 5. 更新平均值显示
    updateAverageDisplay();

    // 6. 发射区间标定信号
    if (m_markedIndices.size() == 2)
    {
        int startIndex = qMin(m_markedIndices[0], m_markedIndices[1]);
        int endIndex = qMax(m_markedIndices[0], m_markedIndices[1]);

        double sum = 0.0;
        int count = 0;
        for (int i = startIndex; i <= endIndex; ++i)
        {
            if (i >= 0 && i < m_series->count())
            {
                sum += m_series->at(i).y();
                count++;
            }
        }
        double avgValue = (count > 0) ? (sum / count) : 0.0;

        emit rangeMarked(startIndex, endIndex, avgValue);
    }
}

void MeasurementChartWidget::updateMarkerLines()
{
    if (!m_series || !m_chart)
        return;

    for (int i = 0; i < m_markedIndices.size() && i < m_markerLines.size(); ++i)
    {
        int index = m_markedIndices[i];

        if (index < 0 || index >= m_series->count())
            continue;

        QGraphicsLineItem *markerLine = m_markerLines[i];
        if (!markerLine)
            continue;

        QPointF actualPoint = m_series->at(index);
        QPointF chartPoint = m_chart->mapToPosition(actualPoint, m_series);
        QRectF plotArea = m_chart->plotArea();

        markerLine->setLine(chartPoint.x(), plotArea.top(),
                            chartPoint.x(), plotArea.bottom());
    }
}

void MeasurementChartWidget::updateAverageDisplay()
{
    if (!m_avgTextItem || !m_series)
        return;

    QString avgText;

    if (m_markedIndices.size() == 2)
    {
        // 计算区间平均值
        int startIndex = qMin(m_markedIndices[0], m_markedIndices[1]);
        int endIndex = qMax(m_markedIndices[0], m_markedIndices[1]);

        double sum = 0.0;
        int count = 0;

        for (int i = startIndex; i <= endIndex; ++i)
        {
            if (i >= 0 && i < m_series->count())
            {
                sum += m_series->at(i).y();
                count++;
            }
        }

        double rangeAvg = (count > 0) ? (sum / count) : 0.0;

        // 获取单位
        QString unit = "";
        QString yTitle = m_axisY->titleText();
        int startPos = yTitle.indexOf('(');
        int endPos = yTitle.indexOf(')');
        if (startPos != -1 && endPos != -1 && endPos > startPos)
        {
            unit = yTitle.mid(startPos + 1, endPos - startPos - 1);
        }

        avgText = QString("Range Avg: %1 %2")
                      .arg(rangeAvg, 0, 'f', 3)
                      .arg(unit);
    }
    else
    {
        // 显示全局平均值
        double globalAvg = (m_measurementCount > 0) ? (m_totalCurrentSum / m_measurementCount) : 0.0;

        QString unit = "";
        QString yTitle = m_axisY->titleText();
        int startPos = yTitle.indexOf('(');
        int endPos = yTitle.indexOf(')');
        if (startPos != -1 && endPos != -1 && endPos > startPos)
        {
            unit = yTitle.mid(startPos + 1, endPos - startPos - 1);
        }

        avgText = QString("Avg: %1 %2")
                      .arg(globalAvg, 0, 'f', 3)
                      .arg(unit);
    }

    m_avgTextItem->setText(avgText);
}

void MeasurementChartWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    
    // 动态调整重置按钮位置到右上角
    if (m_resetButton)
    {
        m_resetButton->move(this->width() - m_resetButton->width() - 10, 10);
    }
}
