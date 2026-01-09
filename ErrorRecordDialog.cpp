#include "ErrorRecordDialog.h"
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>

ErrorRecordDialog::ErrorRecordDialog(const QVector<ErrorRecord> &records, QWidget *parent)
    : QDialog(parent)
    , m_records(records)
    , m_table(nullptr)
    , m_closeButton(nullptr)
{
    initUI();
    loadData();
}

void ErrorRecordDialog::initUI()
{
    setWindowTitle(tr("错误记录"));
    setMinimumSize(800, 400);
    resize(900, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);

    // 标题和统计
    QLabel *titleLabel = new QLabel(this);
    if (m_records.isEmpty()) {
        titleLabel->setText(tr("📋 本次测试没有错误记录"));
        titleLabel->setStyleSheet("font: bold 14pt; color: #27ae60;");
    } else {
        titleLabel->setText(tr("📋 共 %1 条错误记录").arg(m_records.size()));
        titleLabel->setStyleSheet("font: bold 14pt; color: #e74c3c;");
    }
    mainLayout->addWidget(titleLabel);

    // 表格
    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        tr("步骤"), tr("动作"), tr("错误类型"), 
        tr("详细信息"), tr("测量值"), tr("时间")
    });

    // 设置表头属性
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);  // 步骤
    header->setSectionResizeMode(1, QHeaderView::Interactive);       // 动作
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);  // 错误类型
    header->setSectionResizeMode(3, QHeaderView::Stretch);           // 详细信息
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);  // 测量值
    header->setSectionResizeMode(5, QHeaderView::ResizeToContents);  // 时间

    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setStyleSheet(
        "QTableWidget { font-size: 10pt; }"
        "QTableWidget::item:selected { background-color: #3498db; color: white; }"
    );

    mainLayout->addWidget(m_table, 1);

    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_closeButton = new QPushButton(tr("关闭"), this);
    m_closeButton->setStyleSheet(
        "QPushButton { font: 12pt; padding: 8px 25px; background-color: #7f8c8d; "
        "color: white; border-radius: 5px; }"
        "QPushButton:hover { background-color: #95a5a6; }"
    );
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_closeButton);

    mainLayout->addLayout(buttonLayout);
}

void ErrorRecordDialog::loadData()
{
    m_table->setRowCount(m_records.size());

    for (int i = 0; i < m_records.size(); ++i) {
        const ErrorRecord &record = m_records[i];

        // 步骤
        QString stepText = tr("第%1步: %2").arg(record.stepIndex + 1).arg(record.stepName);
        QTableWidgetItem *stepItem = new QTableWidgetItem(stepText);
        m_table->setItem(i, 0, stepItem);

        // 动作
        QTableWidgetItem *actionItem = new QTableWidgetItem(record.actionDescription);
        m_table->setItem(i, 1, actionItem);

        // 错误类型
        QTableWidgetItem *typeItem = new QTableWidgetItem(record.errorType);
        typeItem->setForeground(QBrush(QColor("#e74c3c")));  // 红色
        m_table->setItem(i, 2, typeItem);

        // 详细信息
        QTableWidgetItem *detailItem = new QTableWidgetItem(record.errorDetail);
        m_table->setItem(i, 3, detailItem);

        // 测量值
        QString measureText;
        if (record.hasMeasurementData()) {
            measureText = tr("%1 / %2 mA")
                .arg(record.measuredValue, 0, 'f', 3)
                .arg(record.thresholdValue, 0, 'f', 3);
        } else {
            measureText = "-";
        }
        QTableWidgetItem *measureItem = new QTableWidgetItem(measureText);
        measureItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 4, measureItem);

        // 时间
        QString timeText = record.timestamp.toString("hh:mm:ss");
        QTableWidgetItem *timeItem = new QTableWidgetItem(timeText);
        timeItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 5, timeItem);
    }

    // 调整列宽
    m_table->resizeColumnsToContents();
    // 确保动作列有足够宽度
    if (m_table->columnWidth(1) < 150) {
        m_table->setColumnWidth(1, 150);
    }
}
