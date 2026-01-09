#ifndef ERRORRECORDDIALOG_H
#define ERRORRECORDDIALOG_H

#include <QDialog>
#include <QVector>
#include "domain/ErrorRecord.h"

class QTableWidget;
class QPushButton;

/**
 * @brief 错误记录查看对话框
 * 
 * 以表格形式展示测试过程中记录的所有错误
 */
class ErrorRecordDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param records 错误记录列表
     * @param parent 父窗口
     */
    explicit ErrorRecordDialog(const QVector<ErrorRecord> &records, QWidget *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ErrorRecordDialog() override = default;

private:
    /**
     * @brief 初始化UI
     */
    void initUI();

    /**
     * @brief 加载数据到表格
     */
    void loadData();

private:
    QVector<ErrorRecord> m_records;     ///< 错误记录列表
    QTableWidget *m_table;              ///< 错误记录表格
    QPushButton *m_closeButton;         ///< 关闭按钮
};

#endif // ERRORRECORDDIALOG_H
