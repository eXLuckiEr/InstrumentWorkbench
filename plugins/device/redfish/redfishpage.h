#pragma once
#include <QHash>
#include <QWidget>

#include "iwp/devicemodel.h"

class QLabel;
class QTableWidget;
class QTableWidgetItem;

// Generic key/value table: every model key becomes a row.
class RedfishPage : public QWidget
{
    Q_OBJECT
public:
    RedfishPage(iwp::DeviceModel *model, QWidget *parent = nullptr);

private:
    struct Row {
        QTableWidgetItem *key = nullptr;
        QTableWidgetItem *value = nullptr;
    };
    void refresh(const QString &key);
    void showStatus(iwp::DeviceModel::Status status, const QString &message);

    iwp::DeviceModel *m_model;
    QLabel *m_status;
    QTableWidget *m_table;
    QHash<QString, Row> m_rows;
};
